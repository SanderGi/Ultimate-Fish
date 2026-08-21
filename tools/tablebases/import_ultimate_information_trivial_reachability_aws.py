#!/usr/bin/env python3
"""Authenticate and import information-tablebase trivial-reachability receipts."""

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

import audit_ultimate_tablebase_ledger_sync as sync
import finalize_ultimate_aws_concrete_class as finalizer
import import_ultimate_trivial_reachability_aws as concrete_import
import update_ultimate_tablebase_ledger as ledger


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "tablebases/README.md"
CONFIG = ROOT / "tools/tablebases/ultimate_aws_supervision.json"
REGION = "us-west-2"
BUCKET = "ultimatefish-info-20260808-a4e679c6-831688117652"
SCHEMA_VERSIONS = {
    "ultimate-information-trivial-receipt-v1": 1,
    "ultimate-information-trivial-receipt-v2": 2,
}
SHA256 = re.compile(r"[0-9a-f]{64}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def aws_json(arguments: list[str]) -> dict[str, Any]:
    value = json.loads(subprocess.check_output(["aws", *arguments], text=True))
    if not isinstance(value, dict):
        raise RuntimeError("AWS command did not return an object")
    return value


@contextmanager
def locked(path: Path) -> Iterator[None]:
    lock = path.with_suffix(path.suffix + ".lock")
    with lock.open("a", encoding="utf-8") as stream:
        fcntl.flock(stream.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(stream.fileno(), fcntl.LOCK_UN)


def validate_receipt(value: Any) -> dict[str, Any]:
    if (not isinstance(value, dict) or
            value.get("schema") not in SCHEMA_VERSIONS):
        raise ValueError("invalid information-trivial receipt schema")
    version = SCHEMA_VERSIONS[value["schema"]]
    filename = value.get("filename")
    if (not isinstance(filename, str) or Path(filename).name != filename or
            not filename.endswith(".uftb")):
        raise ValueError("invalid information-trivial receipt filename")
    for name in ("source_sha256", "model_sha256", "overlay_sha256",
                 "binary_sha256", "source_bundle_sha256", "sidecar_sha256"):
        if not isinstance(value.get(name), str) or not SHA256.fullmatch(value[name]):
            raise ValueError(f"{filename}: invalid {name}")
    expected_key = (f"results/information-trivial-v{version}/{filename}/sha256/"
                    f"{value['sidecar_sha256']}/{Path(filename).stem}."
                    f"information-trivial-v{version}.txt")
    if value.get("s3_key") != expected_key or not value.get("s3_version_id"):
        raise ValueError(f"{filename}: sidecar location residual")
    counts = value.get("counts")
    if not isinstance(counts, dict) or set(counts) != {"0", "1"}:
        raise ValueError(f"{filename}: malformed receipt counts")
    for side in counts.values():
        if not isinstance(side, dict) or set(side) != {
                "admitted", "excluded", "trivial"}:
            raise ValueError(f"{filename}: malformed receipt buckets")
        for bucket in side.values():
            if (not isinstance(bucket, dict) or set(bucket) !=
                    {"unknown", "win", "loss", "draw"} or
                    any(not isinstance(item, int) or item < 0
                        for item in bucket.values())):
                raise ValueError(f"{filename}: malformed receipt W/D/L")
    if version == 2:
        substates = value.get("substates")
        detail = value.get("substate_counts")
        if (not isinstance(substates, int) or substates <= 0 or
                not isinstance(detail, dict) or
                set(detail) != {str(index) for index in range(substates)}):
            raise ValueError(f"{filename}: malformed substate coverage")
        for substate in detail.values():
            if not isinstance(substate, dict) or set(substate) != {"0", "1"}:
                raise ValueError(f"{filename}: malformed substate sides")
            for side in substate.values():
                if not isinstance(side, dict) or set(side) != {
                        "admitted", "excluded", "trivial"}:
                    raise ValueError(f"{filename}: malformed substate buckets")
                for bucket in side.values():
                    if (not isinstance(bucket, dict) or set(bucket) !=
                            {"unknown", "win", "loss", "draw"} or
                            any(not isinstance(item, int) or item < 0
                                for item in bucket.values())):
                        raise ValueError(f"{filename}: malformed substate W/D/L")
        for side in ("0", "1"):
            for bucket in ("admitted", "excluded", "trivial"):
                for outcome in ("unknown", "win", "loss", "draw"):
                    if sum(detail[str(index)][side][bucket][outcome]
                           for index in range(substates)) != \
                            counts[side][bucket][outcome]:
                        raise ValueError(
                            f"{filename}: substate conservation residual")
    return value


def download_sidecar(receipt: dict[str, Any], target: Path) -> dict[str, Any]:
    head = aws_json([
        "s3api", "head-object", "--region", REGION, "--bucket", BUCKET,
        "--key", receipt["s3_key"], "--version-id", receipt["s3_version_id"],
        "--output", "json",
    ])
    metadata = head.get("Metadata", {})
    expected = {
        "sha256": receipt["sidecar_sha256"],
        "source-sha256": receipt["source_sha256"],
        "overlay-sha256": receipt["overlay_sha256"],
        "binary-sha256": receipt["binary_sha256"],
    }
    if int(head.get("ContentLength", 0)) <= 1024 or any(
            metadata.get(name) != digest for name, digest in expected.items()):
        raise ValueError(f"{receipt['filename']}: S3 metadata residual")
    aws_json([
        "s3api", "get-object", "--region", REGION, "--bucket", BUCKET,
        "--key", receipt["s3_key"], "--version-id", receipt["s3_version_id"],
        str(target), "--output", "json",
    ])
    if sha256(target) != receipt["sidecar_sha256"]:
        raise ValueError(f"{receipt['filename']}: S3 SHA-256 residual")
    return head


def import_one(document: dict[str, Any], receipt: dict[str, Any],
               temporary: Path) -> dict[str, Any]:
    filename = receipt["filename"]
    rows = {row.filename: row for row in ledger.entries(README.read_text())}
    row = rows.get(filename)
    if (row is None or row.status != "certified" or
            row.result_kind not in {"information v2", "information required"}):
        raise ValueError(f"{filename}: not a certified information row")
    if "[" in row.first or "[" in row.second:
        existing = [item for item in
                    document.get("ledger_information_reachability_sidecars", [])
                    if item.get("filename") == filename]
        if (len(existing) != 1 or "[" not in row.first or "[" not in row.second or
                any(str(existing[0][name]) not in row.storage
                    for name in ("sha256", "version_id"))):
            raise ValueError(
                f"{filename}: incomplete or unauthenticated existing trivial count")
        return {"filename": filename, "existing": True,
                "first": row.first, "second": row.second,
                "sidecar_sha256": existing[0]["sha256"]}

    sidecar = temporary / f"{Path(filename).stem}.txt"
    head = download_sidecar(receipt, sidecar)
    text = sidecar.read_text(encoding="utf-8")
    required = (
        f"information_reachability_binding source_sha256 "
        f"{receipt['source_sha256']} model_sha256 {receipt['model_sha256']}",
        f"information_trivial_overlay_sha256 {receipt['overlay_sha256']}",
        f"information_trivial_binary_sha256 {receipt['binary_sha256']}",
        f"information_trivial_source_bundle_sha256 "
        f"{receipt['source_bundle_sha256']}",
    )
    if any(binding not in text for binding in required):
        raise ValueError(f"{filename}: sidecar binding residual")

    totals, excluded, trivial = finalizer.information_reporting_counts(
        filename, text)

    # The independently parsed receipt must agree with the authenticated text.
    for side in range(2):
        for bucket, parsed in zip(
                ("admitted", "excluded", "trivial"),
                ([totals[side][i] - excluded[side][i] for i in range(4)],
                 excluded[side], trivial[side])):
            recorded = receipt["counts"][str(side)][bucket]
            if parsed != [recorded[name] for name in
                          ("unknown", "win", "loss", "draw")]:
                raise ValueError(f"{filename}: receipt/text count residual")
    first, second, _ = sync.rendered_information(
        sidecar, filename, row.first, row.second)

    version = SCHEMA_VERSIONS[receipt["schema"]]
    binding = (f"information trivial v{version} sha256:"
               f"{receipt['sidecar_sha256']} "
               f"VersionId {receipt['s3_version_id']}")
    storage = row.storage.rstrip("; ") + "; " + binding
    value = {
        "result_kind": "information v2", "first": first, "second": second,
        "reachability": ledger.reachability(first, second), "storage": storage,
    }
    ledger.update(README, [], [], certified_values=[
        filename + "=" + json.dumps(value, separators=(",", ":"))])
    concrete_import.update_result_detail(
        filename, first, second, material=row.material, edges=None, digest=None)

    evidence = {
        "bucket": BUCKET, "key": receipt["s3_key"],
        "version_id": receipt["s3_version_id"],
        "sha256": receipt["sidecar_sha256"],
        "size": int(head["ContentLength"]), "filename": filename,
        "output_sha256": receipt["source_sha256"],
        "model_sha256": receipt["model_sha256"],
        "overlay_sha256": receipt["overlay_sha256"],
        "binary_sha256": receipt["binary_sha256"],
        "source_bundle_sha256": receipt["source_bundle_sha256"],
    }
    sidecars = document.setdefault("ledger_information_reachability_sidecars", [])
    if any(item.get("filename") == filename for item in sidecars):
        raise ValueError(f"{filename}: duplicate information sidecar")
    sidecars.append(evidence)
    return {"filename": filename, "first": first, "second": second,
            "sidecar_sha256": receipt["sidecar_sha256"]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipts", type=Path,
                        help="directory containing *.receipt.json files")
    parser.add_argument("--config", type=Path, default=CONFIG)
    args = parser.parse_args()
    paths = sorted(args.receipts.glob("*.receipt.json"))
    receipts = [validate_receipt(json.loads(path.read_text())) for path in paths]
    if not receipts:
        parser.error("receipt directory is empty")
    filenames = [item["filename"] for item in receipts]
    if len(set(filenames)) != len(filenames):
        parser.error("duplicate receipt filename")

    with locked(args.config), tempfile.TemporaryDirectory(
            prefix="ultimate-information-trivial-import-") as raw:
        document = json.loads(args.config.read_text())
        original_readme = README.read_text()
        try:
            imported = [import_one(document, receipt, Path(raw))
                        for receipt in receipts]
            # Reparse every sidecar-rendered row before committing either file.
            published = {row.filename: row for row in ledger.entries(README.read_text())}
            imported_filenames = {
                item["filename"] for item in imported if not item.get("existing")}
            if any(sync.rendered_information(
                    Path(raw) / f"{Path(item['filename']).stem}.txt",
                    item["filename"],
                    published[item["filename"]].first,
                    published[item["filename"]].second)[:2] !=
                    (published[item["filename"]].first,
                     published[item["filename"]].second)
                   for item in receipts
                   if item["filename"] in imported_filenames):
                raise ValueError("post-import information rendering residual")
        except Exception:
            README.write_text(original_readme)
            raise
        document["ledger_information_reachability_sidecars"].sort(
            key=lambda item: item["filename"])
        args.config.write_text(json.dumps(document, indent=2) + "\n")
    print(json.dumps(imported, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
