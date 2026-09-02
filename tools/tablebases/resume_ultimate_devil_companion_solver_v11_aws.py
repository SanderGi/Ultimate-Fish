#!/usr/bin/env python3
"""Resume companion-Devil checkpoints with restartable-reverse v11."""

from __future__ import annotations

import launch_ultimate_devil_companion_solver_v8_aws as v8
import resume_ultimate_devil_companion_solver_v9_aws as runner

runner.__doc__ = (
    "Resume retained companion-Devil closure checkpoints with the "
    "restartable, child-locality-sharded reverse-edge v11 solver."
)

runner.SOURCE_SHA256 = (
    "bb04e578ce251a304bca3eafd329ed49a04aed7656a29f7431fef8a1d7cca400"
)
runner.SOURCE_VERSION = "vKTPMe4bbaWdgkmQ6ms8CkP9AIK7g0Fi"
runner.BINARY_SHA256 = (
    "db53f32f74d536eaa07fc514cafd1ae330eb3747c619e5b5c73058827fc1feee"
)
runner.BINARY_VERSION = "lAif3Ta98i75vM4crpnL5rQv_8PEDfwU"
runner.SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v11/sha256/{runner.SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v11-source.tar"
)
runner.BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v11/sha256/{runner.BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v11"
)
runner.ROOT = (
    f"/mnt/ultimatefish/devil-spawned-solver-v11-{runner.SOURCE_SHA256[:8]}"
)
runner.BINARY_FILENAME = "ultimate_tablebase-devil-spawned-v11"


def paths(piece: str, opposed: bool, square: int, volume: str) -> tuple[str, ...]:
    orientation = "opposed" if opposed else "same"
    stem = f"{orientation}-{piece}-square-{square}"
    work_root = f"{volume.rstrip('/')}/devil-companion-v8-{v8.SOURCE_SHA256[:8]}"
    return (
        f"ultimatefish-devil-companion-v10-{stem}",
        f"ultimatefish-devil-companion-v11-{stem}",
        f"{work_root}/{orientation}-{piece}/square-{square}",
        f"{work_root}/logs/{stem}-v11.log",
    )


runner.paths = paths


if __name__ == "__main__":
    runner.main()
