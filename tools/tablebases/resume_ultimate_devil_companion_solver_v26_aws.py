#!/usr/bin/env python3
"""Resume authenticated C1 on persistent gp3 with a 512-GiB envelope."""

from __future__ import annotations

import argparse
import re

import launch_ultimate_devil_spawned_solver_aws as remote
import resume_ultimate_devil_companion_solver_v23_aws as base


base.SOURCE_SHA256 = "19fa50ebfa1f5b0319fac5d3f3957d0f37ff5bbe600a22619e6bb93eaf631728"
base.SOURCE_VERSION = "XJkADC_Sn4K6eVKFjaSe6qkD6XmEbbsP"
base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v25/sha256/"
    f"{base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v25-source.tar"
)
base.BINARY_SHA256 = "5732a5db776e2572e1401ce5c4dc2fda90e11ad84ddbffcb3ddd1efdd435e6ec"
base.BINARY_VERSION = "yZElOHwKruG670rGNILIsCK9gZQ2SVKh"
base.BINARY_KEY = (
    "sources/binaries/devil-spawned-solver-v25/sha256/"
    f"{base.BINARY_SHA256}/ultimate_tablebase-devil-spawned-v25"
)
base.ROOT = (
    "/mnt/ultimatefish-devil-v11/devil-spawned-solver-v26-"
    f"{base.SOURCE_SHA256[:8]}"
)
WORK_ROOT = "/mnt/ultimatefish-devil-v11/devil-companion-v26-retained"
VOLUME = "/mnt/ultimatefish-devil-v11"
MEMORY_MAX = 498_216_206_336


def parse_cpus(value: str) -> tuple[int, ...]:
    cpus: set[int] = set()
    for item in value.split(","):
        match = re.fullmatch(r"([0-9]+)(?:-([0-9]+))?", item)
        if not match:
            raise ValueError(f"invalid CPU set: {value}")
        first = int(match.group(1))
        last = int(match.group(2) or first)
        if first > last or first < 0 or last >= 64:
            raise ValueError(f"invalid resized-host CPU set: {value}")
        cpus.update(range(first, last + 1))
    if not cpus:
        raise ValueError("CPU set is empty")
    return tuple(sorted(cpus))


def names(piece: str, opposed: bool, square: int,
          volume: str) -> tuple[str, str, str, str]:
    del volume
    orientation = "opposed" if opposed else "same"
    work = f"{WORK_ROOT}/{orientation}-{piece}/square-{square}"
    unit = f"ultimatefish-devil-companion-v26-{orientation}-{piece}-square-{square}"
    log = f"{WORK_ROOT}/logs/{orientation}-{piece}-square-{square}-v26.log"
    old_unit = f"ultimatefish-devil-companion-v24-{orientation}-{piece}-square-{square}"
    return unit, work, log, old_unit


def configure() -> None:
    base._configure()
    base.v21.MEMORY_HIGH = MEMORY_MAX
    base.v21.MEMORY_MAX = MEMORY_MAX
    base.v21.names = names


def resume(instance: str, piece: str, square: int, cpus: tuple[int, ...],
           limit: int, hash_capacity: int) -> str:
    configure()
    return base.v21.resume(instance, piece, False, square, cpus, limit,
                           hash_capacity, VOLUME)


def status(instance: str, piece: str, square: int) -> str:
    configure()
    return base.v21.status(instance, piece, False, square,
                           VOLUME)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--status", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", default="bishop")
    parser.add_argument("--square", type=int, required=True)
    parser.add_argument("--cpus")
    parser.add_argument("--limit", type=int)
    parser.add_argument("--hash-capacity", type=int)
    args = parser.parse_args()
    if args.status:
        print(status(args.instance, args.piece, args.square))
    else:
        if not args.cpus or args.limit is None or args.hash_capacity is None:
            parser.error("resume requires --cpus, --limit, and --hash-capacity")
        print(resume(args.instance, args.piece, args.square,
                     parse_cpus(args.cpus), args.limit,
                     args.hash_capacity))


if __name__ == "__main__":
    main()
