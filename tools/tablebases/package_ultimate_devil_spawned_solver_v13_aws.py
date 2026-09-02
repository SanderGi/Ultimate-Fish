#!/usr/bin/env python3
"""Build the deterministic 14-byte-key spawned-only Devil v13 bundle."""

from __future__ import annotations

import package_ultimate_devil_spawned_solver_v7_aws as base


base.SCHEMA = "ultimate-devil-spawned-solver-source-v13"
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "max-five-proved-retained-reverse-edges-bellman-parallel-resume-"
    "anonymous-slots-wide-indices-stateless-secondary-square-bitpacked-"
    "restartable-parent-shards-child-locality-buckets-parallel-root-seed-"
    "packed-14-byte-proof-key-checkpoint-v3-v13"
)


if __name__ == "__main__":
    base.main()
