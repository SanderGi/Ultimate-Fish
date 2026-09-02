#!/usr/bin/env python3
"""Resume retained Devil checkpoints with byte-aligned v22 fingerprints."""

from __future__ import annotations

import argparse

import launch_ultimate_devil_spawned_solver_aws as remote
import resume_ultimate_devil_companion_solver_v21_aws as v21


SOURCE_SHA256 = "9397ee8c61ee304537501f41de0dc5ea9c8f865ceefee629467189d3fa8cee66"
SOURCE_VERSION = "3cCJGxMgPPAFSBoDkxbVQfMthZnLAirD"
SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v22/sha256/{SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v22-source.tar"
)
BINARY_SHA256 = "6395376493f785f0a1b1bf7b7c0d6d1db2fd91432533f8466f35ba22c3f861c6"
BINARY_VERSION = "fzyzRvFd..D03LOsKap4UT4EYpuB0ZK2"
BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v22/sha256/{BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v22"
)
ROOT = f"/mnt/ultimatefish/devil-spawned-solver-v22-{SOURCE_SHA256[:8]}"


def _configure() -> None:
    v21.SOURCE_SHA256 = SOURCE_SHA256
    v21.SOURCE_VERSION = SOURCE_VERSION
    v21.SOURCE_KEY = SOURCE_KEY
    v21.BINARY_SHA256 = BINARY_SHA256
    v21.BINARY_VERSION = BINARY_VERSION
    v21.BINARY_KEY = BINARY_KEY
    v21.ROOT = ROOT


def _names(piece: str, opposed: bool, square: int,
           volume: str) -> tuple[str, str, str, str]:
    orientation = "opposed" if opposed else "same"
    work = (f"{volume.rstrip('/')}/devil-companion-v15-34aa7f01/"
            f"{orientation}-{piece}/square-{square}")
    unit = f"ultimatefish-devil-companion-v22-{orientation}-{piece}-square-{square}"
    log = (f"{volume.rstrip('/')}/devil-companion-v15-34aa7f01/logs/"
           f"{orientation}-{piece}-square-{square}-v22.log")
    old_unit = f"ultimatefish-devil-companion-v21-{orientation}-{piece}-square-{square}"
    return unit, work, log, old_unit


def resume(instance: str, piece: str, opposed: bool, square: int,
           cpus: tuple[int, ...], limit: int, hash_capacity: int,
           volume: str) -> str:
    _configure()
    v21.names = _names
    return v21.resume(instance, piece, opposed, square, cpus, limit,
                      hash_capacity, volume)


def status(instance: str, piece: str, opposed: bool, square: int,
           volume: str) -> str:
    _configure()
    v21.names = _names
    return v21.status(instance, piece, opposed, square, volume)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--status", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", default="bishop")
    parser.add_argument("--opposed", action="store_true")
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--cpus")
    parser.add_argument("--limit", type=int)
    parser.add_argument("--hash-capacity", type=int)
    parser.add_argument("--volume", required=True)
    args = parser.parse_args()
    if args.status:
        print(status(args.instance, args.piece, args.opposed, args.square,
                     args.volume))
    else:
        if not args.cpus or args.limit is None or args.hash_capacity is None:
            parser.error(
                "--cpus, --limit, and --hash-capacity are required when resuming")
        print(resume(args.instance, args.piece, args.opposed, args.square,
                     remote.parse_cpus(args.cpus), args.limit,
                     args.hash_capacity, args.volume))


if __name__ == "__main__":
    main()
