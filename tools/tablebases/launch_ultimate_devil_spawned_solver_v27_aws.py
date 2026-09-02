#!/usr/bin/env python3
"""Launch the authenticated hardware-scaled v27 lone-Devil solver."""

from __future__ import annotations

import launch_ultimate_devil_spawned_solver_aws as base


base.SOURCE_SHA256 = "de523aad058723e70820d7cd645fc4ec3a23a0422e7348c836bac29b8f06f214"
base.SOURCE_VERSION = "VlBg9ai6PixKEldf0wlYAIM0USJwvmfk"
base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v27/sha256/"
    f"{base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v27-source.tar"
)
base.BINARY_SHA256 = "6b8fe2bfab9f76fcbfdedfbf67119cae418a749c86b615b40004be3e5b36ea68"
base.BINARY_VERSION = "Mh9h30fIAXagqVQUWn51_xmhQtNrJvMO"
base.BINARY_KEY = (
    "sources/binaries/devil-spawned-solver-v27/sha256/"
    f"{base.BINARY_SHA256}/ultimate_tablebase-devil-spawned-v27"
)
base.SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "retained-reverse-edges-bellman-parallel-resume-seven-byte-key-"
    "read-only-file-mapped-retained-frontier-parallel-degree-reduction-"
    "resolved-writable-reverse-spool-gate-hardware-worker-bound-v27"
)
base.UNIT_PREFIX = "ultimatefish-devil-spawned-v27-square-"
base.GENERATION = "v27"
base.MEMORY_HIGH = 214_748_364_800
base.MEMORY_MAX = 236_223_201_280


if __name__ == "__main__":
    base.main()
