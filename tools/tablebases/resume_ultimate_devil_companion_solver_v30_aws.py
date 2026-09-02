#!/usr/bin/env python3
"""Resume authenticated C1 on the resized host's local NVMe scratch."""

from __future__ import annotations

import resume_ultimate_devil_companion_solver_v27_aws as impl


impl.impl.WORK_ROOT = (
    "/mnt/ultimatefish-devil-fast/devil-companion-v30"
)
impl.impl.VOLUME = "/mnt/ultimatefish-devil-fast"
impl.impl.MEMORY_MAX = 498_216_206_336
impl.impl.base.ROOT = (
    "/mnt/ultimatefish-devil-fast/devil-spawned-solver-v30-"
    f"{impl.impl.base.SOURCE_SHA256[:8]}"
)


def names(piece: str, opposed: bool, square: int,
          volume: str) -> tuple[str, str, str, str]:
    del volume
    orientation = "opposed" if opposed else "same"
    work = f"{impl.impl.WORK_ROOT}/{orientation}-{piece}/square-{square}"
    unit = f"ultimatefish-devil-companion-v30-{orientation}-{piece}-square-{square}"
    log = f"{impl.impl.WORK_ROOT}/logs/{orientation}-{piece}-square-{square}-v30.log"
    old_unit = f"ultimatefish-devil-companion-v27-{orientation}-{piece}-square-{square}"
    return unit, work, log, old_unit


impl.impl.names = names


if __name__ == "__main__":
    impl.impl.main()
