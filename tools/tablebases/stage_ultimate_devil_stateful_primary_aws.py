#!/usr/bin/env python3
"""Restore-authenticate one preserved Devil primary plane onto fleet scratch."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shlex

import launch_ultimate_devil_spawned_solver_aws as base


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "tools/tablebases/ultimate_devil_stateful_recovery.json"


def partition(square: int) -> dict[str, object]:
    data = json.loads(MANIFEST.read_text())
    matches = [row for row in data["partitions"] if row["square"] == square]
    if len(matches) != 1 or matches[0]["status"] != "PRESERVED":
        raise ValueError("square is not an exactly preserved stateful partition")
    row = matches[0]
    for name in ("states", "checkpoint_version", "key_record_bytes",
                 "key_sha256", "key_version_id", "node_sha256",
                 "node_version_id", "receipt_version_id"):
        if not row.get(name):
            raise ValueError(f"preserved partition lacks {name}")
    return row


def stage(instance: str, square: int, volume: str) -> str:
    if not (volume == "/mnt/checkpoint-migrate" or
            volume.startswith("/mnt/ultimatefish")):
        raise ValueError("stage volume is outside the authorized tablebase mounts")
    row = partition(square)
    states = int(row["states"])
    checkpoint_version = int(row["checkpoint_version"])
    key_record_bytes = int(row["key_record_bytes"])
    expected_width = {1: 16, 2: 16, 3: 14, 4: 7}.get(checkpoint_version)
    if expected_width != key_record_bytes:
        raise ValueError("checkpoint generation/key-width mismatch")
    root = f"{volume.rstrip('/')}/devil-stateful-restored-v1"
    work = f"{root}/graphs/square-{square}"
    prefix = f"{work}/devil-{square}"
    key_path, node_path = prefix + ".keys", prefix + ".nodes"
    closure = prefix + ".closure"
    receipt = prefix + ".restore.json"
    key_sha, node_sha = str(row["key_sha256"]), str(row["node_sha256"])
    key_key = (f"results/devil-stateful-v1/partitions/square-{square}/"
               f"sha256/{key_sha}/devil-{square}.keys")
    node_key = (f"results/devil-stateful-v1/partitions/square-{square}/"
                f"sha256/{node_sha}/devil-{square}.nodes")
    version_bytes = "".join(f"\\{byte:03o}" for byte in
                            checkpoint_version.to_bytes(4, "little"))
    return base.send(instance, [
        "set -euo pipefail",
        f"install -d -m 0755 {shlex.quote(work)}",
        f"if test ! -s {shlex.quote(key_path)}; then aws s3api get-object "
        f"--region {base.REGION} --bucket {base.BUCKET} --key {shlex.quote(key_key)} "
        f"--version-id {shlex.quote(str(row['key_version_id']))} "
        f"{shlex.quote(key_path)} >/dev/null; fi",
        f"test \"$(stat -c %s {shlex.quote(key_path)})\" -eq {states * key_record_bytes}",
        f"test \"$(sha256sum {shlex.quote(key_path)} | cut -d ' ' -f1)\" = {key_sha}",
        f"if test ! -s {shlex.quote(node_path)}; then aws s3api get-object "
        f"--region {base.REGION} --bucket {base.BUCKET} --key {shlex.quote(node_key)} "
        f"--version-id {shlex.quote(str(row['node_version_id']))} "
        f"{shlex.quote(node_path)} >/dev/null; fi",
        f"test \"$(stat -c %s {shlex.quote(node_path)})\" -eq {states * 8}",
        f"test \"$(sha256sum {shlex.quote(node_path)} | cut -d ' ' -f1)\" = {node_sha}",
        f"truncate -s 44 {shlex.quote(closure)}",
        f"printf '{version_bytes}' | dd of={shlex.quote(closure)} bs=1 seek=8 "
        "conv=notrunc status=none",
        "jq -n --sort-keys "
        f"--argjson square {square} --argjson states {states} "
        f"--argjson checkpoint_version {checkpoint_version} "
        f"--argjson key_record_bytes {key_record_bytes} "
        f"--arg key_sha256 {key_sha} --arg node_sha256 {node_sha} "
        f"--arg key_version_id {shlex.quote(str(row['key_version_id']))} "
        f"--arg node_version_id {shlex.quote(str(row['node_version_id']))} "
        f"--arg primary_receipt_version_id {shlex.quote(str(row['receipt_version_id']))} "
        "'{schema:\"ultimate-devil-stateful-primary-restore-v1\",square:$square,"
        "states:$states,checkpoint_version:$checkpoint_version,"
        "key_record_bytes:$key_record_bytes,key_sha256:$key_sha256,"
        "node_sha256:$node_sha256,key_version_id:$key_version_id,"
        "node_version_id:$node_version_id,"
        "primary_receipt_version_id:$primary_receipt_version_id,restore_residual:0}' "
        f">{shlex.quote(receipt)}",
        f"cat {shlex.quote(receipt)}",
    ], timeout=1800)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--volume", required=True)
    args = parser.parse_args()
    print(stage(args.instance, args.square, args.volume))


if __name__ == "__main__":
    main()
