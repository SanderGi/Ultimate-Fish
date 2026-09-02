#!/usr/bin/env python3
"""Resume a migrated packed-key Devil companion checkpoint under v14."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base
import launch_ultimate_devil_companion_solver_v11_aws as v11
import migrate_ultimate_devil_companion_v14_aws as migration


MEMORY_HIGH = v11.MEMORY_HIGH
MEMORY_MAX = v11.MEMORY_MAX


def names(piece: str, opposed: bool, square: int, volume: str) -> tuple[str, ...]:
    orientation = "opposed" if opposed else "same"
    root = f"{volume.rstrip('/')}/devil-companion-v14-{migration.SOURCE_SHA256[:8]}"
    work = f"{root}/{orientation}-{piece}/square-{square}"
    unit = f"ultimatefish-devil-companion-v14-{orientation}-{piece}-square-{square}"
    log = f"{root}/logs/{orientation}-{piece}-square-{square}.log"
    source_unit, _, _ = v11.names(piece, opposed, square, volume)
    return unit, work, log, source_unit


def resume(instance: str, piece: str, opposed: bool, square: int,
           cpus: tuple[int, ...], limit: int, volume: str) -> str:
    if piece not in v11.STATELESS or square not in base.FIXED_SQUARES:
        raise ValueError("invalid v14 companion resume material")
    if not cpus or len(set(cpus)) != len(cpus):
        raise ValueError("v14 companion CPU allocation must be nonempty and unique")
    if limit <= 0 or limit >= 2**63 or not volume.startswith("/mnt/ultimatefish"):
        raise ValueError("invalid v14 companion resume resource")
    unit, work, log, source_unit = names(piece, opposed, square, volume)
    binary = (
        f"{migration.ROOT}/ultimate_tablebase-devil-spawned-v14"
    )
    orientation = " --opposing" if opposed else ""
    cpu_set = ",".join(map(str, cpus))
    command = (
        f"exec {shlex.quote(binary)} --piece devil --piece2 {piece}{orientation} "
        f"--workers {len(cpus)} --solve-devil-spawned-square {square} "
        f"--devil-spawned-limit {limit} --devil-spawned-work "
        f"{shlex.quote(work)} >>{shlex.quote(log)} 2>&1"
    )
    prefix = f"{work}/devil-{square}"
    commands = [
        "set -euo pipefail",
        *migration.stage_commands(),
        f"! systemctl is-active --quiet {source_unit}.service",
        f"test -s {shlex.quote(prefix + '.closure')} "
        f"&& test -s {shlex.quote(prefix + '.keys')} "
        f"&& test -s {shlex.quote(prefix + '.frontier')}",
        f"test ! -e {shlex.quote(prefix + '.roots')}",
        f"systemctl is-active --quiet {unit}.service || systemd-run --quiet "
        f"--collect --unit={unit} --property=AllowedCPUs={cpu_set} "
        f"--property=Nice=5 --property=MemoryHigh={MEMORY_HIGH} "
        f"--property=MemoryMax={MEMORY_MAX} --property=OOMPolicy=stop "
        "--setenv=ULTIMATE_TABLEBASE_PRESERVE_SCRATCH=1 "
        f"/bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "-p AllowedCPUs -p MemoryHigh -p MemoryMax --no-pager",
        f"printf '%s\\n' source_sha256={migration.SOURCE_SHA256} "
        f"source_version={migration.SOURCE_VERSION} "
        f"binary_sha256={migration.BINARY_SHA256} "
        f"binary_version={migration.BINARY_VERSION}",
    ]
    return base.send(instance, commands, timeout=300)


def status(instance: str, piece: str, opposed: bool, square: int,
           volume: str) -> str:
    unit, work, log, source_unit = names(piece, opposed, square, volume)
    return base.send(instance, [
        f"systemctl status {unit}.service --no-pager -l || true",
        f"systemctl show {source_unit}.service -p ActiveState -p SubState "
        "-p Result --no-pager || true",
        f"tail -n 40 {shlex.quote(log)} || true",
        f"du -sh {shlex.quote(work)} || true",
    ], timeout=300)


def profile(instance: str, piece: str, opposed: bool, square: int,
            volume: str) -> str:
    unit, _, _, _ = names(piece, opposed, square, volume)
    return base.send(instance, [
        "set -euo pipefail",
        f"pid=$(systemctl show {unit}.service -p MainPID --value)",
        "test \"$pid\" -gt 1",
        f"device=$(findmnt -n -o SOURCE {shlex.quote(volume)})",
        "name=$(basename \"$(readlink -f \"$device\")\")",
        "printf 'pid=%s device=%s name=%s\\n' \"$pid\" \"$device\" \"$name\"",
        f"systemctl show {unit}.service -p MemoryCurrent -p MemoryHigh "
        "-p MemoryMax -p CPUUsageNSec -p TasksCurrent --no-pager",
        "cat /proc/\"$pid\"/io",
        "ps -L -p \"$pid\" -o stat= | sort | uniq -c",
        "iostat -dxm 1 5 \"$name\"",
        "cat /proc/pressure/io",
        "cat /proc/pressure/memory",
    ], timeout=300)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--resume", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--profile", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", required=True, choices=sorted(v11.STATELESS))
    parser.add_argument("--opposed", action="store_true")
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--cpus", default="0-7,16-31")
    parser.add_argument("--limit", type=int, default=12_000_000_000)
    parser.add_argument("--volume", default=v11.DEFAULT_VOLUME)
    args = parser.parse_args()
    if args.resume:
        print(resume(args.instance, args.piece, args.opposed, args.square,
                     base.parse_cpus(args.cpus), args.limit, args.volume))
    elif args.status:
        print(status(args.instance, args.piece, args.opposed, args.square,
                     args.volume))
    else:
        print(profile(args.instance, args.piece, args.opposed, args.square,
                      args.volume))


if __name__ == "__main__":
    main()
