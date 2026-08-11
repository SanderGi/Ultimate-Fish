#!/usr/bin/env python3
"""Deterministic size/class planner for Ultimate Fish tablebases.

This planner deliberately reports conservative *uncompressed* sizes for the
v3 split-plane format.  The generator may make files smaller, but it must not
use optimistic compression ratios when deciding whether the repository's
original 10 GiB target has already been exhausted. Two explicitly requested
classes are then accounted for against a separate narrow overrun ceiling.
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
# Preserve the historically selected repository classes while exact causal
# Penguin membership expands their logical payloads.  Most completed outputs
# will be served from S3, so this is now a 12 GiB planning ceiling rather than
# a promise that every logical payload will be committed to Git.
AUTHORIZED_BUDGET = 12 * 1024**3


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
    Piece("penguin", decisive=True, state_factor=4, stateless=False,
          note="exact causal freeze membership for both Kings and any companion"),
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

DEFERRED_DYNAMIC_K2 = frozenset({"devil", "sludge", "angel"})
COPYCAT_SEPARATORS = frozenset({"penguin", "mage", "fisherman"})


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


def compound_copycat_pair_states(identical_pair: bool = False) -> int:
    """K+linked-Copycat vs K+piece states without reflection folding.

    The Copycat's second board model is derived from the indexed half, so only
    four logical squares are stored.  Horizontal reflection exchanges the two
    typed halves, however, so this codec deliberately retains both orbits just
    like the existing K+Copycat-v-K table.
    """
    result = 2 * SQUARES * (SQUARES - 1) * (SQUARES - 2) * (SQUARES - 3)
    return result // 2 if identical_pair else result


def material_state_factor(piece: Piece, *, four_models: bool = False,
                          other: Piece | None = None) -> int:
    """Return the exact substate factor in this material domain.

    A Penguin records which adjacent pieces it actually froze on its preceding
    move.  Later movement can enter an active Penguin's neighbourhood without
    joining that causal set, so geometry alone cannot recover the state.  Two
    King membership bits give four K+Penguin-v-K substates.  A distinct fourth
    model adds one bit; another Penguin is immune and adds no target bit.
    """
    if piece.name != "penguin":
        return piece.state_factor
    if not four_models or other is None or other.name == "penguin":
        return 4
    return 8


def pair_state_factor(first: Piece, second: Piece) -> int:
    return (material_state_factor(first, four_models=True, other=second) *
            material_state_factor(second, four_models=True, other=first))


def single_material_states(piece: Piece) -> int:
    """Exact K+piece-v-K codec size; this codec retains both reflections."""
    return 2 * placement_states(piece.models) * material_state_factor(piece)


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
            factor = pair_state_factor(first, second)
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


def mirror_copycat_candidates() -> list[dict[str, object]]:
    """User-approved linked-mirror K+K+2 Copycat planning domain.

    One indexed Copycat half implies its exact horizontal-mirror clone, as in
    the bundled K+Copycat-v-K and K+Copycat-v-K+Bishop tables. Starting states
    therefore contain only unsplit linked pairs. Devil/Sludge/Angel remain
    deferred wholesale, while Penguin, Mage, and Fisherman are omitted because
    they can split a pair within this material class. Every retained class is
    closed under the unsplit-pair invariant; no displaced child is scored as a
    draw or silently discarded.
    """
    result: list[dict[str, object]] = []
    for secondary in PIECES:
        if secondary.name in DEFERRED_DYNAMIC_K2 | COPYCAT_SEPARATORS:
            continue
        for opposing in (False, True):
            states = compound_copycat_pair_states(
                identical_pair=secondary.name == "copycat" and not opposing
            ) * secondary.state_factor
            record = class_record(
                (f"Kcopycat{secondary.name}vK" if not opposing else
                 f"KcopycatvK{secondary.name}"),
                states, "kings+2-copycat-mirror",
                primary="copycat", secondary=secondary.name,
                opposing=opposing,
                filename=(f"kcopycat{secondary.name}k.uftb" if not opposing else
                          f"kcopycatk{secondary.name}.uftb"),
                note=("linked horizontal-mirror Copycat compound; "
                      "singleton/independently displaced states excluded"),
            )
            record["mirror_simplification"] = True
            record["truncates_native_separation"] = False
            result.append(record)
    if len(result) != 36 or len({str(row["filename"]) for row in result}) != 36:
        raise RuntimeError("mirror Copycat K+K+2 inventory residual")
    return sorted(result, key=lambda record: str(record["filename"]))


def inventory(budget: int = DEFAULT_BUDGET) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    for piece in PIECES:
        if not piece.decisive:
            continue
        states = single_material_states(piece)
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
                # Pair order normally follows PIECES, but this exact table was
                # generated with Fisherman as the first (Ivory) material owner.
                # Keep the catalog aligned with its header so side-to-move WDL
                # columns and resume checks cannot silently swap the owners.
                opposing_first, opposing_second = first, second
                if first.name == "parasite" and second.name == "fisherman":
                    opposing_first, opposing_second = second, first
                result.append(class_record(
                    f"K{opposing_first.name}vK{opposing_second.name}", states,
                    "kings+2-stateless", primary=opposing_first.name,
                    secondary=opposing_second.name, opposing=True,
                    filename=(f"k{opposing_first.name}k{opposing_second.name}"
                              ".uftb")))
    # Ordinary stateful combinations are admitted deterministically only while
    # their conservative split-plane size fits the original 10 GiB target.
    used = sum(int(record["packed_bytes"]) for record in result)
    for record in stateful_candidates():
        size = int(record["packed_bytes"])
        if used + size <= budget:
            result.append(record)
            used += size

    # Deliberate, narrowly scoped over-budget additions. Copycat is exact only
    # while its linked half remains at the mirrored square; Bishop cannot split
    # or save one half, so this material class is closed under that invariant.
    by_name = {piece.name: piece for piece in PIECES}
    penguin_factor = pair_state_factor(by_name["bishop"], by_name["penguin"])
    requested = (
        class_record(
            "KcopycatvKbishop", compound_copycat_pair_states(),
            "kings+2-requested", primary="copycat", secondary="bishop",
            opposing=True, filename="kcopycatkbishop.uftb",
            note="linked mirrored Copycat compound; displaced/singleton states excluded"),
        class_record(
            "KdragonvKpenguin", placement_states(2) * pair_state_factor(
                by_name["dragon"], by_name["penguin"]),
            "kings+2-requested", primary="dragon", secondary="penguin",
            opposing=True, filename="kdragonkpenguin.uftb"),
        class_record(
            "KbishopvKpenguin", placement_states(2) * penguin_factor,
            "kings+2-requested", primary="bishop", secondary="penguin",
            opposing=True, filename="kbishopkpenguin.uftb",
            note="exact causal Penguin membership supersedes legacy aura bit"),
        class_record(
            "KbishoppenguinvK", placement_states(2) * penguin_factor,
            "kings+2-requested", primary="bishop", secondary="penguin",
            filename="kbishoppenguink.uftb",
            note="exact causal Penguin membership supersedes legacy aura bit"),
        class_record(
            "KbombvKpenguin", placement_states(2) * pair_state_factor(
                by_name["bomb"], by_name["penguin"]),
            "kings+2-requested", primary="bomb", secondary="penguin",
            opposing=True, filename="kbombkpenguin.uftb",
            note="exact causal Penguin membership supersedes legacy aura bit"),
    )
    known = {str(record["filename"]) for record in result}
    result.extend(record for record in requested
                  if str(record["filename"]) not in known)
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
    print(f"baseline budget: {args.budget / 1024**3:.3f} GiB")
    print(f"authorized ceiling: {AUTHORIZED_BUDGET / 1024**3:.3f} GiB")
    if total > AUTHORIZED_BUDGET:
        print("exceeds authorized ceiling")
    elif total > args.budget:
        print("uses approved narrow overrun")
    else:
        print("fits conservative budget")


if __name__ == "__main__":
    main()
