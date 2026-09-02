#!/usr/bin/env python3
"""Build and preserve the Prince v23-compatible 192 GiB-gate binary."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "29247cdf9878bd74a04535e83cde46105ee4c19a7d83737a7be303fe8e44fcd4"
)
build_base.SOURCE_VERSION = "uEooAJ6z8.HqB.vlZnuxWaANIVcaeMh6"
build_base.SOURCE_KEY = (
    "sources/bundles/ghost-prince-capacity-v18/sha256/"
    f"{build_base.SOURCE_SHA256}/ultimatefish-ghost-prince-capacity-v18-source.tar"
)
build_base.SEMANTICS = (
    "prince-v23-exact-parallel-unique-rehash-192g-gate-v18"
)
build_base.GENERATION = "v18"
build_base.EXTRA_DEFINES = "-DULTIMATE_GHOST_EXTRA_SUBSTATES=2"
build_base.PRIMARY_DEFINE = ""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, "prince"))


if __name__ == "__main__":
    main()
