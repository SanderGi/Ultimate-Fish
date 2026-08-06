#!/usr/bin/env python3
"""Differentially compare Chess Ultimate Local Play with Ultimate Fish.

The shipping app is the oracle. A fixture builds both 100-point armies and
executes accepted and rejected actions on the pass-and-play board. Native move
callbacks and turn barriers—not OCR, artwork, or the misleading SetUpMyDot
origin diagnostic—decide whether each action was accepted.
"""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
import json
from pathlib import Path
import random
import re
import sys
import time
from typing import Iterable, Sequence

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.ultimate_phone import (  # noqa: E402
    ArmyPlacementRetry,
    BeliefSet,
    BoardGeometry,
    EngineClient,
    PIECE_COST,
    PhoneGame,
    find_adb,
    make_upn,
    parse_engine_move,
    parse_upn_pieces,
    public_probe_piece,
    rotate_square,
    square_to_scene_index,
    upn_piece_covering,
    upn_repetition_key,
)

DEFAULT_MANIFEST = ROOT / "tests" / "ultimate_local_conformance.json"
MAX_LOCAL_SETUP_ATTEMPTS = 6


@dataclass(frozen=True)
class SelfplayCandidate:
    move: str
    position: str
    features: frozenset[str]
    score: int


def _team(value: object, label: str) -> list[tuple[str, str]]:
    if not isinstance(value, list):
        raise ValueError(f"{label} must be a JSON list")
    result: list[tuple[str, str]] = []
    for index, item in enumerate(value):
        if (not isinstance(item, list) or len(item) != 2 or
                not all(isinstance(part, str) for part in item)):
            raise ValueError(f"{label}[{index}] must be [piece, square]")
        piece, square = item
        if piece not in PIECE_COST:
            raise ValueError(f"{label}[{index}] has unknown piece {piece!r}")
        square_to_scene_index(square)
        if int(square[1:]) > 3:
            raise ValueError(f"{label}[{index}] is outside the deployment zone")
        result.append((piece, square))
    if sum(PIECE_COST[piece] for piece, _ in result) != 100:
        raise ValueError(f"{label} must cost exactly 100 points")
    if sum(piece == "king" for piece, _ in result) != 1:
        raise ValueError(f"{label} must contain exactly one King")
    return result


def rotate_deployment(piece: str, square: str) -> str:
    """Transform Player 2's lower-zone blueprint to its top-zone anchor."""
    if piece != "giant":
        return rotate_square(square)
    file_index = ord(square[0]) - ord("a")
    rank = int(square[1:])
    # Rotating all four occupied cells changes the canonical lower-left anchor,
    # unlike rotating an ordinary one-cell piece's coordinate.
    return f"{chr(ord('a') + 6 - file_index)}{10 - rank}"


def local_upn(
    player1: Sequence[tuple[str, str]],
    player2: Sequence[tuple[str, str]],
) -> str:
    black = [(piece, rotate_deployment(piece, square))
             for piece, square in player2]
    upn = make_upn(player1, black, "w")
    # New Ghosts are hidden from the opposing player even though each local
    # player can see their own model during their pass-and-play turn.
    return re.sub(
        r";ghost,([wb]),([a-h](?:10|[1-9]))(?=;|$)",
        r";ghost,\1,\2,0,0,0,0,0,0,-1,1,-1,0",
        upn,
    )


