#!/usr/bin/env python3
"""Quiesce and nondestructively migrate a live v11/v14 Devil checkpoint to v15."""

from __future__ import annotations

import argparse
import shlex

import build_ultimate_devil_spawned_solver_v15_aws as build
import launch_ultimate_devil_spawned_solver_aws as base
import launch_ultimate_devil_companion_solver_v11_aws as v11
import resume_ultimate_devil_companion_solver_v11_aws as retained_v11
import resume_ultimate_devil_companion_solver_v14_aws as v14


SOURCE_SHA256 = build.SOURCE_SHA256
SOURCE_VERSION = build.SOURCE_VERSION
SOURCE_KEY = build.SOURCE_KEY
BINARY_SHA256 = "6a47a119a85d1fa5f31787a2c1b46424ca563700a4e0383753b8789978a23009"
BINARY_VERSION = "gXQRLEFDBz47SlmgaJ1tNaecRaTcPrdZ"
BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v15/sha256/{BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v15"
)
ROOT = f"/mnt/ultimatefish/devil-spawned-solver-v15-{SOURCE_SHA256[:8]}"
DEFAULT_VOLUME = "/mnt/ultimatefish-devil-v11"


def paths(piece: str, opposed: bool, square: int, volume: str,
          source_generation: str = "v14") -> tuple[str, ...]:
    orientation = "opposed" if opposed else "same"
    if source_generation == "v11":
        _, source_unit, source_work, _ = retained_v11.paths(
            piece, opposed, square, volume)
    elif source_generation == "v14":
        source_unit, source_work, _, _ = v14.names(
            piece, opposed, square, volume)
    else:
        raise ValueError("unsupported Devil checkpoint source generation")
    destination = (
        f"{volume.rstrip('/')}/devil-companion-v15-{SOURCE_SHA256[:8]}/"
        f"{orientation}-{piece}/square-{square}"
    )
    unit = (
        f"ultimatefish-devil-migrate-{source_generation}-v15-"
        f"{orientation}-{piece}-square-{square}"
    )
    log = (
        f"{volume.rstrip('/')}/devil-companion-v15-{SOURCE_SHA256[:8]}/logs/"
        f"migrate-{orientation}-{piece}-square-{square}.log"
    )
    return source_unit, source_work, destination, unit, log


def stage_commands() -> list[str]:
    source = f"{ROOT}/source.tar"
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v15"
    return [
        f"install -d -m 0755 {shlex.quote(ROOT)}",
        f"if ! test -f {shlex.quote(source)} || ! test \"$(sha256sum "
        f"{shlex.quote(source)} | cut -d ' ' -f1)\" = {SOURCE_SHA256}; then "
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(SOURCE_KEY)} --version-id {shlex.quote(SOURCE_VERSION)} "
        f"{shlex.quote(source + '.stage')} >/dev/null; "
        f"test \"$(sha256sum {shlex.quote(source + '.stage')} | cut -d ' ' -f1)\" = "
        f"{SOURCE_SHA256}; mv -f {shlex.quote(source + '.stage')} "
        f"{shlex.quote(source)}; fi",
        f"if ! test -f {shlex.quote(binary)} || ! test \"$(sha256sum "
        f"{shlex.quote(binary)} | cut -d ' ' -f1)\" = {BINARY_SHA256}; then "
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(BINARY_KEY)} --version-id {shlex.quote(BINARY_VERSION)} "
        f"{shlex.quote(binary + '.stage')} >/dev/null; "
        f"test \"$(sha256sum {shlex.quote(binary + '.stage')} | cut -d ' ' -f1)\" = "
        f"{BINARY_SHA256}; chmod 0755 {shlex.quote(binary + '.stage')}; "
        f"mv -f {shlex.quote(binary + '.stage')} {shlex.quote(binary)}; fi",
    ]


