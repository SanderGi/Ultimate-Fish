#!/usr/bin/env python3
"""Preserve the local tablebase cache in versioned S3 before removing it.

This is a one-way preservation helper, not a tablebase source of truth.  It
archives only allowlisted payload extensions, authenticates every physical
file in a manifest, verifies the local stream, uploads content-addressably,
requires a VersionId and SHA metadata, freshly downloads the exact version,
and verifies that stream again.  It never removes source files itself.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import tempfile


ROOT = Path(__file__).resolve().parents[2]
SCHEMA = "ultimate-local-tablebase-snapshot-v1"
PAYLOAD = re.compile(r".+\.(?:uftb(?:\.part[0-9]+)?|ufiw|ufgm|ufgi|uficapture)\Z")


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(8 << 20):
            digest.update(block)
    return digest.hexdigest()


def payload_paths(root: Path) -> list[Path]:
    directory = root / "tablebases"
    paths = sorted(path for path in directory.iterdir()
                   if path.is_file() and not path.is_symlink() and
                   PAYLOAD.fullmatch(path.name))
    if not paths:
        raise RuntimeError("no local tablebase payload files found")
    return paths


def inventory(root: Path, paths: list[Path]) -> dict[str, object]:
    records = []
    for path in paths:
        relative = path.relative_to(root).as_posix()
        records.append({"path": relative, "bytes": path.stat().st_size,
                        "sha256": sha256_path(path)})
    head = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    return {"schema": SCHEMA, "source_commit": head,
            "artifacts": records,
            "physical_files": len(records),
            "physical_bytes": sum(int(row["bytes"]) for row in records)}


def tar_info(name: str, size: int) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = size
    info.mode = 0o444
    info.uid = info.gid = info.mtime = 0
    info.uname = info.gname = ""
    return info


def build_archive(root: Path, manifest: dict[str, object], output: Path,
                  level: int) -> str:
    if output.exists():
        raise RuntimeError(f"archive output already exists: {output}")
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("xb") as destination:
        process = subprocess.Popen(
            ["zstd", f"-{level}", "-T2", "--no-progress", "-c"],
            stdin=subprocess.PIPE, stdout=destination)
        assert process.stdin is not None
        try:
            with tarfile.open(fileobj=process.stdin, mode="w|",
                              format=tarfile.PAX_FORMAT) as archive:
                archive.addfile(tar_info("ARCHIVE.MANIFEST.json", len(manifest_bytes)),
                                io.BytesIO(manifest_bytes))
                for record in manifest["artifacts"]:
                    assert isinstance(record, dict)
                    relative = PurePosixPath(str(record["path"]))
                    path = root.joinpath(*relative.parts)
                    with path.open("rb") as stream:
                        archive.addfile(tar_info(relative.as_posix(),
                                                 int(record["bytes"])), stream)
        finally:
            process.stdin.close()
        if process.wait() != 0:
            raise RuntimeError("zstd tablebase snapshot failed")
    return sha256_path(output)


def verify_archive(path: Path) -> dict[str, object]:
    process = subprocess.Popen(["zstd", "-d", "--no-progress", "-c", str(path)],
                               stdout=subprocess.PIPE)
    assert process.stdout is not None
    manifest: dict[str, object] | None = None
    seen: dict[str, tuple[int, str]] = {}
    with tarfile.open(fileobj=process.stdout, mode="r|") as archive:
        previous = ""
        for member in archive:
            name = member.name
            if (previous and name <= previous) or not member.isfile():
                raise RuntimeError("snapshot members are not sorted regular files")
            previous = name
            pure = PurePosixPath(name)
            if pure.is_absolute() or ".." in pure.parts or member.mtime or member.uid or member.gid:
                raise RuntimeError("unsafe snapshot member")
            stream = archive.extractfile(member)
            if stream is None:
                raise RuntimeError("unreadable snapshot member")
            digest = hashlib.sha256()
            extent = 0
            chunks: list[bytes] = [] if name == "ARCHIVE.MANIFEST.json" else []
            while block := stream.read(8 << 20):
                digest.update(block)
                extent += len(block)
                if name == "ARCHIVE.MANIFEST.json":
                    chunks.append(block)
            if name == "ARCHIVE.MANIFEST.json":
                manifest = json.loads(b"".join(chunks))
            else:
                seen[name] = (extent, digest.hexdigest())
    process.stdout.close()
    if process.wait() != 0 or manifest is None or manifest.get("schema") != SCHEMA:
        raise RuntimeError("snapshot decompression/manifest residual")
    expected = {str(row["path"]): (int(row["bytes"]), str(row["sha256"]))
                for row in manifest.get("artifacts", [])}
    if seen != expected:
        raise RuntimeError("snapshot physical-file hash/extent residual")
    return {"files": len(seen), "bytes": sum(size for size, _ in seen.values()),
            "stream_restore_residual": 0}


def s3_target(prefix: str, digest: str, name: str) -> tuple[str, str, str]:
    if not prefix.startswith("s3://"):
        raise RuntimeError("S3 prefix must begin with s3://")
    bucket, separator, base = prefix[5:].strip("/").partition("/")
    if not bucket:
        raise RuntimeError("S3 prefix has no bucket")
    key = "/".join(filter(None, (base, "snapshots", "sha256", digest, name)))
    return bucket, key, f"s3://{bucket}/{key}"


def preserve(archive: Path, digest: str, prefix: str, work: Path) -> dict[str, object]:
    bucket, key, uri = s3_target(prefix, digest, archive.name)
    versioning = json.loads(subprocess.check_output(
        ["aws", "s3api", "get-bucket-versioning", "--bucket", bucket,
         "--output", "json"], text=True))
    if versioning.get("Status") != "Enabled":
        raise RuntimeError("S3 bucket versioning is required")
    subprocess.run(["aws", "s3", "cp", str(archive), uri, "--metadata",
                    f"sha256={digest},schema={SCHEMA}", "--only-show-errors"],
                   check=True)
    head = json.loads(subprocess.check_output(
        ["aws", "s3api", "head-object", "--bucket", bucket, "--key", key,
         "--output", "json"], text=True))
    metadata = {str(k).lower(): str(v) for k, v in head.get("Metadata", {}).items()}
    version = head.get("VersionId")
    if (int(head.get("ContentLength", -1)) != archive.stat().st_size or
            metadata.get("sha256") != digest or metadata.get("schema") != SCHEMA or
            not isinstance(version, str) or not version or version == "null"):
        raise RuntimeError("S3 snapshot HEAD/version/SHA residual")
    download = work / "fresh" / archive.name
    download.parent.mkdir(parents=True, exist_ok=False)
    subprocess.run(["aws", "s3api", "get-object", "--bucket", bucket,
                    "--key", key, "--version-id", version, str(download)],
                   check=True, stdout=subprocess.DEVNULL)
    if download.stat().st_size != archive.stat().st_size or sha256_path(download) != digest:
        raise RuntimeError("fresh version-pinned S3 snapshot download residual")
    restored = verify_archive(download)
    return {"bucket": bucket, "key": key, "version_id": version,
            "bytes": archive.stat().st_size, "sha256": digest,
            "head_residual": 0, "download_residual": 0, **restored}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--s3-prefix", required=True)
    parser.add_argument("--zstd-level", type=int, default=3, choices=range(1, 20))
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    paths = payload_paths(root)
    manifest = inventory(root, paths)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    temporary = args.output_dir / "local-tablebases.incomplete.tar.zst"
    digest = build_archive(root, manifest, temporary, args.zstd_level)
    archive = args.output_dir / f"local-tablebases-{digest}.tar.zst"
    os.replace(temporary, archive)
    local = verify_archive(archive)
    remote = preserve(archive, digest, args.s3_prefix,
                      Path(tempfile.mkdtemp(prefix="ultimate-local-s3-",
                                           dir=args.output_dir)))
    certificate = {"schema": SCHEMA, "archive": archive.name,
                   "archive_sha256": digest, "archive_bytes": archive.stat().st_size,
                   "manifest": manifest, "local": local, "s3": remote,
                   "safe_to_delete_local_payloads": True}
    certificate_path = args.output_dir / f"local-tablebases-{digest}.certificate.json"
    certificate_path.write_text(json.dumps(certificate, indent=2, sort_keys=True) + "\n")
    cert_digest = sha256_path(certificate_path)
    cert_bucket, cert_key, cert_uri = s3_target(
        args.s3_prefix, digest, f"certificates/{cert_digest}.json")
    subprocess.run(["aws", "s3", "cp", str(certificate_path), cert_uri,
                    "--metadata", f"sha256={cert_digest},schema={SCHEMA}",
                    "--only-show-errors"], check=True)
    cert_head = json.loads(subprocess.check_output(
        ["aws", "s3api", "head-object", "--bucket", cert_bucket, "--key",
         cert_key, "--output", "json"], text=True))
    if (int(cert_head.get("ContentLength", -1)) != certificate_path.stat().st_size or
            cert_head.get("Metadata", {}).get("sha256") != cert_digest or
            not cert_head.get("VersionId")):
        raise RuntimeError("S3 snapshot certificate HEAD residual")
    cert_fresh = args.output_dir / f"certificate-fresh-{cert_digest}.json"
    subprocess.run(["aws", "s3api", "get-object", "--bucket", cert_bucket,
                    "--key", cert_key, "--version-id", cert_head["VersionId"],
                    str(cert_fresh)], check=True, stdout=subprocess.DEVNULL)
    if (cert_fresh.stat().st_size != certificate_path.stat().st_size or
            sha256_path(cert_fresh) != cert_digest or
            cert_fresh.read_bytes() != certificate_path.read_bytes()):
        raise RuntimeError("fresh version-pinned S3 snapshot certificate residual")
    print(json.dumps({"certificate": certificate_path.as_posix(),
                      "certificate_sha256": cert_digest,
                      "certificate_version_id": cert_head["VersionId"],
                      **remote}, sort_keys=True))


if __name__ == "__main__":
    main()