def load_manifest(path: Path) -> dict:
    document = json.loads(path.read_text())
    if document.get("schema") != 1 or not isinstance(document.get("fixtures"), list):
        raise ValueError("local conformance manifest must use schema 1")
    seen: set[str] = set()
    for fixture in document["fixtures"]:
        if not isinstance(fixture, dict) or not isinstance(fixture.get("id"), str):
            raise ValueError("each local fixture needs a string id")
        if fixture["id"] in seen:
            raise ValueError(f"duplicate fixture id {fixture['id']}")
        seen.add(fixture["id"])
        _team(fixture.get("player1"), f"{fixture['id']}.player1")
        _team(fixture.get("player2"), f"{fixture['id']}.player2")
        if not isinstance(fixture.get("steps"), list) or not fixture["steps"]:
            raise ValueError(f"{fixture['id']} needs at least one step")
        for index, step in enumerate(fixture["steps"]):
            if not isinstance(step, dict):
                raise ValueError(
                    f"{fixture['id']}.steps[{index}] must be an object"
                )
            operations = set(step) & {
                "move", "reject", "piece", "native_moves", "native_deaths"
            }
            if len(operations) != 1:
                raise ValueError(
                    f"{fixture['id']}.steps[{index}] must have exactly one "
                    "move, reject, piece, native_moves, or native_deaths assertion"
                )
            if "native_deaths" in step:
                deaths = step["native_deaths"]
                if (not isinstance(deaths, list) or not deaths or
                        not all(isinstance(piece, str) and piece in PIECE_COST
                                for piece in deaths)):
                    raise ValueError(
                        f"{fixture['id']}.steps[{index}].native_deaths must "
                        "be a non-empty list of known piece types"
                    )
                continue
            if "native_moves" in step:
                moves = step["native_moves"]
                if not isinstance(moves, list) or not moves:
                    raise ValueError(
                        f"{fixture['id']}.steps[{index}].native_moves must "
                        "be a non-empty list"
                    )
                for move in moves:
                    if not isinstance(move, str):
                        raise ValueError(
                            f"{fixture['id']}.steps[{index}].native_moves "
                            "must contain strings"
                        )
                    source, target, separator = parse_engine_move(move)
                    if separator != "-" or source == "pass":
                        raise ValueError(
                            f"{fixture['id']}.steps[{index}] native move "
                            f"{move!r} must be an ordinary relocation"
                        )
                continue
            if "piece" in step:
                assertion = step["piece"]
                if (not isinstance(assertion, dict) or
                        set(assertion) != {"type", "square"} or
                        assertion.get("type") not in PIECE_COST or
                        not isinstance(assertion.get("square"), str)):
                    raise ValueError(
                        f"{fixture['id']}.steps[{index}].piece needs a known "
                        "type and square"
                    )
                square_to_scene_index(assertion["square"])
                continue
            action = step.get("move", step.get("reject"))
            if not isinstance(action, str):
                raise ValueError(
                    f"{fixture['id']}.steps[{index}] action must be a string"
                )
            parse_engine_move(action)
    profiles = document.get("selfplay_profiles", [])
    if not isinstance(profiles, list):
        raise ValueError("selfplay_profiles must be a JSON list")
    profile_ids: set[str] = set()
    for profile in profiles:
        if not isinstance(profile, dict) or not isinstance(profile.get("id"), str):
            raise ValueError("each self-play profile needs a string id")
        if profile["id"] in profile_ids:
            raise ValueError(f"duplicate self-play profile id {profile['id']}")
        profile_ids.add(profile["id"])
        _team(profile.get("player1"), f"{profile['id']}.player1")
        _team(profile.get("player2"), f"{profile['id']}.player2")
    return document


def _piece_counts(upn: str) -> Counter[tuple[str, str]]:
    return Counter((piece, color) for piece, color, _square, _state
                   in parse_upn_pieces(upn))


def automatic_minion_transition(before: str, after: str) -> bool:
    """Whether advancing the turn changed any generated Minion state.

    Unity emits an early ``ChangeTurn End`` before it animates a Minion's
    compulsory turn-start move, then another one after that animation.  A
    Local driver must wait for the latter or its next tap can be swallowed and
    the late barrier can be mistaken for acceptance of the following action.
    """
    next_side = after.split(";", 1)[0]

    def signature(upn: str) -> Counter[str]:
        return Counter(
            square
            for piece, color, square, _state in parse_upn_pieces(upn)
            if piece == "minion" and color == next_side
        )

    return signature(before) != signature(after)


def wait_for_native_turn_barriers(
    game: PhoneGame,
    generation: int,
    prefix_length: int,
    expected: int,
    timeout: float = 15.0,
) -> None:
    """Wait for all turn barriers without relying on the consumable queue."""
    if expected <= 0:
        return
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        current_generation, journal = game.events.gameplay_snapshot()
        recent = journal[prefix_length:] if current_generation == generation else journal
        terminals = [
            event for event in recent
            if event.kind in ("terminal_label", "game_over", "out_of_time")
        ]
        if terminals:
            raise AssertionError(
                f"game ended with {terminals[-1].kind} while waiting for "
                "the native turn to settle"
            )
        if sum(event.kind == "turn_end" for event in recent) >= expected:
            return
        time.sleep(0.03)
    raise TimeoutError(
        f"observed fewer than {expected} native turn-end barriers"
    )


