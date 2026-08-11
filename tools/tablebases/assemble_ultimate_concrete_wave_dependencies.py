#!/usr/bin/env python3
"""Assemble one complete concrete dependency wave from versioned S3 results.

This is deliberately not part of the concrete generator model fingerprint.  It
does not solve a tablebase or reinterpret its bytes.  It accepts only complete,
internally consistent wave certificates, downloads the exact S3 VersionId named
by each certificate, restores and authenticates every result archive, combines
those tables with the immutable base dependencies, and emits the dependency
manifest required by the next promotion wave.

Default execution is plan-only.  ``--full`` is Linux/EC2-only, never deletes
data, requires a private versioned S3 destination, and performs both a local
dependency-copy drill and a version-pinned S3 download/restore drill.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import Iterable, Mapping


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "tablebases"))
import run_double_jester_information_capture as preservation  # noqa: E402
import run_ultimate_concrete_tablebase_shard_aws as runner  # noqa: E402


SCHEMA = "ultimate-concrete-wave-dependency-assembly-v1"
ARCHIVE_SCHEMA = "ultimate-concrete-wave-dependencies-archive-v1"
CERTIFICATE_SCHEMA = "ultimate-concrete-wave-dependencies-certificate-v1"


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def expected_outputs(completed_wave: int) -> dict[str, int]:
    if completed_wave not in (0, 1):
        raise ValueError("completed wave must be 0 or 1")
    return {
        str(record["filename"]): wave
        for wave in range(completed_wave + 1)
        for record in runner.wave_inventory(wave)
    }


def _certificate_candidates(directory: Path) -> Iterable[Path]:
    return sorted(directory.rglob("wave-certificate.json"))


def collect_results(directory: Path, completed_wave: int, *,
                    model: str, inventory: str) -> dict[str, dict[str, object]]:
    expected = expected_outputs(completed_wave)
    candidates: dict[str, list[dict[str, object]]] = {}
    for path in _certificate_candidates(directory):
        document = json.loads(path.read_text())
        selection = document.get("selection", {})
        wave = selection.get("wave")
        if not isinstance(wave, int) or wave > completed_wave:
            continue
        if (document.get("schema") != runner.CERTIFICATE_SCHEMA or
                document.get("status") !=
                "head-download-full-sha-archive-restore-verified" or
                document.get("generator_model_sha256") != model or
                document.get("inventory_sha256") != inventory):
            raise RuntimeError(f"invalid concrete wave certificate: {path}")
        for item in document.get("completed", []):
            name = str(item.get("filename", ""))
            s3 = item.get("s3", {})
            output = item.get("output", {})
            archive = item.get("archive", {})
            preserved_status = item.get("status")
            resumed = preserved_status == "resumed-frontier-preserved"
            if (name not in expected or expected[name] != wave or
                    preserved_status not in (
                        "generated-preserved", "resumed-frontier-preserved") or
                    (resumed and not document.get("original_scratch_retained")) or
                    not isinstance(s3, dict) or not isinstance(output, dict) or
                    not isinstance(archive, dict) or
                    any(int(s3.get(key, -1)) != 0 for key in (
                        "head_full_sha_residual", "download_full_sha_residual",
                        "archive_restore_residual")) or
                    not str(s3.get("version_id", "")) or
                    str(s3.get("version_id")) == "null" or
                    str(s3.get("sha256", "")) != str(archive.get("sha256", "")) or
                    int(s3.get("bytes", -1)) != int(archive.get("bytes", -2)) or
                    len(str(output.get("sha256", ""))) != 64 or
                    int(output.get("bytes", 0)) <= 0):
                raise RuntimeError(f"malformed preserved result in {path}: {name}")
            record = dict(item)
            record["certificate_path"] = str(path)
            candidates.setdefault(name, []).append(record)
    missing = sorted(set(expected) - set(candidates))
    extra = sorted(set(candidates) - set(expected))
    if missing or extra:
        raise RuntimeError(
            f"wave certificate coverage residual: missing={len(missing)} "
            f"extra={len(extra)}")
    selected: dict[str, dict[str, object]] = {}
    for name, records in sorted(candidates.items()):
        identities = {
            (str(record["output"]["sha256"]), int(record["output"]["bytes"]))
            for record in records
        }
        if len(identities) != 1:
            raise RuntimeError(f"conflicting preserved outputs for {name}")
        selected[name] = min(records, key=lambda record: (
            str(record["s3"]["sha256"]), str(record["s3"]["key"]),
            str(record["s3"]["version_id"])))
    return selected


def exact_s3_download(record: Mapping[str, object], target: Path) -> dict[str, object]:
    bucket = str(record["bucket"])
    key = str(record["key"])
    version = str(record["version_id"])
    digest = str(record["sha256"])
    extent = int(record["bytes"])
    head = json.loads(subprocess.check_output([
        "aws", "s3api", "head-object", "--bucket", bucket, "--key", key,
        "--version-id", version, "--output", "json"], text=True))
    metadata = {str(k).lower(): str(v) for k, v in head.get("Metadata", {}).items()}
    if (head.get("VersionId") != version or
            int(head.get("ContentLength", -1)) != extent or
            metadata.get("sha256") != digest):
        raise RuntimeError("version-pinned S3 HEAD residual")
    if target.exists():
        raise RuntimeError(f"fresh S3 target already exists: {target}")
    target.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        "aws", "s3api", "get-object", "--bucket", bucket, "--key", key,
        "--version-id", version, str(target)], check=True,
        stdout=subprocess.DEVNULL)
    if target.stat().st_size != extent or sha256_path(target) != digest:
        raise RuntimeError("version-pinned S3 download hash/extent residual")
    return {
        "bucket": bucket, "key": key, "version_id": version,
        "bytes": extent, "sha256": digest, "head_residual": 0,
        "download_residual": 0,
    }


def restore_result(record: Mapping[str, object], download: Path,
                   restore: Path, destination: Path) -> dict[str, object]:
    restored = preservation.restore_zstd_archive(
        download, restore, runner.ARCHIVE_SCHEMA)
    name = str(record["filename"])
    member = restored.get(f"tablebases/{name}")
    if member is None:
        raise RuntimeError(f"result archive lacks tablebases/{name}")
    output = record["output"]
    if (member.stat().st_size != int(output["bytes"]) or
            sha256_path(member) != str(output["sha256"])):
        raise RuntimeError(f"restored output residual: {name}")
    if destination.exists():
        raise RuntimeError(f"dependency destination exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(member, destination)
    destination.chmod(0o444)
    return {
        "filename": name, "bytes": destination.stat().st_size,
        "sha256": sha256_path(destination),
    }


def s3_parts(prefix: str, key: str) -> tuple[str, str, str]:
    bucket, full_key, uri = preservation.s3_object(prefix, key)
    return bucket, full_key, uri


def upload_exact(source: Path, *, digest: str, extent: int, prefix: str,
                 key: str, download: Path,
                 archive_schema: str | None) -> dict[str, object]:
    bucket, full_key, uri = s3_parts(prefix, key)
    subprocess.run(["aws", "s3", "cp", str(source), uri, "--metadata",
                    f"sha256={digest}", "--only-show-errors"], check=True)
    head = json.loads(subprocess.check_output([
        "aws", "s3api", "head-object", "--bucket", bucket, "--key", full_key,
        "--output", "json"], text=True))
    version = str(head.get("VersionId", ""))
    if not version or version == "null":
        raise RuntimeError("versioned S3 bucket is required")
    record = {
        "bucket": bucket, "key": full_key, "version_id": version,
        "bytes": extent, "sha256": digest,
    }
    exact_s3_download(record, download)
    if archive_schema is not None:
        preservation.restore_zstd_archive(
            download, download.with_suffix(".restored"), archive_schema)
    return {**record, "head_residual": 0, "download_residual": 0,
            "restore_residual": 0}


def dependency_records(directory: Path) -> list[dict[str, object]]:
    return [
        {"filename": path.name, "bytes": path.stat().st_size,
         "sha256": sha256_path(path)}
        for path in sorted(directory.glob("*.uftb"))
    ]


def require_empty(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)
    if any(path.iterdir()):
        raise RuntimeError(f"assembly output must be empty: {path}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--certificates", type=Path, required=True)
    parser.add_argument("--base-dependencies", type=Path, required=True)
    parser.add_argument("--base-manifest", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--completed-wave", type=int, choices=(0, 1), required=True)
    parser.add_argument("--generator-model-sha256", required=True)
    parser.add_argument("--inventory-sha256", required=True)
    parser.add_argument("--s3-prefix")
    parser.add_argument("--required-free-bytes", type=int, default=0)
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--aws-execution-ack")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    model = args.generator_model_sha256
    inventory = args.inventory_sha256
    if (not re.fullmatch(r"[0-9a-f]{64}", model) or
            not re.fullmatch(r"[0-9a-f]{64}", inventory)):
        raise RuntimeError("model/inventory SHA-256 binding is malformed")
    if runner.inventory_sha256() != inventory:
        raise RuntimeError("current supported inventory differs from target inventory")
    selected = collect_results(
        args.certificates.resolve(), args.completed_wave,
        model=model, inventory=inventory)
    base_manifest = runner.load_dependency_manifest(args.base_manifest.resolve())
    base_required = runner.base_dependency_filenames()
    missing_base = sorted(set(base_required) - set(base_manifest))
    if missing_base:
        raise RuntimeError(f"base dependency residual: {len(missing_base)} missing")
    expected_names = set(base_required) | set(selected)
    plan = {
        "schema": SCHEMA, "status": "plan-only-full-not-launched",
        "generator_model_sha256": model, "inventory_sha256": inventory,
        "completed_wave": args.completed_wave,
        "next_wave": args.completed_wave + 1,
        "base_dependencies": len(base_required),
        "preserved_wave_outputs": len(selected),
        "dependencies": len(expected_names),
        "logical_bytes": sum(int(base_manifest[name]["bytes"])
                             for name in base_required) +
                         sum(int(record["output"]["bytes"])
                             for record in selected.values()),
        "full_explicitly_requested": bool(args.full), "never_delete": True,
    }
    if not args.full:
        print(json.dumps(plan, indent=2, sort_keys=True))
        return 0
    if (not sys.platform.startswith("linux") or
            args.aws_execution_ack != "EC2" or not args.s3_prefix or
            args.required_free_bytes <= 0):
        raise RuntimeError(
            "--full requires Linux/EC2 acknowledgement, S3, and free-space gate")
    output = args.output_directory.resolve()
    require_empty(output)
    if shutil.disk_usage(output).free < args.required_free_bytes:
        raise RuntimeError("initial free disk is below assembly gate")
    dependencies = output / "dependencies"
    staged_base = runner.stage_dependencies(
        args.base_dependencies.resolve(), dependencies,
        base_manifest, base_required)
    records = list(staged_base["files"])
    downloads = output / "downloads"
    restores = output / "result-restores"
    for name, record in selected.items():
        archive = record["s3"]
        download = downloads / f"{name}.tar.zst"
        exact_s3_download(archive, download)
        records.append(restore_result(
            record, download, restores / name,
            dependencies / name))
    records.sort(key=lambda record: str(record["filename"]))
    if {str(record["filename"]) for record in records} != expected_names:
        raise RuntimeError("assembled dependency name conservation residual")
    manifest = {
        "schema": runner.DEPENDENCY_SCHEMA, "files": records,
        "source_manifest_sha256": sha256_path(args.base_manifest.resolve()),
        "generator_model_sha256": model, "inventory_sha256": inventory,
        "through_wave": args.completed_wave,
    }
    preservation.write_json(dependencies / "manifest.json", manifest)
    verification = output / "local-copy-verification"
    runner.stage_dependencies(
        dependencies, verification, {str(x["filename"]): x for x in records},
        sorted(expected_names))
    archive_files = {
        "manifest.json": dependencies / "manifest.json",
        **{f"tablebases/{record['filename']}":
           dependencies / str(record["filename"]) for record in records},
    }
    (output / "archives").mkdir()
    archive, archive_sha = preservation.content_address_archive(
        output / "archives", f"concrete-through-wave-{args.completed_wave}",
        archive_files, ARCHIVE_SCHEMA)
    archive_key = (
        f"concrete/v2/dependencies/model/{model}/through-wave-"
        f"{args.completed_wave}/sha256/{archive_sha}/{archive.name}")
    archive_s3 = upload_exact(
        archive, digest=archive_sha, extent=archive.stat().st_size,
        prefix=args.s3_prefix, key=archive_key,
        download=output / "s3-verify" / archive.name,
        archive_schema=ARCHIVE_SCHEMA)
    certificate = {
        **plan, "schema": CERTIFICATE_SCHEMA,
        "status": "head-version-download-hash-restore-verified-no-delete",
        "manifest_sha256": sha256_path(dependencies / "manifest.json"),
        "archive": archive_s3, "safe_to_delete_gate": False,
        "local_outputs_retained": True, "local_restores_retained": True,
    }
    certificate_path = output / "wave-dependency-certificate.json"
    preservation.write_json(certificate_path, certificate)
    certificate_sha = sha256_path(certificate_path)
    certificate_key = (
        f"concrete/v2/dependencies/certificates/sha256/{certificate_sha}/"
        "wave-dependency-certificate.json")
    certificate_s3 = upload_exact(
        certificate_path, digest=certificate_sha,
        extent=certificate_path.stat().st_size, prefix=args.s3_prefix,
        key=certificate_key,
        download=output / "s3-verify/wave-dependency-certificate.json",
        archive_schema=None)
    print(json.dumps({
        "status": "wave-dependencies-versioned-s3-restored",
        "certificate_sha256": certificate_sha,
        "certificate_s3": certificate_s3,
        "safe_to_delete_gate": False,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
