#!/usr/bin/env python3
"""Build and preserve the spawned-Devil reachability-fix v30 binary."""

from __future__ import annotations

import build_ultimate_devil_spawned_solver_v23_aws as base


base.VERSION_TAG = "v30"
base.BUILD_CPUS = "8"
base.SOURCE_SHA256 = "f0e86af731493e47c68cd51b64113ff08972b8af7f54c1a44bb34c3e82c852e5"
base.SOURCE_VERSION = "Kyzrs7f8RTOVsZztI3njpIuTzZMV0saR"
base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v30/sha256/"
    f"{base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v30-source.tar"
)
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
