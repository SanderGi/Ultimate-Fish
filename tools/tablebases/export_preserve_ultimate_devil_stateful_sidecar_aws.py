#!/usr/bin/env python3
"""Export, version-preserve, and restore-authenticate a Devil UFDS sidecar."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base
from launch_ultimate_devil_stateful_recompute_aws import parse_cpus


SOURCE_SHA256 = "688397a286e4564b01a05b9cbf7c2d33d84306c96e4ffbbdf74d60f830ee21a6"
BINARY_SHA256 = "0921800f11baab74d1587f429044524a88b6568d7d71e30048d1aa8aafbc64e1"
BINARY_VERSION = "RyUMFdLdDL_HDOXtPy0IvY_od0bCAqY_"
BINARY_KEY = ("sources/devil-stateful-sidecar-exporter-v4/binary/sha256/"
              f"{BINARY_SHA256}/ultimate_devil_stateful_sidecar_exporter")
SCHEMA = "ultimate-devil-stateful-sidecar-v1"
UNIT_PREFIX = "ultimatefish-export-preserve-devil-stateful-v3-square-"


def launch(instance: str, square: int, root: str, cpus: str,
           memory_high: int = 17179869184,
           memory_max: int = 25769803776) -> str:
    parsed = parse_cpus(cpus)
    if memory_high <= 0 or memory_max < memory_high:
        raise ValueError("invalid sidecar memory gates")
    if square not in base.FIXED_SQUARES:
        raise ValueError("invalid fixed Devil square")
    if not (root.startswith("/mnt/ultimatefish") or
            root.startswith("/mnt/checkpoint-migrate")):
        raise ValueError("retained root must be on an Ultimate Fish mount")
    work = f"{root.rstrip('/')}/graphs/square-{square}"
    prefix = f"{work}/devil-{square}"
    keys, nodes = prefix + ".keys", prefix + ".nodes"
    closure = prefix + ".closure"
    output = prefix + ".ufds"
    receipt = prefix + ".ufds.receipt.json"
    log = prefix + ".ufds.export.log"
    stage = f"{root.rstrip('/')}/stateful-sidecar-exporter-v4"
    binary = f"{stage}/ultimate_devil_stateful_sidecar_exporter"
    restore = f"{work}/devil-{square}.ufds.restore"
    unit = f"{UNIT_PREFIX}{square}"
    cpu_set = ",".join(map(str, parsed))
    s3_prefix = f"results/devil-stateful-v1/sidecars/square-{square}"
    command = " && ".join([
        "set -euo pipefail",
        f"test -s {shlex.quote(keys)}",
        f"test -s {shlex.quote(nodes)}",
        f"test \"$(stat -c %s {shlex.quote(closure)})\" -eq 44",
        f"checkpoint_version=$(od -An -tu4 -j8 -N4 {shlex.quote(closure)} | tr -d ' ')",
        "case \"$checkpoint_version\" in 1|2) key_record_bytes=16;; 3) key_record_bytes=14;; 4) key_record_bytes=7;; *) exit 91;; esac",
        f"if test ! -s {shlex.quote(output)}; then {shlex.quote(binary)} "
        f"--keys {shlex.quote(keys)} --nodes {shlex.quote(nodes)} "
        f"--output {shlex.quote(output)} --square {square} --workers {len(parsed)} "
        "--key-record-bytes \"$key_record_bytes\"; fi",
        f"sidecar_sha=$(sha256sum {shlex.quote(output)} | cut -d ' ' -f1)",
        f"sidecar_bytes=$(stat -c %s {shlex.quote(output)})",
        f"sidecar_key={shlex.quote(s3_prefix)}/sha256/$sidecar_sha/devil-{square}.ufds",
        f"aws s3 cp --only-show-errors --region {base.REGION} {shlex.quote(output)} "
        f"s3://{base.BUCKET}/\"$sidecar_key\" --metadata "
        f"schema={SCHEMA},sha256=$sidecar_sha,square={square},source-sha256={SOURCE_SHA256},binary-sha256={BINARY_SHA256}",
        f"sidecar_version=$(aws s3api head-object --region {base.REGION} --bucket {base.BUCKET} "
        "--key \"$sidecar_key\" --query VersionId --output text)",
        f"rm -f {shlex.quote(restore)}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key \"$sidecar_key\" --version-id \"$sidecar_version\" {shlex.quote(restore)} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(restore)} | cut -d ' ' -f1)\" = \"$sidecar_sha\"",
        f"rm -f {shlex.quote(restore)}",
        "states=$(( (sidecar_bytes - 32) / 10 ))",
        "test $((32 + states * 10)) -eq \"$sidecar_bytes\"",
        "jq -n --sort-keys "
        f"--arg schema {shlex.quote(SCHEMA)} --argjson square {square} "
        "--argjson states \"$states\" --argjson bytes \"$sidecar_bytes\" "
        "--arg sha256 \"$sidecar_sha\" --arg s3_key \"$sidecar_key\" "
        "--arg version_id \"$sidecar_version\" "
        f"--arg source_sha256 {SOURCE_SHA256} --arg binary_sha256 {BINARY_SHA256} "
        "--argjson key_record_bytes \"$key_record_bytes\" --argjson checkpoint_version \"$checkpoint_version\" "
        "'{schema:$schema,square:$square,states:$states,bytes:$bytes,sha256:$sha256,s3_key:$s3_key,version_id:$version_id,source_sha256:$source_sha256,binary_sha256:$binary_sha256,key_record_bytes:$key_record_bytes,checkpoint_version:$checkpoint_version,restore_residual:0,sorted_key_residual:0,duplicate_key_residual:0,never_delete:true}' "
        f">{shlex.quote(receipt)}",
        f"receipt_sha=$(sha256sum {shlex.quote(receipt)} | cut -d ' ' -f1)",
        f"receipt_key={shlex.quote(s3_prefix)}/receipts/sha256/$receipt_sha/devil-{square}.json",
        f"receipt_version=$(aws s3api put-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key \"$receipt_key\" --body {shlex.quote(receipt)} --metadata "
        f"schema={SCHEMA},sha256=$receipt_sha --query VersionId --output text)",
        f"printf '%s\\n' DEVIL_STATEFUL_SIDECAR_PRESERVED square={square} "
        "states=\"$states\" sha256=\"$sidecar_sha\" version_id=\"$sidecar_version\" "
        "receipt_version_id=\"$receipt_version\"",
    ])
    return base.send(instance, [
        "set -euo pipefail",
        f"install -d -m 0755 {shlex.quote(stage)}",
        f"if test ! -s {shlex.quote(binary)}; then aws s3api get-object "
        f"--region {base.REGION} --bucket {base.BUCKET} --key {shlex.quote(BINARY_KEY)} "
        f"--version-id {shlex.quote(BINARY_VERSION)} {shlex.quote(binary)} >/dev/null; fi",
        f"test \"$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)\" = {BINARY_SHA256}",
        f"chmod 0755 {shlex.quote(binary)}",
        f"systemctl is-active --quiet {unit}.service || systemd-run --quiet --collect "
        f"--unit={unit} --property=AllowedCPUs={cpu_set} --property=Nice=10 "
        f"--property=MemoryHigh={memory_high} --property=MemoryMax={memory_max} "
        f"/bin/bash -lc {shlex.quote(command + ' >>' + shlex.quote(log) + ' 2>&1')}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "-p AllowedCPUs --no-pager",
    ])


def status(instance: str, square: int, root: str) -> str:
    work = f"{root.rstrip('/')}/graphs/square-{square}"
    prefix = f"{work}/devil-{square}"
    unit = f"{UNIT_PREFIX}{square}"
    return base.send(instance, [
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result --no-pager || true",
        f"systemctl show {unit}.service -p MainPID -p CPUUsageNSec -p MemoryCurrent --no-pager || true",
        f"pid=$(systemctl show {unit}.service -p MainPID --value); "
        "test \"$pid\" = 0 || ps -p \"$pid\" -o pid=,etimes=,time=,pcpu=,pmem=,stat=,cmd=",
        f"systemd-cgls --no-pager --unit {unit}.service 2>/dev/null || true",
        f"tail -n 30 {shlex.quote(prefix + '.ufds.export.log')} || true",
        f"printf 'keys_head '; od -An -tx1 -N 28 {shlex.quote(prefix + '.keys')} || true",
        f"printf 'nodes_head '; od -An -tx1 -N 32 {shlex.quote(prefix + '.nodes')} || true",
        f"du -ch {shlex.quote(prefix)}.ufds* 2>/dev/null | tail -n 5 || true",
        f"printf 'bucket_files '; find {shlex.quote(work)} -maxdepth 1 -type f "
        f"-name {shlex.quote('devil-' + str(square) + '.ufds.bucket-*.tmp')} | wc -l",
        f"stat -c '%n bytes=%s mtime=%Y' {shlex.quote(prefix)}.ufds.tmp "
        f"{shlex.quote(prefix)}.ufds {shlex.quote(prefix)}.ufds.restore "
        "2>/dev/null || true",
        f"ls -l {shlex.quote(root.rstrip('/') + '/stateful-sidecar-exporter-v4')} 2>/dev/null || true",
        f"test ! -s {shlex.quote(prefix + '.ufds.receipt.json')} || "
        f"cat {shlex.quote(prefix + '.ufds.receipt.json')}",
    ])


def stop(instance: str, square: int, root: str) -> str:
    work = f"{root.rstrip('/')}/graphs/square-{square}"
    unit = f"{UNIT_PREFIX}{square}"
    return base.send(instance, [
        f"systemctl stop {unit}.service || true",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result --no-pager || true",
        f"df -B1 {shlex.quote(work)} | tail -n 1",
    ])


def clear_scratch(instance: str, square: int, root: str) -> str:
    work = f"{root.rstrip('/')}/graphs/square-{square}"
    prefix = f"{work}/devil-{square}.ufds"
    unit = f"{UNIT_PREFIX}{square}"
    return base.send(instance, [
        f"test \"$(systemctl is-active {unit}.service || true)\" != active",
        f"test ! -e {shlex.quote(prefix)}",
        f"test ! -e {shlex.quote(prefix + '.receipt.json')}",
        f"before=$(df -B1 --output=avail {shlex.quote(work)} | tail -n 1)",
        f"find {shlex.quote(work)} -maxdepth 1 -type f "
        f"-name {shlex.quote('devil-' + str(square) + '.ufds.bucket-*.tmp')} -delete",
        f"rm -f {shlex.quote(prefix + '.tmp')}",
        f"after=$(df -B1 --output=avail {shlex.quote(work)} | tail -n 1)",
        "printf '%s\\n' DEVIL_SIDECAR_SCRATCH_CLEARED bytes=$((after-before))",
    ])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--stop", action="store_true")
    mode.add_argument("--clear-scratch", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--root", required=True)
    parser.add_argument("--cpus", default="0")
    parser.add_argument("--memory-high", type=int, default=17179869184)
    parser.add_argument("--memory-max", type=int, default=25769803776)
    args = parser.parse_args()
    if args.launch:
        print(launch(args.instance, args.square, args.root, args.cpus,
                     args.memory_high, args.memory_max))
    elif args.stop:
        print(stop(args.instance, args.square, args.root))
    elif args.clear_scratch:
        print(clear_scratch(args.instance, args.square, args.root))
    else:
        print(status(args.instance, args.square, args.root))


if __name__ == "__main__":
    main()
