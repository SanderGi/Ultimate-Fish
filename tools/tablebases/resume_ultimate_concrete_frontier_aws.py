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
import subprocess
import sys
from typing import Any, BinaryIO


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "tablebases"))
import run_double_jester_information_capture as preservation  # noqa: E402
import run_ultimate_concrete_tablebase_shard_aws as concrete  # noqa: E402


SCHEMA = "ultimate-concrete-frontier-resume-v1"
RESULT_SCHEMA = "ultimate-concrete-frontier-resume-result-v1"
EXECUTION_EQUIVALENCE_SCHEMA = \
    "ultimate-concrete-frontier-execution-equivalence-v1"
DEFAULT_MANIFEST = ROOT / "tools/tablebases/ultimate_concrete_wave0_018_resume.json"
CHECKPOINT_MAGIC = b"UFTBCP4\0"
BLOCK = 4 << 20
HOST_MEMORY_RESERVE_BYTES = 8 << 30


def canonical_json(value: object) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(BLOCK), b""):
            digest.update(block)
    return digest.hexdigest()


def historical_generator_model_sha256(root: Path) -> str:
    """Evaluate a retained bundle with its own pinned inventory contract.

    ``generator_model_sha256(root)`` hashes file payloads below ``root`` but
    builds its contract from the currently imported planner.  Once the
    supported inventory expands, that makes an unchanged historical bundle
    appear to have a different model.  A retained bundle includes the exact
    runner and planner that defined its hash, so use those files for the
    historical check.  Tiny unit fixtures without a runnable bundle retain
    the injectable current-helper fallback.
    """
    runner = root / "tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py"
    if not runner.is_file():
        return concrete.generator_model_sha256(root)
    code = (
        "import pathlib,sys;"
        "root=pathlib.Path(sys.argv[1]);"
        "sys.path.insert(0,str(root/'tools/tablebases'));"
        "import run_ultimate_concrete_tablebase_shard_aws as r;"
        "print(r.generator_model_sha256(root))"
    )
    result = subprocess.run(
        [sys.executable, "-c", code, str(root)], check=True, text=True,
        capture_output=True)
    digest = result.stdout.strip()
    if len(digest) != 64 or any(character not in "0123456789abcdef"
                                for character in digest):
        raise RuntimeError("historical source-bundle model output is malformed")
    return digest


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


def reverse_marker_proves_incomplete(
        document: dict[str, Any], log_text: str) -> bool:
    marker = document.get("last_reverse_marker")
    if not isinstance(marker, str) or marker not in log_text:
        return False
    fields = marker.split()
    if len(fields) < 3 or fields[:2] != ["reverse", "states"]:
        return False
    progress = fields[2].split("/")
    if len(progress) != 2 or not all(value.isdigit() for value in progress):
        return False
    completed, total = map(int, progress)
    return total == int(document["states"]) and 0 <= completed < total


