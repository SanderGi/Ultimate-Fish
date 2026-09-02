#!/usr/bin/env python3
"""Build, test, version-preserve, and restore-check Ghost v11 binaries."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "2d2036ae1e0029a2341a11dda4548bfaf504c85bf918156e89910aeb18aec4b0"
)
build_base.SOURCE_VERSION = "TL.n6_nbobhOgl8uC6UpC13K4TvikuRF"
build_base.SOURCE_KEY = (
    "sources/bundles/ghost-parallel-v11/sha256/"
    f"{build_base.SOURCE_SHA256}/ultimatefish-ghost-parallel-v11-source.tar"
)
build_base.SEMANTICS = "ghost-exact-parallel-expanded-unique-index-v11"
build_base.GENERATION = "v11"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", choices=sorted(build_base.PIECES), required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, args.piece))


if __name__ == "__main__":
    main()
