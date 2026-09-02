#!/usr/bin/env python3
"""Build and preserve a parallel Ghost-primary/Penguin v22 solver.

This is built alongside the retained single-core terminal-source run.  It is
not eligible to replace that run until the remote exhaustive normalized-input
self-test and all transition/model bindings match the retained checkpoint.
"""

from __future__ import annotations

import argparse

import build_ultimate_ghost_parallel_v10_aws as build_base


build_base.SOURCE_SHA256 = (
    "bcfda23c2a00580c1e4ecd08faf70447a1ae8ffe144f95bc36a074ecefbe562e"
)
build_base.SOURCE_VERSION = "ERHOaZ9324f9C4VvTmRkY9RZsqK1GG9b"
build_base.SOURCE_KEY = (
    "sources/bundles/checker-action-conditioned-v21/sha256/"
    f"{build_base.SOURCE_SHA256}/"
    "ultimatefish-checker-action-conditioned-v21-source.tar"
)
build_base.SEMANTICS = "penguin-ghost-primary-parallel-v23-candidate"
build_base.GENERATION = "v23"
build_base.EXTRA_DEFINES = (
    "-DULTIMATE_GHOST_EXTRA_SUBSTATES=8 "
    "-DULTIMATE_GHOST_ORDINARY_LOWER_SUBSTATES=4"
)
# kghostkpenguin is physically Ghost-primary/Penguin-secondary.
build_base.PRIMARY_DEFINE = ""
build_base.PIECES["penguin"] = "Penguin"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build_base.build(args.instance, "penguin"))


if __name__ == "__main__":
    main()
