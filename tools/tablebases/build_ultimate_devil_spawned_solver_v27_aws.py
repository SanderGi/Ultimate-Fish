#!/usr/bin/env python3
"""Build and preserve the hardware-scaled Devil v27 binary."""

from __future__ import annotations

import build_ultimate_devil_spawned_solver_v23_aws as base


base.VERSION_TAG = "v27"
base.SOURCE_SHA256 = "de523aad058723e70820d7cd645fc4ec3a23a0422e7348c836bac29b8f06f214"
base.SOURCE_VERSION = "VlBg9ai6PixKEldf0wlYAIM0USJwvmfk"
base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v27/sha256/"
    f"{base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v27-source.tar"
)
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "retained-reverse-edges-bellman-parallel-resume-seven-byte-key-"
    "read-only-file-mapped-retained-frontier-parallel-degree-reduction-"
    "resolved-writable-reverse-spool-gate-hardware-worker-bound-v27"
)


if __name__ == "__main__":
    base.main()
