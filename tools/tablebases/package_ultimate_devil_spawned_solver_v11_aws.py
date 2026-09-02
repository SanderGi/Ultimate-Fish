#!/usr/bin/env python3
"""Build the deterministic restartable-reverse spawned-only Devil v11 bundle."""

from __future__ import annotations

import package_ultimate_devil_spawned_solver_v7_aws as base


base.SCHEMA = "ultimate-devil-spawned-solver-source-v11"
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "max-five-proved-retained-reverse-edges-bellman-parallel-resume-"
    "anonymous-slots-wide-indices-stateless-secondary-square-bitpacked-"
    "restartable-parent-shards-child-locality-buckets-v11"
)


if __name__ == "__main__":
    base.main()
