#!/usr/bin/env python3
"""Build and preserve the exact parallel same-side Pawn/Ghost binary."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "44ed7ae2896d4149ffae401b7ebc41ac955b0c20a0bfbe25f6a7d0f05c4fd9b0"
)
build_base.SOURCE_VERSION = "i1reqLi9pZmc2bWdB1dMOtuSV4BtrcM0"
build_base.SOURCE_KEY = (
    "sources/bundles/pawn-parallel-v25/sha256/"
    f"{build_base.SOURCE_SHA256}/ultimatefish-pawn-parallel-v25-source.tar"
)
build_base.SEMANTICS = (
    "pawn-ghost-same-horizontal-two-substate-promotion-bound-exact-"
    "transition-compatible-parallel-action-conditioned-v25"
)
build_base.GENERATION = "v25"
build_base.PIECES["pawn"] = "Pawn"
build_base.PRIMARY_DEFINE = "-DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY"
build_base.EXTRA_DEFINES = (
    "-DULTIMATE_GHOST_EXTRA_SUBSTATES=2 "
    "-DULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN "
    "-DULTIMATE_GHOST_EXTRA_HORIZONTAL_ONLY"
)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, "pawn"))


if __name__ == "__main__":
    main()
