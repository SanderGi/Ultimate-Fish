#!/usr/bin/env python3
"""Resume retained Devil checkpoints with the independently bounded fingerprint v19 index."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as remote
import resume_ultimate_devil_companion_solver_v15_aws as v15


SOURCE_SHA256 = "947e5cc0bae36c3bbd9a0f33a57bad18b5f6543e4fd4e7f63fbd2a6da0016018"
SOURCE_VERSION = "yEJqVf9r66Z1ZjxrS0TJ5cNApKFDj9Rq"
SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v19/sha256/{SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v19-source.tar"
)
BINARY_SHA256 = "cd98aaa27d8879ce0ddf9b6ee9068e68ad493d3f3649ca0ae61eccff0b82b3a3"
BINARY_VERSION = "VMo8OwaPFFRINRjljNXIjqLI0J1.J9wy"
BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v19/sha256/{BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v19"
)
ROOT = f"/mnt/ultimatefish/devil-spawned-solver-v19-{SOURCE_SHA256[:8]}"
MEMORY_MAX = v15.MEMORY_MAX
# V19 retains v18's exact four-bit fingerprint filter and 90% load target but
# independently bounds disposable hash capacity. The hard 215-GiB safety gate
# remains unchanged; the durable proof-state limit remains a separate bound.
MEMORY_HIGH = MEMORY_MAX

# These retained Bishop checkpoints have already established their practical
# closure envelopes.  Keep the reviewed restart cache floors next to the
# launcher so an operator cannot repeat a too-small disposable hash allocation
# and spend hours rebuilding the exact index only to fail at the first new
# state.  The proof-state limit remains the independent fail-closed bound.
REVIEWED_MIN_HASH_CAPACITY = {
    ("bishop", False, 0): 20_000_000_000,
    ("bishop", False, 1): 20_000_000_000,
    ("bishop", False, 2): 18_000_000_000,
}


def stage_commands() -> list[str]:
    source = f"{ROOT}/source.tar"
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v19"
    return [
        f"install -d -m 0755 {shlex.quote(ROOT)}",
        f"if ! test -f {shlex.quote(source)}; then aws s3api get-object "
        f"--region {remote.REGION} --bucket {remote.BUCKET} --key "
        f"{shlex.quote(SOURCE_KEY)} --version-id {shlex.quote(SOURCE_VERSION)} "
        f"{shlex.quote(source)} >/dev/null; fi",
        f"test \"$(sha256sum {shlex.quote(source)} | cut -d ' ' -f1)\" = "
        f"{SOURCE_SHA256}",
        f"if ! test -f {shlex.quote(binary)}; then aws s3api get-object "
        f"--region {remote.REGION} --bucket {remote.BUCKET} --key "
        f"{shlex.quote(BINARY_KEY)} --version-id {shlex.quote(BINARY_VERSION)} "
        f"{shlex.quote(binary)} >/dev/null; chmod 0755 {shlex.quote(binary)}; fi",
        f"test \"$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)\" = "
        f"{BINARY_SHA256}",
    ]


def names(piece: str, opposed: bool, square: int,
          volume: str) -> tuple[str, str, str, str]:
    orientation = "opposed" if opposed else "same"
    work = (f"{volume.rstrip('/')}/devil-companion-v15-34aa7f01/"
            f"{orientation}-{piece}/square-{square}")
    unit = f"ultimatefish-devil-companion-v19-{orientation}-{piece}-square-{square}"
    log = (f"{volume.rstrip('/')}/devil-companion-v15-34aa7f01/logs/"
           f"{orientation}-{piece}-square-{square}-v19.log")
    old_unit = f"ultimatefish-devil-companion-v18-{orientation}-{piece}-square-{square}"
    return unit, work, log, old_unit


def resume(instance: str, piece: str, opposed: bool, square: int,
           cpus: tuple[int, ...], limit: int, hash_capacity: int,
           volume: str) -> str:
    if (not cpus or limit <= 0 or limit >= 2**63 or
            hash_capacity <= 0 or hash_capacity > limit):
        raise ValueError("invalid v19 Devil resource")
    reviewed_floor = REVIEWED_MIN_HASH_CAPACITY.get((piece, opposed, square))
    if reviewed_floor is not None and hash_capacity < reviewed_floor:
        raise ValueError(
            f"reviewed v19 hash capacity for {piece} square {square} is "
            f"at least {reviewed_floor}")
    unit, work, log, old_unit = names(piece, opposed, square, volume)
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v19"
    orientation = " --opposing" if opposed else ""
    cpu_set = ",".join(map(str, cpus))
    command = (
        f"exec {shlex.quote(binary)} --piece devil --piece2 {piece}{orientation} "
        f"--workers {len(cpus)} --solve-devil-spawned-square {square} "
        f"--devil-spawned-limit {limit} "
        f"--devil-spawned-hash-capacity {hash_capacity} --devil-spawned-work "
        f"{shlex.quote(work)} >>{shlex.quote(log)} 2>&1"
    )
    prefix = f"{work}/devil-{square}"
    commands = [
        "set -euo pipefail",
        *stage_commands(),
        f"test -s {shlex.quote(prefix + '.closure')}",
        f"test -s {shlex.quote(prefix + '.keys')}",
        f"test -f {shlex.quote(prefix + '.frontier')}",
        f"test ! -e {shlex.quote(prefix + '.roots')}",
        f"if systemctl is-active --quiet {old_unit}.service; then "
        f"systemctl stop {old_unit}.service; fi",
        f"! systemctl is-active --quiet {old_unit}.service",
        f"systemctl is-active --quiet {unit}.service || systemd-run --quiet "
        f"--collect --unit={unit} --property=AllowedCPUs={cpu_set} "
        f"--property=Nice=5 --property=MemoryHigh={MEMORY_HIGH} "
        f"--property=MemoryMax={MEMORY_MAX} --property=OOMPolicy=stop "
        "--setenv=ULTIMATE_TABLEBASE_PRESERVE_SCRATCH=1 "
        f"/bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "-p AllowedCPUs -p MemoryHigh -p MemoryMax --no-pager",
        f"printf '%s\\n' source_sha256={SOURCE_SHA256} "
        f"source_version={SOURCE_VERSION} binary_sha256={BINARY_SHA256} "
        f"binary_version={BINARY_VERSION}",
    ]
    return remote.send(instance, [
        f"/bin/bash -lc {shlex.quote(chr(10).join(commands))}"
    ], timeout=300)


def status(instance: str, piece: str, opposed: bool, square: int,
           volume: str) -> str:
    unit, work, log, _ = names(piece, opposed, square, volume)
    prefix = f"{work}/devil-{square}"
    return remote.send(instance, [
        f"pid=$(systemctl show {unit}.service -p MainPID --value)",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "-p MainPID -p CPUUsageNSec -p MemoryCurrent -p MemoryHigh "
        "-p MemoryMax -p AllowedCPUs --no-pager",
        "printf '%s\\n' THREADS",
        "ps -L -p \"$pid\" -o tid=,psr=,pcpu=,stat=,wchan:24= "
        "--sort=-pcpu | head -40 || true",
        "printf '%s\\n' CGROUP_PRESSURE",
        f"cg=$(systemctl show {unit}.service -p ControlGroup --value); "
        "test -r /sys/fs/cgroup\"$cg\"/memory.pressure && "
        "cat /sys/fs/cgroup\"$cg\"/memory.pressure || true",
        "printf '%s\\n' BLOCK_DEVICES",
        "iostat -dx 1 2 2>/dev/null | tail -n 40 || true",
        f"journalctl -u {unit}.service --since '-15 min' --no-pager -n 40 || true",
        f"ls -l {shlex.quote(log)} 2>/dev/null || true",
        f"tail -n 20 {shlex.quote(log)} 2>/dev/null || true",
        f"stat -c '%n bytes=%s allocated=%b*512 mtime=%y' "
        f"{shlex.quote(prefix + '.closure')} "
        f"{shlex.quote(prefix + '.frontier')} "
        f"{shlex.quote(prefix + '.keys')} 2>/dev/null || true",
    ], timeout=300)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--status", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", default="bishop")
    parser.add_argument("--opposed", action="store_true")
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--cpus")
    parser.add_argument("--limit", type=int)
    parser.add_argument("--hash-capacity", type=int)
    parser.add_argument("--volume", required=True)
    args = parser.parse_args()
    if args.status:
        print(status(args.instance, args.piece, args.opposed, args.square,
                     args.volume))
    else:
        if not args.cpus or args.limit is None or args.hash_capacity is None:
            parser.error(
                "--cpus, --limit, and --hash-capacity are required when resuming")
        print(resume(args.instance, args.piece, args.opposed, args.square,
                     remote.parse_cpus(args.cpus), args.limit,
                     args.hash_capacity, args.volume))


if __name__ == "__main__":
    main()
