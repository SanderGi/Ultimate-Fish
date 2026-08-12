#!/usr/bin/env python3
"""Search one public Ranked draft window for the UI and phone controller.

The macro action/reply search lives in ``evolve_ultimate_draft``. Completed
draft leaves are evaluated through Ultimate Fish's public-history API, which
removes exact hidden Ghost coordinates while retaining the exact royal
candidate set established by the first locked pick group.
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.evolve_ultimate_army import position
from tools.evolve_ultimate_draft import (
    first_group_king_candidates,
    PublicDraftState,
    search_public_draft,
    team_points,
)
from tools.ultimate_phone import EngineClient


def replay_history(commands: object) -> PublicDraftState:
    if not isinstance(commands, list):
        raise ValueError("draft history must be an array")
    state = PublicDraftState()
    pending: list[str] = []
    for command in commands:
        if not isinstance(command, str):
            raise ValueError("draft history commands must be strings")
        if command.startswith("draft choose "):
            pending.append(command.removeprefix("draft choose "))
        elif command == "draft commit":
            state = state.apply(pending)
            pending.clear()
        else:
            raise ValueError(f"invalid draft history command: {command}")
    if pending:
        raise ValueError("draft history ends with an uncommitted choice")
    return state


def status_line(state: PublicDraftState) -> str:
    player = "white" if state.color == "w" else "black"
    if state.phase >= 12:
        action, minimum, maximum = "complete", 0, 0
    elif state.action == "ban":
        action, minimum, maximum = "ban", 0, 0
    else:
        from tools.evolve_ultimate_draft import draft_window
        team = state.white if state.color == "w" else state.black
        minimum, maximum, _final = draft_window(state.phase, team)
        action = "pick"
    return (
        f"draft phase {state.phase} player {player} action {action} "
        f"min {minimum} max {maximum} white {team_points(state.white)} "
        f"black {team_points(state.black)}"
    )


def main() -> int:
    request = json.load(sys.stdin)
    state = replay_history(request.get("history"))
    player = request.get("player")
    local_color = "w" if player == "white" else "b" if player == "black" else None
    if local_color is None or state.color != local_color:
        raise ValueError("draft player must match the current public window")
    depth = max(1, min(16, int(request.get("depth", 4))))
    engine_path = str(request.get("engine") or ROOT / "src" / "ultimatefish")
    time_limit = max(0.5, min(20.0, float(request.get("timeLimit", 6.0))))
    deadline = time.monotonic() + time_limit
    cache: dict[tuple[str, tuple[str, ...]], float] = {}
    engine = EngineClient(engine_path)
    try:
        def evaluate(outcome) -> float:
            upn = position(outcome.white, outcome.black, "w")
            enemy_color = "b" if local_color == "w" else "w"
            king_candidates = first_group_king_candidates(
                outcome, enemy_color,
            )
            cache_key = upn, king_candidates
            if cache_key not in cache:
                remaining_ms = max(30, int((deadline - time.monotonic()) * 1000))
                _move, score, _info = engine.search_history(
                    upn, (), depth=depth,
                    observer="white" if local_color == "w" else "black",
                    enemy_king_known=len(king_candidates) == 1,
                    initial_deployment_known=True,
                    enemy_king_candidates=king_candidates,
                    nodes=10_000, movetime_ms=min(500, remaining_ms),
                )
                cache[cache_key] = float(score)
            return cache[cache_key]

        result = search_public_draft(
            state, local_color, evaluate,
            action_width=8, reply_width=4, rollout_width=3,
            time_limit_seconds=time_limit,
        )
    finally:
        engine.close()

    after = state.apply(result.action)
    json.dump({
        "choices": list(result.action),
        "score": result.score,
        "candidates": result.candidates,
        "leaves": result.leaves,
        "elapsed": result.elapsed_seconds,
        "timedOut": result.timed_out,
        "status": status_line(after),
    }, sys.stdout)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        json.dump({"error": str(error)}, sys.stdout)
        sys.stdout.write("\n")
        raise SystemExit(1)
