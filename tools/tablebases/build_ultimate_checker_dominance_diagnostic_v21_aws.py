#!/usr/bin/env python3
"""Build and preserve the Checker v25 exact-witness diagnostic."""

from __future__ import annotations

import argparse

import build_ultimate_checker_parallel_v20_aws as build_base_config


build_base = build_base_config.build_base
build_base.SOURCE_SHA256 = (
    "75bea36f83b8a8236d6d68cb3e231bd1e1970ddf8e1b46b23d30c11ef241fa9b"
)
build_base.SOURCE_VERSION = "gBncetl3X3FlFSjCeRIbrm574741hodn"
build_base.SOURCE_KEY = (
    "sources/bundles/checker-exact-witness-diagnostic-v25/sha256/"
    f"{build_base.SOURCE_SHA256}/"
    "ultimatefish-checker-exact-witness-diagnostic-v25-source.tar"
)
build_base.SEMANTICS = (
    "checker-impossible-forced-padding-exact-witness-diagnostic-v25"
)
build_base.GENERATION = "v25"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, "checker"))


if __name__ == "__main__":
    main()
