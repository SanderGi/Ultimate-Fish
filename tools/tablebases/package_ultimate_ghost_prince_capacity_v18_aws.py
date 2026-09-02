#!/usr/bin/env python3
"""Repack Prince v23 source with exact index growth and a 192 GiB gate."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile


ROOT = Path(__file__).resolve().parents[2]
BASE_SHA256 = "d8f3180c406ef51631a7f72b042fadbfe1c307fbf4c3b9afeab4a64f226d996f"
PREFIX = "ultimatefish-ghost-prince-capacity-v18-source"
SCHEMA = "ultimate-ghost-prince-capacity-source-v18"
SEMANTICS = "prince-v23-exact-parallel-unique-rehash-192g-gate-v18"
REPLACEMENTS = (
    "src/ultimate/tablebases/external_robdd.cpp",
    "src/ultimate/tablebases/external_robdd.h",
    "tests/tablebases/ultimate_external_robdd.cpp",
)
GATE_PATH = "src/ultimate/tablebases/ghost_extra_information_tablebase.cpp"


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
        raise RuntimeError("Prince v23 source base hash mismatch")
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
    gate, gate_mode = members[GATE_PATH]
    old = (b"constexpr std::uint64_t Budget = 97ULL << 30;")
    new = (b"constexpr std::uint64_t Budget = 192ULL << 30;")
    if gate.count(old) != 1:
        raise RuntimeError("Prince v23 allocation gate signature mismatch")
    gate = gate.replace(old, new).replace(
        b"exact external Ghost-extra solve exceeds the 97 GiB gate",
        b"exact external Ghost-extra solve exceeds the 192 GiB gate")
    members[GATE_PATH] = (gate, gate_mode)
    changed = (*REPLACEMENTS, GATE_PATH)
    manifest = {
        "base_sha256": BASE_SHA256,
        "schema": SCHEMA,
        "semantics": SEMANTICS,
        "replacements": [
            {"path": path, "sha256": digest(members[path][0])}
            for path in changed
        ],
    }
    members["GHOST-PRINCE-CAPACITY-V18-MANIFEST.json"] = (
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
