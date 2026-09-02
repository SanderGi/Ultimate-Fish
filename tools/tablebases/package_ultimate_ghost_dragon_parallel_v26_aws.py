#!/usr/bin/env python3
"""Parallelize the exact transition-compatible opposed Ghost/Dragon source."""

from __future__ import annotations

import package_ultimate_checker_parallel_v13_aws as package_base


package_base.BASE_SHA256 = (
    "75a1a89f7022478bc3fdcad8da83d1694e06463be559e0d1fbc804ba40b0cd83"
)
package_base.PREFIX = "ultimatefish-ghost-dragon-parallel-v26-source"


if __name__ == "__main__":
    package_base.main()
