#!/usr/bin/env python3
"""Build and preserve material variants of the exact v18 Ghost verifier."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "8a5cc9d4527059f24ebbbee79e115b433bfa2d052456dd61d10aaae8bd964a1c"
)
build_base.SOURCE_VERSION = "vJRY8OeB2K.OX0KC3MFFkPEKKj_mgWtL"
build_base.SOURCE_KEY = (
    "sources/bundles/checker-parallel-v18/sha256/"
    f"{build_base.SOURCE_SHA256}/ultimatefish-checker-parallel-v18-source.tar"
)
build_base.SEMANTICS = (
    "ghost-exact-parallel-bellman-symbolic-singleton-compaction-v18"
)
build_base.GENERATION = "v18"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument(
        "--piece", choices=("ninja", "prince", "queen"), required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, args.piece))


if __name__ == "__main__":
    main()
