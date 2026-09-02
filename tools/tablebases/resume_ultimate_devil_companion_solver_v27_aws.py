#!/usr/bin/env python3
"""Resume authenticated C1 with the hardware-scaled v27 binary."""

from __future__ import annotations

import resume_ultimate_devil_companion_solver_v26_aws as impl


impl.base.SOURCE_SHA256 = "de523aad058723e70820d7cd645fc4ec3a23a0422e7348c836bac29b8f06f214"
impl.base.SOURCE_VERSION = "VlBg9ai6PixKEldf0wlYAIM0USJwvmfk"
impl.base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v27/sha256/"
    f"{impl.base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v27-source.tar"
)
impl.base.BINARY_SHA256 = "6b8fe2bfab9f76fcbfdedfbf67119cae418a749c86b615b40004be3e5b36ea68"
impl.base.BINARY_VERSION = "Mh9h30fIAXagqVQUWn51_xmhQtNrJvMO"
impl.base.BINARY_KEY = (
    "sources/binaries/devil-spawned-solver-v27/sha256/"
    f"{impl.base.BINARY_SHA256}/ultimate_tablebase-devil-spawned-v27"
)
impl.base.ROOT = (
    "/mnt/ultimatefish-devil-v11/devil-spawned-solver-v27-"
    f"{impl.base.SOURCE_SHA256[:8]}"
)


def names(piece: str, opposed: bool, square: int,
          volume: str) -> tuple[str, str, str, str]:
    del volume
    orientation = "opposed" if opposed else "same"
    work = f"{impl.WORK_ROOT}/{orientation}-{piece}/square-{square}"
    unit = f"ultimatefish-devil-companion-v27-{orientation}-{piece}-square-{square}"
    log = f"{impl.WORK_ROOT}/logs/{orientation}-{piece}-square-{square}-v27.log"
    old_unit = f"ultimatefish-devil-companion-v26-{orientation}-{piece}-square-{square}"
    return unit, work, log, old_unit


impl.names = names


if __name__ == "__main__":
    impl.main()
