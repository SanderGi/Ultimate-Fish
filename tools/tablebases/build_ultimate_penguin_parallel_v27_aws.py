#!/usr/bin/env python3
"""Build the CLI-correct parallel Ghost/Penguin v27 executable on AWS."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "eb8138a84cdf58a292f1eec2639c0d786500e70e6f4ed43297fab536c5373805"
)
build_base.SOURCE_VERSION = "G7xz30JYvVx.lnpyp4YbUdB28fiIYEKc"
build_base.SOURCE_KEY = (
    "sources/bundles/penguin-worker-cli-v27/sha256/"
    f"{build_base.SOURCE_SHA256}/ultimatefish-penguin-worker-cli-v27-source.tar"
)
build_base.SEMANTICS = (
    "penguin-ghost-primary-action-conditioned-v21-with-driver-only-"
    "parallel-worker-cli-v27"
)
build_base.GENERATION = "v27"
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
