#!/usr/bin/env python3
"""Add the parallel solve CLI switches to the authenticated Ghost v21 tree."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile


BASE_SHA256 = (
    "bcfda23c2a00580c1e4ecd08faf70447a1ae8ffe144f95bc36a074ecefbe562e"
)
BASE_PREFIX = "ultimatefish-checker-action-conditioned-v21-source/"
OUTPUT_PREFIX = "ultimatefish-penguin-worker-cli-v27-source"
DRIVER = "src/ultimate/tablebases/ghost_dragon_information_tablebase.cpp"


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
        raise RuntimeError("Ghost v21 source base hash mismatch")
    members: dict[str, tuple[bytes, int]] = {}
    with tarfile.open(fileobj=io.BytesIO(base_payload), mode="r:") as archive:
        for member in archive.getmembers():
            if not member.isfile() or member.name.startswith("._"):
                continue
            if not member.name.startswith(BASE_PREFIX):
                raise RuntimeError(f"unexpected source member {member.name}")
            extracted = archive.extractfile(member)
            if extracted is None:
                raise RuntimeError(f"cannot read {member.name}")
            relative = member.name.removeprefix(BASE_PREFIX)
            if not relative or relative in members:
                raise RuntimeError(f"duplicate/empty source member {member.name}")
            members[relative] = (extracted.read(), member.mode)

    original = members[DRIVER][0].decode()
    anchor = (
        '            else if (option == "--compact-every")\n'
        "                solve.compactEvery = std::stoul(value());\n"
        '            else if (option == "--resume-fixed-point")\n'
        "                solve.resumeFixedPoint = true;\n"
    )
    replacement = (
        '            else if (option == "--compact-every")\n'
        "                solve.compactEvery = std::stoul(value());\n"
        '            else if (option == "--workers")\n'
        "                solve.workers = std::stoul(value());\n"
        '            else if (option == "--resume-fixed-point")\n'
        "                solve.resumeFixedPoint = true;\n"
        '            else if (option == "--resume-converged")\n'
        "                solve.resumeConverged = true;\n"
    )
    if original.count(anchor) != 1:
        raise RuntimeError("parallel worker CLI anchor is not unique")
    patched = original.replace(anchor, replacement)
    members[DRIVER] = (patched.encode(), members[DRIVER][1])
    manifest = {
        "base_sha256": BASE_SHA256,
        "schema": "ultimate-penguin-worker-cli-source-v27",
        "semantics": (
            "unchanged-action-conditioned-v21-model-with-driver-only-"
            "parallel-workers-and-resume-converged-cli-routing"
        ),
        "files": [{"path": DRIVER, "sha256": digest(patched.encode())}],
    }
    members["PENGUIN-WORKER-CLI-V27-MANIFEST.json"] = (
        (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode(), 0o644)

    output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        for relative, (payload, mode) in sorted(members.items()):
            archive.addfile(
                tar_info(f"{OUTPUT_PREFIX}/{relative}", payload, mode),
                io.BytesIO(payload),
            )
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
