#!/usr/bin/env python3
"""Resume only preservation after a completed concrete solve.

This recovery path never invokes the generator. It authenticates the retained
run plan, source bundle, executable, dependency manifest, UFTB, proof log,
result manifest, content-addressed archive, and a fresh local archive restore
before retrying the versioned-S3 upload and writing the ordinary wave
certificate. Failed upload attempts remain preserved in numbered directories.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--work-directory", type=Path, required=True)
    parser.add_argument("--wave", type=int, choices=(0, 1, 2), required=True)
    parser.add_argument("--index", type=int, required=True)
    parser.add_argument("--s3-prefix", required=True)
    return parser.parse_args()


def unused_attempt(root: Path, stem: str) -> Path:
    for attempt in range(1, 10_000):
        candidate = root.with_name(f"{root.name}-{attempt:04d}") / stem
        if not candidate.exists():
            return candidate
    raise RuntimeError(f"too many retained preservation attempts below {root}")


def main() -> int:
    args = parse_args()
    if not args.s3_prefix.startswith("s3://"):
        raise RuntimeError("S3 prefix must start with s3://")
    source_root = args.source_root.resolve(strict=True)
    sys.path.insert(0, str(source_root / "tools/tablebases"))
    import run_ultimate_concrete_tablebase_shard_aws as concrete
    import run_double_jester_information_capture as preservation

    work = args.work_directory.resolve(strict=True)
    rows = concrete.wave_inventory(args.wave)
    if not 0 <= args.index < len(rows):
        raise RuntimeError("class index is outside the selected wave")
    record = rows[args.index]
    selected, measurement = concrete.selection_plan(
        args.wave, args.index, args.index + 1)
    if tuple(selected) != (record,):
        raise RuntimeError("single-class preservation selection residual")
    filename = str(record["filename"])
    stem = Path(filename).stem

    run_plan = json.loads((work / "run-plan.json").read_text())
    model = run_plan.get("generator_model_sha256")
    binary = work / "binary/ultimate_tablebase"
    dependency_manifest = work / "dependencies/manifest.json"
    if (run_plan.get("status") != "full-preflight" or
            not isinstance(model, str) or len(model) != 64 or
            run_plan.get("inventory_sha256") != concrete.inventory_sha256() or
            run_plan.get("selection") != measurement or
            run_plan.get("selected") != [concrete.normalized_record(record)] or
            run_plan.get("binary_sha256") != concrete.sha256_path(binary) or
            run_plan.get("dependency_manifest_sha256") !=
            concrete.sha256_path(dependency_manifest) or
            concrete.generator_model_sha256(work / "bundle") != model):
        raise RuntimeError("retained concrete run-plan binding residual")

    output = work / "outputs" / filename
    log = work / "logs/generate" / f"{stem}.log"
    verification = concrete.parse_uftb(output, record, log)
    result_path = work / "results" / f"{stem}.json"
    result = json.loads(result_path.read_text())
    binary_sha = concrete.sha256_path(binary)
    expected_result = {
        "schema": concrete.ARCHIVE_SCHEMA,
        "generator_model_sha256": model,
        "inventory_sha256": concrete.inventory_sha256(),
        "record": concrete.normalized_record(record),
        "output": verification,
        "proof_log_sha256": concrete.sha256_path(log),
        "binary_sha256": binary_sha,
        "dependency_manifest_sha256": concrete.sha256_path(
            dependency_manifest),
        "bellman_verification_residual": 0,
        "never_delete": True,
    }
    for key, value in expected_result.items():
        if result.get(key) != value:
            raise RuntimeError(f"retained result-manifest residual: {key}")
    resource = result.get("resource_certificate")
    if (not isinstance(resource, dict) or resource.get("violation") is not None or
            resource.get("returncode") != 0 or
            not isinstance(resource.get("samples"), int) or
            resource["samples"] <= 0):
        raise RuntimeError("retained resource certificate residual")

    archives = sorted((work / "archives").glob(f"{stem}-*.tar.zst"))
    if len(archives) != 1:
        raise RuntimeError("retained content-addressed archive cardinality residual")
    archive = archives[0]
    archive_sha = concrete.sha256_path(archive)
    if archive.name != f"{stem}-{archive_sha}.tar.zst":
        raise RuntimeError("retained content-addressed archive name residual")

    restore = unused_attempt(work / "restore-preservation", stem)
    restored = preservation.restore_zstd_archive(
        archive, restore, concrete.ARCHIVE_SCHEMA)
    restored_verification = concrete.parse_uftb(
        restored[f"tablebases/{filename}"], record,
        restored[f"proof/{log.name}"])
    if restored_verification != verification:
        raise RuntimeError("preservation-recovery archive restore residual")

    key = (f"concrete/v2/model/{model}/wave-{args.wave}/sha256/"
           f"{archive_sha}/{archive.name}")
    download = unused_attempt(
        work / "s3-verify-preservation", archive.name)
    remote = preservation.upload_head_download_verify(
        source=archive, digest=archive_sha, extent=archive.stat().st_size,
        prefix=args.s3_prefix, key=key, download=download,
        archive_schema=concrete.ARCHIVE_SCHEMA)
    completed = [{
        "filename": filename,
        "status": "generated-preserved",
        "output": verification,
        "archive": {
            "bytes": archive.stat().st_size,
            "sha256": archive_sha,
            "key": key,
        },
        "s3": remote,
    }]
    certificate = {
        "schema": concrete.CERTIFICATE_SCHEMA,
        "status": "head-download-full-sha-archive-restore-verified",
        "generator_model_sha256": model,
        "inventory_sha256": concrete.inventory_sha256(),
        "selection": measurement,
        "completed": completed,
        "local_outputs_retained": True,
        "local_scratch_retained": True,
        "safe_to_delete_gate": False,
        "preservation_only_resume": True,
    }
    certificate_path = work / "certificates/wave-certificate.json"
    if certificate_path.exists():
        raise RuntimeError("wave certificate already exists")
    preservation.write_json(certificate_path, certificate)
    certificate_sha = concrete.sha256_path(certificate_path)
    certificate_download = unused_attempt(
        work / "s3-verify-preservation", certificate_path.name)
    certificate_remote = preservation.upload_head_download_verify(
        source=certificate_path, digest=certificate_sha,
        extent=certificate_path.stat().st_size, prefix=args.s3_prefix,
        key=(f"concrete/v2/certificates/sha256/{certificate_sha}/"
             f"{certificate_path.name}"),
        download=certificate_download, archive_schema=None)
    print(json.dumps({
        "status": "preservation-only-resume-complete",
        "filename": filename,
        "generator_model_sha256": model,
        "archive_sha256": archive_sha,
        "archive_s3": remote,
        "certificate_sha256": certificate_sha,
        "certificate_s3": certificate_remote,
        "safe_to_delete_gate": False,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
