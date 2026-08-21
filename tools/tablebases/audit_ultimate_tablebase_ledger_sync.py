#!/usr/bin/env python3
"""Verify version-pinned reachability artifacts against supervision and README.

The supervision file stores rendered W/L/D cells so the fleet can update the
ledger without downloading old artifacts on every poll.  Those cached strings
are not proof: this audit downloads each current job's exact reachability
VersionId, verifies its SHA-256 and extent, re-parses admitted/excluded counts,
and compares the result with both supervision and the canonical README.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
from typing import Any

import finalize_ultimate_aws_concrete_class as finalizer
import update_ultimate_tablebase_ledger as ledger


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONFIG = ROOT / "tools/tablebases/ultimate_aws_supervision.json"
DEFAULT_README = ROOT / "tablebases/README.md"
SIDECAR_NAME = re.compile(r"([^/]+)\.reachability-v\d+\.(?:json|txt)$")
RESULT_DIGEST = re.compile(
    r"\bresult(?: UFIW)? sha256:([0-9a-f]{64})\b", re.IGNORECASE)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def artifact_path(cache: Path, certificate: dict[str, Any]) -> Path:
    name = Path(str(certificate["key"])).name
    return cache / f"{certificate['sha256']}-{name}"


def download(certificate: dict[str, Any], target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        "aws", "s3api", "get-object", "--no-cli-pager",
        "--bucket", str(certificate["bucket"]),
        "--key", str(certificate["key"]),
        "--version-id", str(certificate["version_id"]), str(target),
    ], check=True, stdout=subprocess.DEVNULL)


def exact_artifact(cache: Path, certificate: dict[str, Any],
                   *, offline: bool) -> Path:
    target = artifact_path(cache, certificate)
    if not target.is_file():
        if offline:
            raise RuntimeError(f"offline cache miss: {target.name}")
        download(certificate, target)
    actual_size = target.stat().st_size
    actual_sha = sha256(target)
    if (actual_size != int(certificate["size"]) or
            actual_sha != certificate["sha256"]):
        raise RuntimeError(
            f"artifact mismatch for {certificate['key']}: "
            f"{actual_size}/{actual_sha}")
    return target


def json_counts(path: Path) -> tuple[list[list[int]], list[list[int]]]:
    value = json.loads(path.read_text(encoding="utf-8"))
    totals = value.get("totals")
    excluded = value.get("unreachable")
    if (not isinstance(totals, list) or not isinstance(excluded, list) or
            len(totals) != 2 or len(excluded) != 2 or
            any(len(row) != 4 for row in totals + excluded)):
        raise ValueError(f"malformed JSON reachability sidecar: {path}")
    totals = [[int(item) for item in row] for row in totals]
    excluded = [[int(item) for item in row] for row in excluded]
    if any(row[0] for row in totals + excluded):
        raise ValueError(f"unknown reachability states in {path}")
    return totals, excluded


def text_counts(path: Path) -> tuple[list[list[int]], list[list[int]]]:
    text = path.read_text(encoding="utf-8")
    totals = finalizer.counts(finalizer.TOTAL, text)
    # Native legacy `reachability side` rows are excluded states.  New output
    # spells this out and provides a separately checked admitted bucket.
    explicit = "reachability_excluded side " in text
    excluded = finalizer.counts(
        finalizer.EXCLUDED if explicit else finalizer.AUDIT, text)
    if explicit:
        finalizer.validate_explicit_reachability_semantics(
            text, totals, excluded)
    return totals, excluded


def rendered(path: Path, filename: str) -> tuple[str, str, str]:
    totals, excluded = (json_counts(path) if path.suffix == ".json"
                        else text_counts(path))
    order = finalizer.ledger_side_order(filename)
    cells = tuple(finalizer.render(totals[side], excluded[side])
                  for side in order)
    return cells[0], cells[1], ledger.reachability(*cells)


def sidecar_certificates(job: dict[str, Any]) -> list[dict[str, Any]]:
    return [item for item in job.get("s3_certificates", [])
            if SIDECAR_NAME.search(str(item.get("key", "")))]


def sidecar_filename(job: dict[str, Any], certificate: dict[str, Any]) -> str:
    match = SIDECAR_NAME.search(str(certificate["key"]))
    if match is None:
        raise ValueError("not a reachability sidecar")
    candidate = match.group(1) + ".uftb"
    files = list(map(str, job.get("ledger_files", [])))
    if candidate in files:
        return candidate
    if len(files) == 1:
        return files[0]
    raise ValueError(
        f"cannot bind {certificate['key']} to ledger files {files}")


def audit_published_details(text: str) -> dict[str, int]:
    """Reject a stale duplicate details row beneath the canonical ledger."""
    details = ledger.result_rows(text)
    detail_rows = result_digests = 0
    for row in ledger.entries(text):
        if row.status not in {"certified", "preserving"} or not row.filename:
            continue
        detail = details.get(row.filename)
        if detail is not None:
            if detail.first != row.first or detail.second != row.second:
                raise RuntimeError(
                    f"README certified-result details differ for {row.filename}: "
                    f"ledger={(row.first, row.second)!r} "
                    f"details={(detail.first, detail.second)!r}")
            detail_rows += 1
        match = RESULT_DIGEST.search(row.storage)
        if match is None:
            continue
        if detail is None:
            raise RuntimeError(
                f"README result SHA lacks details row for {row.filename}")
        if detail.digest != match.group(1).lower():
            raise RuntimeError(
                f"README result SHA differs for {row.filename}: "
                f"storage={match.group(1).lower()} details={detail.digest}")
        result_digests += 1
    return {"detail_rows": detail_rows, "result_digests": result_digests}


def audit_concrete_result_certificate(
        path: Path, binding: dict[str, Any]) -> None:
    value = json.loads(path.read_text(encoding="utf-8"))
    if value.get("schema") != "ultimate-concrete-k2-s3-certificate-v2":
        raise RuntimeError(f"unexpected concrete certificate schema: {path}")
    completed = value.get("completed")
    if not isinstance(completed, list) or len(completed) != 1:
        raise RuntimeError(f"concrete certificate is not single-class: {path}")
    item = completed[0]
    actual = {
        "filename": str(item.get("filename", "")),
        "result_sha256": str(item.get("output", {}).get("sha256", "")),
        "archive_sha256": str(item.get("archive", {}).get("sha256", "")),
        "archive_version_id": str(item.get("s3", {}).get("version_id", "")),
    }
    expected = {name: str(binding.get(name, "")) for name in actual}
    if actual != expected:
        raise RuntimeError(
            f"concrete certificate binding differs: "
            f"artifact={actual!r} config={expected!r}")


def audit(config_path: Path, readme_path: Path, cache: Path,
          *, offline: bool) -> dict[str, int]:
    config = json.loads(config_path.read_text(encoding="utf-8"))
    readme_text = readme_path.read_text(encoding="utf-8")
    readme_rows = {
        row.filename: row
        for row in ledger.entries(readme_text)
        if row.filename
    }
    audited: set[str] = set()
    artifacts = 0
    result_certificates = 0
    missing_results = 0
    for binding in config.get("ledger_result_certificates", []):
        certificate = binding["certificate"]
        path = exact_artifact(cache, certificate, offline=offline)
        audit_concrete_result_certificate(path, binding)
        filename = str(binding["filename"])
        row = readme_rows.get(filename)
        if row is None:
            raise RuntimeError(f"README lacks certificate-bound {filename}")
        required = (
            str(certificate["sha256"]), str(certificate["version_id"]),
            str(binding["result_sha256"]), str(binding["archive_sha256"]),
            str(binding["archive_version_id"]),
        )
        if any(value not in row.storage for value in required):
            raise RuntimeError(
                f"README storage omits certificate binding for {filename}")
        result_certificates += 1
    for job in config["jobs"]:
        if job.get("superseded_by") or not job.get("ledger_certifies"):
            continue
        results = job.get("ledger_results", {})
        for certificate in sidecar_certificates(job):
            filename = sidecar_filename(job, certificate)
            path = exact_artifact(cache, certificate, offline=offline)
            first, second, reachability = rendered(path, filename)
            actual = {"first": first, "second": second,
                      "reachability": reachability}
            expected = results.get(filename)
            if expected is None:
                missing_results += 1
                print(f"LEDGER_SYNC_MISSING_RESULT job={job['id']} "
                      f"filename={filename} result={json.dumps(actual, sort_keys=True)}")
            else:
                configured = {name: str(expected.get(name, ""))
                              for name in actual}
                if configured != actual:
                    raise RuntimeError(
                        f"{job['id']}: cached reachability differs for {filename}: "
                        f"artifact={actual!r} config={configured!r}")
            row = readme_rows.get(filename)
            if row is None:
                raise RuntimeError(f"README lacks {filename}")
            published = {"first": row.first, "second": row.second,
                         "reachability": row.reachability}
            if published != actual:
                raise RuntimeError(
                    f"{job['id']}: README differs for {filename}: "
                    f"artifact={actual!r} readme={published!r}")
            audited.add(filename)
            artifacts += 1

    # Independently enforce dense-state conservation across every published
    # exact row, including legacy and information results without a standalone
    # reachability sidecar in the current supervision inventory.
    conserved = 0
    for row in readme_rows.values():
        if (row.status not in {"certified", "preserving"} or
                row.states is None or row.first == "—" or row.second == "—"):
            continue
        first = ledger.parse_wdl(row.first)
        second = ledger.parse_wdl(row.second)
        if (sum(first) != row.states // 2 or
                sum(second) != row.states // 2):
            raise RuntimeError(f"README state conservation failed for {row.key}")
        conserved += 1
    result = {"artifacts": artifacts, "files": len(audited),
              "missing_results": missing_results,
              "conserved_rows": conserved,
              "result_certificates": result_certificates}
    result.update(audit_published_details(readme_text))
    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--readme", type=Path, default=DEFAULT_README)
    parser.add_argument("--cache", type=Path)
    parser.add_argument("--offline", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.cache is not None:
        result = audit(args.config, args.readme, args.cache,
                       offline=args.offline)
    else:
        with tempfile.TemporaryDirectory(
                prefix="ultimate-ledger-reachability-") as directory:
            result = audit(args.config, args.readme, Path(directory),
                           offline=args.offline)
    print("LEDGER_SYNC_OK " + " ".join(
        f"{name}={value}" for name, value in result.items()))


if __name__ == "__main__":
    main()
