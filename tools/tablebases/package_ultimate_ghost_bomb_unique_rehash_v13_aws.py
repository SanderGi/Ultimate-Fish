#!/usr/bin/env python3
"""Repack the Bomb v11 source with resumable unique-index growth."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile

BASE_SHA256 = (
    "1a589cae6fc725ef9ea6da3fbe036d45a5d2b6096bb49b5936db29d1813c7da1"
)
PREFIX = "ultimatefish-ghost-bomb-unique-rehash-v13-source"
REPLACEMENTS = (
    "src/ultimate/tablebases/external_robdd.cpp",
    "src/ultimate/tablebases/external_robdd.h",
    "tests/tablebases/ultimate_external_robdd.cpp",
)
SEMANTICS = (
    "ghost-bomb-v11-compatible-exact-parallel-expanded-unique-index-v13"
)
SCHEMA = "ultimate-ghost-bomb-unique-rehash-source-v13"
ROOT = Path(__file__).resolve().parents[2]


def digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def tar_info(name: str, payload: bytes, mode: int) -> tarfile.TarInfo:
    result = tarfile.TarInfo(name)
    result.size = len(payload)
    result.mode = mode
    result.mtime = result.uid = result.gid = 0
    result.uname = result.gname = ""
    return result


def build(base: Path, output: Path) -> dict[str, object]:
    base_payload = base.read_bytes()
    if digest(base_payload) != BASE_SHA256:
        raise RuntimeError("Bomb v11 source base hash mismatch")
    members: dict[str, tuple[bytes, int]] = {}
    with tarfile.open(fileobj=io.BytesIO(base_payload), mode="r:") as archive:
        for member in archive.getmembers():
            if not member.isfile():
                continue
            source = archive.extractfile(member)
            if source is None:
                raise RuntimeError(f"cannot read source member {member.name}")
            members[member.name] = (source.read(), member.mode)
    for relative in REPLACEMENTS:
        members[relative] = ((ROOT / relative).read_bytes(), 0o644)
    manifest = {
        "base_sha256": BASE_SHA256,
        "schema": SCHEMA,
        "semantics": SEMANTICS,
        "replacements": [
            {"path": path, "sha256": digest(members[path][0])}
            for path in REPLACEMENTS
        ],
    }
    manifest_payload = (json.dumps(manifest, indent=2, sort_keys=True) +
                        "\n").encode()
    members["GHOST-BOMB-UNIQUE-REHASH-V13-MANIFEST.json"] = (
        manifest_payload, 0o644)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        for relative, (payload, mode) in sorted(members.items()):
            archive.addfile(tar_info(f"{PREFIX}/{relative}", payload, mode),
                            io.BytesIO(payload))
    return {
        "archive": str(output),
        "sha256": digest(output.read_bytes()),
        "bytes": output.stat().st_size,
        "manifest": manifest,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.base, args.output), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