def reverse_phase_mtime_proves_frontier_complete(
        document: dict[str, Any], source: Path) -> bool:
    evidence = document.get("reverse_phase_evidence")
    if not isinstance(evidence, dict) or evidence.get("schema") != \
            "ultimate-reverse-phase-mtime-v1":
        return False
    frontier_mtime = evidence.get("frontier_planes_max_mtime_ns")
    reverse_mtimes = evidence.get("reverse_plane_mtime_ns")
    if not isinstance(frontier_mtime, int) or not isinstance(reverse_mtimes, dict) or \
            set(reverse_mtimes) != {"offsets", "predecessors"} or \
            not all(isinstance(value, int) for value in reverse_mtimes.values()):
        return False
    planes = document.get("planes")
    reverse = document.get("discarded_reverse_graph")
    if not isinstance(planes, dict):
        return False
    reverse_records = planes if "offsets" in planes else reverse
    if not isinstance(reverse_records, dict):
        return False
    actual_frontier = max(
        (source / planes[name]["relative_path"]).stat().st_mtime_ns
        for name in ("nodes", "degrees"))
    actual_reverse = {
        name: (source / reverse_records[name]["relative_path"]).stat().st_mtime_ns
        for name in ("offsets", "predecessors")
    }
    return (frontier_mtime == actual_frontier and
            reverse_mtimes == actual_reverse and
            min(actual_reverse.values()) > actual_frontier)


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
    if historical_generator_model_sha256(source / "bundle") != \
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
    reverse_marker = reverse_marker_proves_incomplete(document, log_text)
    reverse_phase = reverse_phase_mtime_proves_frontier_complete(
        document, source)
    if (int(predecessor["allocated_bytes"]) >= int(predecessor["bytes"]) and
            not reverse_marker and not reverse_phase):
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
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--full", action="store_true")
    mode.add_argument(
        "--preserve-completed", action="store_true",
        help=("package, upload, download, and restore a completed --local-only "
              "resume without rerunning or rereading the preserved frontier"))
    mode.add_argument(
        "--preserve-completed-artifacts-only", action="store_true",
        help=("preserve a completed --local-only resume without uploading "
              "the generator binary or repository source files"))
    parser.add_argument("--local-only", action="store_true",
                        help="run and verify locally without packaging or S3 upload")
    parser.add_argument("--aws-execution-ack")
    parser.add_argument("--workers", type=int, choices=range(1, 9), default=1,
                        help="parallel workers for deterministic graph scans")
    parser.add_argument(
        "--execution-bundle-root", type=Path,
        help=("authenticated replacement source root used only after the "
              "preserved frontier is loaded"))
    parser.add_argument("--execution-binary", type=Path)
    parser.add_argument("--execution-model-sha256")
    parser.add_argument("--execution-inventory-sha256")
    parser.add_argument("--execution-binary-sha256")
    parser.add_argument("--execution-equivalence-certificate", type=Path)
    parser.add_argument("--execution-equivalence-sha256")
    parser.add_argument("--resident-limit", type=int, default=0)
    parser.add_argument("--scratch-limit", type=int, default=0)
    parser.add_argument("--reverse-edge-bytes-limit", type=int, default=0)
    parser.add_argument("--minimum-free-bytes", type=int, default=0)
    parser.add_argument("--minimum-host-memory-available-bytes", type=int, default=0)
    parser.add_argument("--monitor-interval", type=float, default=5.0)
    parser.add_argument("--s3-prefix")
    return parser.parse_args(argv)


