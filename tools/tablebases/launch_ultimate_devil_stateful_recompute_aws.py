#!/usr/bin/env python3
"""Launch retained stateful lone-Devil partition recomputation.

Every run keeps the canonical keys and final WDL/DTW nodes.  Completion is
only the trigger for ``preserve_ultimate_devil_stateful_partition_aws.py``;
the historical root fragment is a regression slice, never the primary result.
"""

from __future__ import annotations

import argparse
import re
import shlex

import launch_ultimate_devil_spawned_solver_aws as base


SOURCE_SHA256 = "368e7230e5e2c82604b8be1d019311eaa4c344e96c04bc08db249fe9fe24915a"
SOURCE_VERSION = "KdT.6Hl.gP9V8cAqIwoB8CVP1LbO6C4X"
SOURCE_KEY = (f"sources/bundles/devil-spawned-solver-v29/sha256/{SOURCE_SHA256}/"
              "ultimatefish-devil-spawned-solver-v29-source.tar")
BINARY_SHA256 = "acc758ed81e56fba827c1e6ecfccfb464bfcdf8823ce35540cbb7cd3144c7df6"
BINARY_VERSION = "FSOGj.7mJWT3bEsBuXfi_8gvK43gCoSy"
BINARY_KEY = (f"sources/binaries/devil-spawned-solver-v29/sha256/{BINARY_SHA256}/"
              "ultimate_tablebase-devil-spawned-v29")
UNIT_PREFIX = "ultimatefish-devil-stateful-recompute-v1-square-"


def parse_cpus(value: str) -> tuple[int, ...]:
    result: set[int] = set()
    for item in value.split(","):
        match = re.fullmatch(r"([0-9]+)(?:-([0-9]+))?", item)
        if not match:
            raise ValueError("invalid CPU set")
        first = int(match.group(1))
        last = int(match.group(2) or first)
        if first > last or last >= 64:
            raise ValueError("CPU set exceeds the authorized fleet geometry")
        result.update(range(first, last + 1))
    if not result:
        raise ValueError("empty CPU set")
    return tuple(sorted(result))


def launch(instance: str, square: int, cpus: str, volume: str,
           state_limit: int, hash_capacity: int,
           memory_high: int, memory_max: int) -> str:
    parsed = parse_cpus(cpus)
    if square not in base.FIXED_SQUARES:
        raise ValueError("invalid fixed Devil square")
    if not volume.startswith("/mnt/ultimatefish"):
        raise ValueError("work volume must be an Ultimate Fish mount")
    if not 0 < hash_capacity <= state_limit < (1 << 40):
        raise ValueError("invalid Devil state/hash limits")
    if not 8 << 30 <= memory_high <= memory_max:
        raise ValueError("invalid memory gates")
    stage = f"{volume.rstrip('/')}/devil-stateful-recompute-v1/stage-v29"
    root = f"{volume.rstrip('/')}/devil-stateful-recompute-v1"
    work = f"{root}/graphs/square-{square}"
    log = f"{root}/logs/square-{square}.log"
    source = f"{stage}/source.tar"
    binary = f"{stage}/ultimate_tablebase-devil-spawned-v29"
    unit = f"{UNIT_PREFIX}{square}"
    cpu_set = ",".join(map(str, parsed))
    command = (
        f"exec {shlex.quote(binary)} --piece devil --workers {len(parsed)} "
        f"--solve-devil-spawned-square {square} "
        f"--devil-spawned-limit {state_limit} "
        f"--devil-spawned-hash-capacity {hash_capacity} "
        f"--devil-spawned-work {shlex.quote(work)} >>{shlex.quote(log)} 2>&1"
    )
    commands = [
        "set -euo pipefail",
        f"install -d -m 0755 {shlex.quote(stage)} {shlex.quote(work)} "
        f"{shlex.quote(root + '/logs')}",
        f"if test ! -s {shlex.quote(source)}; then aws s3api get-object "
        f"--region {base.REGION} --bucket {base.BUCKET} --key {shlex.quote(SOURCE_KEY)} "
        f"--version-id {shlex.quote(SOURCE_VERSION)} {shlex.quote(source)} >/dev/null; fi",
        f"test \"$(sha256sum {shlex.quote(source)} | cut -d ' ' -f1)\" = {SOURCE_SHA256}",
        f"if test ! -s {shlex.quote(binary)}; then aws s3api get-object "
        f"--region {base.REGION} --bucket {base.BUCKET} --key {shlex.quote(BINARY_KEY)} "
        f"--version-id {shlex.quote(BINARY_VERSION)} {shlex.quote(binary)} >/dev/null; fi",
        f"test \"$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)\" = {BINARY_SHA256}",
        f"chmod 0755 {shlex.quote(binary)}",
        f"test ! -s {shlex.quote(work + f'/devil-{square}.roots')}",
        f"systemctl is-active --quiet {unit}.service || systemd-run --quiet --collect "
        f"--unit={unit} --property=AllowedCPUs={cpu_set} --property=Nice=5 "
        f"--property=MemoryHigh={memory_high} --property=MemoryMax={memory_max} "
        "--property=OOMPolicy=stop --setenv=ULTIMATE_TABLEBASE_PRESERVE_SCRATCH=1 "
        f"/bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "-p AllowedCPUs -p MemoryHigh -p MemoryMax --no-pager",
        f"printf '%s\\n' source_sha256={SOURCE_SHA256} source_version={SOURCE_VERSION} "
        f"binary_sha256={BINARY_SHA256} binary_version={BINARY_VERSION} "
        f"primary_result={shlex.quote(work + f'/devil-{square}.keys')} "
        f"primary_values={shlex.quote(work + f'/devil-{square}.nodes')}",
    ]
    return base.send(instance, commands, timeout=300)


