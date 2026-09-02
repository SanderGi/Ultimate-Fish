#!/usr/bin/env python3
"""Build the fingerprint-filtered Devil v17 source bundle."""

from __future__ import annotations

import package_ultimate_devil_spawned_solver_v7_aws as base


base.SCHEMA = "ultimate-devil-spawned-solver-source-v17"
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "max-five-proved-retained-reverse-edges-bellman-parallel-resume-"
    "anonymous-slots-wide-indices-stateless-secondary-square-bitpacked-"
    "restartable-parent-shards-child-locality-buckets-parallel-root-seed-"
    "seven-byte-combinatorial-proof-key-checkpoint-v4-"
    "nondestructive-v2-v3-migration-stripe-aligned-non-power-of-two-"
    "packed-hash-four-bit-exact-fingerprint-filter-v17"
)


if __name__ == "__main__":
    base.main()
