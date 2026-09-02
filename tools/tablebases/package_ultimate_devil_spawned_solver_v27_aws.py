#!/usr/bin/env python3
"""Package the hardware-scaled Devil v27 source bundle."""

from __future__ import annotations

import package_ultimate_devil_spawned_solver_v7_aws as base


base.SCHEMA = "ultimate-devil-spawned-solver-source-v27"
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "max-five-proved-retained-reverse-edges-bellman-parallel-resume-"
    "seven-byte-combinatorial-proof-key-checkpoint-v4-"
    "byte-aligned-eight-bit-exact-fingerprint-batched-deduplicated-"
    "read-only-file-mapped-retained-frontier-parallel-degree-reduction-"
    "resolved-writable-reverse-spool-gate-hardware-worker-bound-v27"
)


if __name__ == "__main__":
    base.main()
