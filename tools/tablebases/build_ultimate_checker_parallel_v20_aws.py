#!/usr/bin/env python3
"""Build and preserve Checker v21 with complete action-conditioned images."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "bcfda23c2a00580c1e4ecd08faf70447a1ae8ffe144f95bc36a074ecefbe562e"
)
build_base.SOURCE_VERSION = "ERHOaZ9324f9C4VvTmRkY9RZsqK1GG9b"
build_base.SOURCE_KEY = (
    "sources/bundles/checker-action-conditioned-v21/sha256/"
    f"{build_base.SOURCE_SHA256}/"
    "ultimatefish-checker-action-conditioned-v21-source.tar"
)
build_base.SEMANTICS = (
    "checker-v4-exact-complete-action-conditioned-observer-images-parallel-v21"
)
build_base.GENERATION = "v21"
build_base.EXTRA_DEFINES = (
    "-DULTIMATE_GHOST_EXTRA_SUBSTATES=4 "
    "-DULTIMATE_GHOST_EXTRA_IS_CHECKER "
    "-DULTIMATE_GHOST_EXTRA_HORIZONTAL_ONLY "
    "-DULTIMATE_GHOST_ORDINARY_LOWER_DRAW_ONLY"
)
build_base.PRIMARY_DEFINE = ""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, "checker"))


if __name__ == "__main__":
    main()
