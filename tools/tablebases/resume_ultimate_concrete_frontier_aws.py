#!/usr/bin/env python3
"""Resume one authenticated concrete frontier without touching failed scratch.

The ordinary concrete generator already has an exact checkpoint loader.  This
tool authenticates a resource-gated AWS run, proves that its frontier completed
before retrograde propagation began, and composes a native checkpoint in a new
work tree from the preserved node and predecessor-degree planes.  Local
manifests may also bind the discarded partial reverse planes; transportable
manifests retain only their phase evidence because those planes are rebuilt.
Every retained input is opened read-only and is never truncated or renamed.

Default execution is a read-only preflight.  ``--full`` is EC2-only and runs
the already-authenticated generator binary with explicit RSS/disk gates before
performing the ordinary content-addressed S3 upload/download/restore proof.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys
from typing import Any, BinaryIO


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "tablebases"))
import run_double_jester_information_capture as preservation  # noqa: E402
import run_ultimate_concrete_tablebase_shard_aws as concrete  # noqa: E402


SCHEMA = "ultimate-concrete-frontier-resume-v1"
RESULT_SCHEMA = "ultimate-concrete-frontier-resume-result-v1"
DEFAULT_MANIFEST = ROOT / "tools/tablebases/ultimate_concrete_wave0_018_resume.json"
CHECKPOINT_MAGIC = b"UFTBCP4\0"
BLOCK = 4 << 20


def canonical_json(value: object) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(BLOCK), b""):
            digest.update(block)
    return digest.hexdigest()


def load_manifest(path: Path) -> dict[str, Any]:
    document = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(document, dict) or document.get("schema") != SCHEMA:
        raise RuntimeError("frontier resume manifest schema mismatch")
    return document


def authenticate(path: Path, record: dict[str, Any], label: str) -> dict[str, Any]:
    if not path.is_file():
        raise RuntimeError(f"missing {label}: {path}")
    extent = path.stat().st_size
    expected_extent = int(record["bytes"])
    if extent != expected_extent:
        raise RuntimeError(
            f"{label} extent mismatch: {extent} != {expected_extent}")
    digest = sha256_path(path)
    if digest != record["sha256"]:
        raise RuntimeError(f"{label} SHA-256 mismatch")
    return {"path": str(path), "bytes": extent, "sha256": digest}


def host_memory_available_bytes(path: Path = Path("/proc/meminfo")) -> int:
    for line in path.read_text(encoding="ascii").splitlines():
        if line.startswith("MemAvailable:"):
            return int(line.split()[1]) * 1024
    raise RuntimeError("/proc/meminfo lacks MemAvailable")


def _required_record(document: dict[str, Any], name: str) -> dict[str, Any]:
    record = document.get(name)
    if not isinstance(record, dict):
        raise RuntimeError(f"resume manifest lacks {name}")
    digest = str(record.get("sha256", ""))
    if len(digest) != 64 or any(character not in "0123456789abcdef"
                                for character in digest):
        raise RuntimeError(f"resume manifest has malformed {name} SHA-256")
    if int(record.get("bytes", 0)) <= 0:
        raise RuntimeError(f"resume manifest has malformed {name} extent")
    return record


def authenticate_manifest(document: dict[str, Any]) -> dict[str, Any]:
    source = Path(document["source_work_directory"]).resolve()
    if not source.is_dir():
        raise RuntimeError("preserved source work directory is missing")
    record = document.get("record")
    if (not isinstance(record, dict) or
            Path(str(record.get("filename", ""))).name != record.get("filename") or
            record.get("primary") not in concrete.PIECE_INDEX or
            record.get("secondary") not in concrete.PIECE_INDEX):
        raise RuntimeError("resume manifest has a malformed bounded class")
    if int(record.get("states", 0)) != int(document.get("states", -1)):
        raise RuntimeError("resume state count residual")
    if int(document.get("substates", 0)) <= 0:
        raise RuntimeError("resume substate residual")
    selection = document.get("selection")
    if selection is not None:
        if (not isinstance(selection, dict) or selection.get("wave") not in (0, 1, 2) or
                int(selection.get("classes", 0)) != 1 or
                int(selection.get("end", -1)) !=
                int(selection.get("begin", -2)) + 1):
            raise RuntimeError("resume selection residual")
        rows = concrete.wave_inventory(int(selection["wave"]))
        begin = int(selection["begin"])
        if (begin < 0 or begin >= len(rows) or
                concrete.normalized_record(rows[begin]) != record):
            raise RuntimeError("resume selection/class binding residual")

    files: dict[str, Any] = {}
    for name in ("run_plan", "generator_log", "resource_certificate",
                 "binary", "dependency_manifest"):
        expected = _required_record(document, name)
        files[name] = authenticate(
            source / expected["relative_path"], expected, name)

    planes = document.get("planes")
    if not isinstance(planes, dict) or set(planes) not in ({
            "nodes", "degrees"}, {
            "nodes", "degrees", "offsets", "predecessors"}):
        raise RuntimeError(
            "resume manifest must bind the two checkpoint planes, with "
            "optional retained reverse planes")
    for name in sorted(planes):
        expected = _required_record(planes, name)
        path = source / expected["relative_path"]
        files[name] = authenticate(path, expected, f"{name} plane")
        expected_blocks = expected.get("allocated_bytes")
        if expected_blocks is not None and path.stat().st_blocks * 512 != \
                int(expected_blocks):
            raise RuntimeError(f"{name} sparse-allocation residual")

    run_plan = json.loads(Path(files["run_plan"]["path"]).read_text())
    for key in ("generator_model_sha256", "inventory_sha256",
                "binary_sha256", "dependency_manifest_sha256"):
        if run_plan.get(key) != document.get(key):
            raise RuntimeError(f"preserved run-plan {key} residual")
    if run_plan.get("selected") != [record] or run_plan.get("status") != \
            "full-preflight":
        raise RuntimeError("preserved run-plan selection/status residual")
    if concrete.generator_model_sha256(source / "bundle") != \
            document["generator_model_sha256"]:
        raise RuntimeError("preserved source-bundle model residual")
    if files["binary"]["sha256"] != document["binary_sha256"]:
        raise RuntimeError("preserved generator binary residual")
    if files["dependency_manifest"]["sha256"] != \
            document["dependency_manifest_sha256"]:
        raise RuntimeError("preserved dependency manifest residual")

    resource = json.loads(Path(files["resource_certificate"]["path"]).read_text())
    if (resource.get("returncode") != -15 or
            resource.get("violation") != document["resource_violation"] or
            not resource.get("scratch_retained")):
        raise RuntimeError("resource-gate preservation certificate residual")
    log_text = Path(files["generator_log"]["path"]).read_text(
        encoding="utf-8", errors="strict")
    if document["last_frontier_marker"] not in log_text:
        raise RuntimeError("frontier completion evidence is absent")
    if any(marker in log_text for marker in
           ("propagate queue", "verifyok states", "complete states")):
        raise RuntimeError("retrograde may have modified the frontier; refusing resume")
    if "predecessors" in planes:
        predecessor = planes["predecessors"]
    else:
        reverse = document.get("discarded_reverse_graph")
        if not isinstance(reverse, dict) or set(reverse) != {
                "offsets", "predecessors"}:
            raise RuntimeError(
                "transportable resume manifest lacks discarded reverse evidence")
        for name in ("offsets", "predecessors"):
            record = reverse[name]
            if not isinstance(record, dict):
                raise RuntimeError("malformed discarded reverse evidence")
            relative = Path(str(record.get("relative_path", "")))
            if (relative.is_absolute() or ".." in relative.parts or
                    not relative.name or
                    int(record.get("bytes", 0)) <= 0 or
                    int(record.get("allocated_bytes", -1)) < 0):
                raise RuntimeError("malformed discarded reverse evidence")
        predecessor = reverse["predecessors"]
    if int(predecessor["allocated_bytes"]) >= int(predecessor["bytes"]):
        raise RuntimeError("reverse graph was not proven incomplete")

    return {
        "source": str(source), "record": record, "files": files,
        "frontier_status": (
            "authenticated-complete-frontier-reverse-incomplete-"
            "retrograde-not-started"),
    }


def copy_exact(source: Path, output: BinaryIO,
               expected: dict[str, Any]) -> dict[str, Any]:
    digest = hashlib.sha256()
    extent = 0
    with source.open("rb") as stream:
        for block in iter(lambda: stream.read(BLOCK), b""):
            output.write(block)
            digest.update(block)
            extent += len(block)
    actual = digest.hexdigest()
    if extent != int(expected["bytes"]) or actual != expected["sha256"]:
        raise RuntimeError("preserved plane changed while composing checkpoint")
    return {"bytes": extent, "sha256": actual}


def compose_checkpoint(document: dict[str, Any], destination: Path) -> dict[str, Any]:
    """Compose the generator's native checkpoint in a new exclusive file."""
    source = Path(document["source_work_directory"]).resolve()
    record = document["record"]
    header = struct.pack(
        "<8sIIIIII", CHECKPOINT_MAGIC,
        concrete.PIECE_INDEX[str(record["primary"])],
        concrete.PIECE_INDEX[str(record["secondary"])],
        int(bool(record["opposing"])), int(document["states"]),
        int(document["substates"]), int(document["states"]))
    destination.parent.mkdir(parents=True, exist_ok=False)
    with destination.open("xb") as output:
        output.write(header)
        copied = {
            name: copy_exact(
                source / document["planes"][name]["relative_path"], output,
                document["planes"][name])
            for name in ("nodes", "degrees")
        }
        output.flush()
        os.fsync(output.fileno())
    expected_extent = len(header) + sum(item["bytes"] for item in copied.values())
    if destination.stat().st_size != expected_extent:
        raise RuntimeError("native checkpoint extent residual")
    return {
        "path": str(destination), "bytes": expected_extent,
        "sha256": sha256_path(destination), "planes": copied,
        "processed": int(document["states"]),
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--work-directory", type=Path)
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--aws-execution-ack")
    parser.add_argument("--resident-limit", type=int, default=0)
    parser.add_argument("--scratch-limit", type=int, default=0)
    parser.add_argument("--reverse-edge-bytes-limit", type=int, default=0)
    parser.add_argument("--minimum-free-bytes", type=int, default=0)
    parser.add_argument("--minimum-host-memory-available-bytes", type=int, default=0)
    parser.add_argument("--monitor-interval", type=float, default=5.0)
    parser.add_argument("--s3-prefix")
    return parser.parse_args(argv)


def validate_full_gates(args: argparse.Namespace, work: Path,
                        document: dict[str, Any]) -> dict[str, int]:
    if sys.platform == "darwin" or args.aws_execution_ack != "EC2":
        raise RuntimeError("full frontier resume is EC2-only")
    if not args.s3_prefix or args.monitor_interval <= 0:
        raise RuntimeError("full resume requires S3 and a positive monitor interval")
    gates = (args.resident_limit, args.scratch_limit,
             args.reverse_edge_bytes_limit, args.minimum_free_bytes,
             args.minimum_host_memory_available_bytes)
    if any(value <= 0 for value in gates):
        raise RuntimeError("full resume requires every explicit resource gate")
    if args.resident_limit < 16 << 30:
        raise RuntimeError("frontier resume resident gate must be at least 16 GiB")
    available_memory = host_memory_available_bytes()
    if available_memory < args.minimum_host_memory_available_bytes or \
            available_memory < args.resident_limit * 2:
        raise RuntimeError("host memory headroom gate failed")
    available_disk = shutil.disk_usage(work.parent).free
    if available_disk < args.minimum_free_bytes:
        raise RuntimeError("initial resume disk free gate failed")
    # Native checkpoint + new disk-backed planes + packed output/restore.
    checkpoint = 32 + int(document["planes"]["nodes"]["bytes"]) + \
        int(document["planes"]["degrees"]["bytes"])
    reverse = (document["planes"] if "predecessors" in document["planes"]
               else document["discarded_reverse_graph"])
    active_peak = (checkpoint + int(document["planes"]["nodes"]["bytes"]) +
                   int(document["planes"]["degrees"]["bytes"]) +
                   int(reverse["offsets"]["bytes"]) +
                   int(reverse["predecessors"]["bytes"]) +
                   int(document["record"]["packed_bytes"]))
    durable = active_peak + 4 * int(document["record"]["packed_bytes"])
    if available_disk - durable < args.minimum_free_bytes:
        raise RuntimeError("resume durable-footprint disk gate failed")
    if active_peak > args.scratch_limit:
        raise RuntimeError("resume scratch limit is below exact plane footprint")
    return {
        "host_memory_available_bytes": available_memory,
        "filesystem_free_bytes": available_disk,
        "durable_footprint_bytes": durable,
    }


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    document = load_manifest(args.manifest.resolve())
    authenticated = authenticate_manifest(document)
    preflight: dict[str, Any] = {
        "schema": SCHEMA, "status": "authenticated-read-only-preflight",
        "manifest_sha256": sha256_path(args.manifest.resolve()),
        "generator_model_sha256": document["generator_model_sha256"],
        "inventory_sha256": document["inventory_sha256"],
        "record": document["record"],
        "frontier_status": authenticated["frontier_status"],
        "preserved_planes": authenticated["files"],
        "original_scratch_retained": True,
    }
    if not args.full:
        print(json.dumps(preflight, sort_keys=True))
        return 0
    if args.work_directory is None:
        raise RuntimeError("--full requires --work-directory")
    work = args.work_directory.resolve()
    if work == Path(document["source_work_directory"]).resolve():
        raise RuntimeError("resume destination must differ from preserved source")
    if work.exists() and any(work.iterdir()):
        raise RuntimeError("resume destination must be empty")
    resources = validate_full_gates(args, work, document)
    work.mkdir(parents=True, exist_ok=True)

    source = Path(document["source_work_directory"]).resolve()
    binary = work / "binary/ultimate_tablebase"
    binary.parent.mkdir(parents=True)
    shutil.copy2(source / document["binary"]["relative_path"], binary)
    binary.chmod(0o555)
    if sha256_path(binary) != document["binary_sha256"]:
        raise RuntimeError("copied generator binary residual")
    (work / "outputs").mkdir()
    checkpoint_stem = Path(document["record"]["filename"]).stem
    checkpoint = compose_checkpoint(
        document, work / "scratch" / checkpoint_stem)
    preservation.write_json(work / "resume-plan.json", {
        **preflight, "status": "authenticated-native-checkpoint-composed",
        "checkpoint": checkpoint, "resource_preflight": resources,
        "resource_gates": {
            "resident_limit": args.resident_limit,
            "scratch_limit": args.scratch_limit,
            "reverse_edge_bytes_limit": args.reverse_edge_bytes_limit,
            "minimum_free_bytes": args.minimum_free_bytes,
            "minimum_host_memory_available_bytes":
                args.minimum_host_memory_available_bytes,
        },
    })

    record = document["record"]
    output_name = str(record["filename"])
    command = [
        "binary/ultimate_tablebase", "--piece", str(record["primary"]),
        "--piece2", str(record["secondary"]),
    ]
    if record["opposing"]:
        command.append("--opposing")
    command.extend([
        "--output", f"outputs/{output_name}",
        "--checkpoint", f"scratch/{checkpoint_stem}",
        "--checkpoint-every", str(document["states"]), "--disk-backed",
    ])
    environment = dict(os.environ)
    environment["ULTIMATE_TABLEBASE_PRESERVE_SCRATCH"] = "1"
    environment["ULTIMATE_TABLEBASE_PATH"] = str(source / "dependencies")
    log = work / "logs/generate" / f"{checkpoint_stem}.log"
    resource_certificate = concrete.run_logged_monitored(
        command, log, work, environment,
        checkpoint_stem=checkpoint_stem, output_name=output_name,
        scratch_limit=args.scratch_limit, resident_limit=args.resident_limit,
        reverse_edge_bytes_limit=args.reverse_edge_bytes_limit,
        minimum_free_bytes=args.minimum_free_bytes,
        monitor_interval=args.monitor_interval)
    output = work / "outputs" / output_name
    verification = concrete.parse_uftb(output, record, log)
    result = {
        "schema": RESULT_SCHEMA,
        "generator_model_sha256": document["generator_model_sha256"],
        "inventory_sha256": document["inventory_sha256"],
        "record": record, "output": verification,
        "resume_manifest_sha256": preflight["manifest_sha256"],
        "resume_checkpoint": checkpoint,
        "resource_certificate": resource_certificate,
        "original_scratch_retained": True, "never_delete": True,
        "bellman_verification_residual": 0,
    }
    result_path = work / "results" / f"{checkpoint_stem}.json"
    preservation.write_json(result_path, result)
    files = {
        f"tablebases/{output_name}": output,
        f"proof/{result_path.name}": result_path,
        f"proof/{log.name}": log,
        "proof/resume-plan.json": work / "resume-plan.json",
        "proof/resume-manifest.json": args.manifest.resolve(),
        "proof/dependency-manifest.json": source / "dependencies/manifest.json",
        "binary/ultimate_tablebase": binary,
    }
    for relative in concrete.MODEL_SOURCES:
        files[f"sources/{relative}"] = source / "bundle" / relative
    archive, archive_sha = preservation.content_address_archive(
        work / "archives", checkpoint_stem, files, RESULT_SCHEMA)
    restored = preservation.restore_zstd_archive(
        archive, work / "restore-local" / checkpoint_stem, RESULT_SCHEMA)
    if concrete.parse_uftb(restored[f"tablebases/{output_name}"], record,
                           restored[f"proof/{log.name}"])["sha256"] != \
            verification["sha256"]:
        raise RuntimeError("local resume archive restore residual")
    selection = document.get(
        "selection", {"wave": 0, "begin": 18, "end": 19, "classes": 1})
    key = (f"concrete/v2/model/{document['generator_model_sha256']}/"
           f"wave-{selection['wave']}/"
           f"sha256/{archive_sha}/{archive.name}")
    remote = preservation.upload_head_download_verify(
        source=archive, digest=archive_sha, extent=archive.stat().st_size,
        prefix=args.s3_prefix, key=key,
        download=work / "s3-verify" / archive.name,
        archive_schema=RESULT_SCHEMA)
    certificate = {
        "schema": concrete.CERTIFICATE_SCHEMA,
        "status": "head-download-full-sha-archive-restore-verified",
        "generator_model_sha256": document["generator_model_sha256"],
        "inventory_sha256": document["inventory_sha256"],
        "selection": selection,
        "completed": [{
            "filename": output_name, "status": "resumed-frontier-preserved",
            "output": verification,
            "archive": {"bytes": archive.stat().st_size,
                        "sha256": archive_sha, "key": key},
            "s3": remote,
        }],
        "original_scratch_retained": True,
        "local_outputs_retained": True, "local_scratch_retained": True,
        "safe_to_delete_gate": False,
    }
    certificate_path = work / "certificates/wave-certificate.json"
    preservation.write_json(certificate_path, certificate)
    certificate_sha = sha256_path(certificate_path)
    certificate_remote = preservation.upload_head_download_verify(
        source=certificate_path, digest=certificate_sha,
        extent=certificate_path.stat().st_size, prefix=args.s3_prefix,
        key=(f"concrete/v2/certificates/sha256/{certificate_sha}/"
             "wave-certificate.json"),
        download=work / "s3-verify/wave-certificate.json",
        archive_schema=None)
    print(canonical_json({
        "status": "resumed-and-s3-restored",
        "certificate_sha256": certificate_sha,
        "certificate_s3": certificate_remote,
        "original_scratch_retained": True,
    }))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
