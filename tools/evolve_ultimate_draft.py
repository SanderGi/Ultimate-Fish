#!/usr/bin/env python3
"""Coevolve Chess Ultimate Ranked draft policies with engine self-play.

Unlike fixed-army evolution, each matchup executes all twelve native Ranked
windows.  A policy has separate pick, repeat, and ban values plus public-roster
features, so it can react to already locked enemy groups without using hidden
placement information.  The resulting rosters are deployed with the same
corner-King ray safety and footprint constraints as the phone controller.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import functools
import itertools
import json
import math
import os
import random
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

from evolve_ultimate_army import (
    Army,
    Engine,
    PIECE_COST,
    footprint,
    footprint_size,
    play_game,
    position,
    result_summary,
)


PIECES = tuple(PIECE_COST)
INDEX = {piece: index for index, piece in enumerate(PIECES)}

# Current native evolved policy, expressed in selectable-piece order. Keep this
# synchronized with src/ultimate/draft.cpp so continuation leagues challenge
# the policy that actually ships rather than the superseded heuristic.
BASE_PICK = {
    "jester": 1168, "knight": 460, "pawn": 184, "queen": 817,
    "rook": 762, "bishop": 564, "berserker": 798, "bomb": 1149,
    "ninja": 388, "turtle": -1134, "ghost": 502, "mage": 1076,
    "penguin": 1211, "parasite": 798, "devil": 244, "sludge": 385,
    "sniper": 383, "prince": 1797, "checker": -28, "giant": 219,
    "copycat": 1057, "angel": 864, "fisherman": 290, "dragon": -104,
}
BASE_REPEAT = {
    "jester": -104, "knight": -96, "pawn": -276, "queen": 281,
    "rook": 186, "bishop": -174, "berserker": -12, "bomb": -133,
    "ninja": -201, "turtle": 335, "ghost": 343, "mage": -161,
    "penguin": 216, "parasite": -135, "devil": -16, "sludge": 177,
    "sniper": 110, "prince": -161, "checker": 52, "giant": -16,
    "copycat": -112, "angel": 289, "fisherman": -300, "dragon": 169,
}
BASE_BAN = {
    "jester": 1472, "knight": -187, "pawn": -370, "queen": 686,
    "rook": 1749, "bishop": 1538, "berserker": -419, "bomb": 248,
    "ninja": 208, "turtle": -1290, "ghost": 1326, "mage": 1251,
    "penguin": 1131, "parasite": 643, "devil": 316, "sludge": 845,
    "sniper": 891, "prince": 1020, "checker": 76, "giant": -1512,
    "copycat": 32, "angel": -693, "fisherman": 1141, "dragon": 317,
}


@dataclass(frozen=True)
class DraftPolicy:
    pick: tuple[int, ...]
    repeat: tuple[int, ...]
    ban: tuple[int, ...]
    opponent_deny: int = 120
    self_preserve: int = 80
    final_penguin: int = 500

    def __post_init__(self) -> None:
        if not (len(self.pick) == len(self.repeat) == len(self.ban) == len(PIECES)):
            raise ValueError("draft policy arrays must cover every selectable piece")

    def pick_score(
        self, piece: str, own: Sequence[str], final: bool,
        royal_revealed: bool = False,
    ) -> int:
        index = INDEX[piece]
        return (
            self.pick[index]
            + self.repeat[index] * own.count(piece)
            + (self.final_penguin if final and piece == "penguin" else 0)
            - (100_000 if royal_revealed and piece == "jester" else 0)
        )

    def ban_score(self, piece: str, own: Sequence[str], enemy: Sequence[str]) -> int:
        index = INDEX[piece]
        return (
            self.ban[index]
            + self.opponent_deny * enemy.count(piece)
            - self.self_preserve * own.count(piece)
        )


BASE_POLICY = DraftPolicy(
    tuple(BASE_PICK[piece] for piece in PIECES),
    tuple(BASE_REPEAT[piece] for piece in PIECES),
    tuple(BASE_BAN[piece] for piece in PIECES),
    opponent_deny=168, self_preserve=327, final_penguin=-164,
)


@dataclass(frozen=True)
class DraftOutcome:
    white: Army
    black: Army
    white_groups: tuple[tuple[str, ...], ...]
    black_groups: tuple[tuple[str, ...], ...]
    bans: tuple[str, ...]


def team_points(team: Sequence[str]) -> int:
    return sum(PIECE_COST.get(piece, 0) for piece in team)


def team_slots(team: Sequence[str]) -> int:
    return sum(footprint_size(piece) for piece in team)


def draft_window(phase: int, team: Sequence[str]) -> tuple[int, int, bool]:
    locked = team_points(team)
    if phase in (2, 3):
        return locked + 15, 40, False
    if phase == 6:
        return locked + 15, 80, False
    if phase == 7:
        return locked + 40, 90, False
    if phase in (10, 11):
        return locked, 100, True
    raise ValueError(f"phase {phase} is not a pick window")


@functools.lru_cache(maxsize=None)
def can_add_at_least(
    available: tuple[str, ...], slots: int, cap: int, needed: int,
) -> bool:
    """Fast unbounded footprint/material feasibility via integer bitsets."""
    if needed <= 0:
        return True
    if slots <= 0 or cap < needed:
        return False
    reachable = [0] * (slots + 1)
    reachable[0] = 1  # bit N means exactly N additional points
    mask = (1 << (cap + 1)) - 1
    for used in range(slots + 1):
        if not reachable[used]:
            continue
        for piece in available:
            next_slots = used + footprint_size(piece)
            if next_slots <= slots:
                reachable[next_slots] |= (
                    reachable[used] << PIECE_COST[piece]
                ) & mask
    achievable = 0
    for values in reachable:
        achievable |= values
    return bool(achievable >> needed)


PREFERENCES = {
    "jester": ("b1", "h1", "g1"),
    "ghost": ("d2", "e2", "c2", "f2"),
    "dragon": ("d1", "e1", "c1", "f1"),
    "bomb": ("d3", "e3", "c3", "f3"),
    "berserker": ("e3", "d3", "f3", "c3"),
    "parasite": ("c3", "f3", "b3", "g3"),
    "penguin": ("d3", "e3", "c3", "f3", "e2", "d2", "f2", "c2"),
    "sniper": ("h1", "g1", "f1"),
    "copycat": ("h2", "g2", "h3", "g3"),
    "rook": ("h1", "g1", "b1"),
    "queen": ("d1", "e1", "c1"),
    "fisherman": ("c2", "f2", "b2", "g2"),
    "angel": ("c1", "b1", "d1"),
    "devil": ("f2", "c2", "g2"),
}
FALLBACK = tuple(
    f"{file}{rank}" for rank in (3, 2, 1) for file in "defcbgah"
    if not (file == "a" and rank == 1)
)
KING_SHIELD = frozenset({"a2", "b2", "b1"})


def deploy_groups(groups: Sequence[Sequence[str]]) -> Army:
    """Backtrack a safe legal layout for the roster locked by three groups.

    Cheap Giants can be selected in a late remainder after early single-cell
    groups have fragmented every 2x2 region. Planning the final footprint set
    avoids giving such a legal policy an artificial loss in the draft league.
    The phone planner uses the same capacity invariant while placing each
    immutable group and can reserve these large regions incrementally.
    """
    roster = tuple(piece for group in groups for piece in group)
    return _deploy_roster(roster)


@functools.lru_cache(maxsize=None)
def _deploy_roster(roster: tuple[str, ...]) -> Army:
    """Memoized exact deployment for a roster, independent of pick grouping."""
    indexed = sorted(
        enumerate(roster),
        key=lambda item: (footprint_size(item[1]), PIECE_COST[item[1]]),
        reverse=True,
    )
    occupied = {"a1"}
    solid = {"a1"}
    assignments: dict[int, str] = {}

    def clearance(cells: frozenset[str]) -> int:
        return min(
            max(abs(ord(left[0]) - ord(right[0])),
                abs(int(left[1:]) - int(right[1:])))
            for left in cells for right in occupied
        )

    require_shield = True
    failed: set[tuple[int, frozenset[str], frozenset[str], bool]] = set()

    def place(depth: int) -> bool:
        if depth == len(indexed):
            return not require_shield or KING_SHIELD <= solid

        state = depth, frozenset(occupied), frozenset(solid), require_shield
        if state in failed:
            return False

        original_index, piece = indexed[depth]
        candidates = tuple(dict.fromkeys(PREFERENCES.get(piece, ()) + FALLBACK))
        valid = [
            (square, footprint(piece, square)) for square in candidates
            if footprint(piece, square) and not footprint(piece, square) & occupied
        ]
        blocking = piece not in ("ghost", "bomb")
        valid.sort(
            key=lambda item: (
                len(item[1] & (KING_SHIELD - occupied)) if blocking else 0,
                clearance(item[1]),
                -candidates.index(item[0]),
            ),
            reverse=True,
        )
        for square, cells in valid:
            occupied.update(cells)
            if blocking:
                solid.update(cells)
            assignments[original_index] = square
            if place(depth + 1):
                return True
            assignments.pop(original_index)
            if blocking:
                solid.difference_update(cells)
            occupied.difference_update(cells)
        failed.add(state)
        return False

    if not place(0):
        # A legal mutation made entirely from hidden/explosive pieces may have
        # no three-ray shield. Keep it in the league so actual engine play can
        # punish that weakness rather than treating it as a protocol failure.
        require_shield = False
        failed.clear()
        if not place(0):
            raise RuntimeError("drafted roster has no legal deployment")
    return (("king", "a1"), *(
        (piece, assignments[index]) for index, piece in enumerate(roster)
    ))


def simulate_draft(white: DraftPolicy, black: DraftPolicy) -> DraftOutcome:
    policies = {"w": white, "b": black}
    teams = {"w": ["king"], "b": ["king"]}
    groups: dict[str, list[tuple[str, ...]]] = {"w": [], "b": []}
    banned: set[str] = set()
    bans: list[str] = []
    for phase in range(12):
        color = "w" if phase % 2 == 0 else "b"
        enemy_color = "b" if color == "w" else "w"
        policy = policies[color]
        own, enemy = teams[color], teams[enemy_color]
        if phase in (0, 1, 4, 5, 8, 9):
            legal = [piece for piece in PIECES if piece not in banned]
            piece = max(
                legal,
                key=lambda candidate: (
                    policy.ban_score(candidate, own, enemy),
                    -INDEX[candidate],
                ),
            )
            banned.add(piece)
            bans.append(piece)
            continue

        minimum, maximum, final = draft_window(phase, own)
        selected: list[str] = []

        def preserves_minimum(candidate: str) -> bool:
            """Whether cells left can reach this and the next required floor."""
            next_points = team_points(own) + PIECE_COST[candidate]
            slots_left = 24 - team_slots(own) - footprint_size(candidate)

            def can_reach(target: int, cap: int) -> bool:
                if next_points >= target:
                    return True
                points_left = cap - next_points
                return can_add_at_least(
                    tuple(piece for piece in PIECES if piece not in banned),
                    slots_left, points_left, target - next_points,
                )

            if not can_reach(minimum, maximum):
                return False
            # Cheap pieces in the opening group must not consume every board
            # cell needed by the following required-addition window.
            future = 55 if phase == 2 else 80 if phase == 3 else minimum
            future_cap = 80 if phase == 2 else 90 if phase == 3 else maximum
            return can_reach(future, future_cap)

        while team_points(own) < maximum:
            legal = [
                piece for piece in PIECES
                if piece not in banned
                and team_points(own) + PIECE_COST[piece] <= maximum
                and team_slots(own) + footprint_size(piece) <= 24
                # Five disjoint 2x2 models cannot be packed into the 8x3 home
                # zone with its pre-placed corner King, despite summing to the
                # coarse 24-cell cap used by the native draft UI.
                and (piece != "giant" or own.count("giant") < 4)
                and preserves_minimum(piece)
            ]
            if not legal:
                break
            piece = max(
                legal,
                key=lambda candidate: (
                    policy.pick_score(
                        candidate, own, final,
                        royal_revealed=bool(groups[color]),
                    ),
                    -INDEX[candidate],
                ),
            )
            own.append(piece)
            selected.append(piece)
        if team_points(own) < minimum:
            raise RuntimeError(
                f"policy cannot commit phase {phase}: {team_points(own)} < {minimum}"
            )
        groups[color].append(tuple(selected))

    return DraftOutcome(
        deploy_groups(groups["w"]), deploy_groups(groups["b"]),
        tuple(groups["w"]), tuple(groups["b"]), tuple(bans),
    )


def policy_key(policy: DraftPolicy) -> tuple[object, ...]:
    return (
        policy.pick, policy.repeat, policy.ban,
        policy.opponent_deny, policy.self_preserve, policy.final_penguin,
    )


def mutate_policy(parent: DraftPolicy, rng: random.Random) -> DraftPolicy:
    arrays = [list(parent.pick), list(parent.repeat), list(parent.ban)]
    for _change in range(rng.randint(2, 8)):
        values = rng.choice(arrays)
        index = rng.randrange(len(PIECES))
        values[index] = max(-2000, min(2000, values[index] + rng.randint(-300, 300)))
    scalars = [parent.opponent_deny, parent.self_preserve, parent.final_penguin]
    if rng.random() < 0.7:
        index = rng.randrange(3)
        scalars[index] = max(-1000, min(1500, scalars[index] + rng.randint(-200, 200)))
    return DraftPolicy(*map(tuple, arrays), *scalars)


def random_policy(rng: random.Random) -> DraftPolicy:
    return DraftPolicy(
        tuple(BASE_PICK[piece] + rng.randint(-900, 900) for piece in PIECES),
        tuple(rng.randint(-350, 350) for _piece in PIECES),
        tuple(BASE_PICK[piece] + rng.randint(-900, 900) for piece in PIECES),
        rng.randint(-100, 600), rng.randint(-100, 600), rng.randint(-300, 900),
    )


def crossover_policy(
    first: DraftPolicy, second: DraftPolicy, rng: random.Random,
) -> DraftPolicy:
    def cross(left: Sequence[int], right: Sequence[int]) -> tuple[int, ...]:
        return tuple(a if rng.random() < 0.5 else b for a, b in zip(left, right))

    return DraftPolicy(
        cross(first.pick, second.pick),
        cross(first.repeat, second.repeat),
        cross(first.ban, second.ban),
        rng.choice((first.opponent_deny, second.opponent_deny)),
        rng.choice((first.self_preserve, second.self_preserve)),
        rng.choice((first.final_penguin, second.final_penguin)),
    )


def unique_policies(policies: Iterable[DraftPolicy]) -> list[DraftPolicy]:
    answer: list[DraftPolicy] = []
    seen = set()
    for policy in policies:
        key = policy_key(policy)
        if key not in seen:
            seen.add(key)
            answer.append(policy)
    return answer


@dataclass(frozen=True)
class Standing:
    fitness: float
    score: float
    lower_quartile: float
    results: tuple[float, ...]
    policy: DraftPolicy


PolicyJob = tuple[int, int, DraftPolicy, DraftPolicy]


def _job_position_key(job: PolicyJob) -> tuple[str, str]:
    _first_index, _second_index, first_policy, second_policy = job
    first_as_white = simulate_draft(first_policy, second_policy)
    first_as_black = simulate_draft(second_policy, first_policy)
    return (
        position(first_as_white.white, first_as_white.black, "w"),
        position(first_as_black.black, first_as_black.white, "b"),
    )


def _play_chunk(arguments) -> list[tuple[int, int, tuple[float, float]]]:
    engine_path, jobs, depth, nodes, plies = arguments
    first, second = Engine(engine_path), Engine(engine_path)
    completed = []
    # Policy weights frequently collapse to the exact same two deployed
    # positions. Fixed-node search is deterministic, so replaying those games
    # cannot add evidence; cache them inside each worker and attribute the
    # identical result to every policy pairing that produced the position.
    result_cache: dict[tuple[str, str], float] = {}

    def cached_game(perspective: str, start: str) -> float:
        key = perspective, start
        if key not in result_cache:
            first.new_game()
            second.new_game()
            result_cache[key] = play_game(
                first, second, perspective, start, depth, nodes, plies,
            )
        return result_cache[key]

    try:
        for first_index, second_index, first_policy, second_policy in jobs:
            first_as_white = simulate_draft(first_policy, second_policy)
            white_score = cached_game(
                "w", position(first_as_white.white, first_as_white.black, "w"),
            )

            first_as_black = simulate_draft(second_policy, first_policy)
            black_score = cached_game(
                "b", position(first_as_black.black, first_as_black.white, "b"),
            )
            completed.append((first_index, second_index, (white_score, black_score)))
    finally:
        first.close()
        second.close()
    return completed


def evaluate_policies(
    engine: str, policies: Sequence[DraftPolicy], depth: int,
    nodes: int, plies: int, workers: int,
) -> list[Standing]:
    if len(policies) < 2:
        raise ValueError("a draft league requires at least two policies")
    jobs = [
        (first, second, policies[first], policies[second])
        for first, second in itertools.combinations(range(len(policies)), 2)
    ]
    worker_count = min(workers, len(jobs))
    # Keep identical drafted game pairs on one worker so its exact-result
    # cache eliminates duplicates across the entire league, not merely the
    # accidental subset reached by round-robin job slicing.
    grouped: dict[tuple[str, str], list[PolicyJob]] = {}
    for job in jobs:
        grouped.setdefault(_job_position_key(job), []).append(job)
    chunks: list[list[PolicyJob]] = [[] for _worker in range(worker_count)]
    for index, group in enumerate(grouped.values()):
        chunks[index % worker_count].extend(group)
    arguments = [
        (engine, chunk, depth, nodes, plies) for chunk in chunks if chunk
    ]
    if worker_count == 1:
        completed = [_play_chunk(arguments[0])]
    else:
        with concurrent.futures.ThreadPoolExecutor(max_workers=worker_count) as executor:
            completed = list(executor.map(_play_chunk, arguments))

    points = [0.0] * len(policies)
    games = [0] * len(policies)
    results: list[list[float]] = [[] for _policy in policies]
    match_scores: list[list[float]] = [[] for _policy in policies]
    for chunk in completed:
        for first, second, pair in chunk:
            score = sum(pair) / 2
            points[first] += sum(pair)
            games[first] += 2
            results[first].extend(pair)
            match_scores[first].append(score)
            mirrored = tuple(1.0 - value for value in pair)
            points[second] += sum(mirrored)
            games[second] += 2
            results[second].extend(mirrored)
            match_scores[second].append(1.0 - score)

    standings = []
    for index, policy in enumerate(policies):
        score = points[index] / games[index]
        quarter = max(1, math.ceil(len(match_scores[index]) / 4))
        floor = sum(sorted(match_scores[index])[:quarter]) / quarter
        standings.append(Standing(
            score * 0.8 + floor * 0.2, score, floor,
            tuple(results[index]), policy,
        ))
    return sorted(
        standings,
        key=lambda standing: (standing.fitness, standing.score),
        reverse=True,
    )


def policy_json(policy: DraftPolicy) -> dict[str, object]:
    return {
        "pick": dict(zip(PIECES, policy.pick)),
        "repeat": dict(zip(PIECES, policy.repeat)),
        "ban": dict(zip(PIECES, policy.ban)),
        "opponent_deny": policy.opponent_deny,
        "self_preserve": policy.self_preserve,
        "final_penguin": policy.final_penguin,
    }


def standing_json(standing: Standing) -> dict[str, object]:
    against_base = simulate_draft(standing.policy, BASE_POLICY)
    return {
        "fitness": round(standing.fitness, 6),
        "score": round(standing.score, 6),
        "lower_quartile": round(standing.lower_quartile, 6),
        "record": result_summary(standing.results),
        "policy": policy_json(standing.policy),
        "white_vs_base": ";".join(
            f"{piece},{square}" for piece, square in against_base.white
        ),
        "base_black": ";".join(
            f"{piece},{square}" for piece, square in against_base.black
        ),
        "bans_vs_base": list(against_base.bans),
    }


def write_checkpoint(
    path: str, args: argparse.Namespace, generation: int,
    population: Sequence[DraftPolicy], history: Sequence[dict[str, object]],
    finalists: Sequence[Standing] = (),
) -> None:
    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "seed": args.seed,
        "completed_generations": generation,
        "parameters": {
            key: value for key, value in vars(args).items() if key != "output"
        },
        "population": [policy_json(policy) for policy in population],
        "generations": list(history),
        "finalists": [standing_json(standing) for standing in finalists],
    }
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, indent=2) + "\n")
    temporary.replace(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", default=str(Path(__file__).parents[1] / "src/ultimatefish"))
    parser.add_argument("--population", type=int, default=12)
    parser.add_argument("--generations", type=int, default=8)
    parser.add_argument("--depth", type=int, default=20)
    parser.add_argument("--nodes", type=int, default=2_000)
    parser.add_argument("--plies", type=int, default=100)
    parser.add_argument("--finalists", type=int, default=4)
    parser.add_argument("--final-nodes", type=int, default=12_000)
    parser.add_argument("--final-plies", type=int, default=160)
    parser.add_argument("--workers", type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--seed", type=int, default=5732)
    parser.add_argument("--output")
    args = parser.parse_args()
    if args.population < 4 or args.generations < 1 or args.workers < 1:
        parser.error("population >= 4, generations >= 1, and workers >= 1 are required")
    if not 1 <= args.finalists <= args.population:
        parser.error("finalists must be between one and the population size")

    rng = random.Random(args.seed)
    population = [BASE_POLICY]
    while len(population) < args.population:
        population = unique_policies((*population, random_policy(rng)))
    history = []
    champions = [BASE_POLICY]
    standings: list[Standing] = []
    for generation in range(1, args.generations + 1):
        standings = evaluate_policies(
            args.engine, population, args.depth, args.nodes,
            args.plies, args.workers,
        )
        best = standings[0]
        print(
            f"generation {generation}: fitness={best.fitness:.4f} "
            f"score={best.score:.4f} floor={best.lower_quartile:.4f} "
            f"record={result_summary(best.results)}",
            flush=True,
        )
        history.append({"generation": generation, **standing_json(best)})
        champions = unique_policies((*champions, *(item.policy for item in standings[:3])))
        elite = [item.policy for item in standings[:max(2, args.population // 4)]]
        children = list(elite)
        while len(children) < args.population:
            if rng.random() < 0.25 and len(elite) > 1:
                child = crossover_policy(*rng.sample(elite, 2), rng)
            elif rng.random() < 0.12:
                child = random_policy(rng)
            else:
                child = mutate_policy(rng.choice(elite), rng)
            children = unique_policies((*children, child))
        population = children
        if args.output:
            write_checkpoint(args.output, args, generation, population, history)

    finalist_pool = unique_policies((
        BASE_POLICY, *champions, *(item.policy for item in standings[:args.finalists]),
    ))
    preliminary = evaluate_policies(
        args.engine, finalist_pool, args.depth,
        max(args.nodes, min(args.final_nodes // 3, args.nodes * 4)),
        max(args.plies, args.final_plies // 2), args.workers,
    )
    finalists = [item.policy for item in preliminary[:args.finalists]]
    final = evaluate_policies(
        args.engine, finalists, args.depth, args.final_nodes,
        args.final_plies, args.workers,
    )
    print("finalists:")
    for rank, standing in enumerate(final, 1):
        outcome = simulate_draft(standing.policy, BASE_POLICY)
        print(
            f"{rank}. fitness={standing.fitness:.4f} score={standing.score:.4f} "
            f"record={result_summary(standing.results)} "
            f"white={';'.join(piece for piece, _square in outcome.white)}",
            flush=True,
        )
    if args.output:
        write_checkpoint(
            args.output, args, args.generations, population, history, final,
        )


if __name__ == "__main__":
    main()
