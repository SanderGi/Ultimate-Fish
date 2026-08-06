#!/usr/bin/env python3
"""Evolve legal 100-point Chess Ultimate armies with engine self-play.

The search optimizes both roster and placement.  Every game uses the exact
Ultimate Fish move generator, alternates the candidate's color, adjudicates
native threefold repetition, and keeps a small search-evaluation tiebreak only
for games that hit the configured ply cap.
"""

from __future__ import annotations

import argparse
import ast
import concurrent.futures
import functools
import itertools
import json
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

# Recovered from a live 2026-08-05 Unranked match. Five long-range Snipers
# punish exposed high-value formations immediately, while the off-corner King
# proves that saved-army royal placement is not fixed to a1.
SNIPER_ARMY: Army = (
    ("king", "e2"),
    ("sniper", "a3"), ("sniper", "b3"), ("sniper", "e3"),
    ("sniper", "g3"), ("sniper", "h3"),
    ("berserker", "b2"),
)

OPPONENTS = (
    CURRENT_ARMY, LIVE_GIANT_ARMY, PENGUIN_ARMY, BALANCED_ARMY, SPECIAL_ARMY,
    SNIPER_ARMY,
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
    if sum(piece == "king" for piece, _square in army) != 1:
        raise ValueError("an army must contain exactly one king")
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


def random_layout(
    roster: Sequence[str],
    rng: random.Random,
    preferred: Sequence[str | None] | None = None,
) -> Army | None:
    """Pack a roster into the native 8x3 deployment zone.

    Preferred squares let roster mutations preserve most of a proven setup
    instead of randomizing every placement. Backtracking is still allowed to
    displace a preferred piece when a new Giant or Copycat needs the cells.
    """
    if preferred is not None and len(preferred) != len(roster):
        raise ValueError("preferred layout length does not match roster")
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
        wanted = preferred[original_index] if preferred is not None else None
        if wanted in candidates:
            candidates.remove(wanted)
            candidates.insert(0, wanted)
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


def random_roster(
    rng: random.Random,
    preferred_pieces: Sequence[str] = (),
) -> tuple[str, ...]:
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
            preferred_counts = {
                piece: preferred_pieces.count(piece) for piece in feasible
            }
            weights = [
                (1.0 + preferred_counts[piece] * 1.8)
                / (1.0 + counts[piece] * 0.7)
                for piece in feasible
            ]
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
    entries = [(piece, square) for piece, square in parent if piece != "king"]
    roster = [piece for piece, _square in entries]
    roll = rng.random()

    # Most mutations should improve placement locally. The old implementation
    # redrew the entire formation, which made placement fitness almost
    # impossible to inherit across generations.
    if roll < 0.45:
        child = list(parent)
        # King placement is part of the setup and is not fixed to a1 in the
        # shipping builder; live opponents use off-corner royal formations.
        movable = list(range(len(child)))
        rng.shuffle(movable)
        for index in movable:
            occupied: set[str] = set()
            for other, (piece, square) in enumerate(child):
                if other != index:
                    occupied.update(footprint(piece, square))
            candidates = list(SQUARES)
            rng.shuffle(candidates)
            old_square = child[index][1]
            candidates.sort(
                key=lambda square: abs(FILES.index(square[0]) - FILES.index(old_square[0]))
                + abs(int(square[1:]) - int(old_square[1:]))
            )
            for square in candidates:
                cells = footprint(child[index][0], square)
                if square != old_square and cells and not cells & occupied:
                    child[index] = (child[index][0], square)
                    answer = tuple(child)
                    validate_army(answer)
                    return answer
        return parent

    if roll < 0.60:
        return random_layout(roster, rng, [square for _piece, square in entries]) or parent

    for _attempt in range(100):
        remove_count = rng.randint(1, min(4, len(roster)))
        removed_indices = sorted(rng.sample(range(len(roster)), remove_count), reverse=True)
        child_entries = entries.copy()
        removed_entries = [child_entries.pop(index) for index in removed_indices]
        removed = [piece for piece, _square in removed_entries]
        choices = replacement_catalog().get(sum(PIECE_COST[piece] for piece in removed), ())
        if not choices:
            continue
        replacement = rng.choice(choices)
        if sorted(replacement) == sorted(removed):
            continue
        child_entries.extend((piece, None) for piece in replacement)
        child = [piece for piece, _square in child_entries]
        if sum(map(footprint_size, child)) > 23:
            continue
        layout = random_layout(
            child, rng, [square for _piece, square in child_entries],
        )
        if layout is not None:
            return layout
    return random_layout(random_roster(rng), rng) or parent


def crossover_armies(first: Army, second: Army, rng: random.Random) -> Army:
    """Create an exact-cost child biased toward both parent rosters."""
    first_roster = [piece for piece, _square in first if piece != "king"]
    second_roster = [piece for piece, _square in second if piece != "king"]
    for _attempt in range(100):
        roster = random_roster(rng, (*first_roster, *second_roster))
        layout = random_layout(roster, rng)
        if layout is not None:
            return layout
    return mutate_army(rng.choice((first, second)), rng)


def mirror_square(piece: str, square: str) -> str:
    # UPN stores a Giant by the global lower-left anchor of its 2x2 footprint.
    # Mirroring ranks 1-2 to ranks 8-10 therefore maps that anchor with 10-r;
    # single-cell pieces (and both Copycat cells) use the ordinary 11-r map.
    rank = 10 - int(square[1:]) if piece == "giant" else 11 - int(square[1:])
    return f"{square[0]}{rank}"


def position(candidate: Army, opponent: Army, candidate_color: str) -> str:
    white, black = (candidate, opponent) if candidate_color == "w" else (opponent, candidate)
    fields = ["w", "hm=0", "fm=1", "ep=-", "cont=0", "forced=-1", "epv=-1"]
    def deployed(piece: str, color: str, square: str) -> str:
        if piece == "ghost":
            # ``visible`` is relative to the Ghost's opponent. A locally
            # visible newly deployed Ghost is still hidden from enemy rays.
            return f"{piece},{color},{square},0,0,0,0,0,0,-1,1,-1,0"
        return f"{piece},{color},{square}"

    fields.extend(deployed(piece, "w", square) for piece, square in white)
    fields.extend(
        deployed(piece, "b", mirror_square(piece, square))
        for piece, square in black
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

    def search(self, upn: str, depth: int, nodes: int,
               movetime_ms: int = 0) -> tuple[str | None, int]:
        self.set_position(upn)
        command = f"go depth {depth} nodes {nodes}"
        if movetime_ms:
            command += f" movetime {movetime_ms}"
        self.send(command)
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

    def result(self, upn: str) -> str:
        """Return white, black, draw, or ongoing from the rule engine."""
        self.set_position(upn)
        self.send("d")
        answer = self.until(("result ",))
        return answer.split(" ", 2)[1]

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
            result = first.result(upn)
            if result in ("white", "black"):
                victor = "w" if result == "white" else "b"
                return 1.0 if victor == candidate_color else 0.0
            if result == "draw":
                return 0.5
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


def army_key(army: Army) -> tuple[tuple[str, str], ...]:
    return tuple(sorted(army))


def format_team(army: Army) -> str:
    return ";".join(f"{piece},{square}" for piece, square in army)


def parse_team(text: str) -> Army:
    army = tuple(
        tuple(field.split(",", 1))
        for field in text.split(";") if field
    )
    validate_army(army)
    return army


def unique_population(armies: Iterable[Army]) -> list[Army]:
    answer, seen = [], set()
    for army in armies:
        key = army_key(army)
        if key not in seen:
            seen.add(key)
            answer.append(army)
    return answer


@dataclass(frozen=True)
class Standing:
    fitness: float
    score: float
    lower_quartile: float
    results: tuple[float, ...]
    army: Army


# population index A, population index B (-1 for an external opponent), A, B
MatchJob = tuple[int, int, Army, Army]


def build_league_schedule(
    population: Sequence[Army],
    external_opponents: Sequence[Army],
) -> list[MatchJob]:
    """Build a color-balanced round robin plus common archive matches."""
    population_keys = {army_key(army) for army in population}
    external_opponents = [
        army for army in unique_population(external_opponents)
        if army_key(army) not in population_keys
    ]
    schedule = [
        (first, second, population[first], population[second])
        for first, second in itertools.combinations(range(len(population)), 2)
    ]
    schedule.extend(
        (index, -1, army, opponent)
        for index, army in enumerate(population)
        for opponent in external_opponents
    )
    return schedule


def _play_schedule_chunk(arguments):
    engine_path, jobs, depth, nodes, plies = arguments
    first, second = Engine(engine_path), Engine(engine_path)
    completed: list[tuple[int, int, tuple[float, float]]] = []
    try:
        for first_index, second_index, first_army, second_army in jobs:
            results: list[float] = []
            for color in ("w", "b"):
                first.new_game()
                second.new_game()
                results.append(play_game(
                    first, second, color,
                    position(first_army, second_army, color),
                    depth, nodes, plies,
                ))
            completed.append((first_index, second_index, (results[0], results[1])))
    finally:
        first.close()
        second.close()
    return completed


def evaluate_league(
    engine: str,
    population: Sequence[Army],
    external_opponents: Sequence[Army],
    depth: int,
    nodes: int,
    plies: int,
    workers: int,
) -> list[Standing]:
    """Score every setup in a shared, zero-sum coevolution league.

    Each population pair plays both colors exactly once. Every candidate also
    plays the same external hall-of-fame pool. Fitness blends total score with
    the bottom matchup quartile so a brittle counter-setup cannot dominate by
    farming only one common formation.
    """
    schedule = build_league_schedule(population, external_opponents)
    if not schedule:
        raise ValueError("league schedule is empty")
    worker_count = min(workers, len(schedule))
    chunks = [schedule[index::worker_count] for index in range(worker_count)]
    arguments = [
        (engine, chunk, depth, nodes, plies)
        for chunk in chunks if chunk
    ]
    completed: list[list[tuple[int, int, tuple[float, float]]]]
    if worker_count == 1:
        completed = [_play_schedule_chunk(arguments[0])]
    else:
        # Search runs in external C++ processes, so Python threads provide real
        # parallelism without ProcessPool's POSIX semaphore dependency. This is
        # both faster and reliable in macOS sandboxed development environments.
        with concurrent.futures.ThreadPoolExecutor(max_workers=worker_count) as executor:
            completed = list(executor.map(_play_schedule_chunk, arguments))

    points = [0.0] * len(population)
    games = [0] * len(population)
    results: list[list[float]] = [[] for _army in population]
    match_scores: list[list[float]] = [[] for _army in population]
    for chunk in completed:
        for first_index, second_index, pair in chunk:
            first_match = sum(pair) / len(pair)
            points[first_index] += sum(pair)
            games[first_index] += len(pair)
            results[first_index].extend(pair)
            match_scores[first_index].append(first_match)
            if second_index >= 0:
                mirrored = tuple(1.0 - value for value in pair)
                points[second_index] += sum(mirrored)
                games[second_index] += len(mirrored)
                results[second_index].extend(mirrored)
                match_scores[second_index].append(1.0 - first_match)

    standings: list[Standing] = []
    for index, army in enumerate(population):
        if not games[index] or not match_scores[index]:
            raise RuntimeError("candidate received no league games")
        score = points[index] / games[index]
        quarter = max(1, math.ceil(len(match_scores[index]) / 4))
        lower_quartile = sum(sorted(match_scores[index])[:quarter]) / quarter
        fitness = score * 0.8 + lower_quartile * 0.2
        standings.append(Standing(
            fitness, score, lower_quartile, tuple(results[index]), army,
        ))
    return sorted(
        standings,
        key=lambda standing: (standing.fitness, standing.score),
        reverse=True,
    )


def result_summary(results: Sequence[float]) -> str:
    wins = sum(result >= 0.75 for result in results)
    losses = sum(result <= 0.25 for result in results)
    draws = len(results) - wins - losses
    return f"{wins}W-{draws}D-{losses}L"


def standing_json(standing: Standing) -> dict[str, object]:
    return {
        "fitness": round(standing.fitness, 6),
        "score": round(standing.score, 6),
        "lower_quartile": round(standing.lower_quartile, 6),
        "record": result_summary(standing.results),
        "team": format_team(standing.army),
    }


def write_run_state(
    path: str,
    args: argparse.Namespace,
    completed_generations: int,
    rng: random.Random,
    population: Sequence[Army],
    hall: Sequence[Army],
    champions: Sequence[Army],
    history: Sequence[dict[str, object]],
    finalists: Sequence[Standing] = (),
) -> None:
    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "seed": args.seed,
        "completed_generations": completed_generations,
        "parameters": {
            key: value for key, value in vars(args).items()
            if key not in ("output", "resume")
        },
        "rng_state": repr(rng.getstate()),
        "population": [format_team(army) for army in population],
        "hall": [format_team(army) for army in hall],
        "champions": [format_team(army) for army in champions],
        "generations": list(history),
        "finalists": [standing_json(standing) for standing in finalists],
    }
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, indent=2) + "\n")
    temporary.replace(output)


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
    parser.add_argument("--hall-size", type=int, default=12)
    parser.add_argument("--immigrants", type=float, default=0.15)
    parser.add_argument("--crossover", type=float, default=0.25)
    parser.add_argument("--workers", type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--seed", type=int, default=731)
    parser.add_argument("--output", help="write a reproducible JSON result summary")
    parser.add_argument("--resume", help="resume population, archive, and RNG state from JSON")
    args = parser.parse_args()
    if args.population < len(OPPONENTS) or args.generations < 1 or args.workers < 1:
        parser.error(
            f"population >= {len(OPPONENTS)}, generations >= 1, and workers >= 1 are required"
        )
    if args.finalists < 1 or args.hall_size < 1:
        parser.error("finalists and hall-size must be positive")
    if not 0.0 <= args.immigrants <= 1.0 or not 0.0 <= args.crossover <= 1.0:
        parser.error("immigrants and crossover must be between zero and one")
    if args.immigrants + args.crossover > 1.0:
        parser.error("immigrants plus crossover cannot exceed one")

    for army in OPPONENTS:
        validate_army(army)
    rng = random.Random(args.seed)
    start_generation = 1
    if args.resume:
        saved = json.loads(Path(args.resume).read_text())
        population = [parse_team(team) for team in saved["population"]]
        hall = [parse_team(team) for team in saved["hall"]]
        champions = [parse_team(team) for team in saved["champions"]]
        history = list(saved["generations"])
        rng.setstate(ast.literal_eval(saved["rng_state"]))
        start_generation = int(saved["completed_generations"]) + 1
        if len(population) != args.population:
            parser.error("resumed population size does not match --population")
        if not args.output:
            args.output = args.resume
    else:
        population = list(OPPONENTS)
        while len(population) < args.population:
            layout = random_layout(random_roster(rng), rng)
            if layout is not None:
                population = unique_population((*population, layout))
        hall = list(OPPONENTS)
        champions = []
        history = []

    scored: list[Standing] = []
    for generation in range(start_generation, args.generations + 1):
        external = unique_population((*OPPONENTS, *hall[-args.hall_size:]))
        scored = evaluate_league(
            args.engine, population, external, args.depth,
            args.nodes, args.plies, args.workers,
        )
        best = scored[0]
        print(
            f"generation {generation}: fitness={best.fitness:.4f} "
            f"score={best.score:.4f} floor={best.lower_quartile:.4f} "
            f"record={result_summary(best.results)} "
            f"team={format_team(best.army)}",
            flush=True,
        )
        history.append({"generation": generation, **standing_json(best)})
        elite_count = max(4, args.population // 4)
        elite = [standing.army for standing in scored[:elite_count]]
        generation_champions = [
            standing.army for standing in scored[:max(2, args.population // 8)]
        ]
        champions = unique_population((*champions, *generation_champions))
        hall = unique_population((*OPPONENTS, *hall, *generation_champions))
        if len(hall) > len(OPPONENTS) + args.hall_size:
            hall = unique_population((*OPPONENTS, *hall[-args.hall_size:]))

        children = list(elite)
        attempts = 0
        while len(children) < args.population:
            attempts += 1
            roll = rng.random()
            if roll < args.immigrants:
                child = random_layout(random_roster(rng), rng)
            elif roll < args.immigrants + args.crossover and len(elite) >= 2:
                parents = rng.sample(elite, 2)
                child = crossover_armies(parents[0], parents[1], rng)
            else:
                child = mutate_army(rng.choice(elite), rng)
            if child is not None:
                children = unique_population((*children, child))
            if attempts > args.population * 500:
                raise RuntimeError("could not create a diverse next generation")
        population = children
        if args.output:
            write_run_state(
                args.output, args, generation, rng, population, hall,
                champions, history,
            )

    # Re-rank discoveries from every generation in one shared league before
    # spending the final node budget. This avoids selecting a transient setup
    # merely because it exploited the opponents in its own generation.
    candidate_pool = unique_population((
        *champions,
        *(standing.army for standing in scored[:args.finalists * 2]),
        *hall,
    ))
    preliminary = evaluate_league(
        args.engine, candidate_pool, OPPONENTS, args.depth,
        max(args.nodes, min(args.final_nodes // 4, args.nodes * 4)),
        max(args.plies, args.final_plies // 2), args.workers,
    )
    finalists = [standing.army for standing in preliminary[:args.finalists]]
    final_opponents = unique_population((*OPPONENTS, *hall, *candidate_pool))
    final = evaluate_league(
        args.engine, finalists, final_opponents, args.depth,
        args.final_nodes, args.final_plies, args.workers,
    )
    print("finalists:")
    for rank, standing in enumerate(final, 1):
        print(
            f"{rank}. fitness={standing.fitness:.4f} score={standing.score:.4f} "
            f"floor={standing.lower_quartile:.4f} "
            f"record={result_summary(standing.results)} "
            f"--own-team '{format_team(standing.army)}'"
        )

    if args.output:
        write_run_state(
            args.output, args, args.generations, rng, population, hall,
            champions, history, final,
        )


if __name__ == "__main__":
    main()
