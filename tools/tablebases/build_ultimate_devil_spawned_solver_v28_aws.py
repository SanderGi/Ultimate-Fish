#!/usr/bin/env python3
"""Build and preserve the v1-checkpoint-migration Devil v28 binary."""

from __future__ import annotations

import build_ultimate_devil_spawned_solver_v23_aws as base


base.VERSION_TAG = "v28"
base.SOURCE_SHA256 = "d5f609f8e06a2b1a3cf1bacbe1a3d8416b40d8f6be9d42dfc76a0216b53673f7"
base.SOURCE_VERSION = "4WYH7NAWrdBlJfCyerGBCFeiiVngO1U."
base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v28/sha256/"
    f"{base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v28-source.tar"
)
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "retained-reverse-edges-bellman-parallel-resume-seven-byte-key-"
    "read-only-file-mapped-retained-frontier-parallel-degree-reduction-"
    "resolved-writable-reverse-spool-gate-hardware-worker-bound-"
    "authenticated-v1-sixteen-byte-proof-key-migration-v28"
)


if __name__ == "__main__":
    base.main()