def execution_context(args: argparse.Namespace,
                      document: dict[str, Any]) -> dict[str, Any]:
    """Authenticate an optional scheduling-only replacement executable.

    A retained frontier is data produced by one exact model.  Reusing it with
    any other binary must therefore fail closed unless an immutable
    equivalence certificate binds both model hashes and the replacement
    executable.  This is intentionally separate from the frontier manifest:
    the preserved source tree and its evidence remain untouched.
    """
    names = (
        "execution_bundle_root", "execution_binary",
        "execution_model_sha256", "execution_inventory_sha256",
        "execution_binary_sha256",
        "execution_equivalence_certificate",
        "execution_equivalence_sha256",
    )
    supplied = [getattr(args, name) is not None for name in names]
    if any(supplied) and not all(supplied):
        raise RuntimeError(
            "replacement execution requires every source/binary/equivalence binding")
    if not any(supplied):
        source = Path(document["source_work_directory"]).resolve()
        return {
            "bundle_root": source / "bundle",
            "binary": source / document["binary"]["relative_path"],
            "model_sha256": document["generator_model_sha256"],
            "inventory_sha256": document["inventory_sha256"],
            "binary_sha256": document["binary_sha256"],
            "frontier_model_sha256": document["generator_model_sha256"],
            "frontier_inventory_sha256": document["inventory_sha256"],
            "equivalence": None,
        }

    bundle = args.execution_bundle_root.resolve()
    binary = args.execution_binary.resolve()
    certificate_path = args.execution_equivalence_certificate.resolve()
    if not bundle.is_dir() or not binary.is_file() or \
            not certificate_path.is_file():
        raise RuntimeError("replacement execution input is missing")
    if concrete.generator_model_sha256(bundle) != args.execution_model_sha256:
        raise RuntimeError("replacement execution model residual")
    if concrete.inventory_sha256() != args.execution_inventory_sha256:
        raise RuntimeError("replacement execution inventory residual")
    if sha256_path(binary) != args.execution_binary_sha256:
        raise RuntimeError("replacement execution binary residual")
    if sha256_path(certificate_path) != args.execution_equivalence_sha256:
        raise RuntimeError("replacement execution equivalence SHA-256 residual")
    certificate = json.loads(certificate_path.read_text(encoding="utf-8"))
    if (certificate.get("schema") != EXECUTION_EQUIVALENCE_SCHEMA or
            certificate.get("status") !=
                "frontier-semantics-byte-identical-verified" or
            certificate.get("frontier_model_sha256") !=
                document["generator_model_sha256"] or
            certificate.get("execution_model_sha256") !=
                args.execution_model_sha256 or
            certificate.get("frontier_inventory_sha256") !=
                document["inventory_sha256"] or
            certificate.get("execution_inventory_sha256") !=
                args.execution_inventory_sha256 or
            certificate.get("execution_binary_sha256") !=
                args.execution_binary_sha256 or
            certificate.get("output_residual") != 0 or
            certificate.get("bellman_residual") != 0):
        raise RuntimeError("replacement execution equivalence binding residual")
    return {
        "bundle_root": bundle,
        "binary": binary,
        "model_sha256": args.execution_model_sha256,
        "inventory_sha256": args.execution_inventory_sha256,
        "binary_sha256": args.execution_binary_sha256,
        "frontier_model_sha256": document["generator_model_sha256"],
        "frontier_inventory_sha256": document["inventory_sha256"],
        "equivalence": {
            "path": str(certificate_path),
            "bytes": certificate_path.stat().st_size,
            "sha256": args.execution_equivalence_sha256,
            "certificate": certificate,
        },
    }


