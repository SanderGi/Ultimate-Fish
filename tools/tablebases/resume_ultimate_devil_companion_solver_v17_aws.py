#!/usr/bin/env python3
"""Resume retained Devil checkpoints with the fingerprint-filtered v17 index."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as remote
import resume_ultimate_devil_companion_solver_v15_aws as v15


SOURCE_SHA256 = "7cc0dc6b577ab4de550283119bf92b5f1740fa8fe507dcfb085987e41a9a5aef"
SOURCE_VERSION = "sKIOGsHVNnFBlPcDX6jI5vLNoEmoD6AG"
SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v17/sha256/{SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v17-source.tar"
)
BINARY_SHA256 = "8bd1e462edc860939070b1c55aea47b02f64421188777bcc7532d043d9857602"
BINARY_VERSION = "5Gq_NNldsnFS74SNbgT496sXR.KwB1Ky"
BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v17/sha256/{BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v17"
)
ROOT = f"/mnt/ultimatefish/devil-spawned-solver-v17-{SOURCE_SHA256[:8]}"
MEMORY_MAX = v15.MEMORY_MAX
# V17's fingerprint plane eliminates most exact-key misses, so cgroup reclaim
# at the former 212-GiB soft gate only evicts useful retained keys while the
# already-reviewed 215-GiB hard safety bound remains authoritative.
MEMORY_HIGH = MEMORY_MAX


def stage_commands() -> list[str]:
    source = f"{ROOT}/source.tar"
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v17"
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
    unit = f"ultimatefish-devil-companion-v17-{orientation}-{piece}-square-{square}"
    log = (f"{volume.rstrip('/')}/devil-companion-v15-34aa7f01/logs/"
           f"{orientation}-{piece}-square-{square}-v17.log")
    old_unit = f"ultimatefish-devil-companion-v16-{orientation}-{piece}-square-{square}"
    return unit, work, log, old_unit


def resume(instance: str, piece: str, opposed: bool, square: int,
           cpus: tuple[int, ...], limit: int, volume: str) -> str:
    if not cpus or limit <= 0 or limit >= 2**63:
        raise ValueError("invalid v17 Devil resource")
    unit, work, log, old_unit = names(piece, opposed, square, volume)
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v17"
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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", default="bishop")
    parser.add_argument("--opposed", action="store_true")
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--cpus", required=True)
    parser.add_argument("--limit", required=True, type=int)
    parser.add_argument("--volume", required=True)
    args = parser.parse_args()
    print(resume(args.instance, args.piece, args.opposed, args.square,
                 remote.parse_cpus(args.cpus), args.limit, args.volume))


if __name__ == "__main__":
    main()
