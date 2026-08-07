#!/usr/bin/env python3
"""Generate deterministic deep-search/outcome records for Ultimate NNUE training."""

from __future__ import annotations

import argparse
import json
import random
import subprocess
from dataclasses import dataclass
from pathlib import Path

from selfplay_ultimate import STARTS, repetition_key


@dataclass
class Analysis:
    score: int
    depth: int
    nodes: int
    move: str | None


class Engine:
    def __init__(self, path: str) -> None:
        self.path = path
        self.process = subprocess.Popen(
            [path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, bufsize=1,
        )

    def send(self, command: str) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def line(self) -> str:
        assert self.process.stdout is not None
        line = self.process.stdout.readline()
        if not line:
            assert self.process.stderr is not None
            raise RuntimeError(self.process.stderr.read().strip() or "engine stopped")
        return line.rstrip("\n")

    def set_position(self, upn: str) -> None:
        self.send("position upn " + upn)
        response = self.line()
        if response != "positionok":
            raise RuntimeError(f"position rejected: {response}\n{upn}")

    def describe(self, upn: str) -> tuple[list[str], str]:
        self.set_position(upn)
        self.send("moves")
        upn_line = self.line()
        moves_line = self.line()
        _material = self.line()
        result = self.line()
        if not upn_line.startswith("upn ") or not moves_line.startswith("moves"):
            raise RuntimeError(f"unexpected position description: {upn_line!r}, {moves_line!r}")
        return moves_line.split()[1:], result

    def analyze(self, upn: str, nodes: int, depth: int) -> Analysis:
        self.set_position(upn)
        self.send(f"go depth {depth} nodes {nodes}")
        info = self.line().split()
        best = self.line()
        if len(info) < 8 or info[0] != "info" or not best.startswith("bestmove "):
            raise RuntimeError(f"unexpected search response: {' '.join(info)!r}, {best!r}")
        score_kind = info[4]
        score_value = int(info[5])
        if score_kind == "mate":
            score_value = (30_000 - min(127, 2 * abs(score_value))) * (1 if score_value > 0 else -1)
        elif score_kind != "cp":
            raise RuntimeError(f"unknown score kind {score_kind!r}")
        move = best.split(" ", 1)[1]
        return Analysis(
            score=score_value,
            depth=int(info[2]),
            nodes=int(info[info.index("nodes") + 1]),
            move=None if move == "(none)" else move,
        )

    def static_eval(self, upn: str) -> int:
        self.set_position(upn)
        self.send("eval")
        response = self.line().split()
        if len(response) != 3 or response[:2] != ["eval", "cp"]:
            raise RuntimeError(f"unexpected static evaluation: {response!r}")
        return int(response[2])

    def apply(self, upn: str, move: str) -> str:
        self.set_position(upn)
        self.send("move " + move)
        response = self.line()
        if not response.startswith("position "):
            raise RuntimeError(f"generated move {move} failed: {response}\n{upn}")
        return response.split(" ", 1)[1]

    def close(self) -> None:
        if self.process.poll() is None:
            self.send("quit")
            self.process.wait(timeout=5)


def result_value(result: str) -> int | None:
    if result == "result ongoing":
        return None
    if result.startswith("result white"):
        return 1
    if result.startswith("result black"):
        return -1
    return 0


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("engine")
    parser.add_argument("output", type=Path)
    parser.add_argument("--games", type=int, default=20)
    parser.add_argument("--nodes", type=int, default=50_000)
    parser.add_argument("--depth", type=int, default=30)
    parser.add_argument("--plies", type=int, default=160)
    parser.add_argument("--exploration", type=float, default=0.08)
    parser.add_argument("--exploration-plies", type=int, default=24)
    parser.add_argument("--seed", type=int, default=20260806)
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args()
    rng = random.Random(args.seed)
    args.output.parent.mkdir(parents=True, exist_ok=True)

    engine = Engine(args.engine)
    written = 0
    try:
        with args.output.open("w" if args.overwrite else "a", encoding="utf-8") as stream:
            for game in range(args.games):
                fixture, upn = STARTS[game % len(STARTS)]
                records: list[dict[str, object]] = []
                seen = {repetition_key(upn): 1}
                outcome = 0
                for ply in range(args.plies):
                    moves, native_result = engine.describe(upn)
                    terminal = result_value(native_result)
                    if terminal is not None:
                        outcome = terminal
                        break
                    analysis = engine.analyze(upn, args.nodes, args.depth)
                    if analysis.move is None:
                        break
                    records.append({
                        "upn": upn,
                        "score_stm": analysis.score,
                        "static_stm": engine.static_eval(upn),
                        "depth": analysis.depth,
                        "nodes": analysis.nodes,
                        "game": game,
                        "ply": ply,
                        "fixture": fixture,
                    })
                    move = analysis.move
                    if (ply < args.exploration_plies and len(moves) > 1 and
                            rng.random() < args.exploration):
                        alternatives = [candidate for candidate in moves if candidate != move]
                        move = rng.choice(alternatives)
                    upn = engine.apply(upn, move)
                    key = repetition_key(upn)
                    seen[key] = seen.get(key, 0) + 1
                    if seen[key] >= 3:
                        break
                for record in records:
                    record["result_white"] = outcome
                    stream.write(json.dumps(record, separators=(",", ":")) + "\n")
                stream.flush()
                written += len(records)
                print(f"game {game + 1}/{args.games}: {fixture}, {len(records)} records, "
                      f"result {outcome:+d}, total {written}", flush=True)
    finally:
        engine.close()


if __name__ == "__main__":
    main()
