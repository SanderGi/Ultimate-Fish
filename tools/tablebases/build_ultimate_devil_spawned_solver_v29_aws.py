#!/usr/bin/env python3
"""Build and preserve the sequential/restartable Devil v29 binary."""

from __future__ import annotations

import build_ultimate_devil_spawned_solver_v23_aws as base


base.VERSION_TAG = "v29"
base.BUILD_CPUS = "0"
base.SOURCE_SHA256 = "368e7230e5e2c82604b8be1d019311eaa4c344e96c04bc08db249fe9fe24915a"
base.SOURCE_VERSION = "KdT.6Hl.gP9V8cAqIwoB8CVP1LbO6C4X"
base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v29/sha256/"
    f"{base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v29-source.tar"
)
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "retained-reverse-edges-bellman-parallel-resume-seven-byte-key-"
    "read-only-file-mapped-retained-frontier-parallel-degree-reduction-"
    "authenticated-v1-sixteen-byte-proof-key-migration-"
    "adopt-validated-completed-reverse-buckets-"
    "per-shard-sort-buffered-kway-sequential-reverse-merge-v29"
)


if __name__ == "__main__":
    base.main()
