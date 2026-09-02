#!/usr/bin/env python3
"""Build and preserve the Prince-v23-compatible unique-rehash binary."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "b52be3f59d993d669481a51cc0c6f138db25280a55880c61e7c956206b1ca957"
)
build_base.SOURCE_VERSION = "ChV.rPfi0n49WyhQTufUuFvLe.NKtFD3"
build_base.SOURCE_KEY = (
    "sources/bundles/ghost-prince-unique-rehash-v16/sha256/"
    f"{build_base.SOURCE_SHA256}/"
    "ultimatefish-ghost-prince-unique-rehash-v16-source.tar"
)
build_base.SEMANTICS = (
    "prince-v23-compatible-exact-parallel-expanded-unique-index-v16"
)
build_base.GENERATION = "v16"
build_base.EXTRA_DEFINES = "-DULTIMATE_GHOST_EXTRA_SUBSTATES=2"
build_base.PRIMARY_DEFINE = ""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, "prince"))


if __name__ == "__main__":
    main()
