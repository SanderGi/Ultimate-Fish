#!/usr/bin/env python3
"""Repack authenticated Ghost v7 sources with the parallel v8 compactor."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile


ROOT = Path(__file__).resolve().parents[2]
BASE_SHA256 = "1107f9d8fe42a8e709694f30585f9564f52c0275021e1f634c01d9ce41404fd4"
PREFIX = "ultimatefish-ghost-parallel-v8-source"
REPLACEMENTS = (
    "src/ultimate/tablebases/external_robdd.cpp",
    "src/ultimate/tablebases/external_robdd.h",
    "tests/tablebases/ultimate_external_robdd.cpp",
)
SEMANTICS = (
    "ghost-public-extra-exact-unit-geometry-dynamic-bellman-"
    "level-ordered-parallel-"
    "robdd-mark-direct-topological-copy-parallel-unique-rebuild-v8"
)
BELLMAN_SOURCE = "src/ultimate/tablebases/ghost_extra_information_tablebase.cpp"


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
        raise RuntimeError("Ghost v7 source base hash mismatch")
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
    bellman_payload, bellman_mode = members[BELLMAN_SOURCE]
    old_chunk = b"constexpr std::uint32_t GeometryChunk = 32;"
    new_chunk = b"constexpr std::uint32_t GeometryChunk = 1;"
    if bellman_payload.count(old_chunk) != 1:
        raise RuntimeError("Ghost v7 Bellman chunk marker is not unique")
    members[BELLMAN_SOURCE] = (
        bellman_payload.replace(old_chunk, new_chunk), bellman_mode)
    manifest = {
        "base_sha256": BASE_SHA256,
        "schema": "ultimate-ghost-parallel-source-v8",
        "semantics": SEMANTICS,
        "replacements": [
            {"path": relative, "sha256": digest(members[relative][0])}
            for relative in REPLACEMENTS
        ] + [{"path": BELLMAN_SOURCE,
              "sha256": digest(members[BELLMAN_SOURCE][0])}],
    }
    manifest_payload = (
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    ).encode()
    members["GHOST-PARALLEL-V8-MANIFEST.json"] = (manifest_payload, 0o644)
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
