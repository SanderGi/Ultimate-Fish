#!/usr/bin/env python3
"""Measure or run the exact crossed Jester/Ghost solve on AWS.

Measurement is the default.  A full run requires explicit resource ceilings
and a private versioned S3 prefix.  The runner retains every local file and
claims preservation only after version-specific HEAD/download/hash/restore
verification of content-addressed raw and result archives.
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
import time


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
import package_ultimate_crossed_jester_ghost_aws as package  # noqa: E402


RAW_SCHEMA = "ultimate-crossed-jester-ghost-raw-v2"
RESULT_SCHEMA = "ultimate-crossed-jester-ghost-result-v2"
S3_SCHEMA = "ultimate-crossed-jester-ghost-s3-v2"
SHA256 = re.compile(r"[0-9a-f]{64}\Z")


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")


def verify_extracted_manifest(manifest: dict[str, object]) -> None:
    if manifest.get("schema") != package.SCHEMA or not manifest.get("source_commit"):
        raise RuntimeError("crossed bundle is not committed/source-bound")
    records = {record["path"]: record for record in manifest["files"]}
    if set(records) != set(package.BUNDLE_FILES):
        raise RuntimeError("crossed bundle file inventory residual")
    for relative, record in records.items():
        path = ROOT / relative
        if (not path.is_file() or path.stat().st_size != record["bytes"] or
                sha256_path(path) != record["sha256"]):
            raise RuntimeError(f"crossed bundle file residual: {relative}")
    if package.model_fingerprint(ROOT) != manifest.get("model_sha256"):
        raise RuntimeError("crossed extracted model fingerprint residual")


def overlay_binding(path: Path) -> tuple[str, str]:
    header = path.read_bytes()[:160]
    if len(header) != 160 or header[:8] != b"UFIW2\0\0\0":
        raise RuntimeError("lower K+Jester overlay header mismatch")
    source = header[32:96].decode("ascii")
    model = header[96:160].decode("ascii")
    if not SHA256.fullmatch(source) or not SHA256.fullmatch(model):
        raise RuntimeError("lower K+Jester overlay binding is malformed")
    return source, model


def run_logged(command: list[str], log: Path, *, cwd: Path,
               resident_limit: int = 0) -> int:
    log.parent.mkdir(parents=True, exist_ok=True)
    peak = 0
    with log.open("xb") as output:
        process = subprocess.Popen(command, cwd=cwd, stdout=output,
                                   stderr=subprocess.STDOUT)
        while process.poll() is None:
            try:
                status = Path(f"/proc/{process.pid}/status").read_text()
                match = re.search(r"^VmRSS:\s+([0-9]+) kB$", status, re.M)
                if match:
                    peak = max(peak, int(match.group(1)) * 1024)
                    if resident_limit and peak > resident_limit:
                        process.terminate()
                        try:
                            process.wait(timeout=30)
                        except subprocess.TimeoutExpired:
                            process.kill()
                        raise RuntimeError(
                            "crossed child exceeded the resident resource gate")
            except FileNotFoundError:
                pass
            time.sleep(0.1)
        if process.returncode:
            raise RuntimeError(
                f"crossed command failed ({process.returncode}); inspect {log}")
    return peak


def canonical_info(name: str, size: int, mode: int = 0o644) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = size
    info.mode = mode
    info.mtime = info.uid = info.gid = 0
    info.uname = info.gname = ""
    return info


def inventory(files: dict[str, Path], schema: str) -> bytes:
    lines = [schema]
    for name in sorted(files):
        path = files[name]
        lines.append(f"{sha256_path(path)} {path.stat().st_size} {name}")
    return ("\n".join(lines) + "\n").encode()


def write_archive(output: Path, files: dict[str, Path], schema: str) -> str:
    if shutil.which("zstd") is None:
        raise RuntimeError("zstd is required")
    manifest = inventory(files, schema)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("xb") as destination:
        process = subprocess.Popen(
            ["zstd", "-10", "-T1", "--no-progress", "-c"],
            stdin=subprocess.PIPE, stdout=destination)
        assert process.stdin is not None
        try:
            with tarfile.open(fileobj=process.stdin, mode="w|",
                              format=tarfile.PAX_FORMAT) as archive:
                for name in sorted([*files, "ARCHIVE.MANIFEST"]):
                    if name == "ARCHIVE.MANIFEST":
                        archive.addfile(canonical_info(name, len(manifest)),
                                        io.BytesIO(manifest))
                    else:
                        path = files[name]
                        mode = 0o755 if name.startswith("binary/") else 0o644
                        with path.open("rb") as stream:
                            archive.addfile(canonical_info(
                                name, path.stat().st_size, mode), stream)
        finally:
            process.stdin.close()
        if process.wait() != 0:
            raise RuntimeError("crossed zstd archive failed")
    return sha256_path(output)


def restore_archive(source: Path, destination: Path,
                    schema: str) -> dict[str, Path]:
    if destination.exists():
        raise RuntimeError("crossed archive restore destination exists")
    destination.mkdir(parents=True)
    process = subprocess.Popen(["zstd", "-d", "-c", str(source)],
                               stdout=subprocess.PIPE)
    assert process.stdout is not None
    expected: dict[str, tuple[str, int]] | None = None
    restored: dict[str, Path] = {}
    previous = ""
    with tarfile.open(fileobj=process.stdout, mode="r|") as archive:
        for member in archive:
            if previous and member.name <= previous:
                raise RuntimeError("crossed archive index is not sorted/unique")
            previous = member.name
            pure = PurePosixPath(member.name)
            if (not member.isfile() or pure.is_absolute() or ".." in pure.parts or
                    member.mtime or member.uid or member.gid or
                    member.mode not in (0o644, 0o755)):
                raise RuntimeError("unsafe crossed archive member")
            stream = archive.extractfile(member)
            if stream is None:
                raise RuntimeError("unreadable crossed archive member")
            if member.name == "ARCHIVE.MANIFEST":
                lines = stream.read().decode().splitlines()
                if not lines or lines[0] != schema:
                    raise RuntimeError("crossed archive schema mismatch")
                expected = {}
                for line in lines[1:]:
                    digest, extent, name = line.split(" ", 2)
                    if name in expected or not SHA256.fullmatch(digest):
                        raise RuntimeError("crossed archive inventory malformed")
                    expected[name] = (digest, int(extent))
                continue
            target = destination.joinpath(*pure.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as output:
                shutil.copyfileobj(stream, output, 1 << 20)
            target.chmod(member.mode)
            restored[member.name] = target
    process.stdout.close()
    if process.wait() != 0 or expected is None or set(expected) != set(restored):
        raise RuntimeError("crossed archive membership/restore residual")
    for name, path in restored.items():
        digest, extent = expected[name]
        if path.stat().st_size != extent or sha256_path(path) != digest:
            raise RuntimeError("crossed archive restored hash residual")
    return restored


def s3_parts(prefix: str, key: str) -> tuple[str, str, str]:
    if not prefix.startswith("s3://"):
        raise RuntimeError("S3 prefix must start with s3://")
    value = prefix[5:].strip("/")
    bucket, _, base = value.partition("/")
    if not bucket:
        raise RuntimeError("S3 prefix has no bucket")
    full_key = "/".join(part for part in (base.rstrip("/"), key.lstrip("/"))
                        if part)
    return bucket, full_key, f"s3://{bucket}/{full_key}"


def require_versioned_bucket(prefix: str) -> None:
    bucket, _, _ = s3_parts(prefix, "probe")
    result = json.loads(subprocess.check_output(
        ["aws", "s3api", "get-bucket-versioning", "--bucket", bucket,
         "--output", "json"], text=True) or "{}")
    if result.get("Status") != "Enabled":
        raise RuntimeError("crossed preservation requires a versioned S3 bucket")


def upload_verify(source: Path, prefix: str, key: str,
                  download: Path, schema: str | None) -> dict[str, object]:
    digest = sha256_path(source)
    extent = source.stat().st_size
    bucket, full_key, uri = s3_parts(prefix, key)
    subprocess.run(["aws", "s3", "cp", str(source), uri, "--metadata",
                    f"sha256={digest}", "--only-show-errors"], check=True)
    head = json.loads(subprocess.check_output(
        ["aws", "s3api", "head-object", "--bucket", bucket,
         "--key", full_key, "--output", "json"], text=True))
    metadata = {str(k).lower(): str(v)
                for k, v in head.get("Metadata", {}).items()}
    version = head.get("VersionId")
    if (int(head.get("ContentLength", -1)) != extent or
            metadata.get("sha256") != digest or not version):
        raise RuntimeError("crossed S3 HEAD/version/full-SHA residual")
    download.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["aws", "s3api", "get-object", "--bucket", bucket,
                    "--key", full_key, "--version-id", str(version),
                    str(download)], check=True, stdout=subprocess.DEVNULL)
    if download.stat().st_size != extent or sha256_path(download) != digest:
        raise RuntimeError("crossed S3 versioned download residual")
    if schema:
        restore_archive(download, download.with_suffix(".restored"), schema)
    return {"bucket": bucket, "key": full_key, "version_id": version,
            "bytes": extent, "sha256": digest,
            "head_residual": 0, "download_residual": 0,
            "archive_restore_residual": 0}


def parse_preflight(log: Path) -> dict[str, int]:
    text = log.read_text(errors="replace")
    records = [dict(re.findall(r"([a-z_]+) ([0-9]+)", line))
               for line in text.splitlines()
               if line.startswith("crossed_solve_preflight ")]
    resources = next((dict(re.findall(r"([a-z_]+) ([0-9]+)", line))
                      for line in text.splitlines()
                      if line.startswith("crossed_solve_resources ")), None)
    if len(records) != 2 or resources is None:
        raise RuntimeError("crossed measurement preflight certificate missing")
    result = {
        "nodes": int(records[0]["nodes"]),
        "white_variables": int(records[0]["variables"]),
        "black_variables": int(records[1]["variables"]),
        "peak_scratch_bytes": max(int(record["peak_scratch_bytes"])
                                  for record in records),
        "sidecar_bytes": int(resources["sidecar_bytes"]),
        "overlay_bytes": int(resources["overlay_bytes"]),
        "required_free_bytes": int(resources["required_free_bytes"]),
        "actual_free_bytes": int(resources["actual_free_bytes"]),
    }
    if records[0].get("residual") != "0" or records[1].get("residual") != "0":
        raise RuntimeError("crossed measurement residual")
    return result


def binding_args(manifest: dict[str, object], checkpoint_sha: str,
                 lower_overlay: Path) -> list[str]:
    return [
        "--source-sha256", str(manifest["source_sha256"]),
        "--model-sha256", str(manifest["model_sha256"]),
        "--observation-sha256", str(manifest["observation_sha256"]),
        "--checkpoint-sha256", checkpoint_sha,
        "--lower-jester-table-sha256",
        str(manifest["lower_jester_table_sha256"]),
        "--lower-jester-overlay-sha256", sha256_path(lower_overlay),
        "--lower-jester-model-sha256",
        str(manifest["lower_jester_model_sha256"]),
        "--lower-ghost-sidecar-sha256",
        str(manifest["lower_ghost_sidecar_sha256"]),
        "--lower-ghost-source-sha256",
        str(manifest["lower_ghost_source_sha256"]),
        "--lower-ghost-model-sha256",
        str(manifest["lower_ghost_model_sha256"]),
        "--lower-ghost-observation-sha256",
        str(manifest["lower_ghost_observation_sha256"]),
    ]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--checkpoint-sha256", required=True)
    parser.add_argument("--lower-jester-overlay", type=Path, required=True)
    parser.add_argument("--bundle-manifest", type=Path,
                        default=ROOT / "bundle-manifest.json")
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--maximum-solve-scratch-bytes", type=int, default=0)
    parser.add_argument("--maximum-resident-bytes", type=int, default=0)
    parser.add_argument("--maximum-checkpoint-bytes", type=int, default=0)
    parser.add_argument("--minimum-free-bytes", type=int, default=0)
    parser.add_argument("--s3-prefix")
    args = parser.parse_args(sys.argv[1:] if argv is None else argv)

    if not SHA256.fullmatch(args.checkpoint_sha256):
        raise RuntimeError("crossed expected checkpoint SHA is malformed")
    work = args.work.resolve()
    if work.exists() and any(work.iterdir()):
        raise RuntimeError("crossed work directory is not empty")
    work.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(args.bundle_manifest.read_text())
    verify_extracted_manifest(manifest)
    if args.full and (not args.s3_prefix or
                      min(args.maximum_solve_scratch_bytes,
                          args.maximum_resident_bytes,
                          args.maximum_checkpoint_bytes,
                          args.minimum_free_bytes) <= 0):
        raise RuntimeError("crossed --full requires positive resource gates and S3")
    if args.full:
        require_versioned_bucket(args.s3_prefix)

    inputs = work / "inputs"
    inputs.mkdir()
    checkpoint = inputs / "graph.chk"
    lower_overlay = inputs / "kjesterk.ufiw"
    source_table = inputs / "kjesterkghost.uftb"
    lower_table = inputs / "kjesterk.uftb"
    lower_ghost = inputs / "kghostk.ufgm"
    for source, destination in (
            (args.checkpoint.resolve(), checkpoint),
            (args.lower_jester_overlay.resolve(), lower_overlay),
            (ROOT / "tablebases/kjesterkghost.uftb", source_table),
            (ROOT / "tablebases/kjesterk.uftb", lower_table),
            (ROOT / "tablebases/kghostk.ufgm", lower_ghost)):
        shutil.copyfile(source, destination)
        destination.chmod(0o444)
    if sha256_path(checkpoint) != args.checkpoint_sha256:
        raise RuntimeError("crossed staged checkpoint SHA mismatch")
    if (sha256_path(lower_overlay) != manifest["lower_jester_overlay_sha256"] or
            overlay_binding(lower_overlay) !=
            (manifest["lower_jester_table_sha256"],
             manifest["lower_jester_model_sha256"])):
        raise RuntimeError("crossed staged lower Jester overlay residual")

    binary_dir = work / "binary"
    binary_dir.mkdir()
    commands = manifest["commands"]
    built: dict[str, Path] = {}
    for name in ("build", "model_test", "solver_test"):
        command = list(commands[name])
        target = binary_dir / Path(command[-1]).name
        command[-1] = str(target)
        run_logged(command, work / f"logs/{name}.log", cwd=ROOT)
        target.chmod(0o555)
        built[name] = target
    run_logged([str(built["model_test"])], work / "logs/model_test_run.log",
               cwd=work, resident_limit=args.maximum_resident_bytes)
    run_logged([str(built["solver_test"])], work / "logs/solver_test_run.log",
               cwd=work, resident_limit=args.maximum_resident_bytes)

    common = [
        str(built["build"]), "--checkpoint", str(checkpoint),
        "--raw-begin", "0", "--raw-count", str(manifest["raw_public_frames"]),
        "--seed-batch", "1", "--expansion-batch", "1",
        "--checkpoint-sha256", args.checkpoint_sha256,
        "--scratch-directory", str(work / "scratch"),
        "--maximum-solve-scratch-bytes", str(args.maximum_solve_scratch_bytes or
                                                (1 << 63)),
        "--minimum-free-bytes", str(args.minimum_free_bytes),
    ]
    (work / "scratch").mkdir()
    verify = common[:]
    verify.extend(["--verify-checkpoint", "--maximum-resident-bytes",
                   str(args.maximum_resident_bytes or (1 << 63)),
                   "--maximum-checkpoint-bytes",
                   str(args.maximum_checkpoint_bytes or (1 << 63))])
    verify_peak = run_logged(verify, work / "logs/verify_checkpoint.log",
                             cwd=ROOT,
                             resident_limit=args.maximum_resident_bytes)
    measure = common + ["--measure-solve"]
    measure_peak = run_logged(measure, work / "logs/measure.log", cwd=ROOT,
                              resident_limit=args.maximum_resident_bytes)
    measurement = parse_preflight(work / "logs/measure.log")
    measurement["measured_peak_resident_bytes"] = max(verify_peak, measure_peak)
    write_json(work / "measurement.json", measurement)
    if not args.full:
        print(json.dumps({"status": "measurement-complete-full-not-launched",
                          **measurement}, sort_keys=True))
        return 0
    if measurement["peak_scratch_bytes"] > args.maximum_solve_scratch_bytes:
        raise RuntimeError("crossed measured scratch exceeds explicit gate")
    if measurement["measured_peak_resident_bytes"] > args.maximum_resident_bytes:
        raise RuntimeError("crossed measured resident exceeds explicit gate")
    if measurement["actual_free_bytes"] < measurement["required_free_bytes"]:
        raise RuntimeError("crossed measured free disk is insufficient")
    # The no-delete workflow can temporarily hold the live checkpoint, a
    # worst-case-uncompressed raw archive, its local restore, the freshly
    # downloaded versioned object, and that object's restore.  Result-side
    # copies are smaller but receive the same conservative treatment.
    preservation_bytes = (
        measurement["peak_scratch_bytes"] + 5 * checkpoint.stat().st_size +
        6 * (measurement["sidecar_bytes"] + measurement["overlay_bytes"]) +
        args.minimum_free_bytes)
    measurement["preservation_required_free_bytes"] = preservation_bytes
    if measurement["actual_free_bytes"] < preservation_bytes:
        raise RuntimeError("crossed preservation coexistence disk gate failed")
    write_json(work / "measurement.json", measurement)

    result = work / "results/kjesterkghost.ufcross"
    overlay = work / "results/kjesterkghost.ufiw"
    result.parent.mkdir()
    solve = common + [
        "--solve", "--output-sidecar", str(result),
        "--output-overlay", str(overlay),
        "--source-table", str(source_table),
        "--lower-jester-table", str(lower_table),
        "--lower-jester-overlay", str(lower_overlay),
        "--lower-ghost-sidecar", str(lower_ghost),
        *binding_args(manifest, args.checkpoint_sha256, lower_overlay),
    ]
    solve_peak = run_logged(solve, work / "logs/solve.log", cwd=ROOT,
                            resident_limit=args.maximum_resident_bytes)
    solve_text = (work / "logs/solve.log").read_text(errors="replace")
    if (solve_text.count("crossed_fixed_point target ") != 2 or
            "crossed_sidecar_certificate " not in solve_text or
            "crossed_overlay_certificate " not in solve_text or
            solve_text.count("information_summary side ") != 2 or
            re.search(r"[a-z_]+_residual [1-9]", solve_text)):
        raise RuntimeError("crossed solve certificate residual/missing proof")
    sidecar_sha = sha256_path(result)
    overlay_sha = sha256_path(overlay)

    source_files = {f"sources/{relative}": ROOT / relative
                    for relative in package.BUNDLE_FILES
                    if relative.startswith(("src/", "tools/"))}
    raw_files = {
        "raw/graph.chk": checkpoint,
        "tablebases/kjesterkghost.uftb": source_table,
        "tablebases/kjesterk.uftb": lower_table,
        "tablebases/kjesterk.ufiw": lower_overlay,
        "tablebases/kghostk.ufgm": lower_ghost,
        "binary/solver": built["build"],
        "proof/bundle-manifest.json": args.bundle_manifest.resolve(),
        "proof/measurement.json": work / "measurement.json",
        "proof/verify-checkpoint.log": work / "logs/verify_checkpoint.log",
        "proof/measure.log": work / "logs/measure.log",
        "proof/solve.log": work / "logs/solve.log",
        **source_files,
    }
    raw_archive = work / "archives/crossed-jester-ghost-raw.tar.zst"
    raw_sha = write_archive(raw_archive, raw_files, RAW_SCHEMA)
    raw_restore = restore_archive(raw_archive, work / "restore/raw", RAW_SCHEMA)
    restore_verify = [
        str(raw_restore["binary/solver"]), "--checkpoint",
        str(raw_restore["raw/graph.chk"]), "--seed-batch", "1",
        "--expansion-batch", "1", "--verify-checkpoint",
        "--maximum-resident-bytes", str(args.maximum_resident_bytes),
        "--maximum-checkpoint-bytes", str(args.maximum_checkpoint_bytes),
        "--minimum-free-bytes", str(args.minimum_free_bytes),
    ]
    run_logged(restore_verify, work / "logs/restore-checkpoint.log", cwd=ROOT,
               resident_limit=args.maximum_resident_bytes)

    result_manifest = {
        "schema": RESULT_SCHEMA,
        "source_commit": manifest["source_commit"],
        "model_sha256": manifest["model_sha256"],
        "checkpoint_sha256": args.checkpoint_sha256,
        "raw_archive_sha256": raw_sha,
        "sidecar": {"bytes": result.stat().st_size, "sha256": sidecar_sha},
        "overlay": {"bytes": overlay.stat().st_size, "sha256": overlay_sha},
        "measurement": measurement,
        "solve_peak_resident_bytes": solve_peak,
    }
    write_json(work / "results/result-manifest.json", result_manifest)
    result_files = {
        "tablebases/kjesterkghost.ufcross": result,
        "tablebases/kjesterkghost.ufiw": overlay,
        "proof/result-manifest.json": work / "results/result-manifest.json",
        "proof/solve.log": work / "logs/solve.log",
        "proof/restore-checkpoint.log": work / "logs/restore-checkpoint.log",
        "binary/solver": built["build"],
        "tablebases/kjesterk.uftb": lower_table,
        "tablebases/kjesterkghost.uftb": source_table,
        "tablebases/kjesterk.ufiw": lower_overlay,
        "tablebases/kghostk.ufgm": lower_ghost,
        **source_files,
    }
    result_archive = work / "archives/crossed-jester-ghost-result.tar.zst"
    result_sha = write_archive(result_archive, result_files, RESULT_SCHEMA)
    result_restore = restore_archive(
        result_archive, work / "restore/result", RESULT_SCHEMA)
    restore_sidecar = [
        str(result_restore["binary/solver"]), "--checkpoint",
        str(raw_restore["raw/graph.chk"]), "--seed-batch", "1",
        "--expansion-batch", "1", "--verify-result",
        "--output-sidecar", str(result_restore["tablebases/kjesterkghost.ufcross"]),
        "--sidecar-sha256", sidecar_sha,
        "--output-overlay", str(result_restore["tablebases/kjesterkghost.ufiw"]),
        "--overlay-sha256", overlay_sha,
        "--source-table", str(result_restore["tablebases/kjesterkghost.uftb"]),
        *binding_args(manifest, args.checkpoint_sha256,
                      result_restore["tablebases/kjesterk.ufiw"]),
    ]
    run_logged(restore_sidecar, work / "logs/restore-sidecar.log", cwd=ROOT,
               resident_limit=args.maximum_resident_bytes)
    restore_text = (work / "logs/restore-sidecar.log").read_text(
        errors="replace")
    if ("crossed_result_restore_certificate " not in restore_text or
            re.search(r"[a-z_]+_residual [1-9]", restore_text)):
        raise RuntimeError("crossed restored result certificate residual")

    objects = []
    for archive, digest, schema in (
            (raw_archive, raw_sha, RAW_SCHEMA),
            (result_archive, result_sha, RESULT_SCHEMA)):
        key = f"results/sha256/{digest}/{archive.name}"
        objects.append(upload_verify(
            archive, args.s3_prefix, key,
            work / "s3-verify" / archive.name, schema))
    preservation = {
        "schema": S3_SCHEMA,
        "status": "head-version-download-rehash-archive-restore-verified",
        "source_commit": manifest["source_commit"],
        "checkpoint_sha256": args.checkpoint_sha256,
        "sidecar_sha256": sidecar_sha,
        "overlay_sha256": overlay_sha,
        "objects": objects,
        "local_scratch_retained": True,
        "safe_to_delete_gate": False,
    }
    certificate = work / "archives/preservation-certificate.json"
    write_json(certificate, preservation)
    certificate_sha = sha256_path(certificate)
    certificate_record = upload_verify(
        certificate, args.s3_prefix,
        f"manifests/sha256/{certificate_sha}/{certificate.name}",
        work / "s3-verify" / certificate.name, None)
    print(json.dumps({
        "status": "full-solve-s3-restored-no-delete",
        "raw_archive_sha256": raw_sha,
        "result_archive_sha256": result_sha,
        "sidecar_sha256": sidecar_sha,
        "overlay_sha256": overlay_sha,
        "preservation_certificate": certificate_record,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