def status(instance: str, square: int, volume: str) -> str:
    root = f"{volume.rstrip('/')}/devil-stateful-recompute-v1"
    work = f"{root}/graphs/square-{square}"
    unit = f"{UNIT_PREFIX}{square}"
    return base.send(instance, [
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "-p AllowedCPUs -p MemoryCurrent --no-pager || true",
        f"cat /sys/fs/cgroup/system.slice/{unit}.service/memory.events || true",
        f"systemctl status {unit}.service --no-pager --lines=5 || true",
        f"tail -n 30 {shlex.quote(root + f'/logs/square-{square}.log')} || true",
        f"find {shlex.quote(work)} -maxdepth 1 -type f -printf '%f %s\\n' | sort || true",
    ])


def tune(instance: str, square: int, cpus: str, memory_high: int,
         memory_max: int) -> str:
    """Raise an active unit's resource gates without losing its checkpoint."""
    parsed = parse_cpus(cpus)
    if square not in base.FIXED_SQUARES:
        raise ValueError("invalid fixed Devil square")
    if not 8 << 30 <= memory_high <= memory_max:
        raise ValueError("invalid memory gates")
    unit = f"{UNIT_PREFIX}{square}.service"
    cpu_set = ",".join(map(str, parsed))
    return base.send(instance, [
        "set -euo pipefail",
        f"systemctl is-active --quiet {unit}",
        f"systemctl set-property --runtime {unit} AllowedCPUs={cpu_set} "
        f"MemoryHigh={memory_high} MemoryMax={memory_max}",
        f"systemctl show {unit} -p ActiveState -p AllowedCPUs -p MemoryCurrent "
        "-p MemoryHigh -p MemoryMax --no-pager",
        f"cat /sys/fs/cgroup/system.slice/{unit}/memory.events",
    ])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--tune", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--volume", default="/mnt/ultimatefish")
    parser.add_argument("--cpus", default="0")
    parser.add_argument("--state-limit", type=int, default=500_000_000)
    parser.add_argument("--hash-capacity", type=int, default=500_000_000)
    parser.add_argument("--memory-high", type=int, default=64 << 30)
    parser.add_argument("--memory-max", type=int, default=72 << 30)
    args = parser.parse_args()
    if args.launch:
        print(launch(args.instance, args.square, args.cpus, args.volume,
                     args.state_limit, args.hash_capacity,
                     args.memory_high, args.memory_max))
    elif args.tune:
        print(tune(args.instance, args.square, args.cpus,
                   args.memory_high, args.memory_max))
    else:
        print(status(args.instance, args.square, args.volume))


if __name__ == "__main__":
    main()
