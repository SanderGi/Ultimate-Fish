#!/usr/bin/env python3
"""Preserve and restore-authenticate complete stateful Devil proof planes.

The historical Devil preservation path retained only the minion-free root
projection. This tool treats each generation's exact logical proof keys and
the final eight-byte WDL/DTW nodes as primary tablebase data. V1/V2 use
16-byte keys, V3 uses 14-byte keys, and V4 uses compact seven-byte keys.
Capacity padding is excluded from the preserved object while every solved
node is retained.
"""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base


SCHEMA = "ultimate-devil-stateful-partition-v1"
UNIT_PREFIX = "ultimatefish-preserve-devil-stateful-v1-square-"


def _paths(root: str, square: int) -> tuple[str, str, str, str]:
    work = f"{root.rstrip('/')}/graphs/square-{square}"
    prefix = f"{work}/devil-{square}"
    return prefix + ".keys", prefix + ".nodes", prefix + ".roots", work


def launch(instance: str, square: int, root: str) -> str:
    if square not in base.FIXED_SQUARES:
        raise ValueError("invalid fixed Devil square")
    if not root.startswith("/mnt/ultimatefish"):
        raise ValueError("retained Devil root must be on an Ultimate Fish mount")
    keys, nodes, roots, work = _paths(root, square)
    closure = f"{work}/devil-{square}.closure"
    retained = f"{work}/devil-{square}.stateful-v1.keys"
    receipt = f"{work}/devil-{square}.stateful-v1.receipt.json"
    log = f"{work}/devil-{square}.stateful-v1.preserve.log"
    restore = f"{work}/restore-stateful-v1-{square}"
    unit = f"{UNIT_PREFIX}{square}"
    s3_prefix = f"results/devil-stateful-v1/partitions/square-{square}"
    command = " && ".join([
        "set -euo pipefail",
        f"test -s {shlex.quote(keys)}",
        f"test -s {shlex.quote(nodes)}",
        f"test -s {shlex.quote(roots)}",
        f"test \"$(stat -c %s {shlex.quote(closure)})\" -eq 44",
        f"checkpoint_version=$(od -An -tu4 -j8 -N4 {shlex.quote(closure)} | tr -d ' ')",
        f"checkpoint_states=$(od -An -tu8 -j24 -N8 {shlex.quote(closure)} | tr -d ' ')",
        "case \"$checkpoint_version\" in 1|2) key_record_bytes=16;; 3) key_record_bytes=14;; 4) key_record_bytes=7;; *) exit 91;; esac",
        f"nodes_bytes=$(stat -c %s {shlex.quote(nodes)})",
        "test $((nodes_bytes % 8)) -eq 0",
        "states=$((nodes_bytes / 8))",
        "test \"$states\" -gt 0",
        "test \"$states\" -eq \"$checkpoint_states\"",
        "keys_bytes=$((states * key_record_bytes))",
        f"test \"$(stat -c %s {shlex.quote(keys)})\" -ge \"$keys_bytes\"",
        f"if test ! -s {shlex.quote(retained)} || test \"$(stat -c %s {shlex.quote(retained)})\" != \"$keys_bytes\"; then "
        f"tmp={shlex.quote(retained)}.tmp && rm -f \"$tmp\" && "
        f"head -c \"$keys_bytes\" {shlex.quote(keys)} >\"$tmp\" && "
        # Flush only this immutable prefix.  Plain `sync PATH` maps to
        # syncfs(2) on GNU coreutils and can stall behind hundreds of GiB of
        # unrelated dirty proof pages on the same persistent volume.
        f"sync -d \"$tmp\" && mv \"$tmp\" {shlex.quote(retained)}; fi",
        f"key_sha=$(sha256sum {shlex.quote(retained)} | cut -d ' ' -f1)",
        f"node_sha=$(sha256sum {shlex.quote(nodes)} | cut -d ' ' -f1)",
        f"key_key={shlex.quote(s3_prefix)}/sha256/$key_sha/devil-{square}.keys",
        f"node_key={shlex.quote(s3_prefix)}/sha256/$node_sha/devil-{square}.nodes",
        f"aws s3 cp --only-show-errors --region {base.REGION} {shlex.quote(retained)} "
        f"s3://{base.BUCKET}/\"$key_key\" --metadata "
        f"schema={SCHEMA},sha256=$key_sha,square={square},states=$states,record-bytes=$key_record_bytes,checkpoint-version=$checkpoint_version",
        f"aws s3 cp --only-show-errors --region {base.REGION} {shlex.quote(nodes)} "
        f"s3://{base.BUCKET}/\"$node_key\" --metadata "
        f"schema={SCHEMA},sha256=$node_sha,square={square},states=$states,record-bytes=8",
        f"key_version=$(aws s3api head-object --region {base.REGION} --bucket {base.BUCKET} "
        "--key \"$key_key\" --query VersionId --output text)",
        f"node_version=$(aws s3api head-object --region {base.REGION} --bucket {base.BUCKET} "
        "--key \"$node_key\" --query VersionId --output text)",
        "test -n \"$key_version\" && test \"$key_version\" != None",
        "test -n \"$node_version\" && test \"$node_version\" != None",
        f"rm -rf {shlex.quote(restore)} && install -d -m 0700 {shlex.quote(restore)}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key \"$key_key\" --version-id \"$key_version\" {shlex.quote(restore + '/keys')} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(restore + '/keys')} | cut -d ' ' -f1)\" = \"$key_sha\"",
        f"rm -f {shlex.quote(restore + '/keys')}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key \"$node_key\" --version-id \"$node_version\" {shlex.quote(restore + '/nodes')} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(restore + '/nodes')} | cut -d ' ' -f1)\" = \"$node_sha\"",
        f"rm -rf {shlex.quote(restore)}",
        "jq -n --sort-keys "
        f"--arg schema {shlex.quote(SCHEMA)} --argjson square {square} "
        "--argjson states \"$states\" --arg key_sha256 \"$key_sha\" "
        "--argjson key_record_bytes \"$key_record_bytes\" --argjson checkpoint_version \"$checkpoint_version\" "
        "--arg node_sha256 \"$node_sha\" --arg key \"$key_key\" "
        "--arg node \"$node_key\" --arg key_version_id \"$key_version\" "
        "--arg node_version_id \"$node_version\" "
        "'{schema:$schema,square:$square,states:$states,checkpoint_version:$checkpoint_version,key:{bytes:($states*$key_record_bytes),sha256:$key_sha256,s3_key:$key,version_id:$key_version_id},node:{bytes:($states*8),sha256:$node_sha256,s3_key:$node,version_id:$node_version_id},key_record_bytes:$key_record_bytes,node_record_bytes:8,restore_residual:0,never_delete:true}' "
        f">{shlex.quote(receipt)}",
        f"receipt_sha=$(sha256sum {shlex.quote(receipt)} | cut -d ' ' -f1)",
        f"receipt_key={shlex.quote(s3_prefix)}/receipts/sha256/$receipt_sha/devil-{square}.json",
        f"receipt_version=$(aws s3api put-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key \"$receipt_key\" --body {shlex.quote(receipt)} --metadata "
        f"schema={SCHEMA},sha256=$receipt_sha --query VersionId --output text)",
        f"printf '%s\\n' DEVIL_STATEFUL_PARTITION_PRESERVED square={square} states=\"$states\" "
        "key_sha256=\"$key_sha\" node_sha256=\"$node_sha\" receipt_version_id=\"$receipt_version\"",
    ])
    return base.send(instance, [
        "set -euo pipefail",
        f"systemctl is-active --quiet {unit}.service || systemd-run --quiet --collect "
        f"--unit={unit} --property=Nice=10 --property=IOSchedulingClass=best-effort "
        f"--property=IOSchedulingPriority=6 /bin/bash -lc {shlex.quote(command + ' >>' + shlex.quote(log) + ' 2>&1')}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result --no-pager",
        f"printf '%s\\n' log={shlex.quote(log)} receipt={shlex.quote(receipt)}",
    ])


