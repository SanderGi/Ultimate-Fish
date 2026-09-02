#!/usr/bin/env python3
"""Build Ghost v21 variants with action-conditioned owner images."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "99effde369bfa1a42b7fae1440827152fe30aac6187a674afb38335eeb52746a"
)
build_base.SOURCE_VERSION = "PbDDTMXOMXAi9hgHUW8xEQAzaiQf0Hgk"
build_base.SOURCE_KEY = (
    "sources/bundles/checker-parallel-v20/sha256/"
    f"{build_base.SOURCE_SHA256}/ultimatefish-checker-parallel-v20-source.tar"
)
build_base.SEMANTICS = (
    "ghost-exact-parallel-action-conditioned-owner-successor-images-v21"
)
build_base.GENERATION = "v21"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument(
        "--piece", choices=("ninja", "prince", "queen"), required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, args.piece))


if __name__ == "__main__":
    main()
