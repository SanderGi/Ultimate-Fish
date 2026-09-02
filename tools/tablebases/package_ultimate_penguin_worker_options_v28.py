#!/usr/bin/env python3
"""Add worker fields missing from the CLI-correct Ghost/Penguin v27 tree."""

from __future__ import annotations

import argparse
import io
import json
from pathlib import Path
import tarfile

from package_ultimate_penguin_worker_cli_v27 import digest, tar_info


BASE_SHA256 = (
    "eb8138a84cdf58a292f1eec2639c0d786500e70e6f4ed43297fab536c5373805"
)
BASE_PREFIX = "ultimatefish-penguin-worker-cli-v27-source/"
OUTPUT_PREFIX = "ultimatefish-penguin-worker-options-v28-source"
HEADER = "src/ultimate/tablebases/ghost_dragon_information_solver.h"


def build(base: Path, output: Path) -> dict[str, object]:
    base_payload = base.read_bytes()
    if digest(base_payload) != BASE_SHA256:
        raise RuntimeError("Penguin worker CLI v27 base hash mismatch")
    members: dict[str, tuple[bytes, int]] = {}
    with tarfile.open(fileobj=io.BytesIO(base_payload), mode="r:") as archive:
        for member in archive.getmembers():
            if not member.isfile():
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

    original = members[HEADER][0].decode()
    anchor = (
        "    std::uint32_t compactEvery = 1;\n"
        "    std::uint32_t measureIterations = 0;\n"
        "    bool resumeFixedPoint = false;\n"
    )
    replacement = (
        "    std::uint32_t compactEvery = 1;\n"
        "    std::uint32_t workers = 1;\n"
        "    std::uint32_t measureIterations = 0;\n"
        "    bool resumeFixedPoint = false;\n"
        "    bool resumeConverged = false;\n"
    )
    if original.count(anchor) != 1:
        raise RuntimeError("parallel worker option anchor is not unique")
    patched = original.replace(anchor, replacement)
    members[HEADER] = (patched.encode(), members[HEADER][1])
    manifest = {
        "base_sha256": BASE_SHA256,
        "schema": "ultimate-penguin-worker-options-source-v28",
        "semantics": (
            "unchanged-action-conditioned-v21-model-with-driver-and-"
            "solve-options-parallel-worker-routing"
        ),
        "files": [{"path": HEADER, "sha256": digest(patched.encode())}],
    }
    members["PENGUIN-WORKER-OPTIONS-V28-MANIFEST.json"] = (
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