def status(instance: str, square: int, root: str) -> str:
    _, _, _, work = _paths(root, square)
    unit = f"{UNIT_PREFIX}{square}"
    log = f"{work}/devil-{square}.stateful-v1.preserve.log"
    receipt = f"{work}/devil-{square}.stateful-v1.receipt.json"
    return base.send(instance, [
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result --no-pager || true",
        f"systemctl show {unit}.service -p MainPID -p CPUUsageNSec -p MemoryCurrent --no-pager || true",
        f"systemd-cgls --no-pager --unit {unit}.service 2>/dev/null || true",
        f"df -B1 {shlex.quote(work)} | tail -n 1",
        f"ls -l {shlex.quote(work + f'/devil-{square}.stateful-v1.keys')}* 2>/dev/null || true",
        f"stat -c '%n bytes=%s mtime=%Y' "
        f"{shlex.quote(work + f'/restore-stateful-v1-{square}/keys')} "
        f"{shlex.quote(work + f'/restore-stateful-v1-{square}/nodes')} "
        "2>/dev/null || true",
        f"tail -n 30 {shlex.quote(log)} || true",
        f"test ! -s {shlex.quote(receipt)} || cat {shlex.quote(receipt)}",
    ])


def stop(instance: str, square: int, root: str) -> str:
    _, _, _, work = _paths(root, square)
    unit = f"{UNIT_PREFIX}{square}"
    return base.send(instance, [
        f"systemctl stop {unit}.service || true",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result --no-pager || true",
        f"printf '%s\\n' retained_work={shlex.quote(work)}",
    ])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--stop", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--root", required=True)
    args = parser.parse_args()
    if args.launch:
        print(launch(args.instance, args.square, args.root))
    elif args.stop:
        print(stop(args.instance, args.square, args.root))
    else:
        print(status(args.instance, args.square, args.root))


if __name__ == "__main__":
    main()
