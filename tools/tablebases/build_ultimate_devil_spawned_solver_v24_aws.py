#!/usr/bin/env python3
"""Build, preserve, and restore-check the file-mapped-frontier Devil v24 binary."""

from __future__ import annotations

import build_ultimate_devil_spawned_solver_v23_aws as base


base.VERSION_TAG = "v24"
base.SOURCE_SHA256 = "bf3c49ccca68a00cb3733c7105e25b3689e91dbd8f05a5d9127616369511ab7c"
base.SOURCE_VERSION = "ctQNJyTlyEf6vcOibC0vcnLC7QLNl.oE"
base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v24/sha256/"
    f"{base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v24-source.tar"
)
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "retained-reverse-edges-bellman-parallel-resume-seven-byte-key-"
    "eight-bit-exact-fingerprint-batched-hash-sorted-deduplicated-"
    "closure-insert-read-only-file-mapped-retained-frontier-"
    "independent-fail-closed-hash-capacity-v24"
)


if __name__ == "__main__":
    base.main()
