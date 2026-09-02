#!/usr/bin/env python3
"""Prepare authenticated sub-50-GB Hugging Face shards for one UFDS file."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import publish_ultimate_devil_stateful_hf as publisher  # noqa: E402


SHARD_BYTES = 32_000_000_000


def split_file(source: Path, table_dir: Path) -> list[dict[str, object]]:
    parts: list[dict[str, object]] = []
    stem = source.name.removesuffix(".ufds")
    with source.open("rb") as stream:
        index = 0
        remaining = source.stat().st_size
        while remaining:
            size = min(remaining, SHARD_BYTES)
            filename = f"{stem}-part{index:03d}.ufdsp"
            path = table_dir / filename
            digest = hashlib.sha256()
            written = 0
            with path.open("wb") as output:
                while written < size:
                    block = stream.read(min(8 << 20, size - written))
                    if not block:
                        raise RuntimeError("truncated UFDS while preparing shards")
                    output.write(block)
                    digest.update(block)
                    written += len(block)
            parts.append({
                "filename": filename,
                "bytes": size,
                "sha256": digest.hexdigest(),
            })
            remaining -= size
            index += 1
    return parts


def run(certificate: Path, square: int, work_dir: Path,
        manifest_s3_uri: str, workers: int) -> None:
    cert = publisher.validate_certificate(certificate)
    matches = [partition for partition in cert["partitions"]
               if int(partition["square"]) == square]
    if len(matches) != 1:
        raise RuntimeError("square is not uniquely certified")
    partition = matches[0]
    table_dir = work_dir / "tablebases"
    table_dir.mkdir(parents=True, exist_ok=True)
    filename = publisher.remote_name(partition)
    source = table_dir / filename
    auditor = publisher.build_auditor(work_dir)
    publisher.download(partition, source, workers)
    publisher.validate_sidecar(source, partition, auditor, workers)

    manifest_name = filename.removesuffix(".ufds") + ".ufdsm"
    manifest_path = table_dir / manifest_name
    reuse = False
    if manifest_path.is_file():
        try:
            existing = json.loads(manifest_path.read_text())
            reuse = (
                existing.get("schema") == "ultimate-fish-ufds-shard-manifest-v1" and
                existing.get("filename") == filename and
                existing.get("bytes") == source.stat().st_size and
                existing.get("sha256") == partition["sidecar_sha256"] and
                all((table_dir / part["filename"]).is_file() and
                    (table_dir / part["filename"]).stat().st_size == part["bytes"] and
                    publisher.sha256(table_dir / part["filename"]) == part["sha256"]
                    for part in existing.get("parts", ()))
            )
        except (KeyError, OSError, ValueError, TypeError):
            reuse = False
    if reuse:
        manifest = existing
    else:
        parts = split_file(source, table_dir)
        manifest = {
            "schema": "ultimate-fish-ufds-shard-manifest-v1",
            "filename": filename,
            "bytes": source.stat().st_size,
            "sha256": partition["sidecar_sha256"],
            "square": square,
            "parts": parts,
        }
        manifest_path.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    subprocess.run([
        "aws", "s3", "cp", str(manifest_path), manifest_s3_uri,
        "--sse", "AES256",
    ], check=True)
    print("HF_UFDS_SHARDS_PREPARED " + json.dumps({
        "filename": filename,
        "manifest": manifest_name,
        "parts": len(manifest["parts"]),
        "bytes": manifest["bytes"],
        "sha256": manifest["sha256"],
    }, sort_keys=True), flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--certificate", type=Path, required=True)
    parser.add_argument("--square", type=int, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--manifest-s3-uri", required=True)
    parser.add_argument("--workers", type=int, default=32)
    args = parser.parse_args()
    run(args.certificate, args.square, args.work_dir,
        args.manifest_s3_uri, args.workers)


if __name__ == "__main__":
    main()