def move_coverage_features(upn: str, move: str, after: str) -> frozenset[str]:
    """Describe a rules interaction without tying coverage to board cells."""
    source, target, separator = parse_engine_move(move)
    actor = upn_piece_covering(upn, source)
    victim = upn_piece_covering(upn, target)
    actor_name = actor[0] if actor else "unknown"
    if victim:
        victim_name, victim_color, victim_visible = victim
        relation = "allied" if actor and actor[1] == victim_color else "enemy"
        target_name = f"{relation}-{victim_name}-{'visible' if victim_visible else 'hidden'}"
    else:
        target_name = "empty"

    features = {
        f"actor:{actor_name}",
        f"kind:{separator}",
        f"action:{actor_name}:{separator}:{target_name}",
    }
    before_counts = _piece_counts(upn)
    after_counts = _piece_counts(after)
    deltas = []
    for piece_color in sorted(before_counts.keys() | after_counts.keys()):
        delta = after_counts[piece_color] - before_counts[piece_color]
        if not delta:
            continue
        piece, color = piece_color
        item = f"delta:{piece}:{color}:{delta:+d}"
        deltas.append(item)
        features.add(item)
    effect = ",".join(deltas) if deltas else "none"
    features.add(f"effect:{actor_name}:{separator}:{target_name}:{effect}")
    return frozenset(features)


def choose_coverage_move(
    engine: EngineClient,
    upn: str,
    moves: Iterable[str],
    covered: set[str],
    actor_counts: Counter[str],
    visited: set[str],
    rng: random.Random,
) -> SelfplayCandidate | None:
    """Choose a deterministic, rare-interaction-seeking executable action."""
    candidates: list[SelfplayCandidate] = []
    for move in sorted(set(moves)):
        if move == "pass":
            continue
        after = engine.apply(upn, move)
        features = move_coverage_features(upn, move, after)
        actor_feature = next(
            (feature for feature in features if feature.startswith("actor:")),
            "actor:unknown",
        )
        actor_name = actor_feature.split(":", 1)[1]
        novel_actions = sum(
            feature not in covered and feature.startswith("action:")
            for feature in features
        )
        novel_effects = sum(
            feature not in covered and feature.startswith("effect:")
            for feature in features
        )
        novel_other = sum(feature not in covered for feature in features)
        score = 140 * novel_effects + 100 * novel_actions + 12 * novel_other
        score += max(0, 24 - 3 * actor_counts[actor_name])
        if upn_repetition_key(after) in visited:
            score -= 50
        if any(feature in features for feature in
               ("delta:king:w:-1", "delta:king:b:-1")):
            # Keep the lab alive to expose interactions. A royal capture still
            # remains available when it is the only legal non-pass action.
            score -= 10_000
        candidates.append(SelfplayCandidate(move, after, features, score))
    if not candidates:
        return None
    best_score = max(candidate.score for candidate in candidates)
    best = [candidate for candidate in candidates if candidate.score == best_score]
    return rng.choice(best)


def start_local_with_retries(
    game: PhoneGame,
    player1: Sequence[tuple[str, str]],
    player2: Sequence[tuple[str, str]],
    label: str,
) -> None:
    for setup_attempt in range(1, MAX_LOCAL_SETUP_ATTEMPTS + 1):
        try:
            game.start_local(player1, player2)
            return
        except (ArmyPlacementRetry, TimeoutError) as exc:
            if setup_attempt == MAX_LOCAL_SETUP_ATTEMPTS:
                raise
            # Keep authoritative calibration on the same PhoneGame instance,
            # restart Local from a clean scene, and rebuild both armies.
            print(
                f"{label} setup rejected ({exc}); restarting with learned "
                f"calibration ({setup_attempt + 1}/{MAX_LOCAL_SETUP_ATTEMPTS})",
                flush=True,
            )


