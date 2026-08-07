#!/usr/bin/env python3
"""Deterministic size/class planner for Ultimate Fish tablebases.

This planner deliberately reports conservative *uncompressed* sizes for the
v3 split-plane format.  The generator may make files smaller, but it must not
use optimistic compression ratios when deciding whether the repository's
10 GiB tablebase budget has already been exhausted.
"""

from __future__ import annotations

import argparse
import itertools
import json
from dataclasses import dataclass


FILES = 8
RANKS = 10
SQUARES = FILES * RANKS
GITHUB_FILE_LIMIT = 100_000_000
# Leave a full 5 MB safety margin below GitHub's decimal 100 MB hard limit.
DEFAULT_SHARD_LIMIT = 95_000_000
DEFAULT_BUDGET = 10 * 1024**3


@dataclass(frozen=True)
class Piece:
    name: str
    decisive: bool = False
    minor: bool = False
    color_bound: bool = False
    support: bool = False
    state_factor: int = 1
    models: int = 1
    stateless: bool = True
    closed_k2: bool = True
    note: str = ""


# Generated-only Goop, Minion, CheckerKing, CopycatClone, and Halo are encoded
# as state of their deployable parent rather than treated as draft material.
PIECES = (
    Piece("jester", decisive=True),
    Piece("knight", minor=True),
    Piece("pawn", decisive=True, state_factor=2, stateless=False,
          note="moved flag; promotion crosses into Queen tables"),
    Piece("queen", decisive=True),
    Piece("rook", decisive=True),
    Piece("bishop", color_bound=True),
    Piece("berserker", decisive=True, state_factor=10, stateless=False,
          note="power 0..8 plus board-saturating 9+ equivalence bucket"),
    Piece("bomb", decisive=True),
    Piece("ninja", decisive=True),
    Piece("turtle", minor=True),
    Piece("ghost", decisive=True, state_factor=2, stateless=False,
          note="visible/hidden"),
    Piece("mage", support=True),
    Piece("penguin", decisive=True, state_factor=12, stateless=False,
          note="cooldown 0..5 and reachable freeze-aura phase"),
    Piece("parasite", decisive=True),
    Piece("devil", state_factor=4, stateless=False, closed_k2=False,
          note="insufficient alone; generated Minions belong to larger closures"),
    Piece("sludge", stateless=False, closed_k2=False,
          note="insufficient alone; generated Goop belongs to larger closures"),
    Piece("sniper", decisive=True, state_factor=4, stateless=False,
          note="cooldown 0..3"),
    Piece("prince", decisive=True, state_factor=2, stateless=False,
          note="ordinary/forced-second-action phase"),
    Piece("checker", color_bound=True, state_factor=4, stateless=False,
          note="Checker/CheckerKing type and ordinary/forced-jump phase"),
    Piece("giant", decisive=True,
          note="2x2 footprint makes some anchor tuples invalid"),
    Piece("copycat", decisive=True, stateless=False, closed_k2=False,
          note="linked mirror clone is position-derived until an external effect displaces it"),
    Piece("angel", stateless=False, closed_k2=False,
          note="insufficient alone; attachment/host/Halo state"),
    Piece("fisherman", support=True),
    Piece("dragon", decisive=True),
)


