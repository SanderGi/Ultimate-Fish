#!/usr/bin/env python3
"""Nondestructively migrate a live v11 Devil checkpoint to packed v14."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base
import launch_ultimate_devil_companion_solver_v11_aws as v11


SOURCE_SHA256 = "4044755f9a2530ecd529f47856d9fecef4762a6295e3bcd8c30e4fb922ad5231"
SOURCE_VERSION = "8_FQj_Org_G642sz312G3eXb5UybUAq5"
SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v14/sha256/{SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v14-source.tar"
)
BINARY_SHA256 = "d4619620ad7c4718f07cf8f3e8b818494d4416fa9b29db4ce8f489792761fe50"
BINARY_VERSION = "UfLeaYlMybeKNtsw37qdU7vqN.IRIXP2"
BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v14/sha256/{BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v14"
)
ROOT = f"/mnt/ultimatefish/devil-spawned-solver-v14-{SOURCE_SHA256[:8]}"


def paths(piece: str, opposed: bool, square: int, volume: str) -> tuple[str, ...]:
    orientation = "opposed" if opposed else "same"
    source_unit, source_work, _ = v11.names(piece, opposed, square, volume)
    destination = (
        f"{volume.rstrip('/')}/devil-companion-v14-{SOURCE_SHA256[:8]}/"
        f"{orientation}-{piece}/square-{square}"
    )
    unit = f"ultimatefish-devil-migrate-v11-v14-{orientation}-{piece}-square-{square}"
    log = (
        f"{volume.rstrip('/')}/devil-companion-v14-{SOURCE_SHA256[:8]}/logs/"
        f"migrate-{orientation}-{piece}-square-{square}.log"
    )
    return source_unit, source_work, destination, unit, log


def stage_commands() -> list[str]:
    source = f"{ROOT}/source.tar"
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v14"
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
           volume: str) -> str:
    if piece not in v11.STATELESS or square not in base.FIXED_SQUARES:
        raise ValueError("invalid v14 companion migration material")
    if not volume.startswith("/mnt/ultimatefish"):
        raise ValueError("migration volume is not authorized")
    source_unit, source_work, destination, unit, log = paths(
        piece, opposed, square, volume)
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v14"
    source_prefix = f"{source_work}/devil-{square}"
    destination_prefix = f"{destination}/devil-{square}"
    root = destination.rsplit("/", 2)[0]
    command = (
        f"exec {shlex.quote(binary)} --migrate-devil-checkpoint-v2 "
        f"{shlex.quote(source_prefix)} --devil-migration-destination "
        f"{shlex.quote(destination_prefix)} >>{shlex.quote(log)} 2>&1"
    )
    commands = [
        "set -euo pipefail",
        *stage_commands(),
        f"systemctl is-active --quiet {shlex.quote(source_unit)}.service",
        f"test ! -e {shlex.quote(source_prefix + '.roots')}",
        f"install -d -m 0755 {shlex.quote(destination)} "
        f"{shlex.quote(root + '/logs')}",
        f"test ! -e {shlex.quote(destination_prefix + '.closure')} "
        f"&& test ! -e {shlex.quote(destination_prefix + '.keys.tmp')} "
        f"&& test ! -e {shlex.quote(destination_prefix + '.frontier.tmp')}",
        f"systemctl is-active --quiet {unit}.service || systemd-run --quiet "
        f"--collect --unit={unit} --property=AllowedCPUs=0-1 "
        "--property=Nice=10 --property=MemoryMax=4294967296 "
        f"/bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "-p AllowedCPUs -p MemoryMax --no-pager",
        f"printf '%s\\n' source_sha256={SOURCE_SHA256} "
        f"source_version={SOURCE_VERSION} binary_sha256={BINARY_SHA256} "
        f"binary_version={BINARY_VERSION}",
    ]
    return base.send(instance, commands, timeout=300)


def status(instance: str, piece: str, opposed: bool, square: int,
           volume: str) -> str:
    _, _, destination, unit, log = paths(piece, opposed, square, volume)
    return base.send(instance, [
        f"systemctl status {unit}.service --no-pager -l || true",
        f"tail -n 30 {shlex.quote(log)} || true",
        f"du -sh {shlex.quote(destination)} || true",
        f"stat -c '%n %s' {shlex.quote(destination + f'/devil-{square}.closure')} "
        f"{shlex.quote(destination + f'/devil-{square}.keys')} "
        f"{shlex.quote(destination + f'/devil-{square}.frontier')} 2>/dev/null || true",
    ], timeout=300)


def quiesce(instance: str, piece: str, opposed: bool, square: int,
            volume: str) -> str:
    source_unit, _, _, unit, _ = paths(piece, opposed, square, volume)
    return base.send(instance, [
        "set -euo pipefail",
        f"systemctl is-active --quiet {unit}.service",
        f"systemctl stop {source_unit}.service",
        f"systemctl show {source_unit}.service -p ActiveState -p SubState "
        "-p Result -p ExecMainStatus --no-pager",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "--no-pager",
    ], timeout=300)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--quiesce-source", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", required=True, choices=sorted(v11.STATELESS))
    parser.add_argument("--opposed", action="store_true")
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--volume", default=v11.DEFAULT_VOLUME)
    args = parser.parse_args()
    action = launch if args.launch else status if args.status else quiesce
    print(action(args.instance, args.piece, args.opposed, args.square, args.volume))


if __name__ == "__main__":
    main()
