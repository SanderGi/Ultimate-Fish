#!/usr/bin/env python3
"""Repack Prince v23-compatible source with resumable unique-index growth."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import package_ultimate_ghost_parallel_v9_aws as package_base


package_base.BASE_SHA256 = (
    "d8f3180c406ef51631a7f72b042fadbfe1c307fbf4c3b9afeab4a64f226d996f"
)
package_base.PREFIX = "ultimatefish-ghost-prince-unique-rehash-v16-source"
package_base.REPLACEMENTS = (
    "src/ultimate/tablebases/external_robdd.cpp",
    "src/ultimate/tablebases/external_robdd.h",
    "tests/tablebases/ultimate_external_robdd.cpp",
)
package_base.SEMANTICS = (
    "prince-v23-compatible-exact-parallel-expanded-unique-index-v16"
)
package_base.SCHEMA = "ultimate-ghost-prince-unique-rehash-source-v16"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print(json.dumps(package_base.build(args.base, args.output),
                     indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