def run_fixture(
    fixture: dict,
    adb: str,
    device: str,
    engine_path: str,
    verbose: bool,
) -> dict:
    player1 = _team(fixture["player1"], f"{fixture['id']}.player1")
    player2 = _team(fixture["player2"], f"{fixture['id']}.player2")
    upn = local_upn(player1, player2)

    # Reject overlap, footprint, or serialization mistakes before touching the
    # app's saved Local armies.
    validator = EngineClient(engine_path)
    try:
        validator.set_position(upn)
    finally:
        validator.close()

    game = PhoneGame(
        adb, device, engine_path, BoardGeometry(), player1,
        depth=1, nodes=0, movetime_ms=0, belief_limit=1,
        verbose=verbose, configure_army=True,
    )
    observations: list[dict] = []
    try:
        start_local_with_retries(game, player1, player2, fixture["id"])
        game.beliefs = BeliefSet(game.engine, [upn], 1)
        game.perspective_flipped = False
        game.rotate_taps = False
        for index, step in enumerate(fixture["steps"], 1):
            if "native_deaths" in step:
                expected = Counter(step["native_deaths"])
                deadline = time.monotonic() + 4.0
                observed: Counter[str] = Counter()
                while time.monotonic() < deadline:
                    _generation, journal = game.events.gameplay_snapshot()
                    observed = Counter(
                        event.piece for event in journal
                        if event.kind == "dead" and event.piece
                    )
                    if all(observed[piece] >= count
                           for piece, count in expected.items()):
                        break
                    time.sleep(0.04)
                else:
                    missing = expected - observed
                    raise AssertionError(
                        "native death callback(s) not observed: " +
                        ", ".join(sorted(missing.elements()))
                    )
                deaths = list(expected.elements())
                observations.append({
                    "step": index,
                    "native_deaths": deaths,
                    "observed": True,
                })
                print(
                    f"{fixture['id']} native deaths " +
                    ", ".join(deaths) + ": confirmed",
                    flush=True,
                )
                continue
            if "native_moves" in step:
                expected = Counter(step["native_moves"])
                deadline = time.monotonic() + 4.0
                observed: Counter[str] = Counter()
                while time.monotonic() < deadline:
                    _generation, journal = game.events.gameplay_snapshot()
                    observed = Counter(
                        f"{event.source}-{event.target}"
                        for event in journal
                        if event.kind == "move" and event.source and event.target
                    )
                    if all(observed[move] >= count
                           for move, count in expected.items()):
                        break
                    time.sleep(0.04)
                else:
                    missing = expected - observed
                    raise AssertionError(
                        "native automatic move callback(s) not observed: " +
                        ", ".join(sorted(missing.elements()))
                    )
                moves = list(expected.elements())
                observations.append({
                    "step": index,
                    "native_moves": moves,
                    "observed": True,
                })
                print(
                    f"{fixture['id']} native moves " +
                    ", ".join(moves) + ": confirmed",
                    flush=True,
                )
                continue
            if "piece" in step:
                assertion = step["piece"]
                predicted = upn_piece_covering(
                    game.beliefs.positions[0], assertion["square"]
                )
                if (predicted is None or
                        public_probe_piece(predicted[0]) !=
                        public_probe_piece(assertion["type"])):
                    actual = predicted[0] if predicted else "empty"
                    raise AssertionError(
                        f"engine predicts {actual} rather than "
                        f"{assertion['type']} at {assertion['square']}"
                    )
                found = game.locate_public_piece(
                    assertion["type"], [assertion["square"]]
                )
                observations.append({
                    "step": index,
                    "piece": assertion,
                    "found": found,
                })
                print(
                    f"{fixture['id']} piece {assertion['type']} "
                    f"at {found}: confirmed",
                    flush=True,
                )
                continue
            if "reject" in step:
                move = step["reject"]
                position = game.beliefs.positions[0]
                if move in game.engine.legal_moves(position):
                    raise AssertionError(
                        f"fixture rejection {move} is engine-legal"
                    )
                try:
                    event = game.execute(move)
                except TimeoutError:
                    observations.append(
                        {"step": index, "reject": move, "accepted": False}
                    )
                    print(f"{fixture['id']} reject {move}: confirmed", flush=True)
                    continue
                raise AssertionError(
                    f"native accepted engine-illegal {move} ({event.kind})"
                )

            move = step["move"]
            position = game.beliefs.positions[0]
            legal = game.engine.legal_moves(position)
            if move not in legal:
                raise AssertionError(f"fixture move {move} is engine-illegal")
            previous_side = game.beliefs.side
            bomb = game.beliefs.move_causes_bomb_detonation(move)
            predicted = game.engine.apply(position, move)
            event_generation, event_prefix = game.events.gameplay_snapshot()
            event = game.execute(move, bomb)
            if event.kind != "move":
                raise AssertionError(
                    f"native fixture move {move} ended with {event.kind}"
                )
            game.beliefs.apply_known(move)
            if game.beliefs.side != previous_side:
                barriers = 2 if automatic_minion_transition(
                    position, predicted
                ) else 1
                wait_for_native_turn_barriers(
                    game, event_generation, len(event_prefix), barriers
                )
            # Local Play rotates only the rendered camera. Unity diagnostics
            # remain in the global Player-1-bottom board frame, unlike online
            # Onyx where the controller also normalizes logged coordinates.
            game.perspective_flipped = False
            game.rotate_taps = game.beliefs.side == "b"
            observations.append({"step": index, "move": move, "accepted": True})
            print(f"{fixture['id']} move {move}: accepted", flush=True)
            time.sleep(0.08)
    finally:
        game.close()

    return {"id": fixture["id"], "passed": True, "observations": observations}


