#!/usr/bin/env python3
"""Nondestructively stage a quiesced Devil checkpoint on persistent gp3."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as remote


FILES = ("closure", "frontier", "keys")
SOURCE_ROOT = "/mnt/ultimatefish/devil-companion-v15-34aa7f01"
DESTINATION_ROOT = "/mnt/ultimatefish-devil-v11/devil-companion-v26-retained"


def paths(piece: str, square: int) -> tuple[str, str, str, str]:
    material = f"same-{piece}/square-{square}"
    source = f"{SOURCE_ROOT}/{material}"
    destination = f"{DESTINATION_ROOT}/{material}"
    unit = f"ultimatefish-devil-gp3-migrate-v26-same-{piece}-square-{square}"
    receipt = f"{destination}/devil-{square}.gp3-copy-v26.receipt"
    return source, destination, unit, receipt


def launch(instance: str, piece: str, square: int, cpus: str) -> str:
    if square not in remote.FIXED_SQUARES:
        raise ValueError("invalid fixed Devil square")
    source, destination, unit, receipt = paths(piece, square)
    prefix = f"devil-{square}"
    log = f"{DESTINATION_ROOT}/logs/same-{piece}-square-{square}.log"
    lines = [
        "set -euo pipefail",
        f"test -d {shlex.quote(source)}",
        f"test ! -e {shlex.quote(source + '/' + prefix + '.roots')}",
        f"install -d -m 0700 {shlex.quote(destination)}",
        f"install -d -m 0755 {shlex.quote(DESTINATION_ROOT + '/logs')}",
        f"test ! -e {shlex.quote(receipt)}",
    ]
    for suffix in FILES:
        source_file = f"{source}/{prefix}.{suffix}"
        destination_file = f"{destination}/{prefix}.{suffix}"
        stage = destination_file + ".stage"
        source_hash = stage + ".source.sha256"
        destination_hash = stage + ".destination.sha256"
        lines.extend([
            f"test -f {shlex.quote(source_file)}",
            f"test ! -e {shlex.quote(destination_file)}",
            f"test ! -e {shlex.quote(stage)}",
            f"cp --reflink=never --sparse=always {shlex.quote(source_file)} {shlex.quote(stage)}",
            f"dd if={shlex.quote(source_file)} bs=16M iflag=fullblock status=none | sha256sum | cut -d ' ' -f1 >{shlex.quote(source_hash)} & source_hash_pid=$!",
            f"dd if={shlex.quote(stage)} bs=16M iflag=fullblock status=none | sha256sum | cut -d ' ' -f1 >{shlex.quote(destination_hash)} & destination_hash_pid=$!",
            "wait $source_hash_pid",
            "wait $destination_hash_pid",
            f"cmp -s {shlex.quote(source_hash)} {shlex.quote(destination_hash)}",
            f"mv {shlex.quote(stage)} {shlex.quote(destination_file)}",
        ])
    receipt_lines = [
        "format=ultimatefish-devil-gp3-copy-v26",
        f"source={source}",
        f"destination={destination}",
    ]
    lines.append(
        "printf '%s\\n' " + " ".join(shlex.quote(x) for x in receipt_lines)
        + f" >{shlex.quote(receipt + '.stage')}"
    )
    for suffix in FILES:
        source_hash = f"{destination}/{prefix}.{suffix}.stage.source.sha256"
        destination_hash = f"{destination}/{prefix}.{suffix}.stage.destination.sha256"
        lines.extend([
            f"printf '%s  %s\\n' \"$(cat {shlex.quote(source_hash)})\" {shlex.quote(prefix + '.' + suffix)} >>{shlex.quote(receipt + '.stage')}",
            f"rm {shlex.quote(source_hash)} {shlex.quote(destination_hash)}",
        ])
    lines.extend([
        f"mv {shlex.quote(receipt + '.stage')} {shlex.quote(receipt)}",
        f"sync -f {shlex.quote(destination)}",
        "printf '%s\\n' DEVIL_GP3_CHECKPOINT_COPY_AUTHENTICATED",
    ])
    command = "\n".join(lines) + f" >>{shlex.quote(log)} 2>&1"
    commands = [
        "set -euo pipefail",
        f"test $(findmnt -n -o FSTYPE /mnt/ultimatefish-devil-v11) = xfs",
        f"test $(df -B1 --output=avail /mnt/ultimatefish-devil-v11 | tail -1) -ge 1000000000000",
        f"systemd-run --quiet --collect --unit={unit} --property=AllowedCPUs={shlex.quote(cpus)} --property=Nice=10 --property=MemoryMax=4294967296 /bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result -p AllowedCPUs --no-pager",
    ]
    return remote.send(instance, [f"/bin/bash -lc {shlex.quote(chr(10).join(commands))}"], timeout=300)


def status(instance: str, piece: str, square: int) -> str:
    _, destination, unit, receipt = paths(piece, square)
    prefix = f"{destination}/devil-{square}"
    log = f"{DESTINATION_ROOT}/logs/same-{piece}-square-{square}.log"
    commands = [
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result -p ExecMainStatus --no-pager || true",
        f"tail -n 20 {shlex.quote(log)} 2>/dev/null || true",
        f"stat -c '%n size=%s blocks=%b' {shlex.quote(prefix + '.closure')} {shlex.quote(prefix + '.frontier')} {shlex.quote(prefix + '.keys')} 2>/dev/null || true",
        f"cat {shlex.quote(receipt)} 2>/dev/null || true",
    ]
    return remote.send(instance, commands, timeout=300)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", default="bishop")
    parser.add_argument("--square", type=int, required=True)
    parser.add_argument("--cpus", default="0,1")
    args = parser.parse_args()
    if args.launch:
        print(launch(args.instance, args.piece, args.square, args.cpus))
    else:
        print(status(args.instance, args.piece, args.square))


if __name__ == "__main__":
    main()