def placement_states(extra_models: int, identical_pair: bool = False) -> int:
    """Dense states after exact horizontal-reflection canonicalization.

    The Ivory King is restricted to files a-d. Remaining labelled models use
    a falling factorial. Two same-type, same-team extras are interchangeable.
    """
    result = 2 * (SQUARES // 2)  # side to move, canonical Ivory King
    available = SQUARES - 1
    for _ in range(1 + extra_models):  # Onyx King plus extra board models
        result *= available
        available -= 1
    if identical_pair:
        result //= 2
    return result


def split_plane_bytes(states: int) -> int:
    # Two-bit WDL plus one-byte DTW. Rare DTW>=255 exceptions are placed in a
    # separate shard and intentionally excluded from this baseline estimate.
    return (states + 3) // 4 + states


def sufficient_pair(first: Piece, second: Piece, same_team: bool) -> bool:
    if not same_team:
        return first.decisive or second.decisive
    if first.decisive or second.decisive:
        return True
    if first.minor and second.minor:
        return True
    if (first.minor and second.color_bound) or (second.minor and first.color_bound):
        return True
    if (first.support and second.color_bound) or (second.support and first.color_bound):
        return True
    # Two color-bound pieces can occupy opposite square colors, so retain the
    # whole material class even though same-color placements are drawn.
    return first.color_bound and second.color_bound


def class_record(name: str, states: int, phase: str, note: str = "",
                 primary: str = "", secondary: str = "",
                 opposing: bool = False, filename: str = "") -> dict[str, object]:
    size = split_plane_bytes(states)
    return {
        "class": name,
        "phase": phase,
        "states": states,
        "packed_bytes": size,
        "shards": (size + DEFAULT_SHARD_LIMIT - 1) // DEFAULT_SHARD_LIMIT,
        "note": note,
        "primary": primary,
        "secondary": secondary,
        "opposing": opposing,
        "filename": filename,
    }


def stateful_candidates() -> list[dict[str, object]]:
    """Closed K+K+2 classes containing at least one stateful character.

    Spawning, attachment, and linked-multi-model characters are deliberately
    excluded: their exact closure is larger than four board models and cannot
    truthfully be represented by this codec. Candidate order gives every
    closed stateful type one same-team Bomb class before spending the
    remaining budget on the smallest classes. Bomb keeps this coverage tier's
    predecessor graphs much smaller than an equally sized Queen pairing.
    """
    closed = tuple(piece for piece in PIECES if piece.closed_k2)
    result: list[dict[str, object]] = []
    for first_index, first in enumerate(closed):
        for second in closed[first_index:]:
            if first.stateless and second.stateless:
                continue
            factor = first.state_factor * second.state_factor
            if sufficient_pair(first, second, True):
                states = placement_states(
                    first.models + second.models,
                    first == second and first.models == 1) * factor
                result.append(class_record(
                    f"K{first.name}{second.name}vK", states, "kings+2-stateful",
                    primary=first.name, secondary=second.name,
                    filename=f"k{first.name}{second.name}k.uftb"))
            if sufficient_pair(first, second, False):
                states = placement_states(first.models + second.models) * factor
                result.append(class_record(
                    f"K{first.name}vK{second.name}", states, "kings+2-stateful",
                    primary=first.name, secondary=second.name, opposing=True,
                    filename=f"k{first.name}k{second.name}.uftb"))

    stateful_names = {piece.name for piece in closed if not piece.stateless}
    coverage = {
        (name, "bomb", False) for name in stateful_names
    } | {
        ("bomb", name, False) for name in stateful_names
    }
    return sorted(result, key=lambda record: (
        (str(record["primary"]), str(record["secondary"]), bool(record["opposing"]))
        not in coverage,
        int(record["packed_bytes"]), str(record["filename"])))


def inventory(budget: int = DEFAULT_BUDGET) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    for piece in PIECES:
        if not piece.decisive:
            continue
        states = placement_states(piece.models) * piece.state_factor
        single_filename = {"queen": "kqk.uftb", "rook": "krk.uftb"}.get(
            piece.name, f"k{piece.name}k.uftb")
        result.append(class_record(f"K{piece.name}vK", states, "kings+1", piece.note,
                                   primary=piece.name,
                                   filename=single_filename))

    stateless = tuple(piece for piece in PIECES if piece.stateless)
    for first_index, first in enumerate(stateless):
        for second in stateless[first_index:]:
            if sufficient_pair(first, second, True):
                states = placement_states(first.models + second.models,
                                          first == second and first.models == 1)
                result.append(class_record(
                    f"K{first.name}{second.name}vK", states, "kings+2-stateless",
                    primary=first.name, secondary=second.name,
                    filename=f"k{first.name}{second.name}k.uftb"))
            if sufficient_pair(first, second, False):
                states = placement_states(first.models + second.models)
                result.append(class_record(
                    f"K{first.name}vK{second.name}", states, "kings+2-stateless",
                    primary=first.name, secondary=second.name, opposing=True,
                    filename=f"k{first.name}k{second.name}.uftb"))
    # Stateful combinations are admitted deterministically only while their
    # conservative split-plane size fits. This makes the 10 GiB rule a hard
    # inventory invariant instead of a best-effort generator check.
    used = sum(int(record["packed_bytes"]) for record in result)
    for record in stateful_candidates():
        size = int(record["packed_bytes"])
        if used + size <= budget:
            result.append(record)
            used += size
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--budget", type=int, default=DEFAULT_BUDGET)
    args = parser.parse_args()
    records = inventory(args.budget)
    total = sum(int(record["packed_bytes"]) for record in records)
    if args.json:
        print(json.dumps({
            "budget": args.budget,
            "github_file_limit": GITHUB_FILE_LIMIT,
            "shard_limit": DEFAULT_SHARD_LIMIT,
            "classes": records,
            "packed_bytes": total,
            "fits_without_entropy_compression": total <= args.budget,
        }, indent=2))
        return

    for phase, grouped in itertools.groupby(records, key=lambda item: item["phase"]):
        group = list(grouped)
        size = sum(int(item["packed_bytes"]) for item in group)
        print(f"{phase}: {len(group)} classes, {size / 1024**3:.3f} GiB")
    print(f"total: {len(records)} classes, {total / 1024**3:.3f} GiB")
    print(f"budget: {args.budget / 1024**3:.3f} GiB")
    print("compression required" if total > args.budget else "fits conservative budget")


if __name__ == "__main__":
    main()
