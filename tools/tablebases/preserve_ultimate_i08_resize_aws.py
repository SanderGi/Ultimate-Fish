#!/usr/bin/env python3
"""Quiesce and authenticate i-08 instance-store work before a resize."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as remote


INSTANCE = "i-08c0f44a1776cb34a"
UNIT = "ultimatefish-info-solve-kghostkprince-parallel-v7.service"
ROOT = "/mnt/ultimatefish-devil-v11/resize-i08-v1"
SOURCES = (
    "/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostkprince-fresh-v6",
    "/mnt/ultimatefish/ghost-parallel-v6/kghostkprince-canary-striped-bdd-b-8w",
)


def launch() -> str:
    copy_unit = "ultimatefish-preserve-i08-resize-v1"
    log = f"{ROOT}/preserve.log"
    receipt = f"{ROOT}/receipt.txt"
    lines = [
        "set -euo pipefail",
        f"install -d -m 0700 {shlex.quote(ROOT)}",
        f"test ! -e {shlex.quote(receipt)}",
    ]
    for number, source in enumerate(SOURCES):
        destination = f"{ROOT}/payload-{number}"
        stage = destination + ".stage"
        source_manifest = f"{ROOT}/payload-{number}.source.sha256"
        destination_manifest = f"{ROOT}/payload-{number}.destination.sha256"
        source_links = f"{ROOT}/payload-{number}.source.links"
        destination_links = f"{ROOT}/payload-{number}.destination.links"
        lines.extend([
            f"test -d {shlex.quote(source)}",
            f"test ! -e {shlex.quote(destination)}",
            f"test ! -e {shlex.quote(stage)}",
            f"cp -a --reflink=never --sparse=always {shlex.quote(source)} {shlex.quote(stage)}",
            f"(cd {shlex.quote(source)} && find . -type f -print0 | sort -z | xargs -0 -r sha256sum) >{shlex.quote(source_manifest)} & source_hash_pid=$!",
            f"(cd {shlex.quote(stage)} && find . -type f -print0 | sort -z | xargs -0 -r sha256sum) >{shlex.quote(destination_manifest)} & destination_hash_pid=$!",
            "wait $source_hash_pid",
            "wait $destination_hash_pid",
            f"cmp -s {shlex.quote(source_manifest)} {shlex.quote(destination_manifest)}",
            f"(cd {shlex.quote(source)} && find . -type l -printf '%p -> %l\\n' | sort) >{shlex.quote(source_links)}",
            f"(cd {shlex.quote(stage)} && find . -type l -printf '%p -> %l\\n' | sort) >{shlex.quote(destination_links)}",
            f"cmp -s {shlex.quote(source_links)} {shlex.quote(destination_links)}",
            f"mv {shlex.quote(stage)} {shlex.quote(destination)}",
        ])
    lines.extend([
        f"printf '%s\\n' format=ultimatefish-i08-resize-preservation-v1 unit={shlex.quote(UNIT)} >{shlex.quote(receipt + '.stage')}",
        f"sha256sum {shlex.quote(ROOT + '/payload-0.source.sha256')} {shlex.quote(ROOT + '/payload-1.source.sha256')} >>{shlex.quote(receipt + '.stage')}",
        f"mv {shlex.quote(receipt + '.stage')} {shlex.quote(receipt)}",
        f"sync -f {shlex.quote(ROOT)}",
        "printf '%s\\n' I08_RESIZE_CHECKPOINTS_AUTHENTICATED",
    ])
    command = "\n".join(lines) + f" >>{shlex.quote(log)} 2>&1"
    commands = [
        "set -euo pipefail",
        f"systemctl stop {shlex.quote(UNIT)}",
        f"! systemctl is-active --quiet {shlex.quote(UNIT)}",
        f"test $(df -B1 --output=avail /mnt/ultimatefish-devil-v11 | tail -1) -ge 1000000000000",
        f"systemd-run --quiet --collect --unit={copy_unit} --property=AllowedCPUs=0,1 --property=Nice=10 --property=MemoryMax=4294967296 /bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {copy_unit}.service -p ActiveState -p SubState -p Result --no-pager",
    ]
    return remote.send(INSTANCE, [f"/bin/bash -lc {shlex.quote(chr(10).join(commands))}"], timeout=300)


def status() -> str:
    return remote.send(INSTANCE, [
        "systemctl show ultimatefish-preserve-i08-resize-v1.service -p ActiveState -p SubState -p Result -p ExecMainStatus --no-pager || true",
        f"tail -n 20 {shlex.quote(ROOT + '/preserve.log')} 2>/dev/null || true",
        f"cat {shlex.quote(ROOT + '/receipt.txt')} 2>/dev/null || true",
        f"du -sh {shlex.quote(ROOT)} 2>/dev/null || true",
    ], timeout=300)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    args = parser.parse_args()
    print(launch() if args.launch else status())


if __name__ == "__main__":
    main()
