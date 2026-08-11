#!/usr/bin/env python3
"""Run one authenticated exact Bomb/Ghost AWS bundle."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import run_ultimate_reciprocal_bishop_ghost_aws as shared


def merged_transition_is_complete(root: Path, command: list[str]) -> bool:
    prefix = root / shared.transition_prefix(command)
    return (all(Path(f"{prefix}{suffix}").is_file()
                for suffix in shared.TRANSITION_SUFFIXES) and
            (root / "work" / "logs" / "merge.log").is_file())


def validate_manifest(manifest: dict[str, object]) -> None:
    expected_orientation = {
        "kbombghostk.uftb": "same",
        "kbombkghost.uftb": "opposing",
    }.get(manifest.get("filename"))
    implementation = manifest.get("implementation_sha256")
    if (manifest.get("schema") != "ultimate-bomb-ghost-aws-v2" or
            expected_orientation is None or
            manifest.get("orientation") != expected_orientation or
            not isinstance(implementation, str) or
            len(implementation) != 64 or
            any(character not in "0123456789abcdef"
                for character in implementation) or
            manifest.get("geometries") != 492_960 or
            manifest.get("shards") != 64 or
            manifest.get("shard_count_distribution") != {
                "7703": 32, "7702": 32} or
            manifest.get("parallelism") != 29):
        raise RuntimeError("invalid Bomb/Ghost manifest")
    shards = manifest.get("commands", {}).get("shards")
    if not isinstance(shards, list) or len(shards) != 64:
        raise RuntimeError("invalid Bomb/Ghost shard inventory")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path,
                        default=Path("bundle-manifest.json"))
    parser.add_argument("--full", action="store_true",
                        help="continue after measurement to the exact solve")
    args = parser.parse_args()
    root = args.manifest.resolve().parent
    manifest = json.loads(args.manifest.read_text())
    validate_manifest(manifest)
    shared.verify_inputs(root, manifest)
    shared.prepare_workdirs(root)
    work = root / "work"
    commands = manifest["commands"]
    shared.run(commands["build"], root, work / "logs" / "build.log")
    shared.run(commands["self_test"], root,
               work / "logs" / "self-test.log")
    shared.run_ranges(commands["shards"], root,
                      int(manifest["parallelism"]))
    if not merged_transition_is_complete(root, commands["merge"]):
        shared.run(commands["merge"], root, work / "logs" / "merge.log")
    shared.run(commands["measure"], root, work / "logs" / "measure.log")
    if args.full:
        shared.run(commands["solve"], root, work / "logs" / "solve.log")
    shared.artifact_manifest(root, manifest)
    artifact_path = work / "artifact-manifest.json"
    artifact = json.loads(artifact_path.read_text())
    for key in ("schema", "filename", "orientation",
                "implementation_sha256",
                "normalized_source_sha256", "lower_bomb_full_sha256",
                "lower_bomb_source_sha256",
                "lower_bomb_model_sha256"):
        artifact[key] = manifest[key]
    artifact_path.write_text(
        json.dumps(artifact, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
