#!/usr/bin/env python3
"""Build, preserve, and restore-check the fail-fast parallel Devil v25 binary."""

from __future__ import annotations

import build_ultimate_devil_spawned_solver_v23_aws as base


base.VERSION_TAG = "v25"
base.SOURCE_SHA256 = "19fa50ebfa1f5b0319fac5d3f3957d0f37ff5bbe600a22619e6bb93eaf631728"
base.SOURCE_VERSION = "XJkADC_Sn4K6eVKFjaSe6qkD6XmEbbsP"
base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v25/sha256/"
    f"{base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v25-source.tar"
)
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "retained-reverse-edges-bellman-parallel-resume-seven-byte-key-"
    "read-only-file-mapped-retained-frontier-parallel-degree-reduction-"
    "resolved-writable-reverse-spool-gate-v25"
)


if __name__ == "__main__":
    base.main()
