#!/usr/bin/env python3
"""Repack Checker v19 with forced filtering and exact witness diagnostics."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile


ROOT = Path(__file__).resolve().parents[2]
BASE_SHA256 = "d8f3180c406ef51631a7f72b042fadbfe1c307fbf4c3b9afeab4a64f226d996f"
PREFIX = "ultimatefish-checker-exact-witness-diagnostic-v25-source"
GHOST_PATH = "src/ultimate/tablebases/ghost_extra_information_tablebase.cpp"


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
    payload = base.read_bytes()
    if digest(payload) != BASE_SHA256:
        raise RuntimeError("Checker v19 source base hash mismatch")
    members: dict[str, tuple[bytes, int]] = {}
    with tarfile.open(fileobj=io.BytesIO(payload), mode="r:") as archive:
        for member in archive.getmembers():
            if not member.isfile():
                continue
            source = archive.extractfile(member)
            if source is None:
                raise RuntimeError(f"cannot read {member.name}")
            members[member.name.split("/", 1)[1]] = (source.read(), member.mode)

    singleton_anchor = "    [[nodiscard]] std::uint64_t verify_singletons() {"
    end_anchor = "\n    void report_fresh_roots() {"
    legacy = members[GHOST_PATH][0].decode()
    current = (ROOT / GHOST_PATH).read_text()
    legacy_begin = legacy.index(singleton_anchor)
    legacy_end = legacy.index(end_anchor, legacy_begin)
    current_begin = current.index(singleton_anchor)
    current_end = current.index(end_anchor, current_begin)
    replacement = current[current_begin:current_end].replace(
        "valid_world(state, material_)", "valid_world(state)")
    updated = legacy[:legacy_begin] + replacement + legacy[legacy_end:]
    members[GHOST_PATH] = (updated.encode(), 0o644)
    manifest = {
        "base_sha256": BASE_SHA256,
        "schema": "ultimate-checker-exact-witness-diagnostic-source-v25",
        "semantics": (
            "checker-v19-exact-action-conditioned-singleton-dominance-"
            "impossible-forced-continuation-padding-exclusion-child-force-"
            "exact-witness-diagnostic-v25"
        ),
        "replacements": [
            {"path": GHOST_PATH, "sha256": digest(updated.encode())}
        ],
    }
    members["CHECKER-EXACT-WITNESS-DIAGNOSTIC-V25-MANIFEST.json"] = (
        (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode(),
        0o644,
    )
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        for relative, (member_payload, mode) in sorted(members.items()):
            archive.addfile(tar_info(f"{PREFIX}/{relative}", member_payload,
                                     mode), io.BytesIO(member_payload))
    return {"archive": str(output), "sha256": digest(output.read_bytes()),
            "bytes": output.stat().st_size, "manifest": manifest}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.base, args.output), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
