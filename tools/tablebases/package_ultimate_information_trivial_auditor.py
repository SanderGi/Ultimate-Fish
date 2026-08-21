#!/usr/bin/env python3
"""Build the deterministic 16-source information-trivial auditor bundle."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import tarfile

from run_ultimate_concrete_tablebase_shard_aws import MODEL_SOURCES


ROOT = Path(__file__).resolve().parents[2]


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1 << 20):
            digest.update(chunk)
    return digest.hexdigest()


def build(output: Path, root: Path = ROOT) -> str:
    output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(output, "w", format=tarfile.USTAR_FORMAT) as archive:
        for relative in sorted(MODEL_SOURCES):
            payload = (root / relative).read_bytes()
            info = tarfile.TarInfo(relative)
            info.size = len(payload)
            info.mode = 0o644
            info.uid = info.gid = info.mtime = 0
            info.uname = info.gname = ""
            import io
            archive.addfile(info, io.BytesIO(payload))
    return sha256_path(output)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repo", type=Path, default=ROOT)
    args = parser.parse_args()
    digest = build(args.output, args.repo.resolve())
    print(f"bundle_sha256 {digest}")
    print(f"bundle_sources {len(MODEL_SOURCES)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
