#!/usr/bin/env python3
"""Preserve an already verified concrete result after an upload-only failure.

The full concrete runner deliberately retains its output, result manifest,
deterministic archive, and local restore before attempting S3.  This recovery
command consumes only those authenticated products: it never opens scratch or
regenerates the tablebase.  It rechecks the archive name/full SHA, restores it,
checks the embedded UFTB against the result manifest, uploads and downloads the
exact archive, then emits and similarly verifies the normal wave certificate.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

import run_double_jester_information_capture as preservation


ARCHIVE_SCHEMA = "ultimate-concrete-k2-result-v2"
CERTIFICATE_SCHEMA = "ultimate-concrete-k2-s3-certificate-v2"
RESULT_SCHEMA = "ultimate-concrete-k2-result-v2"
ARCHIVE_NAME = re.compile(r"^(?P<stem>.+)-(?P<sha>[0-9a-f]{64})\.tar\.zst$")


def load_object(path: Path) -> dict[str, object]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise RuntimeError(f"JSON object required: {path}")
    return value


def recover(work: Path, filename: str, s3_prefix: str) -> dict[str, object]:
    stem = Path(filename).stem
    result_path = work / "results" / f"{stem}.json"
    output_path = work / "outputs" / filename
    plan_path = work / "run-plan.json"
    result = load_object(result_path)
    plan = load_object(plan_path)
    if result.get("schema") != RESULT_SCHEMA:
        raise RuntimeError("unexpected retained concrete result schema")
    record = result.get("record")
    output = result.get("output")
    if not isinstance(record, dict) or record.get("filename") != filename:
        raise RuntimeError("retained result filename residual")
    if not isinstance(output, dict):
        raise RuntimeError("retained result lacks output verification")
    output_sha = str(output.get("sha256", ""))
    if (output_path.stat().st_size != int(output.get("bytes", -1)) or
            preservation.sha256_path(output_path) != output_sha):
        raise RuntimeError("retained UFTB extent/full-SHA residual")
    for name in ("generator_model_sha256", "inventory_sha256"):
        if result.get(name) != plan.get(name):
            raise RuntimeError(f"retained run-plan {name} residual")
    if result.get("worker_threads") != plan.get("worker_threads"):
        raise RuntimeError("retained worker-thread residual")

    archives = list((work / "archives").glob(f"{stem}-*.tar.zst"))
    if len(archives) != 1:
        raise RuntimeError("exactly one retained content-addressed archive required")
    archive = archives[0]
    match = ARCHIVE_NAME.fullmatch(archive.name)
    if match is None or match.group("stem") != stem:
        raise RuntimeError("retained archive name residual")
    archive_sha = preservation.sha256_path(archive)
    if match.group("sha") != archive_sha:
        raise RuntimeError("retained archive name/full-SHA residual")
    retry_root = work / "preservation-retry"
    retry_root.mkdir(exist_ok=True)
    restored = preservation.restore_zstd_archive(
        archive, retry_root / "restore-local" / stem, ARCHIVE_SCHEMA)
    restored_output = restored.get(f"tablebases/{filename}")
    if restored_output is None or preservation.sha256_path(
            restored_output) != output_sha:
        raise RuntimeError("retained archive UFTB full-SHA residual")

    wave = str(dict(plan.get("selection", {})).get("wave", ""))
    if not wave:
        raise RuntimeError("retained run plan lacks wave")
    archive_key = (
        f"concrete/v2/model/{result['generator_model_sha256']}/"
        f"wave-{wave}/sha256/{archive_sha}/{archive.name}")
    remote = preservation.upload_head_download_verify(
        source=archive, digest=archive_sha, extent=archive.stat().st_size,
        prefix=s3_prefix, key=archive_key,
        download=retry_root / "s3-verify" / archive.name,
        archive_schema=ARCHIVE_SCHEMA)
    completed = [{
        "filename": filename, "status": "generated-preserved",
        "output": output,
        "archive": {"bytes": archive.stat().st_size,
                    "sha256": archive_sha, "key": archive_key},
        "s3": remote,
    }]
    certificate = {
        "schema": CERTIFICATE_SCHEMA,
        "status": "head-download-full-sha-archive-restore-verified",
        "generator_model_sha256": result["generator_model_sha256"],
        "inventory_sha256": result["inventory_sha256"],
        "selection": plan["selection"],
        "worker_threads": result["worker_threads"],
        "completed": completed,
        "local_outputs_retained": True,
        "local_scratch_retained": True,
        "safe_to_delete_gate": False,
    }
    certificate_path = work / "certificates" / "wave-certificate.json"
    preservation.write_json(certificate_path, certificate)
    certificate_sha = preservation.sha256_path(certificate_path)
    certificate_key = (
        f"concrete/v2/certificates/sha256/{certificate_sha}/"
        f"{certificate_path.name}")
    certificate_remote = preservation.upload_head_download_verify(
        source=certificate_path, digest=certificate_sha,
        extent=certificate_path.stat().st_size, prefix=s3_prefix,
        key=certificate_key,
        download=retry_root / "s3-verify" / certificate_path.name,
        archive_schema=None)
    summary = {
        "schema": "ultimate-concrete-preservation-retry-v1",
        "status": "existing-result-preserved-and-restored",
        "filename": filename,
        "output_sha256": output_sha,
        "archive_sha256": archive_sha,
        "archive": remote,
        "certificate_sha256": certificate_sha,
        "certificate": certificate_remote,
        "regenerated": False,
    }
    preservation.write_json(retry_root / "preservation-retry.json", summary)
    return summary


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-directory", type=Path, required=True)
    parser.add_argument("--filename", required=True)
    parser.add_argument("--s3-prefix", required=True)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    print(json.dumps(recover(args.work_directory.resolve(), args.filename,
                             args.s3_prefix), sort_keys=True))


if __name__ == "__main__":
    main()
