#!/usr/bin/env python3
"""Build and preserve the exact parallel opposed Ghost/Dragon binary."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "eaee7253ff9313290dd7b3436f7572466932177e503efd1389c347a4a8985d84"
)
build_base.SOURCE_VERSION = "jalHavk2h4QI0Cy267M3j_11brHWmWXi"
build_base.SOURCE_KEY = (
    "sources/bundles/ghost-dragon-parallel-v26/sha256/"
    f"{build_base.SOURCE_SHA256}/"
    "ultimatefish-ghost-dragon-parallel-v26-source.tar"
)
build_base.SEMANTICS = (
    "opposed-ghost-dragon-transition-compatible-action-conditioned-"
    "parallel-v26"
)
build_base.GENERATION = "v26"
build_base.PIECES["dragon"] = "Dragon"
build_base.PRIMARY_DEFINE = ""
build_base.EXTRA_DEFINES = ""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, "dragon"))


if __name__ == "__main__":
    main()
