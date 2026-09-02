#!/usr/bin/env python3
"""Launch and inspect one certifying stateless-companion Devil v8 partition."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base


SOURCE_SHA256 = "f2103f6a7b0bd2dcb37ee230dc7cd44bc921d98ac5547a239527e082b1b88862"
SOURCE_VERSION = ".OXQMrrqEu.wQh2l59TOf7wNSdRgNc4R"
SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v8/sha256/{SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v8-source.tar"
)
BINARY_SHA256 = "d47ed8a11ee2454585fdec96bd57d49a038cfb20d675a17e9faa4e566022a7eb"
BINARY_VERSION = "32Mp5k.oy0RK0Npej8yscADfaEPOBu5p"
BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v8/sha256/{BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v8"
)
STATELESS = frozenset({
    "jester", "knight", "queen", "rook", "bishop", "bomb", "ninja",
    "turtle", "parasite", "mage", "giant", "fisherman", "dragon",
})
ROOT = f"/mnt/ultimatefish/devil-spawned-solver-v8-{SOURCE_SHA256[:8]}"
DEFAULT_VOLUME = "/mnt/ultimatefish"


def names(piece: str, opposed: bool, square: int,
          volume: str = DEFAULT_VOLUME) -> tuple[str, str, str]:
    orientation = "opposed" if opposed else "same"
    stem = f"{orientation}-{piece}-square-{square}"
    work_root = (
        f"{volume.rstrip('/')}/devil-companion-v8-{SOURCE_SHA256[:8]}"
    )
    return (f"ultimatefish-devil-companion-v8-{stem}",
            f"{work_root}/{orientation}-{piece}/square-{square}",
            f"{work_root}/logs/{stem}.log")


def stage_commands() -> list[str]:
    source = f"{ROOT}/source.tar"
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v8"
    return [
        f"install -d -m 0755 {shlex.quote(ROOT)}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(SOURCE_KEY)} --version-id {shlex.quote(SOURCE_VERSION)} "
        f"{shlex.quote(source)} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(source)} | cut -d ' ' -f1)\" = "
        f"{SOURCE_SHA256}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(BINARY_KEY)} --version-id {shlex.quote(BINARY_VERSION)} "
        f"{shlex.quote(binary)} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)\" = "
        f"{BINARY_SHA256}",
        f"chmod 0755 {shlex.quote(binary)}",
    ]


def launch(instance: str, piece: str, opposed: bool, square: int,
           cpus: tuple[int, ...], limit: int,
           volume: str = DEFAULT_VOLUME) -> str:
    if piece not in STATELESS:
        raise ValueError("v8 companion must have a stateless one-square codec")
    if square not in base.FIXED_SQUARES:
        raise ValueError("fixed Devil square must be on files a-d of ranks 1-3")
    if limit <= 0 or limit >= 2**63:
        raise ValueError("invalid Devil companion graph limit")
    if not (volume == "/mnt/checkpoint-migrate" or
            volume.startswith("/mnt/ultimatefish")):
        raise ValueError("Devil companion work volume is not authorized")
    unit, work, log = names(piece, opposed, square, volume)
    work_root = f"{volume.rstrip('/')}/devil-companion-v8-{SOURCE_SHA256[:8]}"
    binary = f"{ROOT}/ultimate_tablebase-devil-spawned-v8"
    cpu_set = ",".join(map(str, cpus))
    orientation = " --opposing" if opposed else ""
    command = (
        f"exec {shlex.quote(binary)} --piece devil --piece2 {piece}{orientation} "
        f"--workers {len(cpus)} --solve-devil-spawned-square {square} "
        f"--devil-spawned-limit {limit} --devil-spawned-work {shlex.quote(work)} "
        f">>{shlex.quote(log)} 2>&1"
    )
    commands = ["set -euo pipefail", *stage_commands(),
        f"install -d -m 0755 {shlex.quote(work)} {shlex.quote(work_root + '/logs')}",
        f"test ! -e {shlex.quote(work + f'/devil-{square}.roots')}",
        f"systemctl is-active --quiet {unit}.service || systemd-run --quiet "
        f"--collect --unit={unit} --property=AllowedCPUs={cpu_set} "
        "--property=Nice=5 --property=MemoryHigh=161061273600 "
        "--property=MemoryMax=171798691840 --property=OOMPolicy=stop "
        "--setenv=ULTIMATE_TABLEBASE_PRESERVE_SCRATCH=1 "
        f"/bin/bash -lc {shlex.quote(command)}",
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
    parser.add_argument("--cpus", default="1-29,31")
    parser.add_argument("--limit", type=int, default=6_000_000_000)
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