def run_selfplay(
    profile: dict,
    game_number: int,
    seed: int,
    max_plies: int,
    adb: str,
    device: str,
    engine_path: str,
    verbose: bool,
    campaign_covered: set[str] | None = None,
    campaign_actor_counts: Counter[str] | None = None,
) -> dict:
    """Drive both Local players while maximizing rules-interaction coverage."""
    label = f"{profile['id']}#{game_number}"
    player1 = _team(profile["player1"], f"{profile['id']}.player1")
    player2 = _team(profile["player2"], f"{profile['id']}.player2")
    upn = local_upn(player1, player2)
    validator = EngineClient(engine_path)
    try:
        validator.set_position(upn)
    finally:
        validator.close()
    rng = random.Random(seed)
    covered = campaign_covered if campaign_covered is not None else set()
    actor_counts = (
        campaign_actor_counts
        if campaign_actor_counts is not None else Counter()
    )
    game_covered: set[str] = set()
    newly_covered: set[str] = set()
    visited = {upn_repetition_key(upn)}
    trace: list[dict] = []

    game = PhoneGame(
        adb, device, engine_path, BoardGeometry(), player1,
        depth=1, nodes=0, movetime_ms=0, belief_limit=1,
        verbose=verbose, configure_army=True,
    )
    terminal: str | None = None
    try:
        start_local_with_retries(game, player1, player2, label)
        game.beliefs = BeliefSet(game.engine, [upn], 1)
        game.perspective_flipped = False
        game.rotate_taps = False
        for ply in range(1, max_plies + 1):
            current = game.beliefs.positions[0]
            candidate = choose_coverage_move(
                game.engine,
                current,
                game.engine.legal_moves(current),
                covered,
                actor_counts,
                visited,
                rng,
            )
            if candidate is None:
                terminal = "no-executable-move"
                break
            actor_feature = next(
                feature for feature in candidate.features
                if feature.startswith("actor:")
            )
            actor_name = actor_feature.split(":", 1)[1]
            previous_side = game.beliefs.side
            bomb = game.beliefs.move_causes_bomb_detonation(candidate.move)
            event_generation, event_prefix = game.events.gameplay_snapshot()
            try:
                event = game.execute(candidate.move, bomb)
            except TimeoutError as exc:
                prefix = " ".join(item["move"] for item in trace)
                raise AssertionError(
                    f"native rejected engine-legal move {candidate.move} at "
                    f"ply {ply}; profile={profile['id']} seed={seed}; "
                    f"prefix={prefix or '<empty>'}"
                ) from exc

            trace_entry = {
                "ply": ply,
                "move": candidate.move,
                "event": event.kind,
                "new_features": sorted(candidate.features - covered),
            }
            trace.append(trace_entry)
            newly_covered.update(candidate.features - covered)
            covered.update(candidate.features)
            game_covered.update(candidate.features)
            actor_counts[actor_name] += 1
            visited.add(upn_repetition_key(candidate.position))
            game.beliefs.positions = [candidate.position]
            ended = event.kind in (
                "terminal_label", "game_over", "out_of_time"
            )
            if ended:
                terminal = event.kind
            elif game.beliefs.side != previous_side:
                barriers = 2 if automatic_minion_transition(
                    current, candidate.position
                ) else 1
                wait_for_native_turn_barriers(
                    game, event_generation, len(event_prefix), barriers
                )
            after_generation, after_events = game.events.gameplay_snapshot()
            native_slice = (
                after_events[len(event_prefix):]
                if after_generation == event_generation else after_events
            )
            trace_entry["native_events"] = [
                {
                    key: value for key, value in (
                        ("kind", native.kind),
                        ("piece", native.piece),
                        ("source", native.source),
                        ("target", native.target),
                    ) if value is not None
                }
                for native in native_slice
                if native.kind not in ("turn_start", "turn_end")
            ]
            print(
                f"{label} ply {ply}: {candidate.move} ({event.kind}, "
                f"{len(covered)} coverage features)",
                flush=True,
            )
            if ended:
                break
            game.perspective_flipped = False
            game.rotate_taps = game.beliefs.side == "b"
            time.sleep(0.08)
        else:
            terminal = "ply-limit"
    finally:
        game.close()

    return {
        "id": label,
        "profile": profile["id"],
        "game": game_number,
        "seed": seed,
        "passed": True,
        "plies": len(trace),
        "terminal": terminal,
        "coverage": sorted(game_covered),
        "new_coverage": sorted(newly_covered),
        "trace": trace,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--fixture", action="append", default=[])
    parser.add_argument("--no-fixtures", action="store_true")
    parser.add_argument("--profile", action="append", default=[])
    parser.add_argument("--selfplay-games", type=int, default=0)
    parser.add_argument("--selfplay-plies", type=int, default=160)
    parser.add_argument("--seed", type=int, default=731_000)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--device")
    parser.add_argument("--adb")
    parser.add_argument("--engine", default=str(ROOT / "src" / "ultimatefish"))
    parser.add_argument("--report", type=Path)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    document = load_manifest(args.manifest)
    fixtures = [] if args.no_fixtures else document["fixtures"]
    if args.fixture:
        if args.no_fixtures:
            parser.error("--fixture and --no-fixtures cannot be combined")
        requested = set(args.fixture)
        fixtures = [fixture for fixture in fixtures if fixture["id"] in requested]
        missing = requested - {fixture["id"] for fixture in fixtures}
        if missing:
            parser.error("unknown fixture(s): " + ", ".join(sorted(missing)))
    profiles = document.get("selfplay_profiles", [])
    if args.profile:
        requested = set(args.profile)
        profiles = [profile for profile in profiles if profile["id"] in requested]
        missing = requested - {profile["id"] for profile in profiles}
        if missing:
            parser.error("unknown self-play profile(s): " + ", ".join(sorted(missing)))
    if args.selfplay_games < 0:
        parser.error("--selfplay-games cannot be negative")
    if args.selfplay_plies <= 0:
        parser.error("--selfplay-plies must be positive")
    if args.list or args.dry_run:
        for fixture in fixtures:
            print("fixture:" + fixture["id"])
        for profile in profiles:
            print("selfplay:" + profile["id"])
        if args.list or not args.device:
            return
    if not args.device:
        parser.error("--device is required to run Local fixtures")

    adb = find_adb(args.adb)
    results = [
        run_fixture(
            fixture, adb, args.device, args.engine, args.verbose
        )
        for fixture in fixtures
    ]
    selfplay_results = []
    campaign_covered: set[str] = set()
    campaign_actor_counts: Counter[str] = Counter()
    for profile_index, profile in enumerate(profiles):
        for game_number in range(1, args.selfplay_games + 1):
            seed = args.seed + profile_index * 100_000 + game_number - 1
            selfplay_results.append(run_selfplay(
                profile,
                game_number,
                seed,
                args.selfplay_plies,
                adb,
                args.device,
                args.engine,
                args.verbose,
                campaign_covered,
                campaign_actor_counts,
            ))
    report = {
        "schema": 1,
        "app_version": document.get("app_version"),
        "passed": all(result["passed"] for result in
                      [*results, *selfplay_results]),
        "fixtures": results,
        "selfplay": selfplay_results,
        "selfplay_coverage": sorted(campaign_covered),
    }
    encoded = json.dumps(report, indent=2) + "\n"
    if args.report:
        args.report.write_text(encoded)
    print(encoded, end="")


if __name__ == "__main__":
    main()
