#!/usr/bin/env python3
"""Evolve legal 100-point Chess Ultimate armies with engine self-play.

The search optimizes both roster and placement.  Every game uses the exact
Ultimate Fish move generator, alternates the candidate's color, adjudicates
native threefold repetition, and keeps a small search-evaluation tiebreak only
for games that hit the configured ply cap.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import functools
import itertools
import math
import os
import random
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence


PIECE_COST = {
    "jester": 10, "knight": 6, "pawn": 3, "queen": 17, "rook": 13,
    "bishop": 9, "berserker": 15, "bomb": 15, "ninja": 20,
    "turtle": 4, "ghost": 15, "mage": 8, "penguin": 15,
    "parasite": 15, "devil": 15, "sludge": 12, "sniper": 17,
    "prince": 18, "checker": 2, "giant": 1, "copycat": 5,
    "angel": 13, "fisherman": 12, "dragon": 15,
}
PIECES = tuple(PIECE_COST)
FILES = "abcdefgh"
SQUARES = tuple(f"{file}{rank}" for rank in range(1, 4) for file in FILES)
Army = tuple[tuple[str, str], ...]


CURRENT_ARMY: Army = (
    ("king", "a1"),
    ("queen", "b1"), ("queen", "c1"), ("queen", "d1"),
    ("queen", "e1"), ("queen", "f1"),
    ("pawn", "g1"), ("pawn", "h1"), ("pawn", "a3"),
    ("pawn", "b3"), ("pawn", "c3"),
)

PENGUIN_ARMY: Army = (
    ("king", "a1"), ("jester", "b1"), ("dragon", "d1"),
    ("ghost", "d2"), ("penguin", "d3"), ("penguin", "e3"),
    ("penguin", "c3"), ("penguin", "f3"),
)

BALANCED_ARMY: Army = (
    ("king", "a1"), ("jester", "b1"), ("ninja", "c1"),
    ("queen", "d1"), ("sniper", "e1"), ("prince", "f1"),
    ("mage", "g1"), ("knight", "h1"), ("turtle", "a2"),
)

SPECIAL_ARMY: Army = (
    ("king", "a1"), ("jester", "b1"), ("ghost", "c1"),
    ("dragon", "d1"), ("bomb", "e1"), ("parasite", "f1"),
    ("devil", "g1"), ("berserker", "h1"),
)

# Best live-tested army from the first evolutionary pass. Keep it in the
# reference pool so later rules corrections cannot silently benchmark only
# against the older five-Queen baseline.
LIVE_GIANT_ARMY: Army = (
    ("king", "a1"),
    ("queen", "h3"), ("queen", "b3"), ("queen", "h1"),
    ("queen", "c3"), ("queen", "b2"),
    ("pawn", "a2"), ("checker", "g2"), ("checker", "b1"),
    ("pawn", "g1"), ("pawn", "h2"),
    ("giant", "e1"), ("giant", "c1"),
)

OPPONENTS = (
    CURRENT_ARMY, LIVE_GIANT_ARMY, PENGUIN_ARMY, BALANCED_ARMY, SPECIAL_ARMY,
)


def footprint(piece: str, square: str) -> frozenset[str]:
    file_index = FILES.index(square[0])
    rank = int(square[1:])
    if piece == "giant":
        if file_index >= 7 or rank >= 3:
            return frozenset()
        return frozenset(
            f"{FILES[file_index + dx]}{rank + dy}"
            for dx in (0, 1) for dy in (0, 1)
        )
    if piece == "copycat":
        return frozenset((square, f"{FILES[7 - file_index]}{rank}"))
    return frozenset((square,))


def footprint_size(piece: str) -> int:
    return 4 if piece == "giant" else 2 if piece == "copycat" else 1


def army_cost(army: Sequence[tuple[str, str]]) -> int:
    return sum(PIECE_COST.get(piece, 0) for piece, _square in army)


def validate_army(army: Sequence[tuple[str, str]]) -> None:
    if tuple(item for item in army if item[0] == "king") != (("king", "a1"),):
        raise ValueError("an army must contain exactly the fixed king at a1")
    if army_cost(army) != 100:
        raise ValueError(f"army costs {army_cost(army)}, not 100")
    occupied: set[str] = set()
    for piece, square in army:
        if piece != "king" and piece not in PIECE_COST:
            raise ValueError(f"unknown deployable piece: {piece}")
        cells = footprint(piece, square)
        if not cells or any(cell not in SQUARES for cell in cells):
            raise ValueError(f"invalid {piece} placement at {square}")
        if occupied & cells:
            raise ValueError(f"overlapping {piece} placement at {square}")
        occupied.update(cells)


def random_layout(roster: Sequence[str], rng: random.Random) -> Army | None:
    """Pack a roster into the native 8x3 deployment zone."""
    indexed = list(enumerate(roster))
    rng.shuffle(indexed)
    indexed.sort(key=lambda item: footprint_size(item[1]), reverse=True)
    assigned: dict[int, str] = {}

    def place(index: int, occupied: frozenset[str]) -> bool:
        if index == len(indexed):
            return True
        original_index, piece = indexed[index]
        candidates = list(SQUARES)
        rng.shuffle(candidates)
        # Mild home-rank bias keeps royals and sliders from being buried while
        # still allowing the evolutionary score to select unusual formations.
        candidates.sort(key=lambda square: int(square[1:]) + rng.random() * 2.5)
        for square in candidates:
            cells = footprint(piece, square)
            if not cells or cells & occupied:
                continue
            assigned[original_index] = square
            if place(index + 1, occupied | cells):
                return True
            del assigned[original_index]
        return False

    if not place(0, frozenset(("a1",))):
        return None
    army = (("king", "a1"),) + tuple(
        (piece, assigned[index]) for index, piece in enumerate(roster)
    )
    validate_army(army)
    return army


@functools.lru_cache(maxsize=None)
def can_fill(points: int, cells: int, minimum_index: int = 0) -> bool:
    if points == 0:
        return cells >= 0
    if points < 0 or cells <= 0:
        return False
    for index in range(minimum_index, len(PIECES)):
        piece = PIECES[index]
        if can_fill(
            points - PIECE_COST[piece],
            cells - footprint_size(piece),
            index,
        ):
            return True
    return False


def random_roster(rng: random.Random) -> tuple[str, ...]:
    for _attempt in range(200):
        remaining, cells = 100, 23
        roster: list[str] = []
        while remaining:
            feasible = [
                piece for piece in PIECES
                if PIECE_COST[piece] <= remaining
                and footprint_size(piece) <= cells
                and can_fill(
                    remaining - PIECE_COST[piece],
                    cells - footprint_size(piece),
                )
            ]
            if not feasible:
                break
            # Penalize enormous cheap-piece swarms without forbidding them.
            counts = {piece: roster.count(piece) for piece in feasible}
            weights = [1.0 / (1.0 + counts[piece] * 0.7) for piece in feasible]
            piece = rng.choices(feasible, weights=weights, k=1)[0]
            roster.append(piece)
            remaining -= PIECE_COST[piece]
            cells -= footprint_size(piece)
        if not remaining:
            return tuple(roster)
    raise RuntimeError("could not generate a legal 100-point roster")


@functools.lru_cache(maxsize=1)
def replacement_catalog() -> dict[int, tuple[tuple[str, ...], ...]]:
    by_cost: dict[int, list[tuple[str, ...]]] = {}
    for count in range(1, 5):
        for replacement in itertools.combinations_with_replacement(PIECES, count):
            cost = sum(PIECE_COST[piece] for piece in replacement)
            if cost <= 80 and sum(map(footprint_size, replacement)) <= 16:
                by_cost.setdefault(cost, []).append(replacement)
    return {cost: tuple(items) for cost, items in by_cost.items()}


def mutate_army(parent: Army, rng: random.Random) -> Army:
    roster = [piece for piece, _square in parent if piece != "king"]
    if rng.random() < 0.35:
        return random_layout(roster, rng) or parent

    for _attempt in range(100):
        remove_count = rng.randint(1, min(4, len(roster)))
        removed_indices = sorted(rng.sample(range(len(roster)), remove_count), reverse=True)
        child = roster.copy()
        removed = [child.pop(index) for index in removed_indices]
        choices = replacement_catalog().get(sum(PIECE_COST[piece] for piece in removed), ())
        if not choices:
            continue
        replacement = rng.choice(choices)
        if sorted(replacement) == sorted(removed):
            continue
        child.extend(replacement)
        if sum(map(footprint_size, child)) > 23:
            continue
        layout = random_layout(child, rng)
        if layout is not None:
            return layout
    return random_layout(random_roster(rng), rng) or parent


def mirror_square(piece: str, square: str) -> str:
    # UPN stores a Giant by the global lower-left anchor of its 2x2 footprint.
    # Mirroring ranks 1-2 to ranks 8-10 therefore maps that anchor with 10-r;
    # single-cell pieces (and both Copycat cells) use the ordinary 11-r map.
    rank = 10 - int(square[1:]) if piece == "giant" else 11 - int(square[1:])
    return f"{square[0]}{rank}"


def position(candidate: Army, opponent: Army, candidate_color: str) -> str:
    white, black = (candidate, opponent) if candidate_color == "w" else (opponent, candidate)
    fields = ["w", "hm=0", "fm=1", "ep=-", "cont=0", "forced=-1", "epv=-1"]
    fields.extend(f"{piece},w,{square}" for piece, square in white)
    fields.extend(
        f"{piece},b,{mirror_square(piece, square)}" for piece, square in black
    )
    return ";".join(fields)


def side(upn: str) -> str:
    return upn[0]


def winner(upn: str) -> str | None:
    white = ";king,w," in ";" + upn
    black = ";king,b," in ";" + upn
    if white == black:
        return None
    return "w" if white else "b"


def repetition_key(upn: str) -> str:
    return ";".join(
        field for field in upn.split(";")
        if not field.startswith(("hm=", "fm="))
    )


@dataclass
class Engine:
    path: str

    def __post_init__(self) -> None:
        self.process = subprocess.Popen(
            [self.path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, bufsize=1,
        )

    def send(self, command: str) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def until(self, prefixes: tuple[str, ...]) -> str:
        assert self.process.stdout is not None
        for line in self.process.stdout:
            line = line.rstrip("\n")
            if line.startswith(prefixes):
                return line
        raise RuntimeError(f"engine stopped before {prefixes!r}")

    def new_game(self) -> None:
        self.send("ucinewgame")
        self.send("isready")
        self.until(("readyok",))

    def set_position(self, upn: str) -> None:
        self.send("position upn " + upn)
        answer = self.until(("positionok", "info string invalid upn"))
        if answer != "positionok":
            raise RuntimeError(f"engine rejected position: {answer}\n{upn}")

    def search(self, upn: str, depth: int, nodes: int) -> tuple[str | None, int]:
        self.set_position(upn)
        self.send(f"go depth {depth} nodes {nodes}")
        score = 0
        assert self.process.stdout is not None
        for line in self.process.stdout:
            if line.startswith("info "):
                match = re.search(r" score (cp|mate) (-?\d+)", line)
                if match:
                    score = int(match.group(2))
                    if match.group(1) == "mate":
                        score = (30_000 - abs(score)) * (1 if score >= 0 else -1)
            elif line.startswith("bestmove "):
                move = line.rstrip().split(" ", 1)[1]
                return (None if move == "(none)" else move), score
        raise RuntimeError("engine stopped during search")

    def apply(self, upn: str, move: str) -> str:
        self.set_position(upn)
        self.send("move " + move)
        answer = self.until(("position ", "illegalmove"))
        if answer == "illegalmove":
            raise RuntimeError(f"engine rejected generated move {move}\n{upn}")
        return answer.split(" ", 1)[1]

    def close(self) -> None:
        if self.process.poll() is None:
            self.send("quit")
            self.process.wait(timeout=5)


def play_game(
    first: Engine,
    second: Engine,
    candidate_color: str,
    start: str,
    depth: int,
    nodes: int,
    plies: int,
) -> float:
    upn = start
    trajectory: list[str] = []
    seen = {repetition_key(upn): 1}
    for _ply in range(plies):
        current = first if side(upn) == candidate_color else second
        move, _score = current.search(upn, depth, nodes)
        if move is None:
            break
        upn = first.apply(upn, move)
        trajectory.append(move)
        try:
            first.set_position(upn)
        except RuntimeError as exc:
            raise RuntimeError(
                f"engine emitted a non-round-trippable position after {move}\n"
                f"start={start}\nmoves={' '.join(trajectory)}\nposition={upn}"
            ) from exc
        key = repetition_key(upn)
        seen[key] = seen.get(key, 0) + 1
        if seen[key] >= 3:
            return 0.5
        victor = winner(upn)
        if victor:
            return 1.0 if victor == candidate_color else 0.0

    # A capped game is not a real draw.  Use a deliberately small bounded
    # tiebreak so decisive results always dominate the evolutionary ranking.
    evaluator = first if side(upn) == candidate_color else second
    _move, score = evaluator.search(upn, min(depth, 4), min(nodes, 1_000))
    candidate_score = score if side(upn) == candidate_color else -score
    return 0.5 + 0.15 * math.tanh(candidate_score / 600.0)


def score_army(
    engine_path: str,
    army: Army,
    opponents: Sequence[Army],
    depth: int,
    nodes: int,
    plies: int,
) -> tuple[float, tuple[float, ...]]:
    first, second = Engine(engine_path), Engine(engine_path)
    results: list[float] = []
    try:
        for opponent in opponents:
            for color in ("w", "b"):
                first.new_game()
                second.new_game()
                results.append(play_game(
                    first, second, color, position(army, opponent, color),
                    depth, nodes, plies,
                ))
    finally:
        first.close()
        second.close()
    return sum(results) / len(results), tuple(results)


def _score_job(arguments):
    return score_army(*arguments)


def army_key(army: Army) -> tuple[tuple[str, str], ...]:
    return tuple(sorted(army))


def format_team(army: Army) -> str:
    return ";".join(f"{piece},{square}" for piece, square in army)


def unique_population(armies: Iterable[Army]) -> list[Army]:
    answer, seen = [], set()
    for army in armies:
        key = army_key(army)
        if key not in seen:
            seen.add(key)
            answer.append(army)
    return answer


def evaluate_population(
    engine: str,
    population: Sequence[Army],
    opponents: Sequence[Army],
    depth: int,
    nodes: int,
    plies: int,
    workers: int,
) -> list[tuple[float, tuple[float, ...], Army]]:
    jobs = [(engine, army, opponents, depth, nodes, plies) for army in population]
    if workers == 1:
        scores = map(_score_job, jobs)
    else:
        try:
            executor = concurrent.futures.ProcessPoolExecutor(max_workers=workers)
        except (OSError, PermissionError):
            # Sandboxed CI environments can expose CPUs while denying the
            # POSIX semaphores used by ProcessPoolExecutor.
            scores = map(_score_job, jobs)
            return sorted(
                ((*score, army) for score, army in zip(scores, population)),
                key=lambda item: item[0], reverse=True,
            )
        try:
            scores = executor.map(_score_job, jobs)
            scored = [(*score, army) for score, army in zip(scores, population)]
        finally:
            executor.shutdown()
        return sorted(scored, key=lambda item: item[0], reverse=True)
    return sorted(
        ((*score, army) for score, army in zip(scores, population)),
        key=lambda item: item[0], reverse=True,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", default=str(Path(__file__).parents[1] / "src/ultimatefish"))
    parser.add_argument("--population", type=int, default=18)
    parser.add_argument("--generations", type=int, default=5)
    parser.add_argument("--depth", type=int, default=20)
    parser.add_argument("--nodes", type=int, default=3_000)
    parser.add_argument("--plies", type=int, default=100)
    parser.add_argument("--finalists", type=int, default=4)
    parser.add_argument("--final-nodes", type=int, default=15_000)
    parser.add_argument("--final-plies", type=int, default=180)
    parser.add_argument("--workers", type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--seed", type=int, default=731)
    args = parser.parse_args()
    if args.population < 4 or args.generations < 1 or args.workers < 1:
        parser.error("population >= 4, generations >= 1, and workers >= 1 are required")

    for army in OPPONENTS:
        validate_army(army)
    rng = random.Random(args.seed)
    population = list(OPPONENTS)
    while len(population) < args.population:
        layout = random_layout(random_roster(rng), rng)
        if layout is not None:
            population = unique_population((*population, layout))

    hall: list[Army] = list(OPPONENTS)
    for generation in range(1, args.generations + 1):
        scored = evaluate_population(
            args.engine, population, OPPONENTS, args.depth,
            args.nodes, args.plies, args.workers,
        )
        best_score, best_games, best = scored[0]
        print(
            f"generation {generation}: {best_score:.4f} "
            f"games={' '.join(f'{value:.2f}' for value in best_games)} "
            f"team={format_team(best)}",
            flush=True,
        )
        elite_count = max(4, args.population // 4)
        elite = [army for _score, _games, army in scored[:elite_count]]
        hall = unique_population((*hall, *elite))
        children = list(elite)
        while len(children) < args.population:
            if rng.random() < 0.18:
                child = random_layout(random_roster(rng), rng)
            else:
                child = mutate_army(rng.choice(elite), rng)
            if child is not None:
                children = unique_population((*children, child))
        population = children

    # Re-evaluate the strongest distinct discoveries with enough nodes and
    # plies to suppress shallow tactical and arbitrary-cap noise.
    shortlist = unique_population(
        army for _score, _games, army in scored[:args.finalists]
    )
    final = evaluate_population(
        args.engine, shortlist, OPPONENTS, args.depth,
        args.final_nodes, args.final_plies, args.workers,
    )
    print("finalists:")
    for rank, (score, games, army) in enumerate(final, 1):
        print(
            f"{rank}. {score:.4f} games={' '.join(f'{value:.2f}' for value in games)} "
            f"--own-team '{format_team(army)}'"
        )


if __name__ == "__main__":
    main()
