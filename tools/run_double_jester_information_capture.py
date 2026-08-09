#!/usr/bin/env python3
"""Measure or run the exact persistent K+2-Jesters information capture.

The default is deliberately measurement-only.  A capture is launched only
with ``--full`` and explicit scratch/RSS limits.  Full runs use immutable,
hash-checked copies of every source and tablebase dependency, preserve both
the raw fixed-point arena and the compact arbitrary-belief sidecar in
deterministic content-addressed archives, and never remove local data.

An S3 preservation claim is made only after metadata-bound upload, HEAD,
fresh download, full SHA-256 verification, and archive restoration all pass.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tarfile
from typing import Mapping


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import ultimate_information_tablebases as information  # noqa: E402


SEMANTICS = information.SEMANTICS_ID
DOMAIN = "kjesterjesterk"
RAW_ARCHIVE_SCHEMA = "ultimate-double-jester-raw-recovery-v2"
COMPACT_ARCHIVE_SCHEMA = "ultimate-double-jester-arbitrary-result-v2"
RUN_SCHEMA = "ultimate-double-jester-capture-run-v3"
S3_MANIFEST_SCHEMA = "ultimate-double-jester-s3-upload-v2"
S3_CERTIFICATE_SCHEMA = "ultimate-double-jester-s3-certificate-v2"
DEFAULT_REQUIRED_FREE = 100 << 30
CONSERVATIVE_RESIDENT_BYTES = 4 << 30
AUDITED_LOWER_REBIND = {
    "from_model_sha256":
        "095d2301b1cda56e6aa75db7663d7a4b811d36cf837c1482a6c491940b63d831",
    "to_model_sha256":
        "0ed6d361e313623234c21f9a1c800947014ce47320b4a71fca4fb20a255587c2",
    "source_sha256":
        "3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa",
    "provided_full_sha256":
        "d651d8a1142632b705c464c8b8e002575ed95cf6a0e0b327d1d4853cf409eb2d",
    "rebound_full_sha256":
        "ab806963760bcd52163d6acfbb7d5a0c6f2b2dcc8a616a61c16ae060a2d412ee",
    "payload_sha256":
        "f905c5c8f8265b8fd30fa78831280f853a4801b4fa2670c1bf8a2cce8604ea51",
}
CAPTURE_SOURCES = tuple(
    path.relative_to(information.ROOT).as_posix()
    for path in information.DOUBLE_JESTER_CAPTURE_SOLVER_SOURCES
)
BUILD_SOURCES = (
    "src/ultimate/double_jester_information_capture.cpp",
    "src/ultimate/position.cpp",
    "src/ultimate/information.cpp",
    "src/ultimate/information_solver.cpp",
    "src/ultimate/nnue.cpp",
)
PINNED_COMPILER = "/usr/bin/clang++" if sys.platform == "darwin" else "clang++"
PINNED_BUILD = (
    PINNED_COMPILER, "-std=c++17", "-O3", "-DNDEBUG", "-Wall", "-Wextra",
    "-Wpedantic", "-Werror", "-Wno-error=range-loop-construct", "-pthread",
)


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def sha256_payload(path: Path, offset: int = 160) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        stream.seek(offset)
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def capture_model_sha256(root: Path = ROOT) -> str:
    """Return the catalog-owned capture-only solver fingerprint."""
    return information.double_jester_capture_model_fingerprint(root=root)


def overlay_binding(path: Path) -> tuple[str, str]:
    with path.open("rb") as stream:
        header = stream.read(160)
    if len(header) != 160 or header[:8] != b"UFIW2\0\0\0":
        raise ValueError(f"invalid information overlay: {path}")
    try:
        source = header[32:96].decode("ascii")
        model = header[96:160].decode("ascii")
    except UnicodeDecodeError as error:
        raise ValueError(f"non-ASCII information overlay binding: {path}") from error
    if not re.fullmatch(r"[0-9a-f]{64}", source) or not re.fullmatch(
            r"[0-9a-f]{64}", model):
        raise ValueError(f"invalid information overlay SHA binding: {path}")
    return source, model


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def require_empty_work_directory(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)
    if any(path.iterdir()):
        raise RuntimeError(f"work directory must be empty: {path}")


def stage_source_bundle(work_directory: Path, model: str) -> Path:
    bundle = work_directory / "bundle"
    for relative in CAPTURE_SOURCES:
        target = bundle / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / relative, target)
        target.chmod(0o444)
    # The runner is operational provenance, not part of the solver model.
    runner_target = bundle / "tools/run_double_jester_information_capture.py"
    runner_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(Path(__file__).resolve(), runner_target)
    runner_target.chmod(0o444)
    if capture_model_sha256(bundle) != model:
        raise RuntimeError("staged capture source bundle disagrees with model")
    return bundle


def stage_external_inputs(args: argparse.Namespace, work: Path) -> tuple[
        dict[str, Path], dict[str, object]]:
    sources = {
        "input": args.input.resolve(),
        "lower_concrete": args.lower_concrete.resolve(),
        "lower_overlay": args.lower_overlay.resolve(),
        "expected_overlay": args.expected_overlay.resolve(),
    }
    for name, path in sources.items():
        if not path.is_file():
            raise RuntimeError(f"missing {name} dependency: {path}")
    source_sha = sha256_path(sources["input"])
    lower_source, lower_model = overlay_binding(sources["lower_overlay"])
    expected_source, original_model = overlay_binding(sources["expected_overlay"])
    if sha256_path(sources["lower_concrete"]) != lower_source:
        raise RuntimeError("lower concrete SHA-256 disagrees with lower overlay")
    if expected_source != source_sha:
        raise RuntimeError("ordinary overlay source disagrees with input table")
    current_lower_model = information.solver_model_fingerprint("kjesterk.uftb")
    current_original_model = information.solver_model_fingerprint(
        "kjesterjesterk.uftb")
    lower_rebind: dict[str, object] | None = None
    if lower_model != current_lower_model:
        rebound_payload = bytearray(sources["lower_overlay"].read_bytes())
        rebound_payload[96:160] = current_lower_model.encode("ascii")
        audit_actual = {
            "from_model_sha256": lower_model,
            "to_model_sha256": current_lower_model,
            "source_sha256": lower_source,
            "provided_full_sha256": sha256_path(sources["lower_overlay"]),
            "rebound_full_sha256": hashlib.sha256(rebound_payload).hexdigest(),
            "payload_sha256": sha256_payload(sources["lower_overlay"]),
        }
        if not args.allow_audited_lower_rebind:
            raise RuntimeError(
                "lower information overlay model is stale; the known 095d->0ed6 "
                "header-only migration requires --allow-audited-lower-rebind")
        if audit_actual != AUDITED_LOWER_REBIND:
            raise RuntimeError("lower overlay is not the exact audited rebind input")
        lower_rebind = {
            **audit_actual, "header_bytes_rewritten": 64,
            "changed_byte_count": sum(
                left != right
                for left, right in zip(
                    sources["lower_overlay"].read_bytes()[96:160],
                    current_lower_model.encode("ascii"))),
            "payload_residual": 0, "explicitly_authorized": True,
        }
    if original_model != current_original_model:
        raise RuntimeError("ordinary double-Jester overlay model is stale")

    names = {
        "input": "inputs/kjesterjesterk.uftb",
        "lower_concrete": "inputs/kjesterk.uftb",
        "lower_overlay": "inputs/kjesterk.ufiw",
        "expected_overlay": "inputs/kjesterjesterk.expected.ufiw",
    }
    staged: dict[str, Path] = {}
    records: dict[str, object] = {}
    for role in sorted(sources):
        target = work / names[role]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(sources[role], target)
        source_digest = sha256_path(sources[role])
        if sha256_path(target) != source_digest:
            raise RuntimeError(f"staged {role} dependency hash mismatch")
        target.chmod(0o444)
        staged[role] = Path(names[role])
        records[role] = {
            "path": names[role], "bytes": target.stat().st_size,
            "sha256": source_digest,
        }
    if lower_rebind is not None:
        provided = work / "inputs/kjesterk.provided-095d.ufiw"
        shutil.copy2(sources["lower_overlay"], provided)
        provided.chmod(0o444)
        executable = work / staged["lower_overlay"]
        executable.chmod(0o644)
        with executable.open("r+b") as stream:
            stream.seek(96)
            stream.write(current_lower_model.encode("ascii"))
        executable.chmod(0o444)
        rebound_source, rebound_model = overlay_binding(executable)
        if (rebound_source != lower_source or rebound_model != current_lower_model or
                sha256_path(executable) != lower_rebind["rebound_full_sha256"] or
                sha256_payload(executable) != lower_rebind["payload_sha256"]):
            raise RuntimeError("audited lower overlay staged-rebind residual")
        records["lower_overlay"]["provided_path"] = (
            "inputs/kjesterk.provided-095d.ufiw")
        records["lower_overlay"]["provided_sha256"] = lower_rebind[
            "provided_full_sha256"]
        records["lower_overlay"]["sha256"] = sha256_path(executable)
        records["lower_overlay"]["model_sha256"] = current_lower_model
    bindings: dict[str, object] = {
        "source_sha256": source_sha,
        "original_model_sha256": original_model,
        "current_original_model_sha256": current_original_model,
        "expected_overlay_sha256": sha256_path(sources["expected_overlay"]),
        "expected_overlay_payload_sha256": sha256_payload(
            sources["expected_overlay"]),
        "lower_source_sha256": lower_source,
        "lower_model_sha256": current_lower_model,
        "current_lower_model_sha256": current_lower_model,
        "lower_overlay_sha256": sha256_path(work / staged["lower_overlay"]),
        "lower_provided_overlay_sha256": sha256_path(sources["lower_overlay"]),
        "lower_overlay_payload_sha256": sha256_payload(
            sources["lower_overlay"]),
        "lower_header_rebind": lower_rebind,
        "files": records,
    }
    write_json(work / "inputs/manifest.json", {
        "schema": "ultimate-double-jester-capture-inputs-v1",
        "semantics": SEMANTICS,
        "domain": DOMAIN,
        **bindings,
    })
    return staged, bindings


def build_binary(work: Path, source_bundle: Path) -> tuple[Path, list[str]]:
    binary = work / "binary/ultimate_double_jester_information_capture"
    binary.parent.mkdir(parents=True, exist_ok=True)
    command = [
        *PINNED_BUILD, "-Ibundle/src/ultimate",
        *(str(Path("bundle") / source) for source in BUILD_SOURCES),
        "-o", str(Path("binary") / binary.name),
    ]
    if source_bundle != work / "bundle":
        raise RuntimeError("capture source bundle is outside immutable work tree")
    subprocess.run(command, cwd=work, check=True)
    binary.chmod(0o555)
    return binary, command


def run_logged(command: list[str], log: Path, *, cwd: Path) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("xb") as output:
        process = subprocess.run(command, cwd=cwd, stdout=output,
                                 stderr=subprocess.STDOUT)
    if process.returncode:
        raise RuntimeError(f"command failed ({process.returncode}); inspect {log}")


def parse_measurement(log: Path) -> dict[str, int]:
    line = next((line for line in log.read_text(errors="replace").splitlines()
                 if line.startswith("capture_estimate ")), None)
    if line is None:
        raise RuntimeError("capture estimate certificate is missing")
    fields = {name: int(value) for name, value in re.findall(
        r"([a-z_]+) ([0-9]+)", line)}
    required = (
        "variables", "equation_bytes", "token_bytes_upper",
        "offset_cursor_bytes", "reverse_bytes_upper", "solution_bytes",
        "portable_bytes_upper", "total_bytes_upper", "recommended_free_bytes",
    )
    if any(name not in fields for name in required):
        raise RuntimeError("capture estimate certificate is malformed")
    fields["raw_bytes_upper"] = (
        fields["total_bytes_upper"] - fields["portable_bytes_upper"])
    fields["resident_bytes_upper"] = CONSERVATIVE_RESIDENT_BYTES
    fields["solve_launched"] = 0
    return fields


def relative_capture_command(staged: Mapping[str, Path], model: str,
                             bindings: Mapping[str, object],
                             args: argparse.Namespace) -> list[str]:
    return [
        "binary/ultimate_double_jester_information_capture",
        "--input", str(staged["input"]),
        "--lower-information-overlay", str(staged["lower_overlay"]),
        "--lower-concrete", str(staged["lower_concrete"]),
        "--lower-information-source-sha256",
        str(bindings["lower_source_sha256"]),
        "--lower-information-model-sha256",
        str(bindings["lower_model_sha256"]),
        "--output", "results/kjesterjesterk.capture.ufiw",
        "--sidecar", "results/kjesterjesterk.uficapture",
        "--raw-directory", "raw-live",
        "--expected-fresh-overlay", str(staged["expected_overlay"]),
        "--information-source-sha256", str(bindings["source_sha256"]),
        "--information-model-sha256", model,
        "--required-free-bytes", str(args.required_free_bytes),
        "--maximum-raw-bytes", str(args.scratch_limit),
    ]


def require_resource_gate(measurement: Mapping[str, int], work: Path,
                          args: argparse.Namespace) -> None:
    if args.scratch_limit <= 0 or args.resident_limit <= 0:
        raise RuntimeError("--full requires positive --scratch-limit and "
                           "--resident-limit")
    if measurement["raw_bytes_upper"] > args.scratch_limit:
        raise RuntimeError("estimated raw arena exceeds explicit scratch limit")
    if measurement["resident_bytes_upper"] > args.resident_limit:
        raise RuntimeError("resident estimate exceeds explicit resident limit")
    if args.required_free_bytes < measurement["recommended_free_bytes"]:
        raise RuntimeError("required-free gate is below the solver recommendation")
    # Raw arena, compressed archives, local restores, S3 downloads/restores,
    # and portable output must coexist because this runner never deletes.
    raw_copies = 6 if args.s3_prefix else 3
    durable_upper = (raw_copies * measurement["raw_bytes_upper"] +
                     4 * measurement["portable_bytes_upper"])
    available = shutil.disk_usage(work).free
    if available < args.required_free_bytes or durable_upper > available * 9 // 10:
        raise RuntimeError("capture/archive/restore plan exceeds free-disk gate")


def canonical_info(name: str, size: int, mode: int = 0o644) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = size
    info.mode = mode
    info.mtime = info.uid = info.gid = 0
    info.uname = info.gname = ""
    return info


def file_inventory(files: Mapping[str, Path], schema: str) -> bytes:
    lines = [schema]
    for name in sorted(files):
        path = files[name]
        lines.append(f"{sha256_path(path)} {path.stat().st_size} {name}")
    return ("\n".join(lines) + "\n").encode("ascii")


def write_zstd_archive(output: Path, files: Mapping[str, Path],
                       schema: str) -> str:
    if shutil.which("zstd") is None:
        raise RuntimeError("zstd is required for preservation archives")
    inventory = file_inventory(files, schema)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("xb") as destination:
        process = subprocess.Popen(
            ["zstd", "-19", "-T1", "--no-progress", "-c"],
            stdin=subprocess.PIPE, stdout=destination,
        )
        assert process.stdin is not None
        try:
            with tarfile.open(fileobj=process.stdin, mode="w|",
                              format=tarfile.PAX_FORMAT) as archive:
                for name in sorted([*files, "ARCHIVE.MANIFEST"]):
                    if name == "ARCHIVE.MANIFEST":
                        archive.addfile(canonical_info(name, len(inventory)),
                                        io.BytesIO(inventory))
                    else:
                        path = files[name]
                        mode = 0o755 if name.startswith("binary/") else 0o644
                        with path.open("rb") as stream:
                            archive.addfile(canonical_info(
                                name, path.stat().st_size, mode), stream)
        finally:
            process.stdin.close()
        if process.wait() != 0:
            raise RuntimeError("zstd preservation archive failed")
    return sha256_path(output)


def parse_inventory(payload: bytes, schema: str) -> dict[str, tuple[str, int]]:
    lines = payload.decode("ascii").splitlines()
    if not lines or lines[0] != schema:
        raise RuntimeError("preservation archive inventory schema mismatch")
    result: dict[str, tuple[str, int]] = {}
    for line in lines[1:]:
        digest, extent, name = line.split(" ", 2)
        if name in result or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise RuntimeError("malformed preservation archive inventory")
        result[name] = (digest, int(extent))
    return result


def restore_zstd_archive(archive_path: Path, destination: Path,
                         schema: str) -> dict[str, Path]:
    if destination.exists():
        raise RuntimeError(f"restore destination already exists: {destination}")
    destination.mkdir(parents=True)
    process = subprocess.Popen(["zstd", "-d", "-c", str(archive_path)],
                               stdout=subprocess.PIPE)
    assert process.stdout is not None
    inventory_payload: bytes | None = None
    restored: dict[str, Path] = {}
    with tarfile.open(fileobj=process.stdout, mode="r|") as archive:
        previous = ""
        for member in archive:
            name = member.name
            if previous and name <= previous:
                raise RuntimeError("archive index is not sorted and unique")
            previous = name
            pure = PurePosixPath(name)
            if (not member.isfile() or pure.is_absolute() or ".." in pure.parts or
                    member.mtime or member.uid or member.gid or
                    member.mode not in (0o644, 0o755) or
                    (member.mode == 0o755) != name.startswith("binary/")):
                raise RuntimeError("unsafe or noncanonical archive member")
            stream = archive.extractfile(member)
            if stream is None:
                raise RuntimeError("unreadable archive member")
            if name == "ARCHIVE.MANIFEST":
                inventory_payload = stream.read()
                continue
            target = destination.joinpath(*pure.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as output:
                shutil.copyfileobj(stream, output, 1 << 20)
            target.chmod(member.mode)
            restored[name] = target
    process.stdout.close()
    if process.wait() != 0:
        raise RuntimeError("zstd preservation restore failed")
    if inventory_payload is None:
        raise RuntimeError("archive lacks its authenticated inventory")
    inventory = parse_inventory(inventory_payload, schema)
    if set(inventory) != set(restored):
        raise RuntimeError("archive membership residual")
    for name, path in restored.items():
        digest, extent = inventory[name]
        if path.stat().st_size != extent or sha256_path(path) != digest:
            raise RuntimeError("restored archive hash/extent residual")
    return restored


def content_address_archive(directory: Path, stem: str,
                            files: Mapping[str, Path], schema: str) -> tuple[Path, str]:
    temporary = directory / f".{stem}.tar.zst.incomplete"
    digest = write_zstd_archive(temporary, files, schema)
    final = directory / f"{stem}-{digest}.tar.zst"
    if final.exists():
        raise RuntimeError(f"content-addressed archive already exists: {final}")
    temporary.rename(final)
    return final, digest


def s3_object(prefix: str, key: str) -> tuple[str, str, str]:
    if not prefix.startswith("s3://"):
        raise RuntimeError("S3 prefix must start with s3://")
    remainder = prefix[5:].strip("/")
    if not remainder or "/" not in remainder:
        bucket, base = remainder, ""
    else:
        bucket, base = remainder.split("/", 1)
    if not bucket:
        raise RuntimeError("S3 prefix has no bucket")
    full_key = "/".join(part for part in (base.rstrip("/"), key.lstrip("/"))
                        if part)
    return bucket, full_key, f"s3://{bucket}/{full_key}"


def upload_head_download_verify(*, source: Path, digest: str, extent: int,
                                prefix: str, key: str, download: Path,
                                archive_schema: str | None) -> dict[str, object]:
    bucket, full_key, uri = s3_object(prefix, key)
    subprocess.run(["aws", "s3", "cp", str(source), uri,
                    "--metadata", f"sha256={digest}",
                    "--only-show-errors"], check=True)
    head = json.loads(subprocess.check_output(
        ["aws", "s3api", "head-object", "--bucket", bucket,
         "--key", full_key, "--output", "json"], text=True))
    metadata = {str(name).lower(): str(value)
                for name, value in head.get("Metadata", {}).items()}
    if int(head.get("ContentLength", -1)) != extent or metadata.get(
            "sha256") != digest:
        raise RuntimeError("S3 HEAD extent/full-SHA metadata residual")
    if download.exists():
        raise RuntimeError(f"S3 verification download exists: {download}")
    download.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["aws", "s3", "cp", uri, str(download),
                    "--only-show-errors"], check=True)
    if download.stat().st_size != extent or sha256_path(download) != digest:
        raise RuntimeError("fresh S3 download extent/full-SHA residual")
    if archive_schema is not None:
        restore_zstd_archive(download, download.with_suffix(".restored"),
                             archive_schema)
    return {
        "bucket": bucket, "key": full_key, "uri": uri, "bytes": extent,
        "sha256": digest, "etag": head.get("ETag", ""),
        "version_id": head.get("VersionId"), "head_full_sha_residual": 0,
        "download_full_sha_residual": 0, "archive_restore_residual": 0,
    }


def verify_raw_manifest(work: Path, model: str,
                        bindings: Mapping[str, object]) -> dict[str, object]:
    path = work / "raw-live/manifest.json"
    manifest = json.loads(path.read_text())
    generated = work / "results/kjesterjesterk.capture.ufiw"
    sidecar = work / "results/kjesterjesterk.uficapture"
    expected = {
        "schema": "ultimate-double-jester-raw-v2",
        "semantics": SEMANTICS,
        "domain": DOMAIN,
        "source_sha256": bindings["source_sha256"],
        "original_model_sha256": bindings["original_model_sha256"],
        "model_sha256": model,
        "lower_source_sha256": bindings["lower_source_sha256"],
        "lower_model_sha256": bindings["lower_model_sha256"],
        "lower_payload_sha256": bindings["lower_overlay_payload_sha256"],
        "expected_overlay_sha256": bindings["expected_overlay_sha256"],
        "generated_overlay_sha256": sha256_path(generated),
        "sidecar_sha256": sha256_path(sidecar),
    }
    for key, value in expected.items():
        if manifest.get(key) != value:
            raise RuntimeError(f"raw manifest binding residual: {key}")
    total = 0
    for record in manifest.get("files", []):
        raw = work / "raw-live" / str(record["path"])
        if (not raw.is_file() or raw.stat().st_size != int(record["bytes"]) or
                sha256_path(raw) != record["sha256"]):
            raise RuntimeError("raw manifest file hash/extent residual")
        total += raw.stat().st_size
    if total != int(manifest.get("total_bytes", -1)):
        raise RuntimeError("raw manifest total-byte residual")
    if sha256_payload(generated) != bindings["expected_overlay_payload_sha256"]:
        raise RuntimeError("generated/ordinary fresh-overlay payload residual")
    if (int(manifest.get("variables", 0)) <= 0 or
            int(manifest.get("reverse_edges", 0)) <= 0):
        raise RuntimeError("raw manifest fixed-point cardinality residual")
    capture_log = (work / "logs/capture.log").read_text(errors="replace")
    certificate = ("capture_complete exhaustive 1 belief_cap none "
                   "domain_bellman_residual 0 dual_win_residual 0 "
                   "uniform_action_residual 0 singleton_residual 0 "
                   "fresh_overlay_residual 0")
    if certificate not in capture_log:
        raise RuntimeError("capture proof residual certificate is missing")
    return manifest


def archive_file_maps(work: Path) -> tuple[dict[str, Path], dict[str, Path]]:
    common = {
        "binary/ultimate_double_jester_information_capture":
            work / "binary/ultimate_double_jester_information_capture",
        "proof/capture-plan.json": work / "capture-plan.json",
        "proof/input-manifest.json": work / "inputs/manifest.json",
        "proof/measurement.json": work / "measurement.json",
        "proof/build.log": work / "logs/build.log",
        "proof/self-test.log": work / "logs/self-test.log",
        "proof/capture.log": work / "logs/capture.log",
        "inputs/kjesterjesterk.uftb": work / "inputs/kjesterjesterk.uftb",
        "inputs/kjesterk.uftb": work / "inputs/kjesterk.uftb",
        "inputs/kjesterk.ufiw": work / "inputs/kjesterk.ufiw",
        "inputs/kjesterjesterk.expected.ufiw":
            work / "inputs/kjesterjesterk.expected.ufiw",
    }
    provided_lower = work / "inputs/kjesterk.provided-095d.ufiw"
    if provided_lower.exists():
        common["inputs/kjesterk.provided-095d.ufiw"] = provided_lower
    for relative in CAPTURE_SOURCES:
        common[f"sources/{relative}"] = work / "bundle" / relative
    common["sources/tools/run_double_jester_information_capture.py"] = (
        work / "bundle/tools/run_double_jester_information_capture.py")
    raw = dict(common)
    raw["raw/manifest.json"] = work / "raw-live/manifest.json"
    raw_manifest = json.loads((work / "raw-live/manifest.json").read_text())
    for record in raw_manifest["files"]:
        raw[f"raw/{record['path']}"] = work / "raw-live" / record["path"]
    raw["results/kjesterjesterk.capture.ufiw"] = (
        work / "results/kjesterjesterk.capture.ufiw")
    raw["results/kjesterjesterk.uficapture"] = (
        work / "results/kjesterjesterk.uficapture")
    compact = dict(common)
    compact["proof/raw-manifest.json"] = work / "raw-live/manifest.json"
    compact["results/kjesterjesterk.capture.ufiw"] = (
        work / "results/kjesterjesterk.capture.ufiw")
    compact["results/kjesterjesterk.uficapture"] = (
        work / "results/kjesterjesterk.uficapture")
    return raw, compact


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path,
                        default=ROOT / "tablebases/kjesterjesterk.uftb")
    parser.add_argument("--lower-overlay", type=Path, required=True)
    parser.add_argument("--lower-concrete", type=Path,
                        default=ROOT / "tablebases/kjesterk.uftb")
    parser.add_argument("--expected-overlay", type=Path, required=True)
    parser.add_argument("--work-directory", type=Path, required=True)
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--scratch-limit", type=int, default=0)
    parser.add_argument("--resident-limit", type=int, default=0)
    parser.add_argument("--allow-audited-lower-rebind", action="store_true")
    parser.add_argument("--required-free-bytes", type=int,
                        default=DEFAULT_REQUIRED_FREE)
    parser.add_argument("--s3-prefix")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    work = args.work_directory.resolve()
    require_empty_work_directory(work)
    model = capture_model_sha256()
    bundle = stage_source_bundle(work, model)
    staged, bindings = stage_external_inputs(args, work)

    # Compiler and arguments are recorded before execution; stdout/stderr are
    # retained even for a failing AWS toolchain.
    binary, build_command = build_binary(work, bundle)
    write_json(work / "logs/build.log", {
        "schema": "ultimate-double-jester-build-v1",
        "command": build_command,
        "compiler": subprocess.check_output(
            [PINNED_BUILD[0], "--version"], text=True).splitlines()[0],
        "binary_sha256": sha256_path(binary),
    })
    run_logged(["binary/ultimate_double_jester_information_capture", "--self-test"],
               work / "logs/self-test.log", cwd=work)
    run_logged(["binary/ultimate_double_jester_information_capture",
                "--estimate-only"], work / "logs/measurement.log", cwd=work)
    measurement = parse_measurement(work / "logs/measurement.log")
    write_json(work / "measurement.json", measurement)

    command = relative_capture_command(staged, model, bindings, args)
    plan = {
        "schema": RUN_SCHEMA, "semantics": SEMANTICS, "domain": DOMAIN,
        "status": "measurement-complete-full-not-launched",
        "capture_model_sha256": model,
        "binary_sha256": sha256_path(binary),
        "build": build_command, "command": command,
        "bindings": bindings, "measurement": measurement,
        "full_explicitly_requested": bool(args.full),
        "never_delete": True,
    }
    write_json(work / "capture-plan.json", plan)
    if not args.full:
        print(json.dumps({"status": plan["status"], **measurement}, sort_keys=True))
        return 0

    require_resource_gate(measurement, work, args)
    (work / "raw-live").mkdir()
    (work / "results").mkdir()
    run_logged(command, work / "logs/capture.log", cwd=work)
    if capture_model_sha256(bundle) != model:
        raise RuntimeError("staged capture sources changed during solve")
    if sha256_path(binary) != plan["binary_sha256"]:
        raise RuntimeError("capture binary changed during solve")
    raw_manifest = verify_raw_manifest(work, model, bindings)

    plan["status"] = "full-capture-complete-local-verification-pending"
    plan["raw_manifest_sha256"] = sha256_path(work / "raw-live/manifest.json")
    plan["generated_overlay_sha256"] = sha256_path(
        work / "results/kjesterjesterk.capture.ufiw")
    plan["sidecar_sha256"] = sha256_path(
        work / "results/kjesterjesterk.uficapture")
    plan["raw_bytes"] = raw_manifest["total_bytes"]
    write_json(work / "capture-plan.json", plan)

    raw_files, compact_files = archive_file_maps(work)
    raw_archive, raw_sha = content_address_archive(
        work / "archives", "double-jester-raw", raw_files, RAW_ARCHIVE_SCHEMA)
    compact_archive, compact_sha = content_address_archive(
        work / "archives", "double-jester-arbitrary", compact_files,
        COMPACT_ARCHIVE_SCHEMA)
    restore_zstd_archive(raw_archive, work / "restore-local/raw",
                         RAW_ARCHIVE_SCHEMA)
    restore_zstd_archive(compact_archive, work / "restore-local/compact",
                         COMPACT_ARCHIVE_SCHEMA)

    objects = [
        {"name": raw_archive.name, "bytes": raw_archive.stat().st_size,
         "sha256": raw_sha,
         "key": f"results/sha256/{raw_sha}/{raw_archive.name}",
         "schema": RAW_ARCHIVE_SCHEMA},
        {"name": compact_archive.name, "bytes": compact_archive.stat().st_size,
         "sha256": compact_sha,
         "key": f"results/sha256/{compact_sha}/{compact_archive.name}",
         "schema": COMPACT_ARCHIVE_SCHEMA},
    ]
    upload_manifest = {
        "schema": S3_MANIFEST_SCHEMA, "semantics": SEMANTICS,
        "domain": DOMAIN, "bindings": bindings,
        "capture_model_sha256": model, "objects": objects,
        "local_archive_restore_residual": 0, "local_raw_retained": True,
        "safe_to_delete_gate": False,
    }
    upload_manifest_path = work / "archives/s3-upload-manifest.json"
    write_json(upload_manifest_path, upload_manifest)
    s3_status = "not-requested-no-preservation-claim"
    if args.s3_prefix:
        local = {raw_archive.name: raw_archive,
                 compact_archive.name: compact_archive}
        verified = [upload_head_download_verify(
            source=local[str(record["name"])],
            digest=str(record["sha256"]), extent=int(record["bytes"]),
            prefix=args.s3_prefix, key=str(record["key"]),
            download=work / "s3-verify" / str(record["name"]),
            archive_schema=str(record["schema"])) for record in objects]
        manifest_sha = sha256_path(upload_manifest_path)
        manifest_verified = upload_head_download_verify(
            source=upload_manifest_path, digest=manifest_sha,
            extent=upload_manifest_path.stat().st_size, prefix=args.s3_prefix,
            key=f"manifests/sha256/{manifest_sha}/{upload_manifest_path.name}",
            download=work / "s3-verify" / upload_manifest_path.name,
            archive_schema=None)
        certificate = {
            "schema": S3_CERTIFICATE_SCHEMA,
            "status": "head-download-full-sha-archive-restore-verified",
            "objects": verified, "content_addressed_manifest": manifest_verified,
            "bindings": bindings, "capture_model_sha256": model,
            "local_raw_scratch_retained": True, "safe_to_delete_gate": False,
        }
        certificate_path = work / "archives/s3-preservation-certificate.json"
        write_json(certificate_path, certificate)
        certificate_sha = sha256_path(certificate_path)
        certificate_verified = upload_head_download_verify(
            source=certificate_path, digest=certificate_sha,
            extent=certificate_path.stat().st_size, prefix=args.s3_prefix,
            key=(f"certificates/sha256/{certificate_sha}/"
                 f"{certificate_path.name}"),
            download=work / "s3-verify" / certificate_path.name,
            archive_schema=None)
        write_json(work / "archives/s3-preservation-receipt.json", {
            "schema": "ultimate-double-jester-s3-receipt-v1",
            "status": "certificate-head-download-full-sha-verified",
            "certificate": certificate_verified,
            "local_raw_scratch_retained": True, "safe_to_delete_gate": False,
        })
        s3_status = "head-download-full-sha-archive-restore-verified-no-delete"

    print(json.dumps({
        "status": "full-capture-archived-and-locally-restored",
        "raw_archive_sha256": raw_sha,
        "compact_archive_sha256": compact_sha,
        "sidecar_sha256": plan["sidecar_sha256"],
        "s3_preservation": s3_status,
        "local_raw_retained": True,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
