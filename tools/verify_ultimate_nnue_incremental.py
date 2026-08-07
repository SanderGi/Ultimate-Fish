#!/usr/bin/env python3
"""Compare incremental NNUE search with the full-refresh reference build."""

from __future__ import annotations

import argparse
import os
import subprocess

from selfplay_ultimate import STARTS


def search(binary: str, network: str, upn: str, depth: int) -> tuple[list[str], str]:
    environment = dict(os.environ)
    environment["ULTIMATE_NNUE_FILE"] = network
    process = subprocess.run(
        [binary],
        input=f"position upn {upn}\ngo depth {depth}\nquit\n",
        text=True, capture_output=True, check=True, env=environment,
    )
    lines = process.stdout.splitlines()
    info = next(line.split() for line in lines if line.startswith("info depth "))
    best = next(line for line in lines if line.startswith("bestmove "))
    # Wall time is expected to differ; every search-semantic field must match.
    at = info.index("time")
    del info[at:at + 2]
    return info, best


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("incremental")
    parser.add_argument("reference")
    parser.add_argument("network")
    parser.add_argument("--depth", type=int, default=6)
    args = parser.parse_args()
    for name, upn in STARTS:
        incremental = search(args.incremental, args.network, upn, args.depth)
        reference = search(args.reference, args.network, upn, args.depth)
        if incremental != reference:
            raise SystemExit(
                f"{name}: accumulator divergence\nincremental={incremental}\n"
                f"reference={reference}")
        print(f"{name}: exact")


if __name__ == "__main__":
    main()
