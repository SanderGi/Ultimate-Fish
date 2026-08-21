#!/usr/bin/env python3
"""Import version-pinned trivial-reachability sidecars for certified rows.

Each import authenticates the sidecar against the exact output SHA-256 in the
existing version-pinned wave certificate.  It changes only the W/L/D display,
reachability sidecar binding, and supervision evidence; the certified UFTB and
its result certificate remain untouched.
"""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
from typing import Any, Iterator

import finalize_ultimate_aws_concrete_class as finalizer
import update_ultimate_tablebase_ledger as ledger


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "tablebases/README.md"
CONFIG = ROOT / "tools/tablebases/ultimate_aws_supervision.json"
REGION = "us-west-2"
BUCKET = "ultimatefish-info-20260808-a4e679c6-831688117652"
AUDITORS = {
    "68c329e8bd3d248468c176b6e2038c3e0285116250fa1d88459fe1069c9866c4": {
        "overlay": "da64848ffda3fda2256dcf4b9765b1437c979cf69e297083bd89684cf5164366",
        "inventory": "ba8d13582359fcb16c0312e54fb0f23073cd910f0dbf173329c5cbb583a5e143",
        "binary": "56b2ce467815b3a3726f1b068fad91d627012e71fdc24adef3e6bf4ce4c723ff",
    },
    "552b16528f17dc2c6bc4b816a5cb8c8eb13a88799830db71db1323a982570a6e": {
        "overlay": "4347b9593dba9b806a4cc0bbad7ab4aaf4118897ae59f40a017fd431e43016fc",
        "inventory": "ba8d13582359fcb16c0312e54fb0f23073cd910f0dbf173329c5cbb583a5e143",
        "binary": "7e6513eb68b4e6a1fef30f55d50972a8271d7156c542ca5af427267ee5a2d1f9",
    },
    "ba1f775d14c35ea5ba18e01e5b68efb280cb4ca5116d5a0fd94f620f49d10157": {
        "overlay": "039374c2c4cdaaf150dd0705711e29d5fd2b6d8e2f5e019bd53cd1b7e2f2fc46",
        "inventory": "ba8d13582359fcb16c0312e54fb0f23073cd910f0dbf173329c5cbb583a5e143",
        "binary": "64bfd07c98f14c1ecb970cc5b71dd4c42eef05c80211625f486dacf9b9401843",
    },
    "2e7fecc251cff69ddb2bfd956e2dfd40092d495cf45fb0ac652cc4f5a096e05d": {
        "overlay": "9c8d9de646e2d1447fa287f0acbd704129ed736044fd1b92899a4ed36ea17284",
        "inventory": "ba8d13582359fcb16c0312e54fb0f23073cd910f0dbf173329c5cbb583a5e143",
        "binary": "d2f4202810dd8006d3674e7e383c48c2015bc036dd84e081b59fec4fd27a2965",
    },
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def aws_json(arguments: list[str]) -> dict[str, Any]:
    value = json.loads(subprocess.check_output(["aws", *arguments], text=True))
    if not isinstance(value, dict):
        raise RuntimeError("AWS command did not return a JSON object")
    return value


def download(item: dict[str, Any], target: Path) -> None:
    aws_json([
        "s3api", "get-object", "--region", REGION,
        "--bucket", str(item["bucket"]), "--key", str(item["key"]),
        "--version-id", str(item["version_id"]), str(target),
        "--output", "json",
    ])
    if sha256(target) != str(item["sha256"]):
        raise ValueError(f"S3 restore SHA-256 residual: {item['key']}")


def parse_receipt(raw: str) -> dict[str, str]:
    parts = raw.split(",")
    if len(parts) != 3:
        raise ValueError("receipt must be FILENAME,SHA256,VERSION_ID")
    filename, digest, version = parts
    if Path(filename).name != filename or not filename.endswith(".uftb"):
        raise ValueError(f"invalid filename: {filename}")
    if len(digest) != 64 or any(char not in "0123456789abcdef" for char in digest):
        raise ValueError(f"invalid SHA-256: {digest}")
    stem = Path(filename).stem
    return {
        "filename": filename, "sha256": digest, "version_id": version,
        "bucket": BUCKET,
        "key": (f"results/trivial-reachability-v3/{filename}/sha256/"
                f"{digest}/{stem}.reachability-trivial-v3.txt"),
    }


@contextmanager
def locked(path: Path) -> Iterator[None]:
    lock = path.with_suffix(path.suffix + ".lock")
    with lock.open("a", encoding="utf-8") as stream:
        fcntl.flock(stream.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(stream.fileno(), fcntl.LOCK_UN)


def certificate_for(document: dict[str, Any], filename: str,
                    output_sha: str, temporary: Path
                    ) -> tuple[dict[str, Any] | None, dict[str, Any],
                               dict[str, Any]] | None:
    jobs = [job for job in document.get("jobs", [])
            if job.get("ledger_certifies") and
            filename in job.get("ledger_results", {})]
    matches: list[tuple[dict[str, Any], dict[str, Any], dict[str, Any]]] = []
    encoded = finalizer.concrete.encoded_filename(filename)
    for job in jobs:
        for index, item in enumerate(job.get("s3_certificates", [])):
            if not str(item.get("key", "")).endswith("wave-certificate.json"):
                continue
            target = temporary / f"certificate-{len(matches)}-{index}.json"
            download(item, target)
            certificate = json.loads(target.read_text())
            if certificate.get("schema") != finalizer.concrete.CERTIFICATE_SCHEMA:
                continue
            completed = [entry for entry in certificate.get("completed", [])
                         if entry.get("filename") == encoded and
                         entry.get("output", {}).get("sha256") == output_sha]
            if len(completed) == 1:
                matches.append((job, item, completed[0]))
    if len(matches) != 1:
        if matches:
            raise ValueError(
                f"{filename}: expected one matching output certificate, got "
                f"{len(matches)}")
        for index, binding in enumerate(
                document.get("ledger_result_certificates", [])):
            if (binding.get("filename") != filename or
                    binding.get("result_sha256") != output_sha):
                continue
            item = binding["certificate"]
            target = temporary / f"ledger-certificate-{index}.json"
            download(item, target)
            certificate = json.loads(target.read_text())
            completed = [entry for entry in certificate.get("completed", [])
                         if entry.get("filename") ==
                         finalizer.concrete.encoded_filename(filename) and
                         entry.get("output", {}).get("sha256") == output_sha]
            if len(completed) != 1:
                raise ValueError(f"{filename}: ledger certificate residual")
            return None, item, completed[0]
        return None
    return matches[0]


def certificate_from_storage(storage: str, filename: str, output_sha: str,
                             versions: list[dict[str, Any]], temporary: Path
                             ) -> tuple[None, dict[str, Any], dict[str, Any]] | None:
    """Authenticate a legacy-ledger row by its exact certificate VersionId."""
    match = re.search(
        r"result certificate sha256:([0-9a-f]{64}) is VersionId `([^`]+)`",
        storage)
    if match is None:
        match = re.search(
            r"(?:preservation |wave )?certificate sha256:([0-9a-f]{64}) "
            r"VersionId ([^; ]+)", storage)
    if match is None:
        return None
    digest, version = match.groups()
    candidates = [item for item in versions
                  if str(item.get("VersionId")) == version and
                  str(item.get("Key", "")).endswith(".json")]
    if len(candidates) != 1:
        raise ValueError(
            f"{filename}: result certificate VersionId residual")
    source = candidates[0]
    item = {"bucket": BUCKET, "key": str(source["Key"]),
            "version_id": version, "sha256": digest,
            "size": int(source["Size"])}
    target = temporary / f"storage-certificate-{digest}.json"
    download(item, target)
    certificate = json.loads(target.read_text())
    if certificate.get("schema") == "ultimate-local-tablebase-snapshot-v1":
        artifacts = [entry for entry in
                     certificate.get("manifest", {}).get("artifacts", [])
                     if Path(str(entry.get("path", ""))).name == filename and
                     int(entry.get("bytes", 0)) > 1024 and
                     entry.get("sha256") == output_sha]
        if len(artifacts) != 1:
            raise ValueError(f"{filename}: preservation artifact residual")
        return None, item, {"filename": filename,
                            "output": {"sha256": output_sha, "edges": 0}}
    if certificate.get("schema") == "ultimate-concrete-frontier-resume-result-v1":
        record = certificate.get("record", {})
        output = certificate.get("output", {})
        if (record.get("filename") != filename or
                output.get("sha256") != output_sha):
            raise ValueError(f"{filename}: direct result certificate residual")
        return None, item, {"filename": filename, "output": output}
    encoded = finalizer.concrete.encoded_filename(filename)
    completed = [entry for entry in certificate.get("completed", [])
                 if entry.get("filename") == encoded and
                 entry.get("output", {}).get("sha256") == output_sha]
    if len(completed) != 1:
        raise ValueError(f"{filename}: storage certificate output residual")
    return None, item, completed[0]


def import_one(document: dict[str, Any], receipt: dict[str, str],
               temporary: Path, versions: list[dict[str, Any]]) -> dict[str, Any]:
    filename = receipt["filename"]
    rows = {row.filename: row for row in ledger.entries(README.read_text())}
    row = rows[filename]
    if row.status != "certified" or row.result_kind != "concrete":
        raise ValueError(f"{filename}: trivial import requires certified concrete row")

    head = aws_json([
        "s3api", "head-object", "--region", REGION,
        "--bucket", receipt["bucket"], "--key", receipt["key"],
        "--version-id", receipt["version_id"], "--output", "json",
    ])
    metadata = head.get("Metadata", {})
    model_sha = str(metadata.get("model-sha256", ""))
    if (str(metadata.get("sha256")) != receipt["sha256"] or
            model_sha not in AUDITORS):
        raise ValueError(f"{filename}: trivial sidecar metadata residual")
    sidecar = temporary / f"{Path(filename).stem}.txt"
    download(receipt, sidecar)
    text = sidecar.read_text()
    output_sha = str(metadata.get("output-sha256", ""))
    binding = f"reachability_binding filename {filename} output_sha256 {output_sha}"
    if text.splitlines()[0] != binding:
        raise ValueError(f"{filename}: sidecar/output binding residual")
    expected = AUDITORS[model_sha]
    audit_binding = (
        f"trivial_audit_binding source_overlay_sha256 {expected['overlay']} "
        f"model_sha256 {model_sha} inventory_sha256 {expected['inventory']} "
        f"binary_sha256 {expected['binary']}")
    if len(text.splitlines()) < 2 or text.splitlines()[1] != audit_binding:
        raise ValueError(f"{filename}: trivial auditor binding residual")

    details = ledger.result_rows(README.read_text())
    detail = details.get(filename)
    detail_matches = detail is not None and detail.digest == output_sha
    authenticated = None if detail_matches \
        else certificate_for(document, filename, output_sha, temporary)
    if authenticated is None and versions and not detail_matches:
        authenticated = certificate_from_storage(
            row.storage, filename, output_sha, versions, temporary)
    if authenticated is None and (detail is None or detail.digest != output_sha):
        raise ValueError(f"{filename}: certified result digest residual")
    if authenticated is None:
        job = certificate_item = completed = None
    else:
        job, certificate_item, completed = authenticated
    record = finalizer.encoded_record_for(filename)
    totals, excluded, trivial = finalizer.reporting_counts(filename, text)
    prince_factor = 2 ** sum(
        name == "prince" for name in
        (str(record["primary"]), str(record.get("secondary") or "")))
    if any(sum(side) != int(record["states"]) // (2 * prince_factor)
           for side in totals):
        raise ValueError(f"{filename}: total W/L/D conservation residual")
    first, second = (
        finalizer.render(totals[side], excluded[side], trivial[side])
        for side in finalizer.ledger_side_order(filename)
    )
    current_sidecar = (f"reachability v3 sha256:{receipt['sha256']} "
                       f"VersionId {receipt['version_id']}")
    storage, replacements = re.subn(
        r"(?:bound )?reachability(?: v\d+)? sha256:[0-9a-f]{64} VersionId \S+",
        current_sidecar, row.storage, count=1)
    if not replacements:
        storage = row.storage.rstrip("; ") + "; " + current_sidecar
    if authenticated is not None and job is None and output_sha not in storage:
        storage = storage.rstrip("; ") + f"; result sha256:{output_sha}"
    value = {
        "result_kind": "concrete", "first": first, "second": second,
        "reachability": ledger.reachability(first, second), "storage": storage,
    }
    evidence = {
        "bucket": receipt["bucket"], "key": receipt["key"],
        "version_id": receipt["version_id"], "sha256": receipt["sha256"],
        "size": int(head["ContentLength"]),
        "filename": filename, "output_sha256": output_sha,
    }
    if job is not None:
        job.setdefault("ledger_results", {})[filename] = value
        stem = Path(filename).stem
        # A job has exactly one current reachability truth source.  Keeping the
        # older sidecar in this live evidence list would make a sync audit depend
        # on iteration order and could silently restore stale W/L/D cells.
        job["s3_certificates"] = [
            item for item in job.get("s3_certificates", [])
            if not Path(str(item.get("key", ""))).name.startswith(
                stem + ".reachability-")
        ]
        job.setdefault("s3_certificates", []).append(evidence)
    else:
        sidecars = document.setdefault("ledger_reachability_sidecars", [])
        sidecars[:] = [item for item in sidecars
                       if item.get("filename") != filename]
        sidecars.append(evidence)
    ledger.update(README, [], [], certified_values=[
        filename + "=" + json.dumps(value, separators=(",", ":"))])
    create_detail = authenticated is not None and job is None
    update_result_detail(
        filename, first, second, material=row.material,
        edges=(int(completed["output"]["edges"])
               if create_detail and completed is not None else None),
        digest=output_sha if create_detail else None)
    return {"filename": filename, "output_sha256": output_sha,
            "sidecar": evidence, "first": first, "second": second}


def update_result_detail(filename: str, first: str, second: str, *,
                         material: str, edges: int | None,
                         digest: str | None) -> None:
    """Keep an existing digest-bearing certified-details row in sync."""
    text = README.read_text()
    pattern = re.compile(
        rf"^(\| `{re.escape(filename)}` \| [^|]+ \| [^|]+ \| )"
        r"[^|]+( \| )[^|]+( \| `[0-9a-f]{64}` \|)$", re.MULTILINE)
    matches = list(pattern.finditer(text))
    if len(matches) > 1:
        raise ValueError(f"{filename}: duplicate certified-result detail rows")
    if not matches:
        if edges is not None and digest is not None:
            line = (f"| `{filename}` | {material} | {edges:,} | {first} | "
                    f"{second} | `{digest}` |\n")
            README.write_text(text.replace(
                ledger.RESULT_END, line + ledger.RESULT_END, 1))
        return
    def replacement(match: re.Match[str]) -> str:
        tail = (f" | `{digest}` |" if digest is not None else match.group(3))
        return match.group(1) + first + match.group(2) + second + tail
    README.write_text(pattern.sub(replacement, text, count=1))


def explicit_trivial_zeroes(document: dict[str, Any]) -> int:
    """Make authenticated zero trivial subsets explicit in every W/L/D slot."""
    rows = [row for row in ledger.entries(README.read_text())
            if row.status == "certified" and row.result_kind == "concrete"]
    if not rows or any(
            not re.search(
                r"reachability v3 sha256:[0-9a-f]{64} VersionId \S+",
                row.storage) and "[" not in row.first + row.second
            for row in rows):
        raise ValueError(
            "explicit-zero migration requires v3 or prior bracket evidence on "
            "every concrete row")

    def cell(value: str) -> str:
        rendered = []
        for component in value.split("/"):
            match = re.fullmatch(
                r"\s*([0-9][0-9,]*)(?: \[([0-9][0-9,]*)\])?"
                r"(?: \(([0-9][0-9,]*)\))?\s*", component)
            if match is None:
                raise ValueError(f"malformed W/L/D cell: {value!r}")
            whole, trivial, excluded = match.groups()
            rendered.append(
                f"{whole} [{trivial or '0'}]" +
                (f" ({excluded})" if excluded else ""))
        return " / ".join(rendered)

    values = []
    by_filename: dict[str, tuple[str, str]] = {}
    for row in rows:
        first, second = cell(row.first), cell(row.second)
        by_filename[row.filename] = (first, second)
        value = {"result_kind": "concrete", "first": first,
                 "second": second,
                 "reachability": ledger.reachability(first, second),
                 "storage": row.storage}
        values.append(row.filename + "=" + json.dumps(
            value, separators=(",", ":")))
    for job in document.get("jobs", []):
        for filename, value in job.get("ledger_results", {}).items():
            if filename not in by_filename or value.get("result_kind") != "concrete":
                continue
            first, second = by_filename[filename]
            value["first"], value["second"] = first, second
            value["reachability"] = ledger.reachability(first, second)
    ledger.update(README, [], [], certified_values=values)
    for row in rows:
        first, second = by_filename[row.filename]
        update_result_detail(row.filename, first, second,
                             material=row.material, edges=None, digest=None)
    return len(rows)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", action="append", default=[],
                        metavar="FILENAME,SHA256,VERSION_ID")
    parser.add_argument("--receipt-json", type=Path,
                        help="JSON list emitted by the authenticated batch launcher")
    parser.add_argument("--config", type=Path, default=CONFIG)
    parser.add_argument("--versions-json", type=Path,
                        help="exact list-object-versions inventory for legacy certificates")
    parser.add_argument("--explicit-zeroes", action="store_true",
                        help="after full v3 coverage, render every zero trivial subset")
    args = parser.parse_args()
    raw_receipts = list(args.receipt)
    if args.receipt_json:
        document = json.loads(args.receipt_json.read_text())
        if not isinstance(document, list):
            parser.error("--receipt-json must contain a list")
        raw_receipts.extend(
            f"{item['filename']},{item['sidecar_sha256']},{item['sidecar_version_id']}"
            for item in document)
    if not raw_receipts and not args.explicit_zeroes:
        parser.error("at least one --receipt or --receipt-json is required")
    if raw_receipts and args.explicit_zeroes:
        parser.error("--explicit-zeroes cannot be combined with receipts")
    receipts = list(map(parse_receipt, raw_receipts))
    if len({item["filename"] for item in receipts}) != len(receipts):
        parser.error("duplicate receipt filename")
    with locked(args.config), tempfile.TemporaryDirectory(
            prefix="ultimate-trivial-import-") as raw:
        versions = (json.loads(args.versions_json.read_text())
                    if args.versions_json else [])
        if not isinstance(versions, list):
            parser.error("--versions-json must contain a list")
        document = json.loads(args.config.read_text())
        original_readme = README.read_text()
        try:
            results: Any = (explicit_trivial_zeroes(document)
                            if args.explicit_zeroes else
                            [import_one(document, receipt, Path(raw), versions)
                             for receipt in receipts])
        except Exception:
            # Multi-receipt imports are atomic across the README/config pair.
            README.write_text(original_readme)
            raise
        args.config.write_text(json.dumps(document, indent=2) + "\n")
    print(json.dumps(results, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