def preserve_completed(args: argparse.Namespace, document: dict[str, Any], *,
                       artifacts_only: bool = False) -> dict[str, Any]:
    """Preserve one verified local-only resume without touching its scratch."""
    if sys.platform == "darwin" or args.aws_execution_ack != "EC2":
        raise RuntimeError("completed frontier preservation is EC2-only")
    if args.work_directory is None or not args.s3_prefix:
        raise RuntimeError(
            "--preserve-completed requires --work-directory and --s3-prefix")
    if args.local_only:
        raise RuntimeError("--preserve-completed cannot be combined with --local-only")
    work = args.work_directory.resolve()
    if not work.is_dir():
        raise RuntimeError("completed resume work directory is missing")

    manifest_path = args.manifest.resolve()
    manifest_sha = sha256_path(manifest_path)
    source = Path(document["source_work_directory"]).resolve()
    record = document["record"]
    output_name = str(record["filename"])
    checkpoint_stem = Path(output_name).stem
    output = work / "outputs" / output_name
    log = work / "logs/generate" / f"{checkpoint_stem}.log"
    result_path = work / "results" / f"{checkpoint_stem}.json"
    plan_path = work / "resume-plan.json"
    binary = work / "binary/ultimate_tablebase"
    dependency_manifest = source / "dependencies/manifest.json"
    required = (output, log, result_path, plan_path, binary,
                dependency_manifest)
    if any(not path.is_file() for path in required):
        raise RuntimeError("completed resume lacks a required preservation input")

    result = json.loads(result_path.read_text(encoding="utf-8"))
    plan = json.loads(plan_path.read_text(encoding="utf-8"))
    if (result.get("schema") != RESULT_SCHEMA or
            result.get("record") != record or
            result.get("generator_model_sha256") !=
                document["generator_model_sha256"] or
            result.get("inventory_sha256") != document["inventory_sha256"] or
            result.get("resume_manifest_sha256") != manifest_sha or
            result.get("bellman_verification_residual") != 0 or
            plan.get("manifest_sha256") != manifest_sha or
            plan.get("record") != record or
            plan.get("status") != "authenticated-native-checkpoint-composed"):
        raise RuntimeError("completed resume provenance residual")
    if (sha256_path(binary) != document["binary_sha256"] or
            sha256_path(dependency_manifest) !=
                document["dependency_manifest_sha256"] or
            historical_generator_model_sha256(source / "bundle") !=
                document["generator_model_sha256"]):
        raise RuntimeError("completed resume source/binary binding residual")
    verification = concrete.parse_uftb(output, record, log)
    if result.get("output") != verification:
        raise RuntimeError("completed resume result/output residual")

    files: dict[str, Path] = {
        f"tablebases/{output_name}": output,
        f"proof/{result_path.name}": result_path,
        f"proof/{log.name}": log,
        "proof/resume-plan.json": plan_path,
        "proof/resume-manifest.json": manifest_path,
        "proof/dependency-manifest.json": dependency_manifest,
    }
    if not artifacts_only:
        files["binary/ultimate_tablebase"] = binary
        for relative in concrete.MODEL_SOURCES:
            files[f"sources/{relative}"] = source / "bundle" / relative
    archive, archive_sha = preservation.content_address_archive(
        work / "archives", checkpoint_stem, files, RESULT_SCHEMA)
    restored = preservation.restore_zstd_archive(
        archive, work / "restore-local" / checkpoint_stem, RESULT_SCHEMA)
    if concrete.parse_uftb(
            restored[f"tablebases/{output_name}"], record,
            restored[f"proof/{log.name}"])["sha256"] != verification["sha256"]:
        raise RuntimeError("local resume archive restore residual")
    selection = document.get(
        "selection", {"wave": 0, "begin": 18, "end": 19, "classes": 1})
    kind = "artifacts-only" if artifacts_only else "model"
    key = (f"concrete/v2/{kind}/{document['generator_model_sha256']}/"
           f"wave-{selection['wave']}/sha256/{archive_sha}/{archive.name}")
    remote = preservation.upload_head_download_verify(
        source=archive, digest=archive_sha, extent=archive.stat().st_size,
        prefix=args.s3_prefix, key=key,
        download=work / "s3-verify" / archive.name,
        archive_schema=RESULT_SCHEMA)
    certificate = {
        "schema": concrete.CERTIFICATE_SCHEMA,
        "status": ("head-download-full-sha-artifact-only-archive-restore-verified"
                   if artifacts_only else
                   "head-download-full-sha-archive-restore-verified"),
        "generator_model_sha256": document["generator_model_sha256"],
        "inventory_sha256": document["inventory_sha256"],
        "selection": selection,
        "completed": [{
            "filename": output_name,
            "status": ("resumed-frontier-artifacts-preserved"
                       if artifacts_only else "resumed-frontier-preserved"),
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
    return {
        "status": ("completed-local-resume-artifacts-only-s3-restored"
                   if artifacts_only else
                   "completed-local-resume-s3-restored"),
        "certificate_sha256": certificate_sha,
        "certificate_s3": certificate_remote,
        "generator_rerun": False, "preserved_frontier_reread": False,
        "local_scratch_retained": True,
    }


def validate_full_gates(args: argparse.Namespace, work: Path,
                        document: dict[str, Any]) -> dict[str, int]:
    if sys.platform == "darwin" or args.aws_execution_ack != "EC2":
        raise RuntimeError("full frontier resume is EC2-only")
    if ((not args.local_only and not args.s3_prefix) or
            args.monitor_interval <= 0):
        raise RuntimeError(
            "full resume requires S3 unless --local-only is selected, and a "
            "positive monitor interval")
    gates = (args.resident_limit, args.scratch_limit,
             args.reverse_edge_bytes_limit, args.minimum_free_bytes,
             args.minimum_host_memory_available_bytes)
    if any(value <= 0 for value in gates):
        raise RuntimeError("full resume requires every explicit resource gate")
    if args.resident_limit < 16 << 30:
        raise RuntimeError("frontier resume resident gate must be at least 16 GiB")
    available_memory = host_memory_available_bytes()
    required_memory = args.resident_limit + HOST_MEMORY_RESERVE_BYTES
    if available_memory < args.minimum_host_memory_available_bytes or \
            available_memory < required_memory:
        raise RuntimeError("host memory headroom gate failed")
    # The destination is deliberately required not to exist yet.  Probe the
    # nearest existing ancestor so a fresh resume root can pass its fail-closed
    # disk gate without first mutating the filesystem.  In particular,
    # shutil.disk_usage(work.parent) raises FileNotFoundError when both the
    # class directory and its campaign parent are new.
    disk_probe = work.parent
    while not disk_probe.exists():
        parent = disk_probe.parent
        if parent == disk_probe:
            raise RuntimeError("resume destination has no existing ancestor")
        disk_probe = parent
    available_disk = shutil.disk_usage(disk_probe).free
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
    if args.preserve_completed or args.preserve_completed_artifacts_only:
        if args.execution_bundle_root is not None:
            raise RuntimeError(
                "replacement execution preservation must use the original full run")
        print(canonical_json(preserve_completed(
            args, document,
            artifacts_only=args.preserve_completed_artifacts_only)))
        return 0
    authenticated = authenticate_manifest(document)
    execution = execution_context(args, document)
    preflight: dict[str, Any] = {
        "schema": SCHEMA, "status": "authenticated-read-only-preflight",
        "manifest_sha256": sha256_path(args.manifest.resolve()),
        "generator_model_sha256": execution["model_sha256"],
        "frontier_model_sha256": execution["frontier_model_sha256"],
        "frontier_inventory_sha256": execution["frontier_inventory_sha256"],
        "execution_binary_sha256": execution["binary_sha256"],
        "execution_equivalence": execution["equivalence"],
        "inventory_sha256": execution["inventory_sha256"],
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
    shutil.copy2(execution["binary"], binary)
    binary.chmod(0o555)
    if sha256_path(binary) != execution["binary_sha256"]:
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
        "--workers", str(args.workers),
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
        "generator_model_sha256": execution["model_sha256"],
        "frontier_model_sha256": execution["frontier_model_sha256"],
        "frontier_inventory_sha256": execution["frontier_inventory_sha256"],
        "execution_equivalence": execution["equivalence"],
        "inventory_sha256": execution["inventory_sha256"],
        "record": record, "output": verification,
        "resume_manifest_sha256": preflight["manifest_sha256"],
        "resume_checkpoint": checkpoint,
        "resource_certificate": resource_certificate,
        "original_scratch_retained": True, "never_delete": True,
        "bellman_verification_residual": 0,
    }
    result_path = work / "results" / f"{checkpoint_stem}.json"
    preservation.write_json(result_path, result)
    if args.local_only:
        print(canonical_json({
            "status": "resumed-and-verified-local-only",
            "output": verification,
            "result": str(result_path),
            "original_scratch_retained": True,
        }))
        return 0
    files = {
        f"tablebases/{output_name}": output,
        f"proof/{result_path.name}": result_path,
        f"proof/{log.name}": log,
        "proof/resume-plan.json": work / "resume-plan.json",
        "proof/resume-manifest.json": args.manifest.resolve(),
        "proof/dependency-manifest.json": source / "dependencies/manifest.json",
        "binary/ultimate_tablebase": binary,
    }
    if execution["equivalence"] is not None:
        files["proof/execution-equivalence.json"] = Path(
            execution["equivalence"]["path"])
    for relative in concrete.MODEL_SOURCES:
        files[f"sources/{relative}"] = execution["bundle_root"] / relative
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
    key = (f"concrete/v2/model/{execution['model_sha256']}/"
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
        "generator_model_sha256": execution["model_sha256"],
        "frontier_model_sha256": execution["frontier_model_sha256"],
        "frontier_inventory_sha256": execution["frontier_inventory_sha256"],
        "execution_equivalence": execution["equivalence"],
        "inventory_sha256": execution["inventory_sha256"],
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
