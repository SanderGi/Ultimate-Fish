#!/usr/bin/env python3
"""Resume companion-Devil checkpoints with the bit-packed v10 binary."""

from __future__ import annotations

import launch_ultimate_devil_companion_solver_v8_aws as v8
import resume_ultimate_devil_companion_solver_v9_aws as runner


runner.SOURCE_SHA256 = "9ca2ead4f41ca24b32e028ee5529d43823f695f632158df4039224e9056b5fc1"
runner.SOURCE_VERSION = "Xbg1eQwumBpQ2qHO1S5dJSjI8O8lKrOn"
runner.BINARY_SHA256 = "319ffbe6e66a9f4c48b499a30ca85545ed39b158c89b5fbf597722f7d0f80cef"
runner.BINARY_VERSION = "JHM3IPx8mvHSelngZFSyzs6yuAW.QxnE"
runner.SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v10/sha256/{runner.SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v10-source.tar"
)
runner.BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v10/sha256/{runner.BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v10"
)
runner.ROOT = (
    f"/mnt/ultimatefish/devil-spawned-solver-v10-{runner.SOURCE_SHA256[:8]}"
)
runner.BINARY_FILENAME = "ultimate_tablebase-devil-spawned-v10"


def paths(piece: str, opposed: bool, square: int, volume: str) -> tuple[str, ...]:
    orientation = "opposed" if opposed else "same"
    stem = f"{orientation}-{piece}-square-{square}"
    work_root = f"{volume.rstrip('/')}/devil-companion-v8-{v8.SOURCE_SHA256[:8]}"
    return (
        f"ultimatefish-devil-companion-v9-{stem}",
        f"ultimatefish-devil-companion-v10-{stem}",
        f"{work_root}/{orientation}-{piece}/square-{square}",
        f"{work_root}/logs/{stem}-v10.log",
    )


runner.paths = paths


if __name__ == "__main__":
    runner.main()
