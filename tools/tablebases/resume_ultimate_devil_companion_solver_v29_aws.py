#!/usr/bin/env python3
"""Resume authenticated B1 on the resized host's local RAID0 scratch."""

from __future__ import annotations

import resume_ultimate_devil_companion_solver_v27_aws as impl


impl.impl.WORK_ROOT = "/mnt/ultimatefish/devil-companion-v29/same-bishop/square-1"
impl.impl.VOLUME = "/mnt/ultimatefish"
impl.impl.MEMORY_MAX = 450_000_000_000
impl.impl.base.ROOT = (
    "/mnt/ultimatefish/devil-spawned-solver-v29-"
    f"{impl.impl.base.SOURCE_SHA256[:8]}"
)


def names(piece: str, opposed: bool, square: int,
          volume: str) -> tuple[str, str, str, str]:
    del volume
    orientation = "opposed" if opposed else "same"
    work = impl.impl.WORK_ROOT
    unit = f"ultimatefish-devil-companion-v29-{orientation}-{piece}-square-{square}"
    log = f"{work}/devil-{square}.v29.log"
    old_unit = f"ultimatefish-devil-companion-v28-{orientation}-{piece}-square-{square}"
    return unit, work, log, old_unit


impl.impl.names = names


if __name__ == "__main__":
    impl.impl.main()
