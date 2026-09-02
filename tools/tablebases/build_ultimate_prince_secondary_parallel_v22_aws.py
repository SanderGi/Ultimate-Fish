#!/usr/bin/env python3
"""Build and preserve the corrected Ghost-primary Prince binary."""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "d8f3180c406ef51631a7f72b042fadbfe1c307fbf4c3b9afeab4a64f226d996f"
)
build_base.SOURCE_VERSION = "XdBcy75hA5OW3iqg850H0niLoY4mQSsi"
build_base.SOURCE_KEY = (
    "sources/bundles/checker-parallel-v19/sha256/"
    f"{build_base.SOURCE_SHA256}/ultimatefish-checker-parallel-v19-source.tar"
)
build_base.SEMANTICS = (
    "ghost-primary-prince-exact-action-conditioned-parallel-bellman-"
    "singleton-certification-v22"
)
build_base.GENERATION = "v23"
build_base.PRIMARY_DEFINE = ""
build_base.EXTRA_DEFINES = "-DULTIMATE_GHOST_EXTRA_SUBSTATES=2"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, "prince"))


if __name__ == "__main__":
    main()
