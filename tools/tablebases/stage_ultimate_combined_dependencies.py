#!/usr/bin/env python3
"""Extend an authenticated concrete dependency cache with S3-pinned tables."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


SCHEMA = "ultimate-concrete-k2-dependencies-v2"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--index", type=Path, required=True)
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--region", default="us-west-2")
    args = parser.parse_args()
    if args.destination.exists():
        raise RuntimeError("destination already exists")
    base_manifest = json.loads((args.base / "manifest.json").read_text())
    if base_manifest.get("schema") != SCHEMA:
        raise RuntimeError("base dependency schema mismatch")
    additions = json.loads(args.index.read_text())
    args.destination.mkdir(parents=True)
    try:
        for source in args.base.iterdir():
            if source.name != "manifest.json":
                os.link(source, args.destination / source.name)
        records = {row["filename"]: dict(row)
                   for row in base_manifest["files"]}
        for row in additions:
            name, digest = row["filename"], row["sha256"]
            if Path(name).name != name or len(digest) != 64:
                raise RuntimeError("malformed dependency addition")
            target = args.destination / name
            key = f"dependencies/current/sha256/{digest}/{name}"
            subprocess.run([
                "aws", "s3api", "get-object", "--bucket", args.bucket,
                "--key", key, "--region", args.region, str(target),
            ], check=True, stdout=subprocess.DEVNULL)
            if sha256(target) != digest:
                raise RuntimeError(f"dependency hash residual: {name}")
            target.chmod(0o444)
            records[name] = {"filename": name, "sha256": digest,
                             "bytes": target.stat().st_size}
        manifest = {"schema": SCHEMA,
                    "files": [records[name] for name in sorted(records)]}
        (args.destination / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    except Exception:
        shutil.rmtree(args.destination)
        raise
    print(json.dumps({"files": len(records),
                      "manifest_sha256": sha256(
                          args.destination / "manifest.json")}, sort_keys=True))


if __name__ == "__main__":
    main()
