#!/usr/bin/env python3
"""Package the spawned-Devil root-domain reachability fix v30 source."""

from __future__ import annotations

import package_ultimate_devil_spawned_solver_v7_aws as base


base.SCHEMA = "ultimate-devil-spawned-solver-source-v30"
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "retained-reverse-edges-bellman-parallel-resume-seven-byte-key-"
    "read-only-file-mapped-retained-frontier-parallel-degree-reduction-"
    "authenticated-v1-sixteen-byte-proof-key-migration-"
    "adopt-validated-completed-reverse-buckets-"
    "per-shard-sort-buffered-kway-sequential-reverse-merge-"
    "merged-root-domain-native-reachability-v30"
)


if __name__ == "__main__":
    base.main()
