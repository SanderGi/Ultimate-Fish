#!/usr/bin/env python3
"""Launch a fresh certifying companion-Devil partition with solver v11."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base
import launch_ultimate_devil_companion_solver_v8_aws as v8


SOURCE_SHA256 = "bb04e578ce251a304bca3eafd329ed49a04aed7656a29f7431fef8a1d7cca400"
SOURCE_VERSION = "vKTPMe4bbaWdgkmQ6ms8CkP9AIK7g0Fi"
SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v11/sha256/{SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v11-source.tar"
)
BINARY_SHA256 = "db53f32f74d536eaa07fc514cafd1ae330eb3747c619e5b5c73058827fc1feee"
BINARY_VERSION = "lAif3Ta98i75vM4crpnL5rQv_8PEDfwU"
BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v11/sha256/{BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v11"
)
STATELESS = v8.STATELESS
ROOT = f"/mnt/ultimatefish/devil-spawned-solver-v11-{SOURCE_SHA256[:8]}"
DEFAULT_VOLUME = "/mnt/ultimatefish"
# I08's 3-TiB gp3 profile showed the 192-GB sparse key plane thrashing at the
# exact 16,000-IOPS ceiling while this cgroup sat on its former 190-GiB soft
# limit.  Prince/Ghost uses 14 GiB and is hard-capped separately at 24 GiB, so
# Prince/Ghost uses only about 14 GiB and remains hard-capped at 24 GiB.  The
# Devil hard cap therefore still keeps both units below physical RAM with an
# 8-GiB OS margin.  C1 was pinned exactly at the former 205-GiB soft ceiling
# with 0.5/24 busy CPUs, so admit another 7 GiB of reclaimable key-page cache
# without changing that hard safety bound.
MEMORY_HIGH = 227_633_266_688
MEMORY_MAX = 230_854_492_160


def names(piece: str, opposed: bool, square: int,
          volume: str = DEFAULT_VOLUME) -> tuple[str, str, str]:
    orientation = "opposed" if opposed else "same"
    stem = f"{orientation}-{piece}-square-{square}"
    work_root = f"{volume.rstrip('/')}/devil-companion-v11-{SOURCE_SHA256[:8]}"
    return (
        f"ultimatefish-devil-companion-v11-{stem}",
        f"{work_root}/{orientation}-{piece}/square-{square}",
        f"{work_root}/logs/{stem}.log",
    )


def stage_commands() -> list[str]:
    source = f"{ROOT}/source.tar"
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v11"
    return [
        f"install -d -m 0755 {shlex.quote(ROOT)}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(SOURCE_KEY)} --version-id {shlex.quote(SOURCE_VERSION)} "
        f"{shlex.quote(source)} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(source)} | cut -d ' ' -f1)\" = "
        f"{SOURCE_SHA256}",
        # An active service holds the immutable executable open.  Verify and
        # reuse that exact binary instead of provoking ETXTBSY by downloading
        # over it; only a missing/mismatched path is installed atomically.
        f"if ! test -f {shlex.quote(binary)} || ! test \"$(sha256sum "
        f"{shlex.quote(binary)} | cut -d ' ' -f1)\" = {BINARY_SHA256}; then "
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(BINARY_KEY)} --version-id {shlex.quote(BINARY_VERSION)} "
        f"{shlex.quote(binary + '.stage')} >/dev/null; "
        f"test \"$(sha256sum {shlex.quote(binary + '.stage')} | cut -d ' ' -f1)\" = "
        f"{BINARY_SHA256}; chmod 0755 {shlex.quote(binary + '.stage')}; "
        f"mv -f {shlex.quote(binary + '.stage')} {shlex.quote(binary)}; fi",
        f"chmod 0755 {shlex.quote(binary)}",
    ]


def launch(instance: str, piece: str, opposed: bool, square: int,
           cpus: tuple[int, ...], limit: int,
           volume: str = DEFAULT_VOLUME) -> str:
    if piece not in STATELESS:
        raise ValueError("v11 companion must have a stateless one-square codec")
    if square not in base.FIXED_SQUARES:
        raise ValueError("fixed Devil square must be on files a-d of ranks 1-3")
    if limit <= 0 or limit >= 2**63:
        raise ValueError("invalid Devil companion graph limit")
    if not (volume == "/mnt/checkpoint-migrate" or
            volume.startswith("/mnt/ultimatefish")):
        raise ValueError("Devil companion work volume is not authorized")
    if not cpus or len(set(cpus)) != len(cpus):
        raise ValueError("Devil companion CPU allocation must be nonempty and unique")
    unit, work, log = names(piece, opposed, square, volume)
    work_root = f"{volume.rstrip('/')}/devil-companion-v11-{SOURCE_SHA256[:8]}"
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v11"
    cpu_set = ",".join(map(str, cpus))
    orientation = " --opposing" if opposed else ""
    command = (
        f"exec {shlex.quote(binary)} --piece devil --piece2 {piece}{orientation} "
        f"--workers {len(cpus)} --solve-devil-spawned-square {square} "
        f"--devil-spawned-limit {limit} --devil-spawned-work {shlex.quote(work)} "
        f">>{shlex.quote(log)} 2>&1"
    )
    commands = [
        "set -euo pipefail",
        *stage_commands(),
        f"install -d -m 0755 {shlex.quote(work)} {shlex.quote(work_root + '/logs')}",
        f"test ! -e {shlex.quote(work + f'/devil-{square}.roots')}",
        f"systemctl is-active --quiet {unit}.service || systemd-run --quiet "
        f"--collect --unit={unit} --property=AllowedCPUs={cpu_set} "
        f"--property=Nice=5 --property=MemoryHigh={MEMORY_HIGH} "
        f"--property=MemoryMax={MEMORY_MAX} --property=OOMPolicy=stop "
        "--setenv=ULTIMATE_TABLEBASE_PRESERVE_SCRATCH=1 "
        f"/bin/bash -lc {shlex.quote(command)}",
        # Keep an already-running checkpointed service in place while applying
        # reviewed cache-gate repairs; systemctl set-property updates the
        # transient drop-in without replacing its PID.
        f"systemctl set-property --runtime {unit}.service "
        f"MemoryHigh={MEMORY_HIGH} MemoryMax={MEMORY_MAX}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "-p AllowedCPUs -p MemoryHigh -p MemoryMax --no-pager",
        f"printf '%s\\n' source_sha256={SOURCE_SHA256} "
        f"source_version={SOURCE_VERSION} binary_sha256={BINARY_SHA256} "
        f"binary_version={BINARY_VERSION}",
    ]
    return base.send(instance, commands, timeout=300)


def status(instance: str, piece: str, opposed: bool, square: int,
           volume: str = DEFAULT_VOLUME) -> str:
    unit, work, log = names(piece, opposed, square, volume)
    return base.send(instance, [
        f"systemctl status {unit}.service --no-pager -l || true",
        f"tail -n 40 {shlex.quote(log)} || true",
        f"du -sh {shlex.quote(work)} || true",
    ], timeout=300)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", required=True, choices=sorted(STATELESS))
    parser.add_argument("--opposed", action="store_true")
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--cpus", default="0-7,16-31")
    parser.add_argument("--limit", type=int, default=12_000_000_000)
    parser.add_argument("--volume", default=DEFAULT_VOLUME)
    args = parser.parse_args()
    if args.launch:
        print(launch(args.instance, args.piece, args.opposed, args.square,
                     base.parse_cpus(args.cpus), args.limit, args.volume))
    else:
        print(status(args.instance, args.piece, args.opposed, args.square,
                     args.volume))


if __name__ == "__main__":
    main()
