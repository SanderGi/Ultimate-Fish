#!/usr/bin/env python3
"""Measure or run the isolated exact joint-Jester preservation recomputation.

The default is intentionally measurement-only.  ``--full`` is required before
the fixed point is allocated.  A full run first archives the authenticated raw
capture, restores it into a fresh directory, and only then exports the compact
arbitrary-history sidecar through the independent Bellman/D2 verifier.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tarfile


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import package_ultimate_joint_jester_preservation_aws as package  # noqa: E402


RAW_ARCHIVE_SCHEMA = "ultimate-joint-jester-raw-recovery-v1"
RESULT_SCHEMA = "ultimate-joint-jester-arbitrary-result-v1"
ZERO_SHA = "0" * 64


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def overlay_binding(path: Path) -> tuple[str, str]:
    with path.open("rb") as stream:
        header = stream.read(160)
    if len(header) != 160 or header[:8] != b"UFIW2\0\0\0":
        raise RuntimeError(f"invalid UFIW2 overlay: {path}")
    try:
        source = header[32:96].decode("ascii")
        model = header[96:160].decode("ascii")
    except UnicodeDecodeError as error:
        raise RuntimeError(f"non-ASCII UFIW2 binding: {path}") from error
    if not re.fullmatch(r"[0-9a-f]{64}", source) or not re.fullmatch(
            r"[0-9a-f]{64}", model):
        raise RuntimeError(f"invalid UFIW2 SHA binding: {path}")
    return source, model


def canonical_info(name: str, size: int, mode: int = 0o644) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = size
    info.mode = mode
    info.mtime = info.uid = info.gid = 0
    info.uname = info.gname = ""
    return info


def file_inventory(files: dict[str, Path], schema: str) -> bytes:
    lines = [schema]
    for name in sorted(files):
        path = files[name]
        lines.append(f"{sha256_path(path)} {path.stat().st_size} {name}")
    return ("\n".join(lines) + "\n").encode("ascii")


def write_zstd_archive(output: Path, files: dict[str, Path], schema: str) -> str:
    if shutil.which("zstd") is None:
        raise RuntimeError("zstd is required for compact preservation archives")
    inventory = file_inventory(files, schema)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as destination:
        process = subprocess.Popen(
            ["zstd", "-19", "-T1", "--no-progress", "-c"],
            stdin=subprocess.PIPE, stdout=destination,
        )
        assert process.stdin is not None
        try:
            with tarfile.open(fileobj=process.stdin, mode="w|",
                              format=tarfile.PAX_FORMAT) as archive:
                names = sorted([*files, "ARCHIVE.MANIFEST"])
                for name in names:
                    if name == "ARCHIVE.MANIFEST":
                        archive.addfile(canonical_info(name, len(inventory)),
                                        io.BytesIO(inventory))
                    else:
                        path = files[name]
                        mode = 0o755 if name.startswith("binary/") else 0o644
                        with path.open("rb") as stream:
                            archive.addfile(canonical_info(name, path.stat().st_size,
                                                           mode),
                                            stream)
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
    if destination.exists() or any(destination.parent.glob(destination.name)):
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
            if (previous and name <= previous):
                raise RuntimeError("preservation archive index is not sorted/unique")
            previous = name
            pure = PurePosixPath(name)
            if (not member.isfile() or pure.is_absolute() or ".." in pure.parts or
                    member.mtime or member.uid or member.gid or
                    member.mode not in (0o644, 0o755) or
                    (member.mode == 0o755) != name.startswith("binary/")):
                raise RuntimeError("unsafe/noncanonical preservation archive member")
            stream = archive.extractfile(member)
            if stream is None:
                raise RuntimeError("preservation archive member is unreadable")
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
        raise RuntimeError("preservation archive lacks its inventory")
    inventory = parse_inventory(inventory_payload, schema)
    if set(inventory) != set(restored):
        raise RuntimeError("preservation archive membership residual")
    for name, path in restored.items():
        digest, extent = inventory[name]
        if path.stat().st_size != extent or sha256_path(path) != digest:
            raise RuntimeError("preservation archive restored hash/extent residual")
    return restored


def run_logged(command: list[str], log: Path, cwd: Path = ROOT) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("xb") as output:
        process = subprocess.run(command, cwd=cwd, stdout=output,
                                 stderr=subprocess.STDOUT)
    if process.returncode:
        raise RuntimeError(f"command failed ({process.returncode}); inspect {log}")


def parse_measurement(log: Path) -> dict[str, int]:
    line = next((line for line in log.read_text(errors="replace").splitlines()
                 if line.startswith("joint_capture_measurement ")), None)
    if line is None:
        raise RuntimeError("joint preservation measurement certificate is missing")
    fields = dict(re.findall(r"([a-z_]+) ([0-9]+)", line))
    required = ("nodes", "white_variables", "black_variables", "white_tokens",
                "black_tokens", "scratch_bytes", "resident_bytes", "raw_bytes",
                "sidecar_upper_bytes", "solve_launched")
    if any(field not in fields for field in required) or fields["solve_launched"] != "0":
        raise RuntimeError("joint preservation measurement certificate is malformed")
    return {field: int(fields[field]) for field in required}


def bindings(manifest: dict[str, object], lower_overlay: Path,
             proof_sha: str) -> tuple[list[str], str]:
    lower_source, lower_model = overlay_binding(lower_overlay)
    if lower_source != manifest["lower_table_sha256"] or lower_model != manifest["lower_model_sha256"]:
        raise RuntimeError("lower K+Jester overlay binding is stale")
    lower_overlay_sha = sha256_path(lower_overlay)
    return ([
        "--source-sha256", str(manifest["source_sha256"]),
        "--original-model-sha256", str(manifest["original_model_sha256"]),
        "--capture-model-sha256", str(manifest["capture_model_sha256"]),
        "--observation-sha256", str(manifest["observation_sha256"]),
        "--lower-table-sha256", str(manifest["lower_table_sha256"]),
        "--lower-overlay-sha256", lower_overlay_sha,
        "--lower-model-sha256", str(manifest["lower_model_sha256"]),
        "--proof-log-sha256", proof_sha,
    ], lower_overlay_sha)


def common_inputs(binary: Path, work: Path, lower_overlay: Path,
                  binding_args: list[str], *, input_table: Path | None = None,
                  lower_table: Path | None = None) -> list[str]:
    input_table = input_table or ROOT / "tablebases/kjesterkjester.uftb"
    lower_table = lower_table or ROOT / "tablebases/kjesterk.uftb"
    return [str(binary), "--input", str(input_table),
            "--lower-table", str(lower_table),
            "--lower-overlay", str(lower_overlay), "--scratch", str(work / "scratch"),
            *binding_args]


def require_resource_gate(measurement: dict[str, int], work: Path,
                          scratch_limit: int, resident_limit: int,
                          s3_restore_drill: bool) -> None:
    if measurement["scratch_bytes"] + measurement["raw_bytes"] > scratch_limit:
        raise RuntimeError("measured scratch+raw extent exceeds explicit limit")
    if measurement["resident_bytes"] > resident_limit:
        raise RuntimeError("measured resident estimate exceeds explicit limit")
    # Raw source, compact archive, restored copy, and final artifacts coexist.
    durable_copies = 6 if s3_restore_drill else 3
    durable = (durable_copies * measurement["raw_bytes"] +
               3 * measurement["sidecar_upper_bytes"])
    available = shutil.disk_usage(work).free
    if durable > available * 9 // 10:
        raise RuntimeError("measured recovery/archive plan exceeds free disk gate")


def write_json(path: Path, payload: object) -> None:
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


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
    if int(head.get("ContentLength", -1)) != extent or metadata.get("sha256") != digest:
        raise RuntimeError("S3 HEAD extent/full-SHA metadata residual")
    if download.exists():
        raise RuntimeError(f"S3 verification download already exists: {download}")
    download.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["aws", "s3", "cp", uri, str(download),
                    "--only-show-errors"], check=True)
    if download.stat().st_size != extent or sha256_path(download) != digest:
        raise RuntimeError("fresh S3 download extent/full-SHA residual")
    if archive_schema is not None:
        restore_zstd_archive(download, download.with_suffix(".restored"),
                             archive_schema)
    return {"bucket": bucket, "key": full_key, "uri": uri,
            "bytes": extent, "sha256": digest,
            "etag": head.get("ETag", ""), "version_id": head.get("VersionId"),
            "head_full_sha_residual": 0, "download_full_sha_residual": 0,
            "archive_restore_residual": 0}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--lower-overlay", type=Path, required=True)
    parser.add_argument("--expected-overlay", type=Path, required=True)
    parser.add_argument("--bundle-manifest", type=Path,
                        default=ROOT / "bundle-manifest.json")
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--scratch-limit", type=int, default=0)
    parser.add_argument("--resident-limit", type=int, default=0)
    parser.add_argument("--s3-prefix")
    args = parser.parse_args(sys.argv[1:] if argv is None else argv)

    work = args.work.resolve()
    if work.exists() and any(work.iterdir()):
        raise RuntimeError(f"joint preservation work directory is not empty: {work}")
    work.mkdir(parents=True, exist_ok=True)
    (work / "scratch").mkdir()
    manifest = json.loads(args.bundle_manifest.read_text())
    if manifest.get("schema") != package.SCHEMA:
        raise RuntimeError("joint preservation bundle manifest schema mismatch")
    if package.build_manifest(ROOT)["capture_model_sha256"] != manifest.get(
            "capture_model_sha256"):
        raise RuntimeError("joint preservation extracted source/model residual")

    lower_overlay = args.lower_overlay.resolve()
    expected_overlay = args.expected_overlay.resolve()
    binding_args, lower_overlay_sha = bindings(manifest, lower_overlay, ZERO_SHA)
    expected_source, expected_model = overlay_binding(expected_overlay)
    if (expected_source != manifest["source_sha256"] or
            expected_model != manifest["original_model_sha256"]):
        raise RuntimeError("completed ordinary joint-Jester overlay is stale")

    binary = work / "ultimate_joint_jester_preserver"
    build = list(manifest["build"])
    build[-1] = str(binary)
    subprocess.run(build, cwd=ROOT, check=True)
    run_logged([str(binary), "--self-test", "--scratch", str(work / "scratch")],
               work / "logs/self-test.log")

    measure = common_inputs(binary, work, lower_overlay, binding_args)
    measure.insert(1, "--measure")
    run_logged(measure, work / "logs/measurement.log")
    measurement = parse_measurement(work / "logs/measurement.log")
    write_json(work / "measurement.json", measurement)
    if not args.full:
        print(json.dumps({"status": "measurement-complete-full-not-launched",
                          **measurement}, sort_keys=True))
        return 0
    if args.scratch_limit <= 0 or args.resident_limit <= 0:
        raise RuntimeError("--full requires positive scratch/resident limits")
    require_resource_gate(measurement, work, args.scratch_limit,
                          args.resident_limit, bool(args.s3_prefix))

    raw_prefix_argument = Path("raw-live/joint")
    raw_prefix = work / raw_prefix_argument
    raw_prefix.parent.mkdir()
    dense_overlay_argument = Path("raw-live/kjesterkjester.capture.ufiw")
    dense_overlay = work / dense_overlay_argument
    capture = common_inputs(binary, work, lower_overlay, binding_args)
    capture[1:1] = ["--capture", "--raw-prefix", str(raw_prefix_argument),
                    "--output", str(dense_overlay_argument),
                    "--scratch-limit", str(args.scratch_limit),
                    "--resident-limit", str(args.resident_limit)]
    capture_log = work / "raw-live/capture.log"
    run_logged(capture, capture_log, cwd=work)
    if sha256_path(dense_overlay) != sha256_path(expected_overlay):
        raise RuntimeError("capture dense overlay differs from completed ordinary proof")
    proof_sha = sha256_path(capture_log)
    expected_overlay_sha = sha256_path(expected_overlay)
    bind_log = work / "raw-live/bind.log"
    run_logged([str(binary), "--bind-proof-log", "--raw-prefix",
                str(raw_prefix_argument), "--proof-log", "raw-live/capture.log"],
               bind_log, cwd=work)

    plan = {
        "schema": RAW_ARCHIVE_SCHEMA,
        "bindings": {
            "source_sha256": manifest["source_sha256"],
            "original_model_sha256": manifest["original_model_sha256"],
            "capture_model_sha256": manifest["capture_model_sha256"],
            "observation_sha256": manifest["observation_sha256"],
            "lower_table_sha256": manifest["lower_table_sha256"],
            "lower_overlay_sha256": lower_overlay_sha,
            "lower_model_sha256": manifest["lower_model_sha256"],
            "proof_log_sha256": proof_sha,
            "expected_dense_overlay_sha256": expected_overlay_sha,
        },
        "measurement": measurement,
    }
    plan_path = work / "raw-live/run-plan.json"
    write_json(plan_path, plan)
    raw_files = {
        "raw/joint.meta": raw_prefix.with_suffix(".meta"),
        "raw/joint.nodes": raw_prefix.with_suffix(".nodes"),
        "raw/joint.values": raw_prefix.with_suffix(".values"),
        "raw/joint.ranks": raw_prefix.with_suffix(".ranks"),
        "raw/joint.witnesses": raw_prefix.with_suffix(".witnesses"),
        "proof/capture.log": capture_log,
        "proof/bind.log": bind_log,
        "proof/run-plan.json": plan_path,
        "proof/kjesterkjester.capture.ufiw": dense_overlay,
        "tablebases/kjesterk.ufiw": lower_overlay,
        "tablebases/kjesterk.uftb": ROOT / "tablebases/kjesterk.uftb",
        "tablebases/kjesterkjester.uftb": ROOT / "tablebases/kjesterkjester.uftb",
        "binary/ultimate_joint_jester_preserver": binary,
        "bundle/bundle-manifest.json": args.bundle_manifest.resolve(),
    }
    for relative in (*package.MODEL_SOURCES,
                     "tools/package_ultimate_joint_jester_preservation_aws.py",
                     "tools/run_ultimate_joint_jester_preservation_aws.py"):
        raw_files[f"sources/{relative}"] = ROOT / relative
    raw_archive = work / "archives/joint-jester-raw.tar.zst"
    raw_archive_sha = write_zstd_archive(raw_archive, raw_files, RAW_ARCHIVE_SCHEMA)
    restored = restore_zstd_archive(raw_archive, work / "raw-restored",
                                    RAW_ARCHIVE_SCHEMA)

    restored_lower_overlay = restored["tablebases/kjesterk.ufiw"]
    strict_bindings, _ = bindings(manifest, restored_lower_overlay, proof_sha)
    restored_prefix = restored["raw/joint.meta"].with_suffix("")
    sidecar = work / "results/kjesterkjester.ufja"
    sidecar.parent.mkdir()
    export = common_inputs(
        restored["binary/ultimate_joint_jester_preserver"], work,
        restored_lower_overlay, strict_bindings,
        input_table=restored["tablebases/kjesterkjester.uftb"],
        lower_table=restored["tablebases/kjesterk.uftb"],
    )
    export[1:1] = ["--export", "--raw-prefix", str(restored_prefix),
                   "--output-sidecar", str(sidecar)]
    export_log = work / "results/export-restore.log"
    run_logged(export, export_log)
    sidecar_sha = sha256_path(sidecar)

    result_manifest = {
        "schema": RESULT_SCHEMA,
        "sidecar": {"path": "tablebases/kjesterkjester.ufja",
                    "bytes": sidecar.stat().st_size, "sha256": sidecar_sha},
        "raw_recovery": {"bytes": raw_archive.stat().st_size,
                         "sha256": raw_archive_sha},
        "bindings": plan["bindings"],
        "measurement": measurement,
        "restore_log": {"bytes": export_log.stat().st_size,
                        "sha256": sha256_path(export_log)},
    }
    result_manifest_path = work / "results/result-manifest.json"
    write_json(result_manifest_path, result_manifest)
    result_archive = work / "archives/joint-jester-arbitrary.tar.zst"
    result_files = {
        "tablebases/kjesterkjester.ufja": sidecar,
        "proof/result-manifest.json": result_manifest_path,
        "proof/export-restore.log": export_log,
        "proof/kjesterkjester.capture.ufiw": dense_overlay,
        "tablebases/kjesterk.ufiw": lower_overlay,
        "tablebases/kjesterk.uftb": ROOT / "tablebases/kjesterk.uftb",
        "tablebases/kjesterkjester.uftb": ROOT / "tablebases/kjesterkjester.uftb",
        "binary/ultimate_joint_jester_preserver": binary,
        "bundle/bundle-manifest.json": args.bundle_manifest.resolve(),
    }
    for relative in (*package.MODEL_SOURCES,
                     "tools/package_ultimate_joint_jester_preservation_aws.py",
                     "tools/run_ultimate_joint_jester_preservation_aws.py"):
        result_files[f"sources/{relative}"] = ROOT / relative
    result_archive_sha = write_zstd_archive(result_archive, result_files,
                                            RESULT_SCHEMA)
    s3_manifest = {
        "schema": "ultimate-joint-jester-s3-upload-v1",
        "objects": [
            {"name": raw_archive.name, "bytes": raw_archive.stat().st_size,
             "sha256": raw_archive_sha,
             "key": f"results/sha256/{raw_archive_sha}/{raw_archive.name}"},
            {"name": result_archive.name, "bytes": result_archive.stat().st_size,
             "sha256": result_archive_sha,
             "key": f"results/sha256/{result_archive_sha}/{result_archive.name}"},
        ],
    }
    s3_manifest_path = work / "archives/s3-upload-manifest.json"
    write_json(s3_manifest_path, s3_manifest)
    s3_status = "not-requested-no-preservation-claim"
    if args.s3_prefix:
        local_archives = {raw_archive.name: (raw_archive, RAW_ARCHIVE_SCHEMA),
                          result_archive.name: (result_archive, RESULT_SCHEMA)}
        verified_objects = []
        for record in s3_manifest["objects"]:
            local, schema = local_archives[str(record["name"])]
            verified_objects.append(upload_head_download_verify(
                source=local, digest=str(record["sha256"]),
                extent=int(record["bytes"]), prefix=args.s3_prefix,
                key=str(record["key"]),
                download=work / "s3-verify" / str(record["name"]),
                archive_schema=schema))
        manifest_sha = sha256_path(s3_manifest_path)
        manifest_key = (f"manifests/sha256/{manifest_sha}/"
                        f"{s3_manifest_path.name}")
        manifest_verified = upload_head_download_verify(
            source=s3_manifest_path, digest=manifest_sha,
            extent=s3_manifest_path.stat().st_size, prefix=args.s3_prefix,
            key=manifest_key,
            download=work / "s3-verify" / s3_manifest_path.name,
            archive_schema=None)
        completion = {
            "schema": "ultimate-joint-jester-s3-preservation-certificate-v1",
            "status": "head-download-archive-restore-verified",
            "objects": verified_objects,
            "content_addressed_manifest": manifest_verified,
            "local_raw_scratch_retained": True,
            "safe_to_delete_gate": False,
        }
        write_json(work / "archives/s3-preservation-certificate.json", completion)
        s3_status = "head-download-archive-restore-verified-no-delete"
    print(json.dumps({"status": "full-capture-restored-exported",
                      "raw_archive_sha256": raw_archive_sha,
                      "result_archive_sha256": result_archive_sha,
                      "sidecar_sha256": sidecar_sha,
                      "s3_preservation": s3_status}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
