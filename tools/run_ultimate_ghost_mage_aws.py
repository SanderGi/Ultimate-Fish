#!/usr/bin/env python3
"""Run one authenticated exact Mage/Ghost AWS bundle."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import run_ultimate_reciprocal_bishop_ghost_aws as shared


def validate_manifest(manifest: dict[str, object]) -> None:
    expected_orientation = {
        "kghostmagek.uftb": "same",
        "kghostkmage.uftb": "opposing",
    }.get(manifest.get("filename"))
    if (manifest.get("schema") != "ultimate-mage-ghost-aws-v1" or
            expected_orientation is None or
            manifest.get("orientation") != expected_orientation or
            manifest.get("geometries") != 492_960 or
            manifest.get("shards") != 64 or
            manifest.get("shard_count_distribution") != {
                "7703": 32, "7702": 32} or
            manifest.get("parallelism") != 29):
        raise RuntimeError("invalid Mage/Ghost manifest")
    commands = manifest.get("commands")
    shards = commands.get("shards") if isinstance(commands, dict) else None
    if not isinstance(shards, list) or len(shards) != 64:
        raise RuntimeError("invalid Mage/Ghost shard inventory")
    cursor = 0
    for index, command in enumerate(shards):
        if not isinstance(command, list):
            raise RuntimeError("invalid Mage/Ghost shard command")
        begin = int(command[command.index("--geometry-begin") + 1])
        count = int(command[command.index("--geometry-count") + 1])
        prefix = command[command.index("--transition-prefix") + 1]
        if begin != cursor or not prefix.endswith(f"shard-{index:02d}"):
            raise RuntimeError("Mage/Ghost shards are not gap-free")
        cursor += count
    if cursor != 492_960:
        raise RuntimeError("Mage/Ghost shards do not cover the domain")
    for command_name in ("measure", "solve"):
        command = commands.get(command_name)
        if (not isinstance(command, list) or "--compact-every" not in command or
                command[command.index("--compact-every") + 1] != "1"):
            raise RuntimeError("Mage/Ghost compaction cadence is unsafe")


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
    shared.run(commands["merge"], root, work / "logs" / "merge.log")
    shared.run(commands["measure"], root, work / "logs" / "measure.log")
    if args.full:
        shared.run(commands["solve"], root, work / "logs" / "solve.log")
    shared.artifact_manifest(root, manifest)
    artifact_path = work / "artifact-manifest.json"
    artifact = json.loads(artifact_path.read_text())
    for key in ("schema", "filename", "orientation",
                "normalized_source_sha256", "lower_sidecar_sha256",
                "lower_source_sha256", "lower_model_sha256",
                "lower_observation_sha256"):
        artifact[key] = manifest[key]
    artifact_path.write_text(
        json.dumps(artifact, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
