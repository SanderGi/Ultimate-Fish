#!/usr/bin/env python3
"""Resume D3 with parallel reverse buckets after adopting durable bucket 1."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base


SOURCE_SHA256 = "dcf75b3db3fd28ca3d421ae1d163d1f96823e315463f43c78e653dca95fe202b"
SOURCE_VERSION = "rtcW3_tly5fOgmaZrKkZy30PhBblWJ24"
SOURCE_KEY = ("sources/bundles/devil-spawned-solver-v31/sha256/" +
              SOURCE_SHA256 + "/ultimatefish-devil-spawned-solver-v31-source.tar")
BINARY_SHA256 = "2feaff489370e7d29ce467cda786680949cc82bd0b0c4bf8f4d1f4780aa3d9d3"
BINARY_VERSION = "OvpakWhY6.aVEEOlf.oBaQ6Ny16j10d7"
BINARY_KEY = ("sources/binaries/devil-spawned-solver-v31/sha256/" +
              BINARY_SHA256 + "/ultimate_tablebase-devil-spawned-v31")
OLD_UNIT = "ultimatefish-devil-stateful-recompute-v1-square-19.service"
UNIT = "ultimatefish-devil-stateful-parallel-v31-square-19.service"
ROOT = "/mnt/ultimatefish-devil-v11/devil-stateful-recompute-v1"
WORK = ROOT + "/graphs/square-19"
LOG = ROOT + "/logs/square-19.log"
STAGE = ROOT + "/stage-v31"
MEMORY_HIGH = 412316860416  # 384 GiB on the resized 512-GiB host.
MEMORY_MAX = 481036337152   # 448 GiB, retaining 64 GiB for the host.


def resume(instance: str) -> str:
    source = STAGE + "/source.tar"
    binary = STAGE + "/ultimate_tablebase-devil-spawned-v31"
    command = (
        f"exec {shlex.quote(binary)} --piece devil --workers 64 "
        "--solve-devil-spawned-square 19 --devil-spawned-limit 6000000000 "
        "--devil-spawned-hash-capacity 6000000000 "
        f"--devil-spawned-work {shlex.quote(WORK)} >>{shlex.quote(LOG)} 2>&1"
    )
    return base.send(instance, [
        "set -euo pipefail",
        f"test -s {shlex.quote(WORK + '/devil-19.closure')}",
        f"test ! -s {shlex.quote(WORK + '/devil-19.frontier')}",
        f"test -s {shlex.quote(WORK + '/devil-19.keys')}",
        f"test -s {shlex.quote(WORK + '/devil-19.nodes')}",
        f"test ! -s {shlex.quote(WORK + '/devil-19.roots')}",
        f"test \"$(find {shlex.quote(WORK + '/devil-19.reverse-spool')} "
        "-maxdepth 1 -name 'shard-*.complete' -type f | wc -l)\" -eq 512",
        f"grep -Fqx 'devil_reverse_merge square 19 bucket 1/16 sequential 1' "
        f"{shlex.quote(LOG)}",
        f"install -d -m 0755 {shlex.quote(STAGE)}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(SOURCE_KEY)} --version-id {SOURCE_VERSION} "
        f"{shlex.quote(source)} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(source)} | cut -d ' ' -f1)\" = "
        f"{SOURCE_SHA256}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(BINARY_KEY)} --version-id {BINARY_VERSION} "
        f"{shlex.quote(binary)} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)\" = "
        f"{BINARY_SHA256}",
        f"chmod 0755 {shlex.quote(binary)}",
        f"systemctl stop {OLD_UNIT}",
        f"systemctl is-active --quiet {OLD_UNIT} && exit 71 || true",
        f"systemd-run --quiet --collect --unit={UNIT.removesuffix('.service')} "
        "--property=AllowedCPUs=0-63 --property=Nice=5 "
        "--property=MemoryHigh=343597383680 --property=MemoryMax=412316860416 "
        "--property=OOMPolicy=stop "
        "--setenv=ULTIMATE_TABLEBASE_PRESERVE_SCRATCH=1 "
        "--setenv=ULTIMATE_DEVIL_REVERSE_ADOPT_BUCKETS=1 "
        f"/bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {UNIT} -p ActiveState -p SubState -p AllowedCPUs "
        "-p MemoryHigh -p MemoryMax --no-pager",
        f"printf '%s\n' source_sha256={SOURCE_SHA256} "
        f"source_version={SOURCE_VERSION} binary_sha256={BINARY_SHA256} "
        f"binary_version={BINARY_VERSION} adopted_bucket=1 workers=64",
    ], timeout=600)


def status(instance: str) -> str:
    return base.send(instance, [
        f"systemctl show {UNIT} -p ActiveState -p SubState -p Result "
        "-p AllowedCPUs -p MemoryCurrent --no-pager || true",
        f"cat /sys/fs/cgroup/system.slice/{UNIT}/memory.events || true",
        f"systemctl status {UNIT} --no-pager --lines=5 || true",
        f"ps -L -p $(systemctl show {UNIT} -p MainPID --value) "
        "-o pid,tid,psr,pcpu,rss,stat,comm --sort=-pcpu | head -n 70 || true",
        f"pid=$(systemctl show {UNIT} -p MainPID --value); "
        "test \"$pid\" = 0 || printf 'wchan=%s\\n' \"$(cat /proc/$pid/wchan)\"",
        f"pid=$(systemctl show {UNIT} -p MainPID --value); "
        "test \"$pid\" = 0 || sed -n '1,7p' /proc/$pid/io",
        f"pid=$(systemctl show {UNIT} -p MainPID --value); "
        "test \"$pid\" = 0 || grep -E '^(Rss|Pss|Private_Dirty|Swap):' "
        "/proc/$pid/smaps_rollup",
        f"stat -c 'log_bytes=%s log_mtime=%Y' {shlex.quote(LOG)} || true",
        "findmnt -no SOURCE,TARGET,FSTYPE,OPTIONS /mnt/ultimatefish-devil-v11 || true",
        "df -B1 /mnt/ultimatefish-devil-v11 || true",
        "lsblk -o NAME,SIZE,TYPE,MOUNTPOINTS || true",
        "iostat -dx 1 2 2>/dev/null | tail -n 30 || true",
        f"printf 'parallel_buckets_complete='; grep -c "
        f"'devil_reverse_merge square 19 bucket .* parallel 16' {shlex.quote(LOG)} || true",
        f"grep -E 'devil_reverse_(adopt|sort|sequential|merge)|devil_(degree_sum|propagate|verify)|DEVIL_SPAWNED' "
        f"{shlex.quote(LOG)} | tail -n 40 || true",
        f"find {shlex.quote(WORK)} -maxdepth 1 -type f -printf '%f %s\\n' "
        "| sort || true",
    ])


def raise_memory(instance: str) -> str:
    """Raise live D3 gates without restarting or invalidating retained state."""
    return base.send(instance, [
        f"systemctl is-active --quiet {UNIT}",
        f"systemctl set-property --runtime {UNIT} "
        f"MemoryHigh={MEMORY_HIGH} MemoryMax={MEMORY_MAX}",
        f"systemctl show {UNIT} -p ActiveState -p MainPID -p MemoryCurrent "
        "-p MemoryHigh -p MemoryMax --no-pager",
    ])


def phase(instance: str) -> str:
    """Return a compact phase snapshot suitable for frequent supervision."""
    return base.send(instance, [
        f"systemctl show {UNIT} -p ActiveState -p SubState -p Result "
        "-p MainPID -p MemoryCurrent --no-pager || true",
        f"grep -E 'devil_(propagate|verify)|DEVIL_SPAWNED' {shlex.quote(LOG)} "
        "| tail -n 3 || true",
        f"cat /sys/fs/cgroup/system.slice/{UNIT}/memory.events || true",
    ])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--resume", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--raise-memory", action="store_true")
    mode.add_argument("--phase", action="store_true")
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    if args.resume:
        result = resume(args.instance)
    elif args.raise_memory:
        result = raise_memory(args.instance)
    elif args.phase:
        result = phase(args.instance)
    else:
        result = status(args.instance)
    print(result)


if __name__ == "__main__":
    main()