def launch(instance: str, piece: str, opposed: bool, square: int,
           volume: str, source_generation: str = "v14",
           cpus: tuple[int, ...] = (0, 1, 2, 3)) -> str:
    if piece not in v11.STATELESS or square not in base.FIXED_SQUARES:
        raise ValueError("invalid v15 companion migration material")
    if not (volume == "/mnt/checkpoint-migrate" or
            volume.startswith("/mnt/ultimatefish")):
        raise ValueError("migration volume is not authorized")
    if not cpus or len(set(cpus)) != len(cpus):
        raise ValueError("migration CPU allocation must be nonempty and unique")
    source_unit, source_work, destination, unit, log = paths(
        piece, opposed, square, volume, source_generation)
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v15"
    source_prefix = f"{source_work}/devil-{square}"
    destination_prefix = f"{destination}/devil-{square}"
    root = destination.rsplit("/", 2)[0]
    cpu_set = ",".join(map(str, cpus))
    command = (
        f"exec {shlex.quote(binary)} --migrate-devil-checkpoint "
        f"{shlex.quote(source_prefix)} --devil-migration-destination "
        f"{shlex.quote(destination_prefix)} >>{shlex.quote(log)} 2>&1"
    )
    commands = [
        "set -euo pipefail",
        *stage_commands(),
        f"test -s {shlex.quote(source_prefix + '.closure')} "
        f"&& test -s {shlex.quote(source_prefix + '.keys')} "
        f"&& test -s {shlex.quote(source_prefix + '.frontier')}",
        f"test ! -e {shlex.quote(source_prefix + '.roots')}",
        f"install -d -m 0755 {shlex.quote(destination)} "
        f"{shlex.quote(root + '/logs')}",
        f"test ! -e {shlex.quote(destination_prefix + '.closure')} "
        f"&& test ! -e {shlex.quote(destination_prefix + '.keys.tmp')} "
        f"&& test ! -e {shlex.quote(destination_prefix + '.frontier.tmp')}",
        # Stop only after source and destination gates pass.  The committed v14
        # checkpoint remains a complete inactive rollback throughout migration.
        f"if systemctl is-active --quiet {shlex.quote(source_unit)}.service; "
        f"then systemctl stop {shlex.quote(source_unit)}.service; fi",
        f"! systemctl is-active --quiet {shlex.quote(source_unit)}.service",
        f"systemd-run --quiet --collect --unit={unit} "
        f"--property=AllowedCPUs={cpu_set} --property=Nice=10 "
        # Migration is sequential but its destination writeback is charged to
        # the cgroup.  128 GiB lets the largest key conversion overlap its
        # 35-GiB frontier copy and writeback; the quiesced 215/225-GiB source
        # solver leaves far more than this available.
        "--property=MemoryMax=137438953472 "
        f"/bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "-p AllowedCPUs -p MemoryMax --no-pager",
        f"printf '%s\\n' source_sha256={SOURCE_SHA256} "
        f"source_version={SOURCE_VERSION} binary_sha256={BINARY_SHA256} "
        f"binary_version={BINARY_VERSION}",
    ]
    # AWS-RunShellScript may execute command-array entries in separate shell
    # contexts.  One explicit bash keeps `set -euo pipefail` authoritative, so
    # no unit can launch after a failed checkpoint or destination gate.
    return base.send(instance, [
        f"/bin/bash -lc {shlex.quote(chr(10).join(commands))}"
    ], timeout=300)


def status(instance: str, piece: str, opposed: bool, square: int,
           volume: str, source_generation: str = "v14") -> str:
    source_unit, _, destination, unit, log = paths(
        piece, opposed, square, volume, source_generation)
    prefix = f"{destination}/devil-{square}"
    return base.send(instance, [
        f"systemctl status {unit}.service --no-pager -l || true",
        f"systemctl show {source_unit}.service -p ActiveState -p SubState "
        "-p Result --no-pager || true",
        f"tail -n 30 {shlex.quote(log)} || true",
        f"du -sh {shlex.quote(destination)} || true",
        f"stat -c '%n %s' {shlex.quote(prefix + '.closure')} "
        f"{shlex.quote(prefix + '.keys')} "
        f"{shlex.quote(prefix + '.frontier')} 2>/dev/null || true",
    ], timeout=300)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", required=True, choices=sorted(v11.STATELESS))
    parser.add_argument("--opposed", action="store_true")
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--volume", default=DEFAULT_VOLUME)
    parser.add_argument("--source-generation", choices=("v11", "v14"),
                        default="v14")
    parser.add_argument("--cpus", default="0-3")
    args = parser.parse_args()
    if args.launch:
        print(launch(args.instance, args.piece, args.opposed, args.square,
                     args.volume, args.source_generation,
                     base.parse_cpus(args.cpus)))
    else:
        print(status(args.instance, args.piece, args.opposed, args.square,
                     args.volume, args.source_generation))


if __name__ == "__main__":
    main()
