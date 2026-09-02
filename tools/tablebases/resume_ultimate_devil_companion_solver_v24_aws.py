#!/usr/bin/env python3
"""Resume retained Devil checkpoints with a file-mapped v24 frontier."""

from __future__ import annotations

import resume_ultimate_devil_companion_solver_v23_aws as v23


v23.SOURCE_SHA256 = "bf3c49ccca68a00cb3733c7105e25b3689e91dbd8f05a5d9127616369511ab7c"
v23.SOURCE_VERSION = "ctQNJyTlyEf6vcOibC0vcnLC7QLNl.oE"
v23.SOURCE_KEY = (
    "sources/bundles/devil-spawned-solver-v24/sha256/"
    f"{v23.SOURCE_SHA256}/ultimatefish-devil-spawned-solver-v24-source.tar"
)
v23.BINARY_SHA256 = "f655cd0ec7d3981b4f7da8e1f36bf792e1865b6e01f85208611f7cd8ef12c2bd"
v23.BINARY_VERSION = "wJE2AURbjHO_OVBpYezSszX4wNyt_SeU"
v23.BINARY_KEY = (
    "sources/binaries/devil-spawned-solver-v24/sha256/"
    f"{v23.BINARY_SHA256}/ultimate_tablebase-devil-spawned-v24"
)
v23.ROOT = f"/mnt/ultimatefish/devil-spawned-solver-v24-{v23.SOURCE_SHA256[:8]}"


def names(piece: str, opposed: bool, square: int,
          volume: str) -> tuple[str, str, str, str]:
    orientation = "opposed" if opposed else "same"
    work = (f"{volume.rstrip('/')}/devil-companion-v15-34aa7f01/"
            f"{orientation}-{piece}/square-{square}")
    unit = f"ultimatefish-devil-companion-v24-{orientation}-{piece}-square-{square}"
    log = (f"{volume.rstrip('/')}/devil-companion-v15-34aa7f01/logs/"
           f"{orientation}-{piece}-square-{square}-v24.log")
    old_unit = f"ultimatefish-devil-companion-v23-{orientation}-{piece}-square-{square}"
    return unit, work, log, old_unit


v23._names = names


if __name__ == "__main__":
    v23.main()
