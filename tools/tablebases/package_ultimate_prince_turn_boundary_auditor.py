#!/usr/bin/env python3
"""Build a deterministic source bundle for the Prince boundary auditor."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import sys
import tarfile


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import run_ultimate_concrete_tablebase_shard_aws as concrete  # noqa: E402


SCHEMA = "ultimate-prince-turn-boundary-auditor-source-v1"
SEMANTICS = "tablebase-includes-continuations-ledger-reports-cont0-only-v1"


def sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def tar_info(name: str, payload: bytes) -> tarfile.TarInfo:
    value = tarfile.TarInfo(name)
    value.size = len(payload)
    value.mode = 0o644
    value.mtime = value.uid = value.gid = 0
    value.uname = value.gname = ""
    return value


def build(output: Path) -> dict[str, object]:
    members = {name: (ROOT / name).read_bytes()
               for name in concrete.MODEL_SOURCES}
    manifest = {
        "schema": SCHEMA,
        "semantics": SEMANTICS,
        "generator_model_sha256": concrete.generator_model_sha256(),
        "inventory_sha256": concrete.inventory_sha256(),
        "files": [{"path": name, "bytes": len(payload),
                   "sha256": sha256(payload)}
                  for name, payload in sorted(members.items())],
    }
    members["bundle-manifest.json"] = (
        json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        for name, payload in sorted(members.items()):
            archive.addfile(tar_info(name, payload), io.BytesIO(payload))
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
