#!/usr/bin/env python3
"""Build the fully routed parallel Ghost/Penguin v28 executable on AWS."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "2f48ca98b640546617aed314e7c7d4da34333dd904bafeddefbd6e6477c0c19c"
)
build_base.SOURCE_VERSION = "UDSC8i2Vh88n3E19oldfET4LYA3swsfa"
build_base.SOURCE_KEY = (
    "sources/bundles/penguin-worker-options-v28/sha256/"
    f"{build_base.SOURCE_SHA256}/"
    "ultimatefish-penguin-worker-options-v28-source.tar"
)
build_base.SEMANTICS = (
    "penguin-ghost-primary-action-conditioned-v21-with-driver-and-"
    "solve-options-parallel-worker-routing-v28"
)
build_base.GENERATION = "v28"
build_base.EXTRA_DEFINES = (
    "-DULTIMATE_GHOST_EXTRA_SUBSTATES=8 "
    "-DULTIMATE_GHOST_ORDINARY_LOWER_SUBSTATES=4"
)
build_base.PRIMARY_DEFINE = ""
build_base.PIECES["penguin"] = "Penguin"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, "penguin"))


if __name__ == "__main__":
    main()
