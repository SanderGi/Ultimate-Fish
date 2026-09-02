#!/usr/bin/env python3
"""Restore and authenticate i-08 scratch payloads after its resize."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as remote
import preserve_ultimate_i08_resize_aws as preserve


DESTINATIONS = (
    "/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostkprince-fresh-v6",
    "/mnt/ultimatefish/ghost-parallel-v6/kghostkprince-canary-striped-bdd-b-8w",
)


def launch() -> str:
    unit = "ultimatefish-restore-i08-resize-v1"
    log = f"{preserve.ROOT}/restore.log"
    receipt = f"{preserve.ROOT}/restore-receipt.txt"
    lines = ["set -euo pipefail", f"test -s {shlex.quote(preserve.ROOT + '/receipt.txt')}" ]
    for number, destination in enumerate(DESTINATIONS):
        source = f"{preserve.ROOT}/payload-{number}"
        stage = destination + ".stage"
        restored_manifest = f"{preserve.ROOT}/payload-{number}.restored.sha256"
        restored_links = f"{preserve.ROOT}/payload-{number}.restored.links"
        lines.extend([
            f"test -d {shlex.quote(source)}",
            f"test ! -e {shlex.quote(destination)}",
            f"test ! -e {shlex.quote(stage)}",
            f"install -d -m 0755 {shlex.quote(destination.rsplit('/', 1)[0])}",
            f"cp -a --reflink=never --sparse=always {shlex.quote(source)} {shlex.quote(stage)}",
            f"(cd {shlex.quote(stage)} && find . -type f -print0 | sort -z | xargs -0 -r sha256sum) >{shlex.quote(restored_manifest)}",
            f"cmp -s {shlex.quote(preserve.ROOT + f'/payload-{number}.source.sha256')} {shlex.quote(restored_manifest)}",
            f"(cd {shlex.quote(stage)} && find . -type l -printf '%p -> %l\\n' | sort) >{shlex.quote(restored_links)}",
            f"cmp -s {shlex.quote(preserve.ROOT + f'/payload-{number}.source.links')} {shlex.quote(restored_links)}",
            f"mv {shlex.quote(stage)} {shlex.quote(destination)}",
        ])
    lines.extend([
        f"printf '%s\\n' format=ultimatefish-i08-resize-restore-v1 >{shlex.quote(receipt + '.stage')}",
        f"sha256sum {shlex.quote(preserve.ROOT + '/payload-0.restored.sha256')} {shlex.quote(preserve.ROOT + '/payload-1.restored.sha256')} >>{shlex.quote(receipt + '.stage')}",
        f"mv {shlex.quote(receipt + '.stage')} {shlex.quote(receipt)}",
        f"sync -f /mnt/ultimatefish",
        "printf '%s\\n' I08_RESIZE_RESTORE_AUTHENTICATED",
    ])
    command = "\n".join(lines) + f" >>{shlex.quote(log)} 2>&1"
    commands = [
        "set -euo pipefail",
        "test $(findmnt -n -o SOURCE /mnt/ultimatefish) = /dev/nvme3n1",
        f"test ! -e {shlex.quote(receipt)}",
        f"systemd-run --quiet --collect --unit={unit} --property=AllowedCPUs=48,49 --property=Nice=10 --property=MemoryMax=4294967296 /bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result --no-pager",
    ]
    return remote.send(preserve.INSTANCE, [f"/bin/bash -lc {shlex.quote(chr(10).join(commands))}"], timeout=300)


def status() -> str:
    return remote.send(preserve.INSTANCE, [
        "systemctl show ultimatefish-restore-i08-resize-v1.service -p ActiveState -p SubState -p Result -p ExecMainStatus --no-pager || true",
        f"tail -n 20 {shlex.quote(preserve.ROOT + '/restore.log')} 2>/dev/null || true",
        f"cat {shlex.quote(preserve.ROOT + '/restore-receipt.txt')} 2>/dev/null || true",
        "du -sh /mnt/ultimatefish 2>/dev/null || true",
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
