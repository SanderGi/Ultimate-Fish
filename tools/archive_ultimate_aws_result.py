#!/usr/bin/env python3
"""Archive authenticated Ultimate AWS results and verify their S3 restore.

The tablebase runners write ``work/artifact-manifest.json`` after a successful
measurement or solve.  This tool treats that manifest as an allowlist, builds a
deterministic result-only tar.zst, restores it into a fresh directory, and can
then upload/download/restore the content-addressed object through a versioned
S3 bucket.  It deliberately never deletes the source tree or verification
copies.
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
import tarfile
import tempfile
from typing import BinaryIO


SCHEMA = "ultimate-aws-result-archive-v1"
KIND = re.compile(r"[a-z0-9][a-z0-9._-]{0,95}\Z")


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def write_json(path: Path, payload: object) -> None:
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def _safe_relative(value: object) -> PurePosixPath:
    relative = PurePosixPath(str(value))
    if (not str(relative) or relative.is_absolute() or ".." in relative.parts or
            "." in relative.parts):
        raise RuntimeError(f"unsafe artifact path: {value}")
    return relative


def authenticated_inventory(root: Path, artifact_manifest: Path,
                            kind: str) -> dict[str, object]:
    if not KIND.fullmatch(kind):
        raise RuntimeError("archive kind must be a short lowercase identifier")
    root = root.resolve(strict=True)
    manifest = json.loads(artifact_manifest.read_text(encoding="utf-8"))
    records = manifest.get("artifacts")
    if not isinstance(records, list) or not records:
        raise RuntimeError("artifact manifest has no result inventory")
    authenticated: list[dict[str, object]] = []
    seen: set[str] = set()
    for raw in records:
        if not isinstance(raw, dict):
            raise RuntimeError("artifact inventory record is not an object")
        relative = _safe_relative(raw.get("path"))
        name = relative.as_posix()
        if name in seen:
            raise RuntimeError(f"duplicate artifact inventory path: {name}")
        seen.add(name)
        path = root.joinpath(*relative.parts)
        if path.is_symlink() or not path.is_file():
            raise RuntimeError(f"artifact is missing or not a regular file: {name}")
        resolved = path.resolve(strict=True)
        try:
            resolved.relative_to(root)
        except ValueError as error:
            raise RuntimeError(f"artifact escapes result root: {name}") from error
        extent = path.stat().st_size
        digest = sha256_path(path)
        if extent != int(raw.get("bytes", -1)) or digest != raw.get("sha256"):
            raise RuntimeError(f"artifact extent/full-SHA mismatch: {name}")
        authenticated.append({"path": name, "bytes": extent, "sha256": digest})
    authenticated.sort(key=lambda record: str(record["path"]))
    bindings = {key: value for key, value in manifest.items()
                if key != "artifacts"}
    return {"schema": SCHEMA, "kind": kind, "bindings": bindings,
            "artifacts": authenticated}


def _tar_info(name: str, extent: int) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = extent
    info.mode = 0o444
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    info.mtime = 0
    return info


def _add_file(archive: tarfile.TarFile, name: str, stream: BinaryIO,
              extent: int) -> None:
    archive.addfile(_tar_info(name, extent), stream)


def build_archive(root: Path, artifact_manifest: Path, kind: str,
                  output_dir: Path) -> tuple[Path, dict[str, object]]:
    root = root.resolve(strict=True)
    inventory = authenticated_inventory(root, artifact_manifest, kind)
    manifest_bytes = (json.dumps(inventory, indent=2, sort_keys=True) + "\n").encode()
    output_dir.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix="ultimate-result-", dir=output_dir))
    try:
        raw_tar = temporary / "result.tar"
        compressed = temporary / "result.tar.zst"
        with tarfile.open(raw_tar, "w", format=tarfile.PAX_FORMAT) as archive:
            _add_file(archive, "archive-manifest.json", io.BytesIO(manifest_bytes),
                      len(manifest_bytes))
            for record in inventory["artifacts"]:
                relative = _safe_relative(record["path"])
                path = root.joinpath(*relative.parts)
                with path.open("rb") as stream:
                    _add_file(archive, "payload/" + relative.as_posix(), stream,
                              int(record["bytes"]))
        with compressed.open("wb") as output:
            subprocess.run(["zstd", "-19", "-T1", "--no-progress", "-c",
                            str(raw_tar)], stdout=output, check=True)
        digest = sha256_path(compressed)
        final = output_dir / f"{kind}-{digest}.tar.zst"
        if final.exists():
            if final.stat().st_size != compressed.stat().st_size or sha256_path(final) != digest:
                raise RuntimeError(f"content-addressed archive collision: {final}")
        else:
            os.replace(compressed, final)
        certificate = {"schema": SCHEMA, "kind": kind,
                       "archive": final.name, "bytes": final.stat().st_size,
                       "sha256": digest, "safe_to_delete_gate": False}
        verify_archive(final, output_dir / f"restore-{digest}")
        return final, certificate
    finally:
        shutil.rmtree(temporary, ignore_errors=True)


def verify_archive(archive_path: Path, restore_dir: Path) -> dict[str, object]:
    if restore_dir.exists():
        raise RuntimeError(f"restore directory already exists: {restore_dir}")
    restore_dir.mkdir(parents=True)
    raw_tar = restore_dir / "archive.tar"
    try:
        with raw_tar.open("wb") as output:
            subprocess.run(["zstd", "-d", "--no-progress", "-c",
                            str(archive_path)], stdout=output, check=True)
        with tarfile.open(raw_tar, "r:") as archive:
            members = archive.getmembers()
            names = [member.name for member in members]
            if (not names or names[0] != "archive-manifest.json" or
                    len(names) != len(set(names)) or
                    any(not member.isfile() for member in members)):
                raise RuntimeError("archive member inventory is not canonical")
            manifest_stream = archive.extractfile(members[0])
            if manifest_stream is None:
                raise RuntimeError("archive manifest is unreadable")
            manifest = json.loads(manifest_stream.read())
            if manifest.get("schema") != SCHEMA:
                raise RuntimeError("archive manifest schema mismatch")
            expected = {"archive-manifest.json"}
            for record in manifest.get("artifacts", []):
                relative = _safe_relative(record.get("path"))
                member_name = "payload/" + relative.as_posix()
                expected.add(member_name)
                try:
                    member = archive.getmember(member_name)
                except KeyError as error:
                    raise RuntimeError(f"archive artifact is missing: {relative}") from error
                stream = archive.extractfile(member)
                if stream is None:
                    raise RuntimeError(f"archive artifact is unreadable: {relative}")
                output = restore_dir.joinpath(*relative.parts)
                output.parent.mkdir(parents=True, exist_ok=True)
                digest = hashlib.sha256()
                extent = 0
                with output.open("xb") as destination:
                    while block := stream.read(8 * 1024 * 1024):
                        destination.write(block)
                        digest.update(block)
                        extent += len(block)
                if (extent != int(record.get("bytes", -1)) or
                        digest.hexdigest() != record.get("sha256")):
                    raise RuntimeError(f"restored artifact residual: {relative}")
            if set(names) != expected:
                raise RuntimeError("archive contains undeclared members")
        return {"schema": SCHEMA, "archive_restore_residual": 0,
                "artifacts": len(manifest["artifacts"])}
    finally:
        raw_tar.unlink(missing_ok=True)


def s3_object(prefix: str, key: str) -> tuple[str, str, str]:
    if not prefix.startswith("s3://"):
        raise RuntimeError("S3 prefix must start with s3://")
    remainder = prefix[5:].strip("/")
    bucket, separator, base = remainder.partition("/")
    if not bucket:
        raise RuntimeError("S3 prefix has no bucket")
    full_key = "/".join(part for part in (base.rstrip("/"), key.lstrip("/"))
                        if part)
    return bucket, full_key, f"s3://{bucket}/{full_key}"


def upload_and_restore(archive: Path, certificate: dict[str, object],
                       prefix: str, verification_dir: Path) -> dict[str, object]:
    digest = str(certificate["sha256"])
    extent = int(certificate["bytes"])
    key = f"results/sha256/{digest}/{archive.name}"
    bucket, full_key, uri = s3_object(prefix, key)
    versioning = json.loads(subprocess.check_output(
        ["aws", "s3api", "get-bucket-versioning", "--bucket", bucket,
         "--output", "json"], text=True))
    if versioning.get("Status") != "Enabled":
        raise RuntimeError("result bucket versioning is not enabled")
    subprocess.run(["aws", "s3", "cp", str(archive), uri, "--metadata",
                    f"sha256={digest},archive-schema={SCHEMA}",
                    "--only-show-errors"], check=True)
    head = json.loads(subprocess.check_output(
        ["aws", "s3api", "head-object", "--bucket", bucket, "--key",
         full_key, "--output", "json"], text=True))
    metadata = {str(key).lower(): str(value)
                for key, value in head.get("Metadata", {}).items()}
    if (int(head.get("ContentLength", -1)) != extent or
            metadata.get("sha256") != digest or
            metadata.get("archive-schema") != SCHEMA or
            not head.get("VersionId")):
        raise RuntimeError("S3 HEAD version/extent/full-SHA residual")
    verification_dir.mkdir(parents=True, exist_ok=False)
    download = verification_dir / archive.name
    subprocess.run(["aws", "s3", "cp", uri, str(download),
                    "--only-show-errors"], check=True)
    if download.stat().st_size != extent or sha256_path(download) != digest:
        raise RuntimeError("fresh S3 download extent/full-SHA residual")
    restored = verify_archive(download, verification_dir / "restored")
    return {"schema": SCHEMA, "bucket": bucket, "key": full_key, "uri": uri,
            "version_id": head["VersionId"], "etag": head.get("ETag", ""),
            "bytes": extent, "sha256": digest,
            "head_residual": 0, "download_residual": 0, **restored,
            "safe_to_delete_gate": False}


def upload_certificate(certificate_path: Path, archive_digest: str,
                       prefix: str, verification_dir: Path) -> dict[str, object]:
    digest = sha256_path(certificate_path)
    extent = certificate_path.stat().st_size
    key = (f"results/sha256/{archive_digest}/certificates/"
           f"{digest}.json")
    bucket, full_key, uri = s3_object(prefix, key)
    subprocess.run(["aws", "s3", "cp", str(certificate_path), uri,
                    "--metadata", f"sha256={digest},schema={SCHEMA}",
                    "--only-show-errors"], check=True)
    head = json.loads(subprocess.check_output(
        ["aws", "s3api", "head-object", "--bucket", bucket, "--key",
         full_key, "--output", "json"], text=True))
    metadata = {str(key).lower(): str(value)
                for key, value in head.get("Metadata", {}).items()}
    if (int(head.get("ContentLength", -1)) != extent or
            metadata.get("sha256") != digest or
            metadata.get("schema") != SCHEMA or not head.get("VersionId")):
        raise RuntimeError("S3 certificate HEAD/version/full-SHA residual")
    verification_dir.mkdir(parents=True, exist_ok=False)
    download = verification_dir / certificate_path.name
    subprocess.run(["aws", "s3", "cp", uri, str(download),
                    "--only-show-errors"], check=True)
    if download.stat().st_size != extent or sha256_path(download) != digest:
        raise RuntimeError("fresh S3 certificate download residual")
    return {"bucket": bucket, "key": full_key, "uri": uri,
            "version_id": head["VersionId"], "etag": head.get("ETag", ""),
            "bytes": extent, "sha256": digest,
            "head_residual": 0, "download_residual": 0}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--artifact-manifest", type=Path, required=True)
    parser.add_argument("--kind", required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--s3-prefix")
    args = parser.parse_args()
    archive, certificate = build_archive(
        args.root, args.artifact_manifest, args.kind, args.output_dir)
    certificate["local_archive_restore_residual"] = 0
    if args.s3_prefix:
        certificate["s3"] = upload_and_restore(
            archive, certificate, args.s3_prefix,
            args.output_dir / f"s3-verify-{certificate['sha256']}")
    else:
        certificate["s3"] = {"status": "not-requested-no-preservation-claim"}
    certificate_path = archive.with_suffix(archive.suffix + ".certificate.json")
    write_json(certificate_path, certificate)
    receipt: dict[str, object] = {"certificate": certificate}
    if args.s3_prefix:
        receipt["certificate_object"] = upload_certificate(
            certificate_path, str(certificate["sha256"]), args.s3_prefix,
            args.output_dir / f"certificate-s3-verify-{certificate['sha256']}")
    print(json.dumps(receipt, sort_keys=True))


if __name__ == "__main__":
    main()
