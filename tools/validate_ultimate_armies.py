#!/usr/bin/env python3
"""Re-rank evolved Ultimate setups at a larger, color-balanced search budget."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path

from evolve_ultimate_army import (
    OPPONENTS,
    evaluate_league,
    format_team,
    parse_team,
    result_summary,
    standing_json,
    unique_population,
)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", help="JSON checkpoint produced by evolve_ultimate_army.py")
    parser.add_argument("--engine", default=str(Path(__file__).parents[1] / "src/ultimatefish"))
    parser.add_argument("--top", type=int, default=2)
    parser.add_argument("--depth", type=int, default=20)
    parser.add_argument("--nodes", type=int, default=10_000)
    parser.add_argument("--plies", type=int, default=160)
    parser.add_argument("--workers", type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--output")
    args = parser.parse_args()
    if args.top < 2 or args.nodes < 1 or args.plies < 1 or args.workers < 1:
        parser.error("top >= 2 and positive search limits/workers are required")

    source = json.loads(Path(args.results).read_text())
    saved_finalists = [parse_team(item["team"]) for item in source.get("finalists", ())]
    if len(saved_finalists) < args.top:
        parser.error("results file does not contain enough finalists")
    population = saved_finalists[:args.top]
    hall = [parse_team(team) for team in source.get("hall", ())]
    external = unique_population((*OPPONENTS, *hall, *saved_finalists[args.top:]))
    standings = evaluate_league(
        args.engine, population, external, args.depth,
        args.nodes, args.plies, args.workers,
    )

    for rank, standing in enumerate(standings, 1):
        print(
            f"{rank}. fitness={standing.fitness:.4f} score={standing.score:.4f} "
            f"floor={standing.lower_quartile:.4f} "
            f"record={result_summary(standing.results)} "
            f"--own-team '{format_team(standing.army)}'"
        )

    if args.output:
        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps({
            "source": str(Path(args.results)),
            "depth": args.depth,
            "nodes": args.nodes,
            "plies": args.plies,
            "opponents": len(external),
            "standings": [standing_json(standing) for standing in standings],
        }, indent=2) + "\n")


if __name__ == "__main__":
    main()
