#!/usr/bin/env python3
"""Repack the Prince-compatible Ghost v6 source with only the ROBDD rehash fix."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import package_ultimate_ghost_parallel_v9_aws as package_base


package_base.BASE_SHA256 = (
    "d22956a40eb30167adbad15ac5d02b30d7901b616014b5683d0c5c3ba5d555c5"
)
package_base.PREFIX = "ultimatefish-ghost-unique-rehash-v12-source"
package_base.REPLACEMENTS = (
    "src/ultimate/tablebases/external_robdd.cpp",
    "src/ultimate/tablebases/external_robdd.h",
    "tests/tablebases/ultimate_external_robdd.cpp",
)
package_base.SEMANTICS = (
    "ghost-v6-compatible-exact-parallel-expanded-unique-index-v12"
)
package_base.SCHEMA = "ultimate-ghost-unique-rehash-source-v12"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print(json.dumps(package_base.build(args.base, args.output),
                     indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
