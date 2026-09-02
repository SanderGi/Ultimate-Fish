#!/usr/bin/env python3
"""Nondestructively copy a quiesced Devil checkpoint to instance-local NVMe."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as remote


SUFFIXES = ("closure", "frontier", "keys")
GRAPH_SUFFIXES = (
    "nodes",
    "degrees",
    "offsets",
    "predecessors",
    "predecessor-sides",
    "reverse-spool",
)


def paths(piece: str, opposed: bool, square: int, source_volume: str,
          destination_volume: str) -> tuple[str, str, str, str, str]:
    orientation = "opposed" if opposed else "same"
    relative = (
        f"devil-companion-v15-34aa7f01/{orientation}-{piece}/square-{square}"
    )
    source = f"{source_volume.rstrip('/')}/{relative}"
    destination = f"{destination_volume.rstrip('/')}/{relative}"
    source_unit = (
        f"ultimatefish-devil-companion-v16-{orientation}-{piece}-square-{square}"
    )
    migration_unit = (
        f"ultimatefish-devil-nvme-migrate-v17-{orientation}-{piece}-square-{square}"
    )
    receipt = f"{destination}/devil-{square}.nvme-copy-v17.receipt"
    return source, destination, source_unit, migration_unit, receipt


def validate_volume(source: str, destination: str) -> None:
    if not (source == "/mnt/checkpoint-migrate" or
            source.startswith("/mnt/ultimatefish")):
        raise ValueError("source must be an Ultimate Fish mount")
    if destination != "/mnt/ultimatefish":
        raise ValueError("destination must be instance-local /mnt/ultimatefish")
    if source.rstrip("/") == destination.rstrip("/"):
        raise ValueError("source and destination volumes must differ")


def launch(instance: str, piece: str, opposed: bool, square: int,
           source_volume: str, destination_volume: str, cpu: int) -> str:
    validate_volume(source_volume, destination_volume)
    if square not in remote.FIXED_SQUARES or not 0 <= cpu < 31:
        raise ValueError("invalid Devil square or migration CPU")
    source, destination, source_unit, migration_unit, receipt = paths(
        piece, opposed, square, source_volume, destination_volume)
    prefix = f"devil-{square}"
    source_prefix = f"{source}/{prefix}"
    destination_prefix = f"{destination}/{prefix}"
    log_dir = destination.rsplit("/", 2)[0] + "/logs"
    log = f"{log_dir}/nvme-migrate-{piece}-square-{square}-v17.log"

    copy_lines = [
        "set -euo pipefail",
        f"install -d -m 0755 {shlex.quote(destination)} {shlex.quote(log_dir)}",
    ]
    for suffix in SUFFIXES:
        source_file = f"{source_prefix}.{suffix}"
        destination_file = f"{destination_prefix}.{suffix}"
        stage = destination_file + ".stage"
        source_hash = stage + ".source.sha256"
        destination_hash = stage + ".destination.sha256"
        copy_lines.extend([
            f"test -f {shlex.quote(source_file)}",
            f"test ! -e {shlex.quote(destination_file)}",
            f"test ! -e {shlex.quote(stage)}",
            f"cp --reflink=never --sparse=always {shlex.quote(source_file)} "
            f"{shlex.quote(stage)}",
            # Source gp3 and destination NVMe are independent devices.  Hash
            # them concurrently so authentication does not add two serial
            # full-file reads to a checkpoint migration.
            f"dd if={shlex.quote(source_file)} bs=16M iflag=fullblock "
            f"status=none | sha256sum | cut -d ' ' -f1 >"
            f"{shlex.quote(source_hash)} & source_hash_pid=$!",
            f"dd if={shlex.quote(stage)} bs=16M iflag=fullblock status=none | "
            f"sha256sum | cut -d ' ' -f1 >{shlex.quote(destination_hash)} & "
            "destination_hash_pid=$!",
            "wait $source_hash_pid",
            "wait $destination_hash_pid",
            f"cmp -s {shlex.quote(source_hash)} {shlex.quote(destination_hash)}",
            f"rm {shlex.quote(destination_hash)}",
            f"mv {shlex.quote(stage)} {shlex.quote(destination_file)}",
        ])
    for suffix in GRAPH_SUFFIXES:
        source_file = f"{source_prefix}.{suffix}"
        destination_file = f"{destination_prefix}.{suffix}"
        copy_lines.extend([
            f"test ! -e {shlex.quote(destination_file)}",
            f"ln -s {shlex.quote(source_file)} {shlex.quote(destination_file)}",
            f"test \"$(readlink -f {shlex.quote(destination_file)})\" = "
            f"{shlex.quote(source_file)}",
        ])
    receipt_lines = [
        "format=ultimatefish-devil-nvme-copy-v17",
        f"source={source}",
        f"destination={destination}",
    ]
    copy_lines.append(
        "printf '%s\\n' "
        + " ".join(shlex.quote(line) for line in receipt_lines)
        + f" >{shlex.quote(receipt)}"
    )
    for suffix in SUFFIXES:
        source_hash = f"{destination_prefix}.{suffix}.stage.source.sha256"
        copy_lines.append(
            f"printf '%s  %s\\n' \"$(cat {shlex.quote(source_hash)})\" "
            f"{shlex.quote(prefix + '.' + suffix)} >>{shlex.quote(receipt)}"
        )
        copy_lines.append(f"rm {shlex.quote(source_hash)}")
    copy_lines.extend([
        f"sync -f {shlex.quote(destination)}",
        f"printf '%s\\n' DEVIL_CHECKPOINT_LOCAL_COPY_AUTHENTICATED "
        f">>{shlex.quote(log)}",
    ])
    migration_command = chr(10).join(copy_lines) + f" >>{shlex.quote(log)} 2>&1"

    commands = [
        "set -euo pipefail",
        f"test -s {shlex.quote(source_prefix + '.closure')}",
        f"test -s {shlex.quote(source_prefix + '.frontier')}",
        f"test -s {shlex.quote(source_prefix + '.keys')}",
        f"test ! -e {shlex.quote(source_prefix + '.roots')}",
        f"test ! -e {shlex.quote(destination_prefix + '.closure')}",
        f"test ! -e {shlex.quote(destination_prefix + '.frontier')}",
        f"test ! -e {shlex.quote(destination_prefix + '.keys')}",
        f"if systemctl is-active --quiet {source_unit}.service; then "
        f"systemctl stop {source_unit}.service; fi",
        f"! systemctl is-active --quiet {source_unit}.service",
        f"systemd-run --quiet --collect --unit={migration_unit} "
        f"--property=AllowedCPUs={cpu},{cpu + 1} --property=Nice=10 "
        f"--property=MemoryMax=4294967296 /bin/bash -lc "
        f"{shlex.quote(migration_command)}",
        f"systemctl show {migration_unit}.service -p ActiveState -p SubState "
        "-p Result -p AllowedCPUs -p MemoryMax --no-pager",
    ]
    return remote.send(instance, [
        f"/bin/bash -lc {shlex.quote(chr(10).join(commands))}"
    ], timeout=300)


def status(instance: str, piece: str, opposed: bool, square: int,
           source_volume: str, destination_volume: str) -> str:
    validate_volume(source_volume, destination_volume)
    source, destination, source_unit, migration_unit, receipt = paths(
        piece, opposed, square, source_volume, destination_volume)
    prefix = f"{destination}/devil-{square}"
    log = (
        destination.rsplit("/", 2)[0]
        + f"/logs/nvme-migrate-{piece}-square-{square}-v17.log"
    )
    commands = [
        f"systemctl show {migration_unit}.service -p ActiveState -p SubState "
        "-p Result -p ExecMainStatus --no-pager || true",
        f"systemctl show {source_unit}.service -p ActiveState -p SubState "
        "-p Result --no-pager || true",
        f"tail -n 20 {shlex.quote(log)} 2>/dev/null || true",
        f"stat -c '%n %s' {shlex.quote(prefix + '.closure')} "
        f"{shlex.quote(prefix + '.frontier')} {shlex.quote(prefix + '.keys')} "
        "2>/dev/null || true",
        f"cat {shlex.quote(receipt)} 2>/dev/null || true",
    ]
    return remote.send(instance, commands, timeout=300)


def finalize(instance: str, piece: str, opposed: bool, square: int,
             source_volume: str, destination_volume: str, cpu: int) -> str:
    """Replace an interrupted legacy receipt pass without recopying payloads."""
    validate_volume(source_volume, destination_volume)
    if square not in remote.FIXED_SQUARES or not 0 <= cpu < 31:
        raise ValueError("invalid Devil square or finalization CPU")
    source, destination, source_unit, migration_unit, receipt = paths(
        piece, opposed, square, source_volume, destination_volume)
    prefix = f"devil-{square}"
    source_prefix = f"{source}/{prefix}"
    destination_prefix = f"{destination}/{prefix}"
    finalizer_unit = migration_unit + "-finalize"
    log = (
        destination.rsplit("/", 2)[0]
        + f"/logs/nvme-migrate-{piece}-square-{square}-v17.log"
    )
    stage_receipt = receipt + ".stage"
    lines = [
        "set -euo pipefail",
        f"test ! -e {shlex.quote(source_prefix + '.roots')}",
        f"test ! -e {shlex.quote(destination_prefix + '.roots')}",
        f"test ! -e {shlex.quote(stage_receipt)}",
        f"printf '%s\\n' {shlex.quote('format=ultimatefish-devil-nvme-copy-v17')} "
        f"{shlex.quote('source=' + source)} {shlex.quote('destination=' + destination)} "
        f">{shlex.quote(stage_receipt)}",
    ]
    for suffix in SUFFIXES:
        source_file = f"{source_prefix}.{suffix}"
        destination_file = f"{destination_prefix}.{suffix}"
        source_hash = f"{stage_receipt}.{suffix}.source.sha256"
        destination_hash = f"{stage_receipt}.{suffix}.destination.sha256"
        lines.extend([
            f"test -f {shlex.quote(source_file)}",
            f"test -f {shlex.quote(destination_file)}",
            f"dd if={shlex.quote(source_file)} bs=16M iflag=fullblock status=none | "
            f"sha256sum | cut -d ' ' -f1 >{shlex.quote(source_hash)} & "
            "source_hash_pid=$!",
            f"dd if={shlex.quote(destination_file)} bs=16M iflag=fullblock "
            f"status=none | sha256sum | cut -d ' ' -f1 >"
            f"{shlex.quote(destination_hash)} & destination_hash_pid=$!",
            "wait $source_hash_pid",
            "wait $destination_hash_pid",
            f"cmp -s {shlex.quote(source_hash)} {shlex.quote(destination_hash)}",
            f"printf '%s  %s\\n' \"$(cat {shlex.quote(source_hash)})\" "
            f"{shlex.quote(prefix + '.' + suffix)} >>{shlex.quote(stage_receipt)}",
            f"rm {shlex.quote(source_hash)} {shlex.quote(destination_hash)}",
        ])
    for suffix in GRAPH_SUFFIXES:
        source_file = f"{source_prefix}.{suffix}"
        destination_file = f"{destination_prefix}.{suffix}"
        lines.append(
            f"test \"$(readlink -f {shlex.quote(destination_file)})\" = "
            f"{shlex.quote(source_file)}"
        )
    lines.extend([
        f"mv -f {shlex.quote(stage_receipt)} {shlex.quote(receipt)}",
        f"sync -f {shlex.quote(destination)}",
        f"printf '%s\\n' DEVIL_CHECKPOINT_LOCAL_COPY_AUTHENTICATED "
        f">>{shlex.quote(log)}",
    ])
    command = chr(10).join(lines) + f" >>{shlex.quote(log)} 2>&1"
    commands = [
        "set -euo pipefail",
        f"if systemctl is-active --quiet {migration_unit}.service; then "
        f"systemctl stop {migration_unit}.service; fi",
        f"! systemctl is-active --quiet {migration_unit}.service",
        f"! systemctl is-active --quiet {source_unit}.service",
        f"systemd-run --quiet --collect --unit={finalizer_unit} "
        f"--property=AllowedCPUs={cpu},{cpu + 1} --property=Nice=10 "
        f"--property=MemoryMax=4294967296 /bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {finalizer_unit}.service -p ActiveState -p SubState "
        "-p Result -p AllowedCPUs -p MemoryMax --no-pager",
    ]
    return remote.send(instance, [
        f"/bin/bash -lc {shlex.quote(chr(10).join(commands))}"
    ], timeout=300)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--finalize", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", default="bishop")
    parser.add_argument("--opposed", action="store_true")
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--source-volume", required=True)
    parser.add_argument("--destination-volume", default="/mnt/ultimatefish")
    parser.add_argument("--cpu", default=0, type=int)
    args = parser.parse_args()
    if args.launch:
        print(launch(args.instance, args.piece, args.opposed, args.square,
                     args.source_volume, args.destination_volume, args.cpu))
    elif args.finalize:
        print(finalize(args.instance, args.piece, args.opposed, args.square,
                       args.source_volume, args.destination_volume, args.cpu))
    else:
        print(status(args.instance, args.piece, args.opposed, args.square,
                     args.source_volume, args.destination_volume))


if __name__ == "__main__":
    main()
