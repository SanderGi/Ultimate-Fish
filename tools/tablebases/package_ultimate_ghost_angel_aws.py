#!/usr/bin/env python3
"""Build a deterministic source-only bundle for exact Angel/Ghost solves."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile

import ultimate_information_tablebases as information


ROOT = Path(__file__).resolve().parents[2]
SCHEMA = "ultimate-ghost-angel-source-v5"
EXTRA_SOURCES = (
    ROOT / "src/ultimate/nnue.h",
    ROOT / "src/ultimate/nnue.cpp",
    ROOT / "tools/tablebases/ultimate_information_tablebases.py",
    ROOT / "tools/tablebases/run_ultimate_reciprocal_bishop_ghost_aws.py",
    ROOT / "tools/tablebases/run_ultimate_ghost_ordinary_aws.py",
)


def sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def archive_info(name: str, payload: bytes) -> tarfile.TarInfo:
    value = tarfile.TarInfo(name)
    value.size = len(payload)
    value.mode = 0o644
    value.mtime = value.uid = value.gid = 0
    value.uname = value.gname = ""
    return value


def build(output: Path) -> dict[str, object]:
    paths = set(information.ANGEL_GHOST_SOLVER_SOURCES) | set(EXTRA_SOURCES)
    members = {
        path.relative_to(ROOT).as_posix(): path.read_bytes()
        for path in paths
    }
    manifest = {
        "schema": SCHEMA,
        "semantics": (
            "perfect-recall-hidden-ghost-angel-attachment-"
            "forced-action-attached-child-link-symmetry-terminal-v5"
        ),
        "models": {
            filename: information.solver_model_fingerprint(filename)
            for filename in ("kghostangelk.uftb", "kghostkangel.uftb")
        },
        "files": [
            {"path": name, "bytes": len(payload), "sha256": sha256(payload)}
            for name, payload in sorted(members.items())
        ],
    }
    members["bundle-manifest.json"] = (
        json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        for name, payload in sorted(members.items()):
            archive.addfile(archive_info(name, payload), io.BytesIO(payload))
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    manifest = build(args.output)
    print(json.dumps({"archive": str(args.output),
                      "sha256": sha256(args.output.read_bytes()),
                      "manifest": manifest}, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
