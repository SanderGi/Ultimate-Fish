#!/usr/bin/env python3
"""Benchmark deadline-aware engine search for Ranked Ban and Pick windows."""

from __future__ import annotations

import argparse
from pathlib import Path

try:
    from evolve_ultimate_army import Engine, position
    from evolve_ultimate_draft import PublicDraftState, search_public_draft
except ModuleNotFoundError:
    from tools.evolve_ultimate_army import Engine, position
    from tools.evolve_ultimate_draft import PublicDraftState, search_public_draft


def run_window(
    engine: Engine, state: PublicDraftState, seconds: float, nodes: int,
    action_width: int, reply_width: int, rollout_width: int, leaf_ms: int,
) -> None:
    cache: dict[str, float] = {}

    def evaluate(outcome) -> float:
        upn = position(outcome.white, outcome.black, state.color)
        if upn not in cache:
            _move, score = engine.search(upn, 24, nodes, leaf_ms)
            cache[upn] = float(score if state.color == "w" else -score)
        return cache[upn]

    result = search_public_draft(
        state, state.color, evaluate,
        action_width=action_width,
        reply_width=reply_width,
        rollout_width=rollout_width,
        time_limit_seconds=seconds,
    )
    print(
        f"phase={state.phase} action={state.action} choice={' '.join(result.action)} "
        f"score={result.score:+.0f} candidates={result.candidates} "
        f"leaves={result.leaves} unique={len(cache)} "
        f"elapsed={result.elapsed_seconds:.3f}s timed_out={result.timed_out}"
    )


def main() -> None:
    workspace = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", default=str(workspace / "src" / "ultimatefish"))
    parser.add_argument("--ban-seconds", type=float, default=1.5)
    parser.add_argument("--pick-seconds", type=float, default=6.0)
    parser.add_argument("--ban-nodes", type=int, default=4_000)
    parser.add_argument("--pick-nodes", type=int, default=10_000)
    args = parser.parse_args()

    engine = Engine(args.engine)
    try:
        run_window(
            engine, PublicDraftState(), args.ban_seconds, args.ban_nodes,
            12, 3, 2, 180,
        )
        after_bans = PublicDraftState().apply(("penguin",)).apply(("prince",))
        run_window(
            engine, after_bans, args.pick_seconds, args.pick_nodes,
            5, 3, 3, 600,
        )
        revealed_rosters = (
            after_bans.apply(("queen", "queen"))
            .apply(("queen", "queen"))
        )
        run_window(
            engine, revealed_rosters, args.ban_seconds, args.ban_nodes,
            12, 3, 2, 180,
        )
    finally:
        engine.close()


if __name__ == "__main__":
    main()
