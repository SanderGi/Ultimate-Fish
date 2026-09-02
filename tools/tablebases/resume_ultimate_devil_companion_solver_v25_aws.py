#!/usr/bin/env python3
"""Resume retained Devil checkpoints with fail-fast parallel v25."""

from __future__ import annotations

import resume_ultimate_devil_companion_solver_v23_aws as base


base.SOURCE_SHA256 = "19fa50ebfa1f5b0319fac5d3f3957d0f37ff5bbe600a22619e6bb93eaf631728"
base.SOURCE_VERSION = "XJkADC_Sn4K6eVKFjaSe6qkD6XmEbbsP"
base.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v25/sha256/"
    f"{base.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v25-source.tar"
)
base.BINARY_SHA256 = "5732a5db776e2572e1401ce5c4dc2fda90e11ad84ddbffcb3ddd1efdd435e6ec"
base.BINARY_VERSION = "yZElOHwKruG670rGNILIsCK9gZQ2SVKh"
base.BINARY_KEY = (
    "sources/binaries/devil-spawned-solver-v25/sha256/"
    f"{base.BINARY_SHA256}/ultimate_tablebase-devil-spawned-v25"
)
base.ROOT = f"/mnt/ultimatefish/devil-spawned-solver-v25-{base.SOURCE_SHA256[:8]}"


def names(piece: str, opposed: bool, square: int,
          volume: str) -> tuple[str, str, str, str]:
    orientation = "opposed" if opposed else "same"
    work = (f"{volume.rstrip('/')}/devil-companion-v15-34aa7f01/"
            f"{orientation}-{piece}/square-{square}")
    unit = f"ultimatefish-devil-companion-v25-{orientation}-{piece}-square-{square}"
    log = (f"{volume.rstrip('/')}/devil-companion-v15-34aa7f01/logs/"
           f"{orientation}-{piece}-square-{square}-v25.log")
    old_unit = f"ultimatefish-devil-companion-v21-{orientation}-{piece}-square-{square}"
    return unit, work, log, old_unit


base._names = names


if __name__ == "__main__":
    base.main()
