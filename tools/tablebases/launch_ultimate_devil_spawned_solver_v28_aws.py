#!/usr/bin/env python3
"""Launch the authenticated v1-migration-capable lone-Devil v28 solver."""

from __future__ import annotations

import launch_ultimate_devil_spawned_solver_aws as base


base.SOURCE_SHA256 = "d5f609f8e06a2b1a3cf1bacbe1a3d8416b40d8f6be9d42dfc76a0216b53673f7"
base.SOURCE_VERSION = "4WYH7NAWrdBlJfCyerGBCFeiiVngO1U."
base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v28/sha256/"
    f"{base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v28-source.tar"
)
base.BINARY_SHA256 = "dff4c83d3b9234948a91b3c83baeacff5f9e38c1c9abbefca31bb0879fb1591a"
base.BINARY_VERSION = "AtA5AZ7a95hPmVLjqfchIOKCLxpZMzu5"
base.BINARY_KEY = (
    "sources/binaries/devil-spawned-solver-v28/sha256/"
    f"{base.BINARY_SHA256}/ultimate_tablebase-devil-spawned-v28"
)
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "retained-reverse-edges-bellman-parallel-resume-seven-byte-key-"
    "read-only-file-mapped-retained-frontier-parallel-degree-reduction-"
    "resolved-writable-reverse-spool-gate-hardware-worker-bound-"
    "authenticated-v1-sixteen-byte-proof-key-migration-v28"
)
base.UNIT_PREFIX = "ultimatefish-devil-spawned-v28-square-"
base.GENERATION = "v28"
# The reverse merge maps roughly 267 GiB of retained arrays plus its read-only
# spool.  On the 256-GiB host, a 200-GiB soft gate needlessly throttles clean
# mapped pages even when host PSI is zero.  This C1-only launcher therefore
# uses a hard 228-GiB cgroup limit as its cache/reclaim boundary.  The host
# retains roughly 11 GB beyond that maximum for its other active jobs and the
# operating system, while ``MemoryMax`` still prevents an unsafe allocation.
base.MEMORY_HIGH = 244_813_135_872
base.MEMORY_MAX = 244_813_135_872


if __name__ == "__main__":
    base.main()
