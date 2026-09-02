#!/usr/bin/env python3
"""Parallelize the exact transition-compatible Pawn/Ghost v12 source."""

from __future__ import annotations

import package_ultimate_checker_parallel_v13_aws as package_base


package_base.BASE_SHA256 = (
    "773cd700b4abb5adfb3501f0ab5b5468065aa8d979771f3d0e8f8425910d192d"
)
package_base.PREFIX = "ultimatefish-pawn-parallel-v25-source"


if __name__ == "__main__":
    package_base.main()
