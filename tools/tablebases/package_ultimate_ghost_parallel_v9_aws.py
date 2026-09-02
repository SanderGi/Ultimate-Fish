#!/usr/bin/env python3
"""Repack v8 with the checked-in parallel extra-state Ghost solver."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile


ROOT = Path(__file__).resolve().parents[2]
BASE_SHA256 = "2d1ff830eb622de7880af5961cf1c3e4b6f14817a657da4103b9227a88400be7"
PREFIX = "ultimatefish-ghost-parallel-v9-source"
REPLACEMENTS = (
    "src/ultimate/position.cpp",
    "src/ultimate/position.h",
    "src/ultimate/tablebases/external_robdd.cpp",
    "src/ultimate/tablebases/external_robdd.h",
    "src/ultimate/tablebases/ghost_dragon_information_solver.cpp",
    "src/ultimate/tablebases/ghost_dragon_information_solver.h",
    "src/ultimate/tablebases/ghost_dragon_information_tablebase.cpp",
    "src/ultimate/tablebases/ghost_extra_information_tablebase.cpp",
    "src/ultimate/tablebases/ghost_public_extra_information_solver.cpp",
    "src/ultimate/tablebases/ghost_public_extra_information_solver.h",
    "src/ultimate/tablebases/ghost_public_extra_model.cpp",
    "tests/tablebases/ultimate_external_robdd.cpp",
)
SEMANTICS = (
    "ghost-extra-parallel-bellman-and-certification-with-exact-"
    "observation-remap-and-parallel-robdd-compaction-v9"
)
SCHEMA = "ultimate-ghost-parallel-source-v9"


def digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def tar_info(name: str, payload: bytes, mode: int = 0o644) -> tarfile.TarInfo:
    result = tarfile.TarInfo(name)
    result.size = len(payload)
    result.mode = mode
    result.mtime = result.uid = result.gid = 0
    result.uname = result.gname = ""
    return result


def build(base: Path, output: Path) -> dict[str, object]:
    base_payload = base.read_bytes()
    if digest(base_payload) != BASE_SHA256:
        raise RuntimeError("Ghost v8 source base hash mismatch")
    members: dict[str, tuple[bytes, int]] = {}
    with tarfile.open(fileobj=io.BytesIO(base_payload), mode="r:") as archive:
        for member in archive.getmembers():
            if not member.isfile():
                continue
            source = archive.extractfile(member)
            if source is None:
                raise RuntimeError(f"cannot read source member {member.name}")
            relative = member.name.split("/", 1)[1]
            members[relative] = (source.read(), member.mode)
    for relative in REPLACEMENTS:
        members[relative] = ((ROOT / relative).read_bytes(), 0o644)
    manifest = {
        "base_sha256": BASE_SHA256,
        "schema": SCHEMA,
        "semantics": SEMANTICS,
        "replacements": [
            {"path": relative, "sha256": digest(members[relative][0])}
            for relative in REPLACEMENTS
        ],
    }
    members["GHOST-PARALLEL-V9-MANIFEST.json"] = (
        (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode(),
        0o644,
    )
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
