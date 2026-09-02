#!/usr/bin/env python3
"""Build and preserve the parallel reverse-bucket Devil solver v31 binary."""

from __future__ import annotations

import build_ultimate_devil_spawned_solver_v23_aws as base


base.VERSION_TAG = "v31"
base.BUILD_CPUS = "0-15"
base.SOURCE_SHA256 = "dcf75b3db3fd28ca3d421ae1d163d1f96823e315463f43c78e653dca95fe202b"
base.SOURCE_VERSION = "rtcW3_tly5fOgmaZrKkZy30PhBblWJ24"
base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v31/sha256/"
    f"{base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v31-source.tar"
)
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "retained-reverse-edges-bellman-parallel-resume-seven-byte-key-"
    "authenticated-v1-sixteen-byte-proof-key-migration-"
    "adopt-validated-completed-reverse-buckets-"
    "per-shard-sort-buffered-kway-parallel-reverse-bucket-merge-"
    "merged-root-domain-native-reachability-v31"
)


if __name__ == "__main__":
    base.main()
