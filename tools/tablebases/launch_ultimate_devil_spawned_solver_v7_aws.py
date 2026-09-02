#!/usr/bin/env python3
"""Stage, launch, and inspect the wide-index spawned-only Devil solver v7."""

from __future__ import annotations

import argparse

import launch_ultimate_devil_spawned_solver_aws as base


base.SOURCE_SHA256 = "8d9fc6dfc8b648c343f442a4cf7a0c0eb8c0d118cd826584c65b22f93bc28f24"
base.SOURCE_VERSION = "LSgUY04193XBQ4mA4SDpZjCwb9xxjO2b"
base.SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v7/sha256/{base.SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v7-source.tar"
)
base.BINARY_SHA256 = "9e3788b73537ba879bc7b6c9a2f3444cfd521d5c18c74d6b1bd952f04d7b7a0f"
base.BINARY_VERSION = "r8lnVHNE8kAH6745DhT0187b52EDdC_5"
base.BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v7/sha256/{base.BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v7"
)
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "max-five-proved-retained-reverse-edges-bellman-parallel-resume-"
    "anonymous-slots-wide-indices-v7"
)
base.UNIT_PREFIX = "ultimatefish-devil-spawned-v7-square-"
base.GENERATION = "v7"
base.MAX_LIMIT = 2**63
base.WORK_ROOT_OVERRIDE = "/mnt/ultimatefish/devil-spawned-solver-v6-b3a358fe"
base.MEMORY_HIGH = 161061273600
base.MEMORY_MAX = 171798691840


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--preserve", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--cpus", default="0-31")
    parser.add_argument("--volume", default="/mnt/ultimatefish")
    parser.add_argument("--limit", type=int, default=6_000_000_000)
    args = parser.parse_args()
    if args.launch:
        print(base.launch(args.instance, args.square, base.parse_cpus(args.cpus),
                          args.volume, args.limit))
    elif args.preserve:
        print(base.preserve(args.instance, args.square, args.volume))
    else:
        print(base.status(args.instance, args.square, args.volume))


if __name__ == "__main__":
    main()
