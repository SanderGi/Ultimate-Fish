#!/usr/bin/env python3
"""Build and preserve the exact Checker-v4-compatible parallel binary."""

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
    "checker-v4-exact-model-parallel-bellman-symbolic-singleton-compact-v18"
)
build_base.GENERATION = "v18"
build_base.EXTRA_DEFINES = (
    "-DULTIMATE_GHOST_EXTRA_SUBSTATES=4 "
    "-DULTIMATE_GHOST_EXTRA_IS_CHECKER "
    "-DULTIMATE_GHOST_EXTRA_HORIZONTAL_ONLY "
    "-DULTIMATE_GHOST_ORDINARY_LOWER_DRAW_ONLY"
)
# The opposed K+Ghost vs K+Checker v4 checkpoint has the ordinary Checker as
# the extra piece.  EXTRA_PRIMARY reverses that contract and makes the binary
# reject the authenticated transition header before opening the checkpoint.
build_base.PRIMARY_DEFINE = ""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, "checker"))


if __name__ == "__main__":
    main()
