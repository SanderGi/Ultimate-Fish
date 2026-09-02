#!/usr/bin/env python3
"""Build and preserve the Prince-compatible parallel unique-rehash binary."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "c1013b4dda5fb0f8b89c3936c2f880efa4184d726acda78381f043480e873e3f"
)
build_base.SOURCE_VERSION = "4t7_NHtAmilXTHxe_SBQ8LAJhhlORfFf"
build_base.SOURCE_KEY = (
    "sources/bundles/ghost-unique-rehash-v12/sha256/"
    f"{build_base.SOURCE_SHA256}/ultimatefish-ghost-unique-rehash-v12-source.tar"
)
build_base.SEMANTICS = "ghost-v6-compatible-exact-parallel-unique-rehash-v12"
build_base.GENERATION = "v12"
build_base.EXTRA_DEFINES = "-DULTIMATE_GHOST_EXTRA_SUBSTATES=2"
build_base.PRIMARY_DEFINE = ""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, "prince"))


if __name__ == "__main__":
    main()
