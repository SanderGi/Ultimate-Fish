#!/usr/bin/env python3
"""Resume a v8 companion-Devil checkpoint with the packed-slot v9 binary."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_companion_solver_v8_aws as v8
import launch_ultimate_devil_spawned_solver_aws as base


SOURCE_SHA256 = "b1b73ebcf338a2a49ecd33d47a6408c9f344de8ccc6947c8b1a5dbb5c597f4c7"
SOURCE_VERSION = "5iPT2zkxMI4nHVz5mzMek3ea0jEQuW2V"
BINARY_SHA256 = "4a8573ddcf1a9019c7116d278548de5d59e20e9024c21c9666b633808e6c5e6c"
BINARY_VERSION = "lcpeskwa2JS74WRy5fDAnQjp4ovnv0Hi"
SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v9/sha256/{SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v9-source.tar"
)
BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v9/sha256/{BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v9"
)
ROOT = f"/mnt/ultimatefish/devil-spawned-solver-v9-{SOURCE_SHA256[:8]}"
BINARY_FILENAME = "ultimate_tablebase-devil-spawned-v9"


def paths(piece: str, opposed: bool, square: int, volume: str) -> tuple[str, ...]:
    orientation = "opposed" if opposed else "same"
    stem = f"{orientation}-{piece}-square-{square}"
    work_root = f"{volume.rstrip('/')}/devil-companion-v8-{v8.SOURCE_SHA256[:8]}"
    return (
        f"ultimatefish-devil-companion-v8-{stem}",
        f"ultimatefish-devil-companion-v9-{stem}",
        f"{work_root}/{orientation}-{piece}/square-{square}",
        f"{work_root}/logs/{stem}-v9.log",
    )


def stage_commands() -> list[str]:
    source = f"{ROOT}/source.tar"
    binary = f"{ROOT}/{BINARY_FILENAME}"
    return [
        f"install -d -m 0755 {shlex.quote(ROOT)}",
        f"if test ! -f {shlex.quote(source)}; then aws s3api get-object "
        f"--region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(SOURCE_KEY)} --version-id "
        f"{shlex.quote(SOURCE_VERSION)} {shlex.quote(source)} >/dev/null; fi",
        f"test \"$(sha256sum {shlex.quote(source)} | cut -d ' ' -f1)\" = "
        f"{SOURCE_SHA256}",
        f"if test ! -f {shlex.quote(binary)}; then aws s3api get-object "
        f"--region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(BINARY_KEY)} --version-id "
        f"{shlex.quote(BINARY_VERSION)} {shlex.quote(binary)} >/dev/null; fi",
        f"test \"$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)\" = "
        f"{BINARY_SHA256}",
        f"chmod 0755 {shlex.quote(binary)}",
    ]


def resume(instance: str, piece: str, opposed: bool, square: int,
           cpus: tuple[int, ...], volume: str, limit: int) -> str:
    if piece not in v8.STATELESS or square not in base.FIXED_SQUARES:
        raise ValueError("invalid stateless companion-Devil partition")
    if not (volume == "/mnt/checkpoint-migrate" or
            volume.startswith("/mnt/ultimatefish")):
        raise ValueError("Devil companion work volume is not authorized")
    old_unit, unit, work, log = paths(piece, opposed, square, volume)
    binary = f"{ROOT}/{BINARY_FILENAME}"
    orientation = " --opposing" if opposed else ""
    cpu_set = ",".join(map(str, cpus))
    command = (
        f"exec {shlex.quote(binary)} --piece devil --piece2 {piece}{orientation} "
        f"--workers {len(cpus)} --solve-devil-spawned-square {square} "
        f"--devil-spawned-limit {limit} --devil-spawned-work {shlex.quote(work)} "
        f">>{shlex.quote(log)} 2>&1"
    )
    commands = ["set -euo pipefail", *stage_commands(),
        f"test -s {shlex.quote(work + f'/devil-{square}.closure')}",
        f"test -s {shlex.quote(work + f'/devil-{square}.frontier')}",
        f"test -s {shlex.quote(work + f'/devil-{square}.keys')}",
        f"install -d -m 0755 {shlex.quote(log.rsplit('/', 1)[0])}",
        f"systemctl stop {shlex.quote(old_unit)}.service || true",
        f"systemctl is-active --quiet {shlex.quote(unit)}.service || "
        f"systemd-run --quiet --collect --unit={shlex.quote(unit)} "
        f"--property=AllowedCPUs={cpu_set} --property=Nice=5 "
        # The packed 2^34-slot index plus a multi-billion-state frontier needs
        # more than 150 GiB of resident/file-backed working set.  At 150 GiB,
        # systemd reclaim throttles a nominal 28-30-worker closure to one CPU.
        # The assigned 256-GiB hosts retain 22 GiB below the hard ceiling for
        # the kernel and co-resident jobs with these measured, restart-stable
        # gates.  Most of this cgroup charge is reclaimable mapped index data.
        "--property=MemoryHigh=234075717632 --property=MemoryMax=241591910400 "
        "--property=OOMPolicy=stop --setenv=ULTIMATE_TABLEBASE_PRESERVE_SCRATCH=1 "
        f"/bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {shlex.quote(unit)}.service -p ActiveState -p SubState "
        "-p Result -p AllowedCPUs -p MemoryHigh -p MemoryMax --no-pager",
    ]
    return base.send(instance, commands, timeout=300)


def status(instance: str, piece: str, opposed: bool, square: int,
           volume: str) -> str:
    _, unit, work, log = paths(piece, opposed, square, volume)
    return base.send(instance, [
        f"systemctl status {shlex.quote(unit)}.service --no-pager -l || true",
        f"tail -n 40 {shlex.quote(log)} || true",
        f"du -sh {shlex.quote(work)} || true",
    ])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--resume", action="store_true")
    mode.add_argument("--status", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", required=True, choices=sorted(v8.STATELESS))
    parser.add_argument("--opposed", action="store_true")
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--cpus", default="1-29,31")
    parser.add_argument("--volume", default=v8.DEFAULT_VOLUME)
    parser.add_argument("--limit", type=int, default=6_000_000_000)
    args = parser.parse_args()
    if args.resume:
        print(resume(args.instance, args.piece, args.opposed, args.square,
                     base.parse_cpus(args.cpus), args.volume, args.limit))
    else:
        print(status(args.instance, args.piece, args.opposed, args.square,
                     args.volume))


if __name__ == "__main__":
    main()
