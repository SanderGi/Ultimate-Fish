#!/usr/bin/env python3
"""Build and preserve v19 Ghost variants with exact tail work stealing."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "d8f3180c406ef51631a7f72b042fadbfe1c307fbf4c3b9afeab4a64f226d996f"
)
build_base.SOURCE_VERSION = "XdBcy75hA5OW3iqg850H0niLoY4mQSsi"
build_base.SOURCE_KEY = (
    "sources/bundles/checker-parallel-v19/sha256/"
    f"{build_base.SOURCE_SHA256}/ultimatefish-checker-parallel-v19-source.tar"
)
build_base.SEMANTICS = (
    "ghost-exact-parallel-bellman-symbolic-singleton-compaction-"
    "one-geometry-work-stealing-v19"
)
build_base.GENERATION = "v19"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument(
        "--piece", choices=("ninja", "prince", "queen"), required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, args.piece))


if __name__ == "__main__":
    main()
