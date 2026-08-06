#!/usr/bin/env python3
"""Fast, no-LLM Android controller for the Chess Ultimate app.

The controller deliberately limits screenshots to initial public-position
discovery.  Piece selections and moves are then read from the app's Unity log,
while decisions come exclusively from the local Ultimate Fish executable.

Release builds currently log exact coordinates for invisible Ghost moves and
the concrete King/Jester prefab name.  Those are private game state, so this
program masks them and searches a bounded set of positions consistent with
what the player is allowed to know.
"""

from __future__ import annotations

import argparse
import itertools
import json
import os
import queue
import re
import signal
import subprocess
import sys
import threading
import time
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator, Sequence


PIECE_NAMES = (
    "king", "jester", "knight", "pawn", "queen", "rook", "bishop",
    "berserker", "bomb", "ninja", "turtle", "ghost", "mage", "goop",
    "penguin", "parasite", "devil", "minion", "sludge", "sniper",
    "prince", "checker", "checkerKing", "giant", "copycat",
    "copycatClone", "angel", "halo", "fisherman", "dragon",
)

# Cosmetic prefab names that do not contain their rules-level character name.
# The opponent's Kong is still treated only as a public royal silhouette; the
# belief initializer branches King/Jester identity when more than one exists.
PREFAB_ALIASES = {
    "kong": "king",
    "undead": "minion",
    # The shipping prefab and native class retain the old character name.
    "freeze": "penguin",
}

PIECE_COST = dict(zip(PIECE_NAMES, (
    0, 10, 6, 3, 17, 13, 9, 15, 15, 20, 4, 15, 8, 0, 15,
    15, 15, 0, 12, 17, 18, 2, 2, 1, 5, 0, 13, 0, 12, 15,
)))

DRAFT_PIECES = tuple(
    piece for piece in PIECE_NAMES
    if piece not in ("king", "goop", "minion", "checkerKing",
                     "copycatClone", "halo")
)

# Native GameManager.LoadCharacters stably sorts selectable prefabs by the
# Character.value table.  The order among equal-valued pieces is the serialized
# prefab order recovered from the verified 5.731 APK and live ArmyMove identity
# feedback. Dragon and Berserker share a value but their serialized order is
# the reverse of the earlier visual-only assumption.
POT_SORT_ORDER = (
    "giant", "checker", "pawn", "turtle", "copycat", "knight", "mage",
    "bishop", "jester", "sludge", "fisherman", "rook", "angel",
    "berserker", "bomb", "ghost", "penguin", "parasite", "devil",
    "dragon", "queen", "sniper", "prince", "ninja",
)

# This is the saved 100-point local army used by the controller.  A future
# army can be supplied with --own-team without changing any phone logic.
DEFAULT_OWN_TEAM = (
    ("king", "a1"),
    ("queen", "h3"), ("queen", "b3"), ("queen", "c3"),
    ("queen", "b2"),
    ("pawn", "a2"), ("pawn", "h2"),
    ("giant", "e1"), ("giant", "c1"),
    ("pawn", "g2"), ("pawn", "e3"), ("pawn", "a3"),
    ("dragon", "g1"),
)

PREFAB_NAME = r"([A-Za-z][A-Za-z0-9_ -]*?)"
MOVE_RE = re.compile(rf"\b{PREFAB_NAME}(?:\(Clone\))? moves (\d+) --> (\d+)")
SELECT_RE = re.compile(
    rf"\b{PREFAB_NAME}(?:\(Clone\))?GetAvailableMoves was called")
# Square.OnPointerDown prints the logical board coordinate before Character's
# selection callback.  This is stronger than piece identity in formations with
# several CopyCats: an oversized linked model can intercept a tap many cells
# away while still reporting the same piece type as the intended actor.
POINTER_SQUARE_RE = re.compile(r"(?:^|:\s)([0-7]):([0-9])\s*$")
DEAD_RE = re.compile(rf"\b{PREFAB_NAME}(?:\(Clone\))? DEAD")
DIE_RE = re.compile(rf"\b{PREFAB_NAME}(?:\(Clone\))?:Die\(\)")
ATTACK_RE = re.compile(
    rf"attacking {PREFAB_NAME}(?:\(Clone\))?(?=\s*$)"
)
BOT_FROM_RE = re.compile(r"\bfrom ([a-h](?:10|[1-9]))\s*$")
BOT_TO_RE = re.compile(r"\bto ([a-h](?:10|[1-9]))\s*$")
BOT_CHARACTER_RE = re.compile(rf"\bCharacter {PREFAB_NAME}\s*$")
DOT_RE = re.compile(r"SetUpMyDot was called with (\d+) (\d+)")
ARMY_POINTS_RE = re.compile(
    r"GetPoints\(\) - player1 points : (\d+) - player2 points : (\d+)"
)
DRAFT_SPAWN_RE = re.compile(
    rf"(?:^|:\s){PREFAB_NAME}(?:\(Clone\))?\s+([0-7]):([0-9])\s*$"
)
BAN_TEXTURE_RE = re.compile(
    rf"\bTEXT?URE ASSIGNED TO {PREFAB_NAME}\s*$", re.IGNORECASE
)
DRAFT_TURN_RE = re.compile(
    r"\bmyBoard\.turn != team:\s*(True|False)\s*$", re.IGNORECASE
)
PLAYER_TEAM_RE = re.compile(
    r"\bGameManager\.Instance\.playerTeam != team:\s*(True|False)\s*$",
    re.IGNORECASE,
)
ARMY_DROP_RE = re.compile(r"(?:^|:\s)(\d+):(\d+)\s+-\s+(\d+):(\d+)\s*$")
ARMY_MOVE_RE = re.compile(r"(?:^|:\s)ArmyMove(?:\s+([A-Za-z]+))?\s*$")
ENGINE_MOVE_RE = re.compile(r"^([a-h](?:10|[1-9]))([-~@x!&])([a-h](?:10|[1-9]))$")


def scene_index_to_square(index: int) -> str:
    """Convert Unity's x*10+y coordinate to Ultimate Fish algebraic."""
    file_index, rank_index = divmod(index, 10)
    if not (0 <= file_index < 8 and 0 <= rank_index < 10):
        raise ValueError(f"invalid Unity square index: {index}")
    return f"{chr(ord('a') + file_index)}{rank_index + 1}"


def controller_movetime(command: str, requested_ms: int | None) -> int:
    """Resolve an explicit iterative-deepening limit; omitted is unlimited."""
    del command  # Kept in the API so modes can gain distinct policies later.
    if requested_ms is not None:
        if requested_ms < 0:
            raise ValueError("move time cannot be negative")
        return requested_ms
    return 0


def square_to_scene_index(square: str) -> int:
    match = re.fullmatch(r"([a-h])(10|[1-9])", square)
    if not match:
        raise ValueError(f"invalid square: {square}")
    return (ord(match.group(1)) - ord("a")) * 10 + int(match.group(2)) - 1


def square_sort_key(square: str) -> tuple[int, int]:
    return int(square[1:]), ord(square[0]) - ord("a")


def rotate_square(square: str) -> str:
    """Rotate a square 180 degrees for an Onyx player's camera."""
    return f"{chr(ord('h') - (ord(square[0]) - ord('a')))}{11 - int(square[1:])}"


def adjacent_toward(source: str, target: str) -> str:
    """Return the cell one queen-direction step from source toward target."""
    source_file, target_file = ord(source[0]), ord(target[0])
    source_rank, target_rank = int(source[1:]), int(target[1:])
    file_delta = (target_file > source_file) - (target_file < source_file)
    rank_delta = (target_rank > source_rank) - (target_rank < source_rank)
    if not (file_delta or rank_delta):
        raise ValueError("source and target must differ")
    if (source_file != target_file and source_rank != target_rank and
            abs(target_file - source_file) != abs(target_rank - source_rank)):
        raise ValueError("target is not on a queen ray from source")
    return f"{chr(source_file + file_delta)}{source_rank + rank_delta}"


def public_probe_piece(piece: str) -> str:
    """Collapse private or linked prefab labels to public tap identities."""
    if piece in ("king", "jester"):
        return "king"
    if piece in ("copycat", "copycatClone"):
        # Selecting either half can invoke GetAvailableMoves on both linked
        # prefabs. Their squares, not callback order, identify the pair.
        return "copycatPair"
    if piece in ("checker", "checkerKing"):
        # Promotion is stored in Checker's action byte; the live GameObject
        # and GetAvailableMoves diagnostic retain the ``checker`` prefab name.
        return "checker"
    return piece


def normalize_copycat_probes(
    probed: Sequence[tuple[str, str]],
) -> list[tuple[str, str]]:
    """Convert public CopyCat-pair cells into one linked engine pair."""
    pair_squares = {square for piece, square in probed if piece == "copycatPair"}
    normalized = [(piece, square) for piece, square in probed
                  if piece != "copycatPair"]
    remaining = set(pair_squares)
    while remaining:
        square = min(remaining, key=square_sort_key)
        mirror = f"{chr(ord('h') - (ord(square[0]) - ord('a')))}{square[1:]}"
        if mirror not in remaining:
            raise RuntimeError(
                f"public CopyCat cell {square} has no mirrored partner at {mirror}")
        first, second = sorted((square, mirror), key=square_sort_key)
        normalized.extend((("copycat", first), ("copycatClone", second)))
        remaining.remove(square)
        remaining.remove(mirror)
    return sorted(normalized, key=lambda item: square_sort_key(item[1]))


def parse_engine_move(move: str) -> tuple[str, str, str]:
    if move == "pass":
        return "pass", "pass", "pass"
    match = ENGINE_MOVE_RE.fullmatch(move)
    if not match:
        raise ValueError(f"unsupported engine move: {move}")
    return match.group(1), match.group(3), match.group(2)


def is_delayed_local_sniper_callback(
    event: AppEvent,
    targets: Sequence[str],
    bot_action: AppEvent | None,
) -> bool:
    """Identify the duplicate stay-put callback emitted after a Bomb shot."""
    return (
        event.kind == "move" and event.piece == "sniper" and
        event.source is not None and event.source == event.target and
        not targets and bot_action is None
    )


@dataclass(frozen=True)
class AppEvent:
    kind: str
    piece: str | None = None
    source: str | None = None
    target: str | None = None
    raw: str = ""
    payload: object | None = None


@dataclass(frozen=True)
class ModelPieceRecord:
    """Network/replay piece record before its privacy boundary is applied."""

    piece: str
    team: int
    x: int
    y: int
    action: int = 0
    cooldown: int = 0
    freeze_count: int = 0
    turn_moved: int = 0
    army: bool = False


@dataclass(frozen=True)
class OnlineStartState:
    pieces: tuple[ModelPieceRecord, ...]
    max_points: int | None = None


class ArmyPlacementRetry(RuntimeError):
    """A measured builder misdrop collided and requires a clean rebuild."""


class OnlineAuthenticationRequired(RuntimeError):
    """The restored account was rejected and needs an interactive sign-in."""


@dataclass(frozen=True)
class OpeningTerminal:
    """A public result reached before an Onyx position could be initialized."""

    result: str
    reason: str


@dataclass(frozen=True)
class RankedPlacementResult:
    """Authoritative result of one live Ranked deployment drag."""

    square: str
    piece: str
    local_points: int
    point_history: tuple[int, ...] = ()


def canonical_piece_name(prefab: str) -> str | None:
    """Map base and cosmetic prefab names to one rules-level piece type."""
    normalized = re.sub(r"[^a-z0-9]", "", prefab.lower())
    for alias, piece in PREFAB_ALIASES.items():
        if alias in normalized:
            return piece
    candidates = [
        piece for piece in PIECE_NAMES
        if re.sub(r"[^a-z0-9]", "", piece.lower()) in normalized
    ]
    return max(candidates, key=len) if candidates else None


def _model_field(item: dict, name: str, default=None):
    wanted = re.sub(r"[^a-z0-9]", "", name.lower())
    for key, value in item.items():
        if re.sub(r"[^a-z0-9]", "", str(key).lower()) == wanted:
            return value
    return default


def parse_model_piece(item: object) -> ModelPieceRecord:
    """Validate one SignalR ``Model_Piece`` without retaining its skin."""
    if not isinstance(item, dict):
        raise ValueError("Model_Piece must be a JSON object")
    raw_type = _model_field(item, "type")
    if isinstance(raw_type, str) and raw_type.isdigit():
        raw_type = int(raw_type)
    if isinstance(raw_type, int) and not isinstance(raw_type, bool):
        if not 0 <= raw_type < len(PIECE_NAMES):
            raise ValueError(f"unknown Model_Piece type {raw_type}")
        piece = PIECE_NAMES[raw_type]
    elif isinstance(raw_type, str):
        piece = canonical_piece_name(raw_type)
        if piece is None:
            raise ValueError(f"unknown Model_Piece type {raw_type!r}")
    else:
        raise ValueError("Model_Piece type is missing")

    def integer(name: str, default=None) -> int:
        value = _model_field(item, name, default)
        if isinstance(value, str) and re.fullmatch(r"-?\d+", value):
            value = int(value)
        if not isinstance(value, int) or isinstance(value, bool):
            raise ValueError(f"Model_Piece {name} is not an integer")
        return value

    team, x, y = integer("team"), integer("x"), integer("y")
    if team not in (0, 1) or not 0 <= x < 8 or not 0 <= y < 10:
        raise ValueError(f"invalid Model_Piece coordinate/team {team}:{x}:{y}")
    army = _model_field(item, "army", False)
    if not isinstance(army, bool):
        raise ValueError("Model_Piece army is not boolean")
    return ModelPieceRecord(
        piece, team, x, y,
        integer("action", 0), integer("cd", 0),
        integer("freezeCount", 0), integer("turnMoved", 0), army,
    )


def parse_structured_network_state(
    line: str,
) -> tuple[str, OnlineStartState | tuple[ModelPieceRecord, ...]] | None:
    """Parse a serialized SignalR start/group record when a log source has it.

    Stock 5.73 logcat prints only an event marker, but instrumented diagnostics
    and replay exports use the same JSON object. Supporting both the SignalR
    invocation envelope and a direct response keeps the privacy sanitizer
    independent from whichever read-only source supplies that object.
    """
    decoder = json.JSONDecoder()
    decoded = None
    for index, character in enumerate(line):
        if character not in "[{":
            continue
        try:
            decoded, _end = decoder.raw_decode(line[index:])
            break
        except json.JSONDecodeError:
            continue
    if decoded is None:
        return None

    target = ""
    payload = decoded
    if isinstance(decoded, dict):
        target = str(_model_field(decoded, "target", "")).lower()
        arguments = _model_field(decoded, "arguments")
        if isinstance(arguments, list) and arguments:
            payload = arguments[0]
    lowered = line.lower()
    if not target:
        if "onstartgame" in lowered:
            target = "onstartgame"
        elif "onspawnpiecegroup" in lowered:
            target = "onspawnpiecegroup"

    if "onstartgame" in target:
        if not isinstance(payload, dict):
            raise ValueError("OnStartGame payload must be an object")
        items = _model_field(payload, "model_Pieces")
        if not isinstance(items, list):
            raise ValueError("OnStartGame model_Pieces is missing")
        max_points = _model_field(payload, "maxPoints")
        if max_points is not None:
            if isinstance(max_points, str) and max_points.isdigit():
                max_points = int(max_points)
            if not isinstance(max_points, int) or isinstance(max_points, bool):
                raise ValueError("OnStartGame maxPoints is not an integer")
        return "start", OnlineStartState(
            tuple(parse_model_piece(item) for item in items), max_points)

    if "onspawnpiecegroup" in target:
        items = payload
        if isinstance(payload, dict):
            items = _model_field(payload, "pieces")
        if not isinstance(items, list):
            raise ValueError("OnSpawnPieceGroup payload must be a piece list")
        return "spawn", tuple(parse_model_piece(item) for item in items)
    return None


def parse_unity_line(line: str) -> AppEvent | None:
    structured = parse_structured_network_state(line)
    if structured:
        kind, payload = structured
        return AppEvent(
            "start_game" if kind == "start" else "draft_pick_committed",
            raw=line,
            payload=payload,
        )
    # Character.CompareDragDisplacement logs the exact deployment coordinate
    # chosen by Unity before ArmyMove resolves overlaps.  Capturing this error-
    # priority diagnostic lets the builder verify logical placement instead of
    # assuming that a drag to a visual cell center landed in that cell.
    match = ARMY_DROP_RE.search(line)
    if match:
        return AppEvent(
            "army_drop",
            source=f"{match.group(1)}:{match.group(2)}",
            target=f"{match.group(3)}:{match.group(4)}",
            raw=line,
        )
    match = ARMY_MOVE_RE.search(line)
    if match:
        # Giant's override omits the prefab name; it is the only blank
        # ArmyMove diagnostic in the recovered builder.
        piece = canonical_piece_name(match.group(1) or "giant")
        return AppEvent("army_piece", piece=piece, raw=line) if piece else None
    match = DRAFT_TURN_RE.search(line)
    if match:
        return AppEvent(
            "draft_turn_probe",
            source="opponent" if match.group(1).lower() == "true" else "local",
            raw=line,
        )
    match = PLAYER_TEAM_RE.search(line)
    if match:
        return AppEvent(
            "player_team_probe",
            source="opponent" if match.group(1).lower() == "true" else "local",
            raw=line,
        )
    # After a Ranked group becomes public, Square.Spawn logs the full rendered
    # board as ``prefab x:y`` records. Royals are already masked to ``king`` by
    # the app. Discard a Ghost coordinate at parse time so private information
    # cannot enter any controller journal or engine position.
    match = DRAFT_SPAWN_RE.search(line)
    if match:
        piece = canonical_piece_name(match.group(1))
        if piece:
            source = None if piece == "ghost" else f"{match.group(2)}:{match.group(3)}"
            return AppEvent("draft_piece_spawn", piece=piece, source=source)
    # Bot.RecordAiMove prints the authoritative public action as three adjacent
    # lines before the ordinary animation callback. This is especially useful
    # for stay-put actions: Devil/Mage/Fisherman later log source->source even
    # though this diagnostic retains their visible target.
    match = BOT_FROM_RE.search(line)
    if match:
        return AppEvent("bot_from", source=match.group(1), raw=line)
    match = BOT_TO_RE.search(line)
    if match:
        return AppEvent("bot_to", target=match.group(1), raw=line)
    match = BOT_CHARACTER_RE.search(line)
    if match:
        piece = canonical_piece_name(match.group(1))
        return AppEvent("bot_piece", piece=piece, raw=line) if piece else None
    match = MOVE_RE.search(line)
    if match:
        piece = canonical_piece_name(match.group(1))
        if piece is None:
            return None
        return AppEvent("move", piece,
                        scene_index_to_square(int(match.group(2))),
                        scene_index_to_square(int(match.group(3))), line)
    match = POINTER_SQUARE_RE.search(line)
    if match:
        return AppEvent(
            "pointer_square",
            source=scene_index_to_square(
                int(match.group(1)) * 10 + int(match.group(2))
            ),
            raw=line,
        )
    match = SELECT_RE.search(line)
    if match:
        piece = canonical_piece_name(match.group(1))
        return AppEvent("selected", piece, raw=line) if piece else None
    match = DEAD_RE.search(line)
    if match:
        piece = canonical_piece_name(match.group(1))
        return AppEvent("dead", piece, raw=line) if piece else None
    # Bomb detonations use the method diagnostic rather than Character's
    # ordinary ``<prefab> DEAD`` line.  It is an important animation barrier:
    # the network ChangeTurn message can arrive several seconds before this
    # local death/explosion has actually finished resolving.
    match = DIE_RE.search(line)
    if match:
        piece = canonical_piece_name(match.group(1))
        return AppEvent("dead", piece, raw=line) if piece else None
    match = ATTACK_RE.search(line)
    if match:
        piece = canonical_piece_name(match.group(1))
        return AppEvent("attack", piece, raw=line) if piece else None
    # Visibility is public UI state, but the corresponding move diagnostic can
    # contain a previously hidden origin. Keep this signal coordinate-free.
    if "Ghost:MakeVis()" in line:
        return AppEvent("ghost_visible", raw=line)
    if "Ghost:MakeInvis()" in line:
        return AppEvent("ghost_hidden", raw=line)
    if "ChangeTurn Start" in line:
        return AppEvent("turn_start", raw=line)
    if "ChangeTurn End" in line:
        return AppEvent("turn_end", raw=line)
    if "LoadBoardDraft" in line:
        return AppEvent("draft_board_loaded", raw=line)
    if "Board:LoadBoard(" in line:
        return AppEvent("board_loaded", raw=line)
    match = DOT_RE.search(line)
    if match:
        index = int(match.group(1)) * 10 + int(match.group(2))
        return AppEvent("dot_ready", source=scene_index_to_square(index), raw=line)
    match = ARMY_POINTS_RE.search(line)
    if match:
        return AppEvent(
            "army_points", source=match.group(1), target=match.group(2), raw=line
        )
    if "Character:SetUpMyDot" in line:
        return AppEvent("dot_ready", raw=line)
    if "!!!CompareDragDisplacement" in line:
        return AppEvent("touch_end", raw=line)
    if "Board:OutOfTime" in line:
        return AppEvent("out_of_time", raw=line)
    # Online GameOver is dispatched before the winning move's local attack
    # animation and well before OpenGameOverMenu. It is the only timely result
    # barrier when an Onyx player is knocked out on Ivory's opening move.
    if "GameOver MESSAGE" in line:
        return AppEvent("game_over", raw=line)
    if "OpenGameOverMenu" in line:
        return AppEvent("game_over", raw=line)
    normalized_terminal = re.sub(r"[^a-z0-9]+", " ", line.lower()).strip()
    if normalized_terminal.endswith("checkmate"):
        return AppEvent("terminal_label", source="checkmate", raw=line)
    if normalized_terminal.endswith("knock out"):
        return AppEvent("terminal_label", source="knockout", raw=line)
    if ("board state repeated 3 times" in normalized_terminal
            or "50 moves passed without a knock out" in normalized_terminal
            or normalized_terminal.endswith("stalemate")):
        return AppEvent("terminal_label", source="draw", raw=line)
    # The native GameFound prompt has a ten-second timer.  Treat the network
    # dispatch as a first-class event so the controller accepts immediately
    # instead of discovering the prompt through comparatively slow screenshots.
    if "GameFound MESSAGE" in line:
        return AppEvent("match_found", raw=line)
    if "OnJoinQueue MESSAGE" in line:
        return AppEvent("queue_joined", raw=line)
    if "OnSanityCheck MESSAGE" in line:
        return AppEvent("sanity_check", raw=line)
    # OnBanCharacter applies the newly public lock texture and names the exact
    # character pot. The shipping build misspells this as TEXURE; accept the
    # corrected TEXTURE form too. This avoids overlapping-pot image diffs.
    match = BAN_TEXTURE_RE.search(line)
    if match:
        piece = canonical_piece_name(match.group(1))
        return AppEvent("draft_ban_piece", piece=piece, raw=line) if piece else None
    # Marker-only stock logs still synchronize Ranked phases. A structured
    # payload, when supplied by a diagnostic/replay source, was parsed above
    # and buffered behind the post-reveal sanitizer instead.
    if "OnStartGame MESSAGE" in line:
        return AppEvent("start_game", raw=line)
    if "OnSpawnPieceGroup MESSAGE" in line:
        return AppEvent("draft_pick_committed", raw=line)
    if ("OnBanCharacter MESSAGE" in line or
            "NetworkManager:OnBanCharacter(Type)" in line):
        return AppEvent("draft_ban_committed", raw=line)
    if "ShopItem:OnPointerClick" in line:
        return AppEvent("shop_item", raw=line)
    return None


@dataclass(frozen=True)
class BoardGeometry:
    """Pixel geometry after the board's intro animation settles."""

    width: int = 1080
    height: int = 2400
    left: float = 72.0
    top: float = 610.0
    right: float = 1008.0
    bottom: float = 1748.0

    @property
    def cell_width(self) -> float:
        return (self.right - self.left) / 8

    @property
    def cell_height(self) -> float:
        return (self.bottom - self.top) / 10

    def point(self, square: str) -> tuple[int, int]:
        file_index = ord(square[0]) - ord("a")
        rank = int(square[1:])
        if not (0 <= file_index < 8 and 1 <= rank <= 10):
            raise ValueError(f"invalid board square: {square}")
        # Rank 10 is at the top of the rendered board.
        x = self.left + (file_index + 0.5) * self.cell_width
        y = self.top + (10 - rank + 0.5) * self.cell_height
        return round(x), round(y)

    def drag_destination(
        self, source: str, target: str, overshoot: float = 0.40
    ) -> tuple[int, int]:
        """End a swipe well inside the destination cell.

        Android's synthetic swipe can deliver its final ACTION_UP without a
        preceding motion sample at the nominal endpoint.  On a two-cell
        Fisherman drag that left Unity's last hover one rank short and made
        CompareDragDisplacement report source-to-source.  A bounded overshoot
        remains inside the target cell while ensuring Unity receives a motion
        event across its boundary.
        """
        source_x, source_y = self.point(source)
        target_x, target_y = self.point(target)
        dx = (target_x > source_x) - (target_x < source_x)
        dy = (target_y > source_y) - (target_y < source_y)
        x = target_x + dx * self.cell_width * overshoot
        y = target_y + dy * self.cell_height * overshoot
        return (
            round(min(self.right - 2, max(self.left + 2, x))),
            round(min(self.bottom - 2, max(self.top + 2, y))),
        )

    def scaled(self, width: int, height: int) -> "BoardGeometry":
        return BoardGeometry(
            width, height,
            self.left * width / self.width,
            self.top * height / self.height,
            self.right * width / self.width,
            self.bottom * height / self.height,
        )


@dataclass(frozen=True)
class DeploymentGeometry:
    """Pixel geometry of the local 8x3 army-building board."""

    width: int = 1080
    height: int = 2400
    left: float = 54.0
    top: float = 1334.0
    right: float = 1026.0
    bottom: float = 1668.0

    @property
    def cell_width(self) -> float:
        return (self.right - self.left) / 8

    @property
    def cell_height(self) -> float:
        return (self.bottom - self.top) / 3

    def point(self, square: str) -> tuple[int, int]:
        file_index = ord(square[0]) - ord("a")
        rank = int(square[1:])
        if not (0 <= file_index < 8 and 1 <= rank <= 3):
            raise ValueError(f"invalid deployment square: {square}")
        return (
            round(self.left + (file_index + 0.5) * self.cell_width),
            round(self.top + (3 - rank + 0.5) * self.cell_height),
        )

    def giant_drop_point(self, anchor: str) -> tuple[int, int]:
        """Choose a clear in-cell raycast point for a Giant anchor.

        The shipping Local builder reliably clears tall/adjacent models by
        dropping near the bottom of the requested cell. It remains inside the
        same Square collider, after which Giant.getClosestIntersection performs
        its own four-square visual centering.
        """
        x, y = self.point(anchor)
        return x, round(y + self.cell_height * 0.41)

    def drop_point(self, piece: str, square: str) -> tuple[int, int]:
        """Return a native-validated pointer-up point for a deployment pot."""
        if piece == "giant":
            return self.giant_drop_point(square)
        x, y = self.point(square)
        if piece == "prince":
            # Both f1 and g1 landed one rank high in Local until its adaptive
            # builder learned +52 px on a 111 px cell. Apply the same measured
            # model/pointer correction before a Ranked clock starts.
            y = round(y + self.cell_height * 0.47)
        return x, y

    def scaled(self, width: int, height: int) -> "DeploymentGeometry":
        return DeploymentGeometry(
            width, height,
            self.left * width / self.width,
            self.top * height / self.height,
            self.right * width / self.width,
            self.bottom * height / self.height,
        )


# Local Play's two pass-and-play builders sit 81 px below the online/CPU
# builder on the reference 1080x2400 phone.  Native ArmyMove coordinates are
# the calibration oracle; the visible row boundaries are y=1415/1526/1637/1749.
LOCAL_DEPLOYMENT_GEOMETRY = DeploymentGeometry(top=1415.0, bottom=1749.0)
# Ranked keeps the full Board object while showing three rows of character pots
# directly above the local deployment zone. These are the measured gray-cell
# edges on the reference 1080x2400 portrait layout; the earlier cyan-aura/pot
# rectangle is not raycastable board space.
RANKED_DEPLOYMENT_GEOMETRY = DeploymentGeometry(
    left=60.0, top=1344.0, right=1032.0, bottom=1668.0,
)


def detect_outline_squares(image, geometry: BoardGeometry, color: str = "red",
                           ranks: Iterable[int] = (8, 9, 10)) -> list[str]:
    """Find occupied public squares from team-colored character outlines.

    ``image`` is a Pillow RGB image.  Sampling the center 82% of a cell avoids
    the decorative red material counter above the board and outline bleed into
    adjacent cells.  Enemy Checkers use an almost-black body with only a thin
    red rim, so red occupancy also accepts a large dark center.  This prevents
    their public squares from being mistaken for empty cells.
    """
    try:
        import numpy as np
    except ImportError as exc:  # pragma: no cover - environment guidance
        raise RuntimeError("initial board discovery requires numpy") from exc

    rgb = np.asarray(image.convert("RGB"))
    geometry = geometry.scaled(*image.size)
    red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
    if color == "red":
        mask = ((red > 205) & (red > green * 1.32) & (red > blue * 1.13))
        dark_mask = (red < 65) & (green < 65) & (blue < 70)
    elif color == "blue":
        mask = ((blue > 150) & (blue > red * 1.12) & (green > red * 1.05))
        dark_mask = None
    else:
        raise ValueError("outline color must be red or blue")

    half_w = max(8, round(geometry.cell_width * 0.41))
    half_h = max(8, round(geometry.cell_height * 0.42))
    area_scale = (image.size[0] / 1080) * (image.size[1] / 2400)
    # Mage's wide blue cloak can cover most of its own red outline; 600 still
    # clears empty-cell noise on the calibrated board while retaining that
    # thin visible rim. False positives from outline spill are harmless because
    # probe_enemy also requires a native piece-selection acknowledgement.
    threshold = max(40, round(600 * area_scale))
    dark_threshold = max(100, round(1500 * area_scale))
    occupied: list[str] = []
    for rank in ranks:
        for file_index in range(8):
            square = f"{chr(ord('a') + file_index)}{rank}"
            x, y = geometry.point(square)
            count = int(mask[max(0, y - half_h):y + half_h,
                             max(0, x - half_w):x + half_w].sum())
            dark_count = 0
            if dark_mask is not None:
                dark_count = int(dark_mask[
                    max(0, y - half_h):y + half_h,
                    max(0, x - half_w):x + half_w,
                ].sum())
            if count >= threshold or dark_count >= dark_threshold:
                occupied.append(square)
    return occupied


def consensus_square_sets(observations: Sequence[Iterable[str]],
                          minimum_hits: int | None = None) -> list[str]:
    """Majority-vote already detected square sets."""
    frames = [set(squares) for squares in observations]
    if not frames:
        raise ValueError("square consensus requires at least one observation")
    if minimum_hits is None:
        minimum_hits = 1 if len(frames) < 3 else len(frames) // 2 + 1
    if not 1 <= minimum_hits <= len(frames):
        raise ValueError("square consensus threshold is outside the observation count")
    hits: Counter[str] = Counter()
    for squares in frames:
        hits.update(squares)
    return sorted(
        (square for square, count in hits.items() if count >= minimum_hits),
        key=square_sort_key,
    )


def consensus_outline_squares(images: Sequence, geometry: BoardGeometry,
                              color: str = "red",
                              ranks: Iterable[int] = (8, 9, 10),
                              minimum_hits: int | None = None) -> list[str]:
    """Retain public occupancy that persists across settled video frames.

    Character idle animations can temporarily cover a thin team-colored rim,
    while an emote's closing animation can create a one-frame false edge.  A
    majority vote over three or more unobscured frames rejects both cases.  A
    single supplied frame remains supported for offline fixtures and manual
    probing, but the live initializer always supplies at least three.
    """
    frames = list(images)
    if not frames:
        raise ValueError("outline consensus requires at least one frame")
    return consensus_square_sets(
        [detect_outline_squares(image, geometry, color, ranks) for image in frames],
        minimum_hits,
    )


def detect_pot_centers(image) -> list[tuple[int, int]]:
    """Find the orange character-pot bodies in an army/draft screen."""
    import numpy as np

    rgb = np.asarray(image.convert("RGB"))
    height, width = rgb.shape[:2]
    red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
    orange = ((red > 150) & (green > 60) & (green < 175) & (blue < 85)
              & (red > green * 1.30))
    area_scale = width * height / (1080 * 2400)
    projection = orange.sum(axis=1)
    search_start = round(height * 0.30)
    search_end = round(height * 0.60)
    # Pot rims are the three strongest long horizontal orange runs.  Recover
    # their rows first, then component-label each row separately.  This is
    # important for characters such as Mage whose orange model pixels join two
    # vertically adjacent pots into one component in the fully unlocked 8x3
    # layout.
    candidates = sorted(
        range(search_start, search_end),
        key=lambda y: int(projection[y]),
        reverse=True,
    )
    row_peaks: list[int] = []
    separation = round(height * 0.040)
    minimum_projection = width * 0.20
    for y in candidates:
        if projection[y] < minimum_projection:
            break
        if all(abs(y - existing) >= separation for existing in row_peaks):
            row_peaks.append(y)
            if len(row_peaks) == 3:
                break
    if len(row_peaks) != 3:
        return []
    row_peaks.sort()
    boundaries = [
        max(0, row_peaks[0] - round(height * 0.040)),
        (row_peaks[0] + row_peaks[1]) // 2,
        (row_peaks[1] + row_peaks[2]) // 2,
        min(height, row_peaks[2] + round(height * 0.045)),
    ]
    centers = []
    for row in range(3):
        start, end = boundaries[row], boundaries[row + 1]
        for component in _components(orange[start:end]):
            if len(component) < 1800 * area_scale:
                continue
            ys = [point[0] + start for point in component]
            xs = [point[1] for point in component]
            box_width = max(xs) - min(xs) + 1
            box_height = max(ys) - min(ys) + 1
            center_x = (min(xs) + max(xs)) // 2
            center_y = (min(ys) + max(ys)) // 2
            if (width * 0.050 < box_width < width * 0.120
                    and height * 0.025 < box_height < height * 0.080):
                centers.append((center_x, center_y))
    # Component centers vary slightly with the character mesh occluding the
    # pot.  Row-major ordering is stable and useful for offline calibration.
    return sorted(centers, key=lambda point: (round(point[1] / (height * 0.05)), point[0]))


def map_ranked_pots(image) -> dict[str, tuple[int, int]]:
    """Map every owned Ranked pot to its native character type.

    The live fully unlocked Android layout contains three rows of eight pots.
    Characters are assigned column-major (top/middle/bottom) in the stable
    value/prefab order recovered from the native build.
    """
    centers = detect_pot_centers(image)
    if len(centers) != len(POT_SORT_ORDER):
        raise RuntimeError(
            f"Ranked requires 24 visible owned character pots; found {len(centers)}")
    rows: list[list[tuple[int, int]]] = []
    for point in sorted(centers, key=lambda value: value[1]):
        if not rows or point[1] - sum(y for _, y in rows[-1]) / len(rows[-1]) > image.height * 0.035:
            rows.append([point])
        else:
            rows[-1].append(point)
    if len(rows) != 3:
        raise RuntimeError(f"expected three character-pot rows, found {len(rows)}")
    top, middle, bottom = (sorted(row) for row in rows)
    if tuple(map(len, (top, middle, bottom))) != (8, 8, 8):
        raise RuntimeError(
            "unexpected full pot layout: "
            f"top={len(top)} middle={len(middle)} bottom={len(bottom)}")
    traversal: list[tuple[int, int]] = []
    for column in range(8):
        traversal.extend((top[column], middle[column], bottom[column]))
    return dict(zip(POT_SORT_ORDER, traversal))


def resolve_army_drag_identity(
    requested: str,
    reported: str | None,
    starting_points: int,
    point_history: Sequence[int],
) -> str | None:
    """Resolve the one known ambiguous native ``ArmyMove`` diagnostic.

    CopyCat's placement override can emit the same blank ``ArmyMove`` line as
    Giant. The parser conservatively names a blank line Giant, but the exact
    material transition distinguishes the five-point CopyCat from the
    one-point Giant. No other identity mismatch is rewritten here.
    """
    final_points = point_history[-1] if point_history else starting_points
    added = final_points - min((starting_points, *point_history))
    if (requested == "copycat" and reported == "giant" and
            added == PIECE_COST["copycat"]):
        return "copycat"
    return reported


def detect_builder_giant_anchors(
    image, expected_count: int | None = None,
    geometry: DeploymentGeometry = DeploymentGeometry(),
) -> list[str] | None:
    """Locate Giant logical 2x2 anchors in the saved-army builder.

    Unity does not emit the ordinary ``x:y`` placement record for Giant
    drags.  Its large pink model is nevertheless a stable visual marker. The
    component centroid sits roughly a quarter-cell above the center of its
    logical footprint. Adjacent meshes can touch, so when the caller knows the
    expected count, deterministically split merged pink pixels with k-means.

    ``None`` is deliberately conservative: Ready must never be clicked when
    Giant models cannot be distinguished from ordinary models.
    """
    try:
        import numpy as np
    except ImportError as exc:  # pragma: no cover - environment guidance
        raise RuntimeError("Giant placement verification requires numpy") from exc

    rgb = np.asarray(image.convert("RGB"))
    geometry = geometry.scaled(*image.size)
    left = max(0, round(geometry.left))
    top = max(0, round(geometry.top))
    right = min(image.width, round(geometry.right))
    bottom = min(image.height, round(geometry.bottom))
    board = rgb[top:bottom, left:right]
    red, green, blue = board[:, :, 0], board[:, :, 1], board[:, :, 2]
    pink = (
        (red > 165)
        & (blue > 90)
        & (red > green * 1.15)
        & (blue > green * 0.85)
    )
    area_scale = image.width * image.height / (1080 * 2400)
    minimum_area = max(800, round(8000 * area_scale))
    candidates: list[tuple[int, list[tuple[int, int]]]] = []
    for component in _components(pink):
        if len(component) < minimum_area:
            continue
        ys = [point[0] for point in component]
        xs = [point[1] for point in component]
        width = max(xs) - min(xs) + 1
        height = max(ys) - min(ys) + 1
        if (width >= geometry.cell_width * 1.35
                and height >= geometry.cell_height * 0.75):
            candidates.append((len(component), component))
    components = [component for _area, component in candidates]
    if expected_count is not None and expected_count > 0:
        if not components:
            return None
        if len(components) != expected_count:
            pixels = np.asarray(
                [point for component in components for point in component],
                dtype=float,
            )
            if len(pixels) < expected_count * minimum_area * 0.45:
                return None
            normalized = pixels / np.asarray(
                [geometry.cell_height, geometry.cell_width], dtype=float
            )
            global_center = normalized.mean(axis=0)
            seeds = [normalized[np.argmax(
                ((normalized - global_center) ** 2).sum(axis=1)
            )]]
            while len(seeds) < expected_count:
                distances = np.min(np.stack([
                    ((normalized - seed) ** 2).sum(axis=1) for seed in seeds
                ]), axis=0)
                seeds.append(normalized[np.argmax(distances)])
            centers = np.asarray(seeds)
            labels = np.zeros(len(normalized), dtype=int)
            for _iteration in range(20):
                distances = np.stack([
                    ((normalized - center) ** 2).sum(axis=1)
                    for center in centers
                ], axis=1)
                new_labels = distances.argmin(axis=1)
                if np.array_equal(new_labels, labels) and _iteration:
                    break
                labels = new_labels
                if any(not np.any(labels == index)
                       for index in range(expected_count)):
                    return None
                centers = np.asarray([
                    normalized[labels == index].mean(axis=0)
                    for index in range(expected_count)
                ])
            components = [
                [(int(y), int(x)) for y, x in pixels[labels == index]]
                for index in range(expected_count)
            ]

    anchors = []
    for component in components:
        if len(component) < minimum_area * 0.45:
            return None
        center_x = left + sum(point[1] for point in component) / len(component)
        center_y = top + sum(point[0] for point in component) / len(component)
        # The recovered Giant mesh centroid is 25-29 px above its logical
        # footprint center at 1080x2400, or 0.24 deployment-cell heights.
        logical_center_y = center_y + geometry.cell_height * 0.24
        file_index = round(
            (center_x - geometry.left) / geometry.cell_width - 1.0
        )
        rank = round(
            3.0 - (logical_center_y - geometry.top) / geometry.cell_height
        )
        if not (0 <= file_index <= 6 and 1 <= rank <= 2):
            return None
        anchors.append(f"{chr(ord('a') + file_index)}{rank}")
    if len(set(anchors)) != len(anchors):
        return None
    return sorted(anchors, key=square_sort_key)


def detect_builder_giant_anchor(
    image, geometry: DeploymentGeometry = DeploymentGeometry()
) -> str | None:
    """Backward-compatible single-Giant detector."""
    anchors = detect_builder_giant_anchors(image, 1, geometry)
    return anchors[0] if anchors and len(anchors) == 1 else None


def verify_builder_placement(
    image,
    expected_team: Sequence[tuple[str, str]],
    native_confirmed: Counter[tuple[str, str]],
    geometry: DeploymentGeometry = DeploymentGeometry(),
) -> None:
    """Require exact native/visual placement evidence before clicking Ready."""
    expected_ordinary = Counter(
        item for item in expected_team if item[0] not in ("king", "giant")
    )
    if native_confirmed != expected_ordinary:
        missing = expected_ordinary - native_confirmed
        unexpected = native_confirmed - expected_ordinary
        details = []
        if missing:
            details.append("missing " + ", ".join(
                f"{piece}@{square} x{count}"
                for (piece, square), count in sorted(missing.items())
            ))
        if unexpected:
            details.append("unexpected " + ", ".join(
                f"{piece}@{square} x{count}"
                for (piece, square), count in sorted(unexpected.items())
            ))
        raise ArmyPlacementRetry(
            "pre-Ready native placement verification failed: " + "; ".join(details)
        )

    expected_giants = [
        square for piece, square in expected_team if piece == "giant"
    ]
    detected_giants = detect_builder_giant_anchors(
        image, len(expected_giants) if expected_giants else None, geometry
    )
    if expected_giants:
        expected_giants = sorted(expected_giants, key=square_sort_key)
        if detected_giants != expected_giants:
            raise ArmyPlacementRetry(
                "pre-Ready Giant footprints mismatch: expected "
                f"{' '.join(expected_giants)}, detected "
                f"{' '.join(detected_giants) if detected_giants else 'none'}"
            )
    elif detected_giants:
        raise ArmyPlacementRetry(
            "pre-Ready found unexpected Giant footprints at "
            + " ".join(detected_giants)
        )


def _components(mask) -> list[list[tuple[int, int]]]:
    """Small 4-connected-component helper that keeps OpenCV optional."""
    import numpy as np

    height, width = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    components = []
    for y, x in zip(*np.nonzero(mask)):
        if seen[y, x]:
            continue
        stack = [(int(y), int(x))]
        seen[y, x] = True
        pixels = []
        while stack:
            yy, xx = stack.pop()
            pixels.append((yy, xx))
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                ny, nx = yy + dy, xx + dx
                if (0 <= ny < height and 0 <= nx < width and mask[ny, nx]
                        and not seen[ny, nx]):
                    seen[ny, nx] = True
                    stack.append((ny, nx))
        components.append(pixels)
    return sorted(components, key=len, reverse=True)


def read_material_counter(image, tesseract: str = "tesseract") -> int:
    """Read the public three-tile red material counter.

    Isolating the actual glyph components first makes the bundled stylized 3D
    counter legible to ordinary Tesseract.  The game's ``1`` is intentionally
    corrected by its narrow aspect ratio because Tesseract otherwise calls it
    a ``7``.  This is deterministic OCR over three tiny glyphs, not per-turn
    board recognition.
    """
    try:
        import numpy as np
        from PIL import Image
    except ImportError as exc:  # pragma: no cover - environment guidance
        raise RuntimeError("material OCR requires Pillow and numpy") from exc

    rgb = np.asarray(image.convert("RGB"))
    height, width = rgb.shape[:2]
    red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
    red_mask = ((red > 180) & (red > green * 1.25) & (red > blue * 1.15))
    x0, x1 = round(width * 0.60), round(width * 0.97)
    y0, y1 = round(height * 0.17), round(height * 0.25)
    tile_boxes = []
    for component in _components(red_mask[y0:y1, x0:x1]):
        if len(component) < 1000 * width * height / (1080 * 2400):
            continue
        ys = [point[0] for point in component]
        xs = [point[1] for point in component]
        box = (min(xs) + x0, min(ys) + y0, max(xs) + x0 + 1, max(ys) + y0 + 1)
        box_width, box_height = box[2] - box[0], box[3] - box[1]
        if width * 0.04 < box_width < width * 0.11 and height * 0.015 < box_height < height * 0.05:
            tile_boxes.append(box)
    tile_boxes.sort()
    if len(tile_boxes) != 3:
        raise RuntimeError(f"expected three material digit tiles, found {len(tile_boxes)}")

    glyphs = []
    narrow = []
    for left, top, right, bottom in tile_boxes:
        inset_x = max(3, round((right - left) * 0.14))
        inset_top = max(3, round((bottom - top) * 0.15))
        inset_bottom = max(2, round((bottom - top) * 0.09))
        tile = rgb[top + inset_top:bottom - inset_bottom,
                   left + inset_x:right - inset_x]
        tr, tg, tb = tile[:, :, 0], tile[:, :, 1], tile[:, :, 2]
        light = (tg > 155) & (tb > 115) & (tr > 180)
        interior = []
        for component in _components(light):
            if not component:
                continue
            # Perspective/intro easing can shift a glyph down by a pixel.  A
            # real digit may therefore touch the bottom crop edge (observed
            # with the final 9 in the public ``099`` counter).  Decorative
            # tile/background components still span a horizontal edge, while
            # a digit remains inset from both the left and right edges.
            if any(x in (0, light.shape[1] - 1) for _, x in component):
                continue
            interior.append(component)
        if not interior:
            raise RuntimeError("could not isolate a material digit glyph")
        component = max(interior, key=len)
        ys = [point[0] for point in component]
        xs = [point[1] for point in component]
        glyph = np.zeros((max(ys) - min(ys) + 1, max(xs) - min(xs) + 1), dtype=np.uint8)
        for y, x in component:
            glyph[y - min(ys), x - min(xs)] = 255
        narrow.append(glyph.shape[1] / glyph.shape[0] < 0.90)
        resampling = getattr(Image, "Resampling", Image).NEAREST
        glyphs.append(np.asarray(Image.fromarray(255 - glyph).resize((60, 90), resampling)))

    canvas = np.full((110, 220), 255, dtype=np.uint8)
    for index, glyph in enumerate(glyphs):
        canvas[10:100, 10 + index * 70:70 + index * 70] = glyph
    # PGM avoids a temporary file and is accepted by Tesseract on stdin.
    header = f"P5\n{canvas.shape[1]} {canvas.shape[0]}\n255\n".encode()
    result = subprocess.run(
        [tesseract, "stdin", "stdout", "--psm", "7",
         "-c", "tessedit_char_whitelist=0123456789"],
        input=header + canvas.tobytes(), stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL, check=True,
    )
    digits = re.sub(r"\D", "", result.stdout.decode(errors="replace"))
    if len(digits) == 3 or (len(digits) == 4 and narrow[0]):
        # The only observed four-character output is the leading narrow ``1``
        # expanding to ``71``. Alignment from the right preserves the two
        # independently segmented following tiles; the narrow test restores 1.
        digits = digits[-3:]
        corrected = [
            "1" if is_narrow else digit
            for digit, is_narrow in zip(digits, narrow)
        ]
    else:
        # Tesseract occasionally reads the shipping font's leading ``1`` as
        # two characters (live ``100`` -> ``7100``) even though segmentation
        # found exactly three tiles. Fall back to one glyph per OCR call. The
        # narrow-glyph test is stronger than OCR for ``1``; other tiles retain
        # the final whitelisted digit from their isolated image.
        corrected = []
        for glyph, is_narrow in zip(glyphs, narrow):
            if is_narrow:
                corrected.append("1")
                continue
            isolated = np.full((110, 80), 255, dtype=np.uint8)
            isolated[10:100, 10:70] = glyph
            isolated_header = (
                f"P5\n{isolated.shape[1]} {isolated.shape[0]}\n255\n".encode()
            )
            single = subprocess.run(
                [tesseract, "stdin", "stdout", "--psm", "10",
                 "-c", "tessedit_char_whitelist=0123456789"],
                input=isolated_header + isolated.tobytes(),
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=True,
            )
            single_digits = re.sub(
                r"\D", "", single.stdout.decode(errors="replace"))
            if not single_digits:
                raise RuntimeError(
                    f"material OCR returned {digits!r}; isolated glyph was empty")
            corrected.append(single_digits[-1])
    if len(corrected) != 3:
        raise RuntimeError(f"material OCR returned {digits!r}")
    return int("".join(corrected))


def find_text_centers(image, target: str, tesseract: str = "tesseract",
                      exact: bool = False) -> list[tuple[int, int]]:
    """Locate UI words without relying on Unity accessibility metadata."""
    import io

    encoded = io.BytesIO()
    image.convert("RGB").save(encoded, format="PNG")
    result = subprocess.run(
        [tesseract, "stdin", "stdout", "--psm", "11", "tsv"],
        input=encoded.getvalue(), stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL, check=True,
    )
    wanted = re.sub(r"[^A-Z]", "", target.upper())
    candidates = []
    for line in result.stdout.decode(errors="replace").splitlines()[1:]:
        fields = line.split("\t", 11)
        if len(fields) != 12:
            continue
        observed = re.sub(r"[^A-Z]", "", fields[11].upper())
        matches = observed == wanted if exact else wanted in observed
        if not matches:
            continue
        try:
            left, top, width, height = map(int, fields[6:10])
            confidence = float(fields[10])
        except ValueError:
            continue
        candidates.append((confidence, width * height,
                           (left + width // 2, top + height // 2)))
    candidates.sort(reverse=True)
    return [point for _confidence, _area, point in candidates]


def find_text_center(image, target: str, tesseract: str = "tesseract",
                     exact: bool = False) -> tuple[int, int] | None:
    """Locate the best prominent UI word."""
    points = find_text_centers(image, target, tesseract, exact)
    return points[0] if points else None


def read_game_over_result(image, tesseract: str = "tesseract") -> str:
    """Read the public result or terminal-condition headline.

    Quest text below the headline can contain words such as ``WIN`` even after
    a loss, so OCR is restricted to the upper condition banner and the central
    result overlay. CHECKMATE/KNOCK-OUT describe how the game ended rather
    than which player won; callers resolve those using whose move just ended.
    """
    import io
    from PIL import Image

    def classify(observations: Iterable[str]) -> str | None:
        observed = re.sub(r"[^A-Z]", "", " ".join(observations).upper())
        for token, result in (
            ("VICTORY", "win"), ("DEFEAT", "loss"), ("DRAW", "draw"),
            ("CHECKMATE", "checkmate"), ("KNOCKOUT", "knockout"),
        ):
            if token in observed:
                return result
        return None

    width, height = image.size
    resampling = getattr(type(image), "Resampling", None)
    if resampling is None:
        resampling = getattr(Image, "Resampling", Image)
    observations = []
    headlines = []
    for top, bottom in ((0.04, 0.25), (0.32, 0.64)):
        headline = image.crop((
            0, round(height * top), width, round(height * bottom),
        ))
        headline = headline.resize(
            (max(1000, headline.width * 2), max(300, headline.height * 2)),
            resampling.LANCZOS,
        )
        headlines.append(headline)
        encoded = io.BytesIO()
        headline.convert("RGB").save(encoded, format="PNG")
        result = subprocess.run(
            [tesseract, "stdin", "stdout", "--psm", "11"],
            input=encoded.getvalue(), stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, check=True,
        )
        observations.append(result.stdout.decode(errors="replace").upper())
    classified = classify(observations)
    if classified:
        return classified

    # The result screen uses a thick white font with a blue drop shadow.
    # Tesseract can return no text at all on the RGB image even though the
    # headline is visually unambiguous. Isolate bright lettering
    # and retry as a single text block. This recognized the captured Ranked
    # timeout victory where the unprocessed pass returned an empty string.
    try:
        import numpy as np
        for headline in headlines:
            enlarged = headline.resize(
                (headline.width * 3 // 2, headline.height * 3 // 2),
                resampling.LANCZOS,
            )
            rgb = np.asarray(enlarged.convert("RGB"))
            bright = rgb.min(axis=2) > 175
            isolated = Image.fromarray(
                np.where(bright, 0, 255).astype("uint8"), mode="L"
            )
            encoded = io.BytesIO()
            isolated.save(encoded, format="PNG")
            result = subprocess.run(
                [tesseract, "stdin", "stdout", "--psm", "6"],
                input=encoded.getvalue(), stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL, check=True,
            )
            classified = classify([
                result.stdout.decode(errors="replace")
            ])
            if classified:
                return classified
    except ImportError:  # pragma: no cover - raw OCR remains the fallback
        pass
    # The game's extruded yellow condition font is reliably visible but can
    # be illegible to Tesseract. Both CHECKMATE and KNOCK-OUT occupy most of
    # the upper banner: measured captures contain ~77k yellow pixels there,
    # versus ~5k incidental yellow pixels on the timeout/defeat overlay.
    try:
        import numpy as np
        rgb = np.asarray(image.convert("RGB"))
        upper = rgb[
            round(height * 0.04):round(height * 0.25),
            round(width * 0.04):round(width * 0.96),
        ]
        red, green, blue = upper[:, :, 0], upper[:, :, 1], upper[:, :, 2]
        yellow = (
            (red > 210) & (green > 130) & (green < 235) & (blue < 100)
            & (red > green * 1.08)
        )
        area_scale = width * height / (1080 * 2400)
        if int(yellow.sum()) > max(3000, round(30_000 * area_scale)):
            return "terminal"
    except ImportError:  # pragma: no cover - OCR remains the fallback
        pass
    return "unknown"


def changed_pot(before, after, pots: dict[str, tuple[int, int]],
                radius: int = 58) -> tuple[str, dict[str, float]]:
    """Identify the publicly banned pot from its newly rendered lock.

    Draft backgrounds and character idle animations keep moving, so a global
    screenshot difference is noisy.  Comparing equally sized crops at every
    known pot and selecting a clear relative outlier makes the public lock
    animation deterministic without reading the private pick payload.
    """
    import numpy as np

    old = np.asarray(before.convert("RGB"), dtype=np.int16)
    new = np.asarray(after.convert("RGB"), dtype=np.int16)
    if old.shape != new.shape:
        raise ValueError("draft screenshots must have the same dimensions")
    height, width = old.shape[:2]
    scores: dict[str, float] = {}
    for piece, (x, y) in pots.items():
        x0, x1 = max(0, x - radius), min(width, x + radius + 1)
        y0, y1 = max(0, y - radius), min(height, y + radius + 1)
        if x0 >= x1 or y0 >= y1:
            raise ValueError(f"pot {piece} lies outside the screenshot")
        delta = np.abs(new[y0:y1, x0:x1] - old[y0:y1, x0:x1])
        # A lock changes many pixels strongly; the 80th percentile suppresses
        # tiny anti-aliasing and idle-animation differences.
        scores[piece] = float(np.percentile(delta.max(axis=2), 80))
    ordered = sorted(scores.items(), key=lambda item: item[1], reverse=True)
    if not ordered:
        raise RuntimeError("no draft pots are calibrated")
    best_piece, best = ordered[0]
    runner_up = ordered[1][1] if len(ordered) > 1 else 0.0
    if best < 18.0 or (runner_up > 0.0 and best < runner_up * 1.25):
        raise RuntimeError(
            "public ban did not produce a unique pot change: "
            + ", ".join(f"{piece}={score:.1f}" for piece, score in ordered[:4]))
    return best_piece, scores


class DraftDeployment:
    """Deterministic legal home-zone placement for incrementally drafted pieces."""

    # Put tactically active pieces near the front while keeping the two royals
    # and long-range support behind them.  Engine search starts from the exact
    # resulting board after reveal, so this policy can be replaced by measured
    # self-play placements without changing the phone protocol.
    PREFERENCES = {
        # Wide models must be placed contiguously from an open edge. Maximum-
        # clearance order (a,g,e,c) leaves the final c-file target between two
        # Giant colliders even though the logical 2x2 packing is legal.
        "giant": ("a2", "c2", "e2", "g2", "c1", "e1", "g1"),
        "jester": ("b1", "h1", "g1"),
        "ghost": ("d2", "e2", "c2", "f2"),
        "dragon": ("d1", "e1", "c1", "f1"),
        "bomb": ("d3", "e3", "c3", "f3"),
        "berserker": ("e3", "d3", "f3", "c3"),
        "parasite": ("c3", "f3", "b3", "g3"),
        "penguin": ("d3", "e3", "c3", "f3", "e2", "d2", "f2", "c2"),
        "sniper": ("h1", "g1", "f1"),
        # CopyCat's wide pot collider has landed one file toward the center in
        # live Ranked. Start far from the center so the measured native anchor
        # and its mirror still have room; the controller records the actual
        # CompareDragDisplacement coordinate before reserving either cell.
        "copycat": ("h2", "g2", "h3", "g3"),
        "rook": ("h1", "g1", "b1"),
        "queen": ("d1", "e1", "c1"),
        "fisherman": ("c2", "f2", "b2", "g2"),
        "angel": ("c1", "b1", "d1"),
        "devil": ("f2", "c2", "g2"),
        # The far-right Ranked pot gives Prince a large model/pointer offset.
        # Native Local placement is reliable on the central/right back rank;
        # a live b1 target physically landed on c2 and blocked Giant packing.
        "prince": ("f1", "g1", "e1", "d1", "b1"),
    }
    FALLBACK = tuple(
        f"{file}{rank}"
        for rank in (3, 2, 1) for file in "defcbgah"
        if not (file == "a" and rank == 1)
    )
    # The Ranked board supplies the King on a1 before the first pick.  Empty
    # a2/b2/b1 rays permit a long-range piece to capture it on Ivory's opening
    # move, before an Onyx controller can search.  Prefer placements which
    # close those rays, while retaining live-drop clearance as the tie-breaker.
    # Ghosts do not count because they are invisible to the attacker; Bombs
    # are unsafe adjacent shields because their capture also kills the King.
    KING_SHIELD = frozenset({"a2", "b2", "b1"})
    NON_BLOCKING_SHIELDS = frozenset({"ghost", "bomb"})

    def __init__(self):
        self.occupied = {"a1"}
        self.team: list[tuple[str, str]] = [("king", "a1")]

    @staticmethod
    def _mirror(square: str) -> str:
        return f"{chr(ord('h') - (ord(square[0]) - ord('a')))}{square[1:]}"

    @staticmethod
    def _giant_cells(anchor: str) -> set[str]:
        file_index, rank = ord(anchor[0]) - ord("a"), int(anchor[1:])
        if file_index >= 7 or rank >= 3:
            return set()
        return {
            f"{chr(ord('a') + file_index + dx)}{rank + dy}"
            for dx in (0, 1) for dy in (0, 1)
        }

    def cells(self, piece: str, square: str) -> set[str]:
        cells = {square}
        if piece == "giant":
            cells = self._giant_cells(square)
        elif piece == "copycat":
            cells.add(self._mirror(square))
        return cells

    def propose(
        self, piece: str, excluded: Iterable[str] = (),
        maximize_clearance: bool = False,
    ) -> str:
        excluded = set(excluded)
        candidates = self.PREFERENCES.get(piece, ()) + self.FALLBACK
        valid: list[tuple[str, set[str]]] = []
        for square in dict.fromkeys(candidates):
            cells = self.cells(piece, square)
            if square not in excluded and cells and not (cells & self.occupied):
                valid.append((square, cells))
        if valid and maximize_clearance:
            def distance(candidate: set[str]) -> int:
                return min(
                    max(abs(ord(left[0]) - ord(right[0])),
                        abs(int(left[1:]) - int(right[1:])))
                    for left in candidate for right in self.occupied
                )

            def safety(candidate_piece: str, cells: set[str]) -> int:
                if candidate_piece in self.NON_BLOCKING_SHIELDS:
                    return 0
                return len(cells & (self.KING_SHIELD - self.occupied))

            best = max(
                (safety(piece, cells), distance(cells))
                for _square, cells in valid
            )
            # ``valid`` retains the tactical preference ordering for equally
            # safe and clear cells.  Safety prevents a zero-ply royal loss;
            # clearance then avoids neighbouring meshes intercepting the drop.
            return next(
                square for square, cells in valid
                if (safety(piece, cells), distance(cells)) == best
            )
        if valid:
            return valid[0][0]
        raise RuntimeError(f"no legal deployment cells remain for {piece}")

    def plan(
        self,
        pieces: Sequence[str],
        excluded: Sequence[set[str]] | None = None,
    ) -> list[str]:
        """Backtrack a complete landing plan for one still-mutable group.

        Greedy maximum-clearance placement can strand a later wide model even
        when the 8x3 zone has a legal packing (four opening Giants are the
        smallest live-relevant example).  Ranked reveals all local choices for
        the current group before any are dragged, so plan the remaining group
        as a unit and recompute after every native landing correction.
        """
        blocked = self.occupied.copy()
        excluded = excluded or tuple(set() for _piece in pieces)
        if len(excluded) != len(pieces):
            raise ValueError("deployment exclusions must align with pieces")
        answer: list[str] = []

        def distance(cells: set[str]) -> int:
            return min(
                max(abs(ord(left[0]) - ord(right[0])),
                    abs(int(left[1:]) - int(right[1:])))
                for left in cells for right in blocked
            )

        def place(index: int) -> bool:
            if index == len(pieces):
                return True
            piece = pieces[index]
            candidates = self.PREFERENCES.get(piece, ()) + self.FALLBACK
            valid: list[tuple[str, set[str]]] = []
            for square in dict.fromkeys(candidates):
                cells = self.cells(piece, square)
                if (square not in excluded[index] and cells
                        and not cells & blocked):
                    valid.append((square, cells))
            valid.sort(
                key=lambda item: (
                    0 if piece in self.NON_BLOCKING_SHIELDS or piece == "prince" else
                    len(item[1] & (self.KING_SHIELD - blocked)),
                    0 if piece in ("giant", "prince") else distance(item[1]),
                    -candidates.index(item[0]),
                ),
                reverse=True,
            )
            for square, cells in valid:
                blocked.update(cells)
                answer.append(square)
                if place(index + 1):
                    return True
                answer.pop()
                blocked.difference_update(cells)
            return False

        if not place(0):
            raise RuntimeError(
                "remaining Ranked group has no legal deployment packing: "
                + " ".join(pieces)
            )
        return answer

    def reserve(self, piece: str, square: str) -> None:
        cells = self.cells(piece, square)
        if not cells or cells & self.occupied:
            raise RuntimeError(
                f"native deployment of {piece}@{square} overlaps locked cells"
            )
        self.occupied.update(cells)
        self.team.append((piece, square))

    def release(self, piece: str, square: str) -> None:
        """Forget one still-unlocked placement removed by a native collision."""
        try:
            self.team.remove((piece, square))
        except ValueError as exc:
            raise RuntimeError(
                f"cannot release unreserved deployment {piece}@{square}"
            ) from exc
        self.occupied.difference_update(self.cells(piece, square))

    def place(self, piece: str) -> str:
        square = self.propose(piece)
        self.reserve(piece, square)
        return square


class EventStream:
    GAMEPLAY_KINDS = frozenset({
        "move", "attack", "dead", "ghost_visible", "ghost_hidden",
        "turn_start", "turn_end", "terminal_label", "game_over",
        "out_of_time",
    })

    def _accept_ban_callback(self, now: float) -> bool:
        """Collapse repeated stack frames emitted by one Ban callback."""
        if now - self.last_ban_callback_at < 0.25:
            return False
        self.last_ban_callback_at = now
        return True

    def __init__(self, adb: str, device: str):
        self.process = subprocess.Popen(
            # ``-T 1`` tails only the newest record before following.  Without
            # it, old timeout/game-over records from earlier matches can be
            # mistaken for live terminal events during process startup.
            [adb, "-s", device, "logcat", "-T", "1", "-v", "brief",
             "Unity:V", "*:S"],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
            bufsize=1,
        )
        self.events: queue.Queue[AppEvent] = queue.Queue()
        self.network_lock = threading.Lock()
        self.online_start: OnlineStartState | None = None
        self.ranked_groups: list[tuple[ModelPieceRecord, ...]] = []
        self.draft_ban_count = 0
        self.draft_ban_pieces: list[str] = []
        self.draft_spawn_lock = threading.Lock()
        self.draft_spawn_generation = 0
        self.draft_spawn_complete = False
        self.draft_spawns: list[AppEvent] = []
        self.draft_spawn_updated_at = float("-inf")
        self.last_ban_callback_at = float("-inf")
        # Keep a second, non-consuming public gameplay journal for the short
        # interval between Board.LoadBoard and search initialization. Queue
        # consumers intentionally discard unrelated log records while waiting
        # for tap acknowledgements; on the Onyx side that used to discard the
        # opponent's opening move and let us probe a board mid-animation.
        self.gameplay_lock = threading.Lock()
        self.gameplay_generation = 0
        self.gameplay_events: list[AppEvent] = []
        # GetPoints is public, exact native state. Keep the maximum total seen
        # for each player since Board.LoadBoard: board spawning can emit
        # partial increasing totals, while an Onyx opening capture can reduce
        # the later value before we finish probing. The per-generation maximum
        # therefore preserves the exact starting material without OCR.
        self.points_lock = threading.Lock()
        self.initial_points: list[int | None] = [None, None]
        self.points_frozen = False
        self.bot_source: str | None = None
        self.bot_target: str | None = None
        self.thread = threading.Thread(target=self._read, daemon=True)
        self.thread.start()

    def _record_gameplay_event(self, event: AppEvent) -> None:
        """Journal one public event within the current loaded-board generation."""
        with self.gameplay_lock:
            # Ranked creates its interactive board with LoadBoardDraft and
            # reuses that object after the final reveal. Treat it as the same
            # hard generation boundary as ordinary Board.LoadBoard; otherwise
            # a prior match's terminal event survives into await_onyx_opening
            # and the next startup forfeits a still-live game.
            if event.kind in ("board_loaded", "draft_board_loaded"):
                self.gameplay_generation += 1
                self.gameplay_events.clear()
                with self.points_lock:
                    self.initial_points = [None, None]
                    self.points_frozen = False
            elif event.kind in self.GAMEPLAY_KINDS:
                self.gameplay_events.append(event)
                if event.kind in ("move", "attack"):
                    with self.points_lock:
                        self.points_frozen = True
        if event.kind == "army_points":
            assert event.source is not None and event.target is not None
            values = (int(event.source), int(event.target))
            with self.points_lock:
                for index, value in enumerate(values):
                    previous = self.initial_points[index]
                    if previous is None or (not self.points_frozen and value > previous):
                        self.initial_points[index] = value

    def points_snapshot(self) -> tuple[int, int] | None:
        """Return non-consuming native point maxima for the loaded board."""
        with self.points_lock:
            if any(value is None for value in self.initial_points):
                return None
            return int(self.initial_points[0]), int(self.initial_points[1])

    def initial_points_snapshot(self) -> tuple[int, int] | None:
        """Compatibility name for gameplay's frozen starting totals."""
        return self.points_snapshot()

    def _read(self) -> None:
        assert self.process.stdout is not None
        for line in self.process.stdout:
            event = parse_unity_line(line.rstrip())
            if event is not None:
                if event.kind == "draft_ban_committed":
                    # One OnBanCharacter callback prints its method frame under
                    # several Unity diagnostics (dot teardown, then texture
                    # assignment). It is one phase transition, not multiple
                    # public bans. A real following phase cannot complete in
                    # this sub-frame interval.
                    if not self._accept_ban_callback(time.monotonic()):
                        continue
                    with self.network_lock:
                        self.draft_ban_count += 1
                elif event.kind == "draft_turn_probe":
                    # Bind the native turn predicate to the exact draft phase
                    # at which Unity evaluated it. A remote Ban can finish
                    # during slow addressable/pot calibration.
                    with self.network_lock:
                        event = AppEvent(
                            event.kind, event.piece, event.source, event.target,
                            event.raw, self.draft_ban_count,
                        )
                if event.kind == "bot_from":
                    self.bot_source = event.source
                    self.bot_target = None
                    continue
                if event.kind == "bot_to":
                    self.bot_target = event.target
                    continue
                if event.kind == "bot_piece":
                    if self.bot_source and self.bot_target and event.piece:
                        event = AppEvent(
                            "bot_action", event.piece,
                            self.bot_source, self.bot_target, event.raw,
                        )
                    else:
                        self.bot_source = None
                        self.bot_target = None
                        continue
                    self.bot_source = None
                    self.bot_target = None
                self._record_gameplay_event(event)
                if event.kind == "start_game" and isinstance(
                        event.payload, OnlineStartState):
                    with self.network_lock:
                        self.online_start = event.payload
                elif event.kind == "draft_pick_committed" and isinstance(
                        event.payload, tuple):
                    with self.network_lock:
                        self.ranked_groups.append(event.payload)
                elif event.kind == "draft_ban_piece" and event.piece:
                    with self.network_lock:
                        self.draft_ban_pieces.append(event.piece)
                if event.kind == "draft_pick_committed":
                    with self.draft_spawn_lock:
                        self.draft_spawn_generation += 1
                        self.draft_spawn_complete = False
                        self.draft_spawns.clear()
                        self.draft_spawn_updated_at = time.monotonic()
                elif event.kind == "draft_piece_spawn":
                    with self.draft_spawn_lock:
                        if self.draft_spawn_generation:
                            self.draft_spawns.append(event)
                            self.draft_spawn_updated_at = time.monotonic()
                elif event.kind == "sanity_check":
                    with self.draft_spawn_lock:
                        if self.draft_spawn_generation:
                            self.draft_spawn_complete = True
                self.events.put(event)

    def gameplay_snapshot(self) -> tuple[int, tuple[AppEvent, ...]]:
        """Return public events since the latest completed board load.

        This journal is independent of ``wait``/``drain`` so perspective and
        deployment taps cannot consume a real opening action.
        """
        with self.gameplay_lock:
            return self.gameplay_generation, tuple(self.gameplay_events)

    def opening_action_started(self) -> bool:
        """Whether a real action has begun since the current board loaded."""
        _generation, events = self.gameplay_snapshot()
        return any(
            event.kind in ("move", "attack", "dead", "ghost_visible",
                           "ghost_hidden")
            for event in events
        )

    def replay_gameplay_journal(self) -> None:
        """Replay board-load gameplay after an initial-position tap scan."""
        _generation, events = self.gameplay_snapshot()
        self.drain()
        for event in events:
            self.events.put(event)

    def reset_network_state(self) -> None:
        with self.network_lock:
            self.online_start = None
            self.ranked_groups.clear()
            self.draft_ban_count = 0
            self.draft_ban_pieces.clear()
        with self.draft_spawn_lock:
            self.draft_spawn_generation = 0
            self.draft_spawn_complete = False
            self.draft_spawns.clear()
            self.draft_spawn_updated_at = float("-inf")
        self.last_ban_callback_at = float("-inf")

    def start_state(self) -> OnlineStartState | None:
        with self.network_lock:
            return self.online_start

    def ranked_state(self) -> OnlineStartState | None:
        """Return buffered private groups only after the caller sees reveal."""
        with self.network_lock:
            if self.online_start is None and not self.ranked_groups:
                return None
            initial = self.online_start.pieces if self.online_start else ()
            groups = tuple(piece for group in self.ranked_groups for piece in group)
            maximum = self.online_start.max_points if self.online_start else 100
            return OnlineStartState(initial + groups, maximum)

    def ranked_bans_completed(self) -> int:
        """Return the non-consuming count of unique public Ban callbacks."""
        with self.network_lock:
            return self.draft_ban_count

    def ranked_ban_snapshot(self) -> tuple[str, ...]:
        """Return exact public Ban names independently of queue consumers."""
        with self.network_lock:
            return tuple(self.draft_ban_pieces)

    def ranked_spawn_snapshot(self) -> tuple[int, bool, tuple[AppEvent, ...]]:
        """Return the non-consuming public spawn journal for the latest group."""
        with self.draft_spawn_lock:
            # OnSanityCheck may precede the Square.Spawn coroutines, so it is
            # useful diagnostic evidence but not an end delimiter. A group is
            # consumable only after at least one public record and log quiet.
            complete = (
                bool(self.draft_spawns)
                and time.monotonic() - self.draft_spawn_updated_at >= 0.12
            )
            return (
                self.draft_spawn_generation,
                complete,
                tuple(self.draft_spawns),
            )

    def drain(self) -> AppEvent | None:
        """Discard queued noise while returning any terminal game event.

        A remote resignation can arrive while the engine is searching.  The
        next action used to empty the queue and silently discard that result,
        then try to tap the already-closed board.  Most callers intentionally
        ignore this return value while preparing menus; gameplay callers can
        preserve the authoritative terminal barrier.
        """
        terminal = None
        while True:
            try:
                event = self.events.get_nowait()
            except queue.Empty:
                return terminal
            if (terminal is None and
                    event.kind in ("terminal_label", "game_over", "out_of_time")):
                terminal = event

    def wait(self, kinds: str | Iterable[str], timeout: float,
             predicate=None) -> AppEvent:
        accepted = {kinds} if isinstance(kinds, str) else set(kinds)
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"timed out waiting for {sorted(accepted)}")
            try:
                event = self.events.get(timeout=remaining)
            except queue.Empty as exc:
                raise TimeoutError(f"timed out waiting for {sorted(accepted)}") from exc
            if event.kind in accepted and (predicate is None or predicate(event)):
                return event

    def close(self) -> None:
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()


class AdbDevice:
    def __init__(self, adb: str, device: str):
        self.adb = adb
        self.device = device
        self.shell = subprocess.Popen(
            [adb, "-s", device, "shell"], stdin=subprocess.PIPE,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, text=True,
            bufsize=1,
        )

    def command(self, command: str) -> None:
        if self.shell.poll() is not None:
            raise RuntimeError("persistent adb shell stopped")
        assert self.shell.stdin is not None
        self.shell.stdin.write(command + "\n")
        self.shell.stdin.flush()

    def tap(self, x: int, y: int) -> None:
        self.command(f"input tap {int(x)} {int(y)}")

    def tap_sync(self, x: int, y: int) -> None:
        """Deliver a timing-critical menu tap before returning."""
        subprocess.run(
            [self.adb, "-s", self.device, "shell", "input", "tap",
             str(int(x)), str(int(y))],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

    def tap_square(self, geometry: BoardGeometry, square: str) -> None:
        self.tap(*geometry.point(square))

    def drag(self, source: tuple[int, int], target: tuple[int, int],
             duration_ms: int = 180) -> None:
        self.command(
            f"input swipe {int(source[0])} {int(source[1])} "
            f"{int(target[0])} {int(target[1])} {int(duration_ms)}"
        )

    def drag_sync(self, source: tuple[int, int], target: tuple[int, int],
                  duration_ms: int = 180) -> None:
        """Deliver a timing-critical drag and wait for Android completion."""
        subprocess.run(
            [self.adb, "-s", self.device, "shell", "input", "swipe",
             str(int(source[0])), str(int(source[1])),
             str(int(target[0])), str(int(target[1])), str(int(duration_ms))],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

    def keep_awake(self) -> None:
        self.command("svc power stayon true")
        self.command("settings put system screen_off_timeout 2147483647")

    def restart_app(self, package: str) -> None:
        self.command(f"am force-stop {package}")
        self.launch_app(package)

    def launch_app(self, package: str) -> None:
        """Bring an installed app forward without clearing its current state."""
        self.command(
            f"monkey -p {package} -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1"
        )

    def is_package_running(self, package: str) -> bool:
        """Return whether Android still has a process for ``package``."""
        result = subprocess.run(
            [self.adb, "-s", self.device, "shell", "pidof", package],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
            check=False,
        )
        return result.returncode == 0 and bool(result.stdout.strip())

    def wait_for_package_exit(self, package: str, timeout: float) -> bool:
        """Return true if ``package`` exits during a short failure grace period.

        Android's low-memory killer can race the controller's final event
        timeout by a few hundred milliseconds.  Keeping this polling helper in
        the failure path prevents an OS kill from being reported as a native
        rules rejection without slowing successful moves.
        """
        deadline = time.monotonic() + timeout
        while True:
            if not self.is_package_running(package):
                return True
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return False
            time.sleep(min(0.10, remaining))

    def screenshot(self):
        try:
            from PIL import Image
            import io
        except ImportError as exc:  # pragma: no cover - environment guidance
            raise RuntimeError("initial board discovery requires Pillow") from exc
        command = [
            self.adb, "-s", self.device, "exec-out", "screencap", "-p",
        ]
        last_detail = "no screenshot attempt completed"
        for attempt in range(4):
            result = subprocess.run(
                command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                check=False,
            )
            if result.returncode:
                stderr = result.stderr.decode("utf-8", errors="replace").strip()
                last_detail = stderr or f"adb exited {result.returncode}"
            else:
                try:
                    with Image.open(io.BytesIO(result.stdout)) as captured:
                        captured.load()
                        return captured.convert("RGB")
                except (OSError, ValueError) as exc:
                    last_detail = (
                        f"invalid image payload ({len(result.stdout)} bytes): {exc}"
                    )
            if attempt < 3:
                time.sleep(0.08 * (attempt + 1))
        raise RuntimeError(
            "adb screencap failed after 4 attempts: " + last_detail
        )

    def close(self) -> None:
        if self.shell.poll() is None:
            self.shell.terminate()
            try:
                self.shell.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.shell.kill()


class EngineClient:
    def __init__(self, path: str):
        self.path = path
        self.process = subprocess.Popen(
            [path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, bufsize=1,
        )

    def send(self, command: str) -> None:
        if self.process.poll() is not None:
            raise RuntimeError(f"engine stopped: {self.path}")
        assert self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def until(self, prefixes: str | Sequence[str]) -> str:
        accepted = (prefixes,) if isinstance(prefixes, str) else tuple(prefixes)
        assert self.process.stdout is not None
        for line in self.process.stdout:
            line = line.rstrip()
            if line.startswith(accepted):
                return line
        raise RuntimeError(f"engine exited while waiting for {accepted}")

    def set_position(self, upn: str) -> None:
        self.send("position upn " + upn)
        result = self.until(("positionok", "info string invalid upn"))
        if result != "positionok":
            raise ValueError(f"engine rejected position: {result}\n{upn}")

    def legal_moves(self, upn: str) -> list[str]:
        self.set_position(upn)
        self.send("moves")
        line = self.until("moves")
        return line.split()[1:]

    def apply(self, upn: str, move: str) -> str:
        self.set_position(upn)
        self.send("move " + move)
        result = self.until(("position ", "illegalmove"))
        if result == "illegalmove":
            raise ValueError(f"illegal move {move} for {upn}")
        return result.split(" ", 1)[1]

    def search(self, upn: str, depth: int, nodes: int = 0,
               movetime_ms: int = 0,
               root_moves: Sequence[str] = ()) -> tuple[str | None, int, str]:
        self.set_position(upn)
        limits = ["go", "depth", str(depth)]
        if nodes:
            limits += ["nodes", str(nodes)]
        if movetime_ms:
            limits += ["movetime", str(movetime_ms)]
        if root_moves:
            limits += ["searchmoves", *root_moves]
        self.send(" ".join(limits))
        info = self.until("info depth ")
        best = self.until("bestmove ").split(" ", 1)[1]
        score_match = re.search(r" score (cp|mate) (-?\d+)", info)
        score = int(score_match.group(2)) if score_match else 0
        if score_match and score_match.group(1) == "mate":
            score = (1 if score >= 0 else -1) * (30000 - abs(score))
        return (None if best == "(none)" else best), score, info

    def search_beliefs(self, positions: Sequence[str], depth: int,
                       nodes: int = 0,
                       movetime_ms: int = 0,
                       draw_moves: Sequence[str] = ()) -> tuple[str | None, int, str]:
        """Let Ultimate Fish select one root for a public information set."""
        if not positions:
            raise ValueError("cannot search an empty belief set")
        self.send("belief clear")
        if self.until(("beliefok", "info string")) != "beliefok":
            raise RuntimeError("engine could not clear its belief set")
        for position in positions:
            self.send("belief add " + position)
            result = self.until(("beliefok", "info string invalid belief"))
            if result != "beliefok":
                raise ValueError(f"engine rejected belief: {result}\n{position}")
        limits = ["belief", "go", "depth", str(depth)]
        if nodes:
            limits += ["nodes", str(nodes)]
        if movetime_ms:
            limits += ["movetime", str(movetime_ms)]
        if draw_moves:
            limits += ["drawmoves", *draw_moves]
        self.send(" ".join(limits))
        info = self.until("info depth ")
        best = self.until("bestmove ").split(" ", 1)[1]
        score_match = re.search(r" score (cp|mate) (-?\d+)", info)
        score = int(score_match.group(2)) if score_match else 0
        if score_match and score_match.group(1) == "mate":
            score = (1 if score >= 0 else -1) * (30000 - abs(score))
        return (None if best == "(none)" else best), score, info

    def draft_new(self) -> None:
        self.send("draft new")
        result = self.until(("draftok", "info string draft error"))
        if result != "draftok":
            raise RuntimeError(result)

    def draft_status(self) -> dict[str, int | str]:
        self.send("draft status")
        line = self.until("draft phase ")
        match = re.fullmatch(
            r"draft phase (\d+) player (white|black) action "
            r"(ban|pick|complete) min (\d+) max (\d+) white (\d+) black (\d+)",
            line,
        )
        if not match:
            raise RuntimeError(f"malformed draft status: {line}")
        return {
            "phase": int(match.group(1)), "player": match.group(2),
            "action": match.group(3), "min": int(match.group(4)),
            "max": int(match.group(5)), "white": int(match.group(6)),
            "black": int(match.group(7)),
        }

    def draft_auto(self) -> list[str]:
        self.send("draft auto")
        line = self.until(("draftauto", "info string draft error"))
        if line.startswith("info string"):
            raise RuntimeError(line)
        choices = line.split()[1:]
        unknown = [piece for piece in choices if piece not in DRAFT_PIECES]
        if unknown:
            raise RuntimeError(f"engine returned unknown draft pieces: {unknown}")
        return choices

    def draft_choose(self, piece: str) -> None:
        if piece not in DRAFT_PIECES:
            raise ValueError(f"piece is not draft-selectable: {piece}")
        self.send("draft choose " + piece)
        result = self.until(("draftok", "info string draft error"))
        if result != "draftok":
            raise RuntimeError(result)

    def draft_commit(self) -> None:
        self.send("draft commit")
        result = self.until(("draftok", "info string draft error"))
        if result != "draftok":
            raise RuntimeError(result)

    def close(self) -> None:
        if self.process.poll() is None:
            self.send("quit")
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()


def parse_upn_pieces(upn: str) -> list[tuple[str, str, str, list[str]]]:
    pieces = []
    for field in upn.split(";")[1:]:
        if "=" in field:
            continue
        values = field.split(",")
        if len(values) >= 3:
            pieces.append((values[0], values[1], values[2], values[3:]))
    return pieces


def upn_side(upn: str) -> str:
    return upn.split(";", 1)[0]


def upn_repetition_key(upn: str) -> str:
    """Strip counters that do not change the legal position."""
    return ";".join(
        field for field in upn.split(";")
        if not field.startswith(("hm=", "fm="))
    )


def upn_piece_at(upn: str, square: str) -> tuple[str, str, bool] | None:
    for piece, color, location, state in parse_upn_pieces(upn):
        if location != square:
            continue
        visible = len(state) < 6 or state[5] != "0"
        return piece, color, visible
    return None


def upn_piece_covering(upn: str, square: str) -> tuple[str, str, bool] | None:
    """Return the actor occupying a logical cell, including Giant tiles."""
    direct = upn_piece_at(upn, square)
    if direct is not None:
        return direct
    for piece, color, anchor, state in parse_upn_pieces(upn):
        if piece != "giant" or square not in giant_footprint(anchor):
            continue
        visible = len(state) < 6 or state[5] != "0"
        return piece, color, visible
    return None


def make_upn(own_team: Sequence[tuple[str, str]],
             enemy_team: Sequence[tuple[str, str]], side: str = "w") -> str:
    fields = [side, "hm=0", "fm=1", "ep=-", "cont=0", "forced=-1", "epv=-1"]
    fields.extend(f"{piece},w,{square}" for piece, square in own_team)
    fields.extend(f"{piece},b,{square}" for piece, square in enemy_team)
    return ";".join(fields)


def move_matches(move: str, source: str, target: str) -> bool:
    try:
        move_source, move_target, _ = parse_engine_move(move)
    except ValueError:
        return False
    return move_source == source and move_target == target


def is_castling_move(position: str, move: str) -> bool:
    """Recognize native King/Jester two-file castling notation."""
    try:
        source, target, separator = parse_engine_move(move)
    except ValueError:
        return False
    if separator != "-" or int(source[1:]) != int(target[1:]):
        return False
    if abs(ord(source[0]) - ord(target[0])) != 2:
        return False
    actor = upn_piece_at(position, source)
    return bool(actor and actor[0] in ("king", "jester"))


def enemy_rook_castle_probe_square(position: str, move: str) -> str | None:
    """Return the native Rook landing for a forced-timeout castle.

    SimulatedKing accepts the first unmoved Rook on its horizontal scan even
    when that Rook belongs to the opponent. The live models relocate, then
    board.turn and playerTeam disagree forever. The landing Rook is the clean
    native probe for confirming that silent terminal transition.
    """
    if not is_castling_move(position, move):
        return None
    source, target, _separator = parse_engine_move(move)
    actor = upn_piece_at(position, source)
    if actor is None:
        return None
    direction = 1 if ord(target[0]) > ord(source[0]) else -1
    file_index = ord(source[0]) - ord("a") + direction
    rank = source[1:]
    while 0 <= file_index < 8:
        square = f"{chr(ord('a') + file_index)}{rank}"
        occupant = upn_piece_covering(position, square)
        if occupant is not None:
            if occupant[0] != "rook" or occupant[1] == actor[1]:
                return None
            rook_file = ord(target[0]) - ord("a") - direction
            return f"{chr(ord('a') + rook_file)}{rank}"
        file_index += direction
    return None


def special_drag_overshoots(source: str, target: str) -> tuple[float, ...]:
    """Try special-action endpoints in an order suited to their distance.

    Synthetic swipes need a deep endpoint so Android emits a motion sample
    inside the destination boundary after the dragged 3D model catches up.
    The endpoint remains within the target cell; progressively shallower
    samples are retained only for unusually large target colliders.
    """
    file_distance = abs(ord(source[0]) - ord(target[0]))
    rank_distance = abs(int(source[1:]) - int(target[1:]))
    if max(file_distance, rank_distance) > 2:
        return (0.40, 0.24, 0.08)
    return (0.40, 0.24, 0.08)


def is_copycat_square(position: str, square: str) -> bool:
    """Recognize an explicit CopyCat half or an implicit mirrored clone."""
    actor = upn_piece_at(position, square)
    if actor and actor[0] in ("copycat", "copycatClone"):
        return True
    mirror = f"{chr(ord('h') - (ord(square[0]) - ord('a')))}{square[1:]}"
    mirrored = upn_piece_at(position, mirror)
    # Compact/custom UPN names only the deployable half; the native parser
    # creates its CopyCatClone on the mirrored square.
    return bool(mirrored and mirrored[0] == "copycat")


def giant_footprint(anchor: str) -> set[str]:
    file_index = ord(anchor[0]) - ord("a")
    rank = int(anchor[1:])
    return {
        f"{chr(ord('a') + file_index + dx)}{rank + dy}"
        for dx in (0, 1) for dy in (0, 1)
    }


def native_move_matches(position: str, move: str, event: AppEvent) -> bool:
    """Match an app move record to an engine move.

    Ordinary characters log their engine source and destination.  Giant logs
    are different: Unity can report whichever one of the four footprint cells
    was raycast/dragged at each endpoint, while UPN stores only the lower-left
    anchor.  Matching footprint membership retains the public coordinates and
    deterministically recovers the engine's anchor-to-anchor move.
    """
    if not event.source or not event.target:
        return False
    if event.piece != "giant":
        return move_matches(move, event.source, event.target)
    try:
        source, target, _ = parse_engine_move(move)
    except ValueError:
        return False
    actor = upn_piece_at(position, source)
    return bool(
        actor and actor[0] == "giant"
        and event.source in giant_footprint(source)
        and event.target in giant_footprint(target)
    )


def giant_anchors(footprint_squares: Iterable[str]) -> list[str]:
    """Exactly tile selected Giant cells into non-overlapping 2x2 pieces.

    Adjacent Giants form one connected outline component, so connected-component
    collapsing incorrectly turns two side-by-side pieces into one.  Every live
    footprint cell raycasts the same Giant class, however, which gives us an
    exact-cover problem over 2x2 board rectangles.
    """
    cells = set(footprint_squares)
    if not cells:
        return []

    def covers(cell: str) -> list[tuple[str, set[str]]]:
        file_index = ord(cell[0]) - ord("a")
        rank = int(cell[1:])
        candidates = []
        for dx in (0, 1):
            for dy in (0, 1):
                anchor_file = file_index - dx
                anchor_rank = rank - dy
                if not (0 <= anchor_file < 7 and 1 <= anchor_rank < 10):
                    continue
                anchor = f"{chr(ord('a') + anchor_file)}{anchor_rank}"
                covered = giant_footprint(anchor)
                if covered <= cells:
                    candidates.append((anchor, covered))
        return candidates

    def solve(remaining: set[str]) -> list[str] | None:
        if not remaining:
            return []
        cell = min(remaining, key=square_sort_key)
        for anchor, covered in sorted(covers(cell)):
            if not covered <= remaining:
                continue
            rest = solve(remaining - covered)
            if rest is not None:
                return [anchor] + rest
        return None

    result = solve(cells)
    if result is not None:
        return sorted(result, key=square_sort_key)

    # A character mesh can obscure one colored outline even though the other
    # three cells all raycast to the same Giant.  Recover only a unique 3-of-4
    # footprint; one or two cells remain ambiguous and fail closed.
    completion_candidates = []
    for file_index in range(7):
        for rank in range(1, 10):
            anchor = f"{chr(ord('a') + file_index)}{rank}"
            covered = giant_footprint(anchor)
            if len(covered & cells) >= 3:
                completion_candidates.append((anchor, covered))
    completions = []

    def complete(remaining: set[str], used: set[str], anchors: list[str]) -> None:
        if not remaining:
            completions.append(tuple(sorted(anchors, key=square_sort_key)))
            return
        cell = min(remaining, key=square_sort_key)
        for anchor, covered in completion_candidates:
            observed = covered & cells
            if cell not in observed or covered & used:
                continue
            complete(remaining - observed, used | covered, anchors + [anchor])

    complete(set(cells), set(), [])
    unique = sorted(set(completions))
    if len(unique) == 1:
        return list(unique[0])
    raise RuntimeError(
        "selected Giant cells do not form complete or uniquely completable 2x2 footprints: "
        + " ".join(sorted(cells, key=square_sort_key)))


class BeliefSet:
    def __init__(self, engine: EngineClient, positions: Iterable[str], limit: int = 64):
        self.engine = engine
        self.limit = limit
        self.positions = self._bounded(positions)
        if not self.positions:
            raise ValueError("a belief set cannot be empty")

    def _bounded(self, positions: Iterable[str]) -> list[str]:
        unique = sorted(set(positions))
        if len(unique) <= self.limit:
            return unique
        # Evenly retain the lexicographic range instead of biasing toward the
        # first files when the initial hidden-Ghost combination count is large.
        return [unique[round(i * (len(unique) - 1) / (self.limit - 1))]
                for i in range(self.limit)]

    @property
    def side(self) -> str:
        sides = {upn_side(position) for position in self.positions}
        if len(sides) != 1:
            raise RuntimeError(f"belief side-to-move diverged: {sides}")
        return next(iter(sides))

    def apply_known(self, move: str) -> None:
        next_positions = []
        for position in self.positions:
            if move in self.engine.legal_moves(position):
                next_positions.append(self.engine.apply(position, move))
        if not next_positions:
            raise RuntimeError(f"known move is inconsistent with every belief: {move}")
        self.positions = self._bounded(next_positions)

    @staticmethod
    def _bomb_count(position: str) -> int:
        return sum(piece == "bomb" for piece, _color, _square, _state
                   in parse_upn_pieces(position))

    def _move_causes_bomb_detonation(self, position: str, move: str) -> bool:
        after = self.engine.apply(position, move)
        return self._bomb_count(after) < self._bomb_count(position)

    def move_causes_bomb_detonation(self, move: str) -> bool:
        """Whether a known legal action detonates a Bomb in any belief."""
        return any(
            self._move_causes_bomb_detonation(position, move)
            for position in self.positions
            if move in self.engine.legal_moves(position)
        )

    def observation_causes_bomb_detonation(self, event: AppEvent) -> bool:
        """Whether a public coordinate move detonates a Bomb in any belief."""
        if event.kind != "move" or not event.source or not event.target:
            return False
        for position in self.positions:
            candidates = [
                move for move in self.engine.legal_moves(position)
                if native_move_matches(position, move, event)
            ]
            typed = []
            for move in candidates:
                source, _target, _separator = parse_engine_move(move)
                actor = upn_piece_at(position, source)
                if (actor and public_probe_piece(actor[0]) ==
                        public_probe_piece(event.piece)):
                    typed.append(move)
            if typed:
                candidates = typed
            if any(self._move_causes_bomb_detonation(position, move)
                   for move in candidates):
                return True
        return False

    def observe_unlogged_bomb_capture(self) -> list[str]:
        """Apply a Bomb capture whose attacker died before its move callback.

        Unity logs ``attacking bomb`` and the full public death animation, but
        an attacking piece killed by the blast never reaches HasMovedHandler
        and therefore emits no ``<piece> moves source --> target`` record. The
        target Bomb is already a known local piece, so retain every legal
        capture of such a Bomb and let the ordinary belief bound handle any
        genuine ambiguity.
        """
        next_positions = []
        notations = set()
        for position in self.positions:
            for move in self.engine.legal_moves(position):
                if move == "pass":
                    continue
                source, target, _separator = parse_engine_move(move)
                actor = upn_piece_at(position, source)
                victim = upn_piece_covering(position, target)
                if (actor is None or victim is None or victim[0] != "bomb"
                        or actor[1] == victim[1]):
                    continue
                applied = self.engine.apply(position, move)
                if self._bomb_count(applied) >= self._bomb_count(position):
                    continue
                next_positions.append(applied)
                notations.add(move)
        if not next_positions:
            raise RuntimeError(
                "public Bomb attack has no legal capture in any retained belief"
            )
        self.positions = self._bounded(next_positions)
        return sorted(notations)

    def observe_unlogged_parasite_possession(self, action: AppEvent) -> None:
        """Apply a public Parasite attack whose attacker never moves.

        Native Parasite possession destroys the attacking Parasite and changes
        the target's team in place.  Consequently Character.Move emits no
        ordinary ``parasite moves X --> Y`` record.  The online release
        diagnostic still publicly reports the visible source, destination and
        actor, which is sufficient to select the exact engine transition.
        """
        if (action.kind != "bot_action" or action.piece != "parasite" or
                not action.source or not action.target or
                action.source == action.target):
            raise ValueError(
                "unlogged Parasite possession requires a visible bot action"
            )
        self.observe_move(
            AppEvent(
                "move", "parasite", action.source, action.target, action.raw
            ),
            False,
        )

    def observe_unlogged_public_action(self, action: AppEvent) -> None:
        """Apply a visible non-Ghost action that dies before Move logging."""
        if (action.kind != "bot_action" or not action.piece or
                action.piece == "ghost" or not action.source or
                not action.target or action.source == action.target):
            raise ValueError(
                "unlogged public action requires a visible non-Ghost move"
            )
        self.observe_move(
            AppEvent(
                "move", action.piece, action.source, action.target, action.raw
            ),
            False,
        )

    def observe_unlogged_death_action(
        self,
        dead_pieces: Iterable[str],
        attack_target: str | None = None,
    ) -> list[str]:
        """Infer an action represented only by public death callbacks.

        Some native interactions destroy the moving model before
        ``HasMovedHandler`` can print coordinates. Online play still exposes
        one or more dying prefabs and the final turn barrier. Hidden collision
        victims do not always emit their own callback, so the observed types
        are a lower bound. Filter *all* legal transitions by that public
        signature and retain every matching belief; ambiguity remains an
        information set rather than becoming a controller guess.
        """
        observed = {
            public_probe_piece(piece)
            for piece in dead_pieces
            if piece
        }
        if not observed:
            raise ValueError("unlogged death inference needs a death signature")
        public_target = (
            public_probe_piece(attack_target) if attack_target else None
        )
        next_positions = []
        notations = set()
        for position in self.positions:
            before = Counter(
                public_probe_piece(piece)
                for piece, _color, _square, _state
                in parse_upn_pieces(position)
            )
            for move in self.engine.legal_moves(position):
                if move == "pass":
                    continue
                _source, target, _separator = parse_engine_move(move)
                victim = upn_piece_covering(position, target)
                if (public_target is not None and
                        (victim is None or
                         public_probe_piece(victim[0]) != public_target)):
                    continue
                applied = self.engine.apply(position, move)
                if upn_side(applied) == upn_side(position):
                    # A native ChangeTurn barrier cannot describe the first
                    # half of a Prince move or a continuing Checker jump.
                    continue
                after = Counter(
                    public_probe_piece(piece)
                    for piece, _color, _square, _state
                    in parse_upn_pieces(applied)
                )
                removed = set((before - after).elements())
                if not observed <= removed:
                    continue
                next_positions.append(applied)
                notations.add(move)
        if not next_positions:
            raise RuntimeError(
                "public death callbacks have no legal transition in any "
                "retained belief: " + ", ".join(sorted(observed))
            )
        self.positions = self._bounded(next_positions)
        return sorted(notations)

    def observe_continuation(self) -> None:
        """Condition ambiguous royal identities on a public continuing game."""
        ongoing = [
            position for position in self.positions
            if self.engine.legal_moves(position)
        ]
        if not ongoing:
            raise RuntimeError(
                "the app continued but every retained belief is terminal"
            )
        self.positions = self._bounded(ongoing)

    def observe_move(self, event: AppEvent, conceal_ghost_coordinates: bool) -> None:
        if event.kind != "move" or not event.piece:
            raise ValueError("observe_move requires a move event")
        next_positions = []
        for position in self.positions:
            legal = self.engine.legal_moves(position)
            if conceal_ghost_coordinates and event.piece == "ghost":
                candidates = []
                for move in legal:
                    if move == "pass":
                        continue
                    source, target, _ = parse_engine_move(move)
                    actor = upn_piece_at(position, source)
                    if not actor or actor[0] != "ghost" or actor[1] != "b":
                        continue
                    # An already-hidden Ghost conceals both endpoints.  After
                    # a public attack, however, its next quiet move has a
                    # publicly known source and only the destination becomes
                    # hidden.  Release diagnostics leak both coordinates, but
                    # use only what the opponent could see on the board.
                    # No public attack/reveal accompanied this callback, so
                    # the Ghost must have made a quiet move to an empty cell.
                    # Retaining captures here silently removed our material in
                    # impossible worlds and manufactured phantom mate threats.
                    if ((not actor[2] or source == event.source)
                            and upn_piece_covering(position, target) is None):
                        candidates.append(move)
            else:
                assert event.source and event.target
                candidates = [move for move in legal
                              if native_move_matches(position, move, event)]
                typed = []
                for move in candidates:
                    source, _, _ = parse_engine_move(move)
                    actor = upn_piece_at(position, source)
                    if (actor and public_probe_piece(actor[0]) ==
                            public_probe_piece(event.piece)):
                        typed.append(move)
                if typed:
                    candidates = typed
            for move in candidates:
                next_positions.append(self.engine.apply(position, move))
        if (event.piece == "ghost" and not conceal_ghost_coordinates and
                event.source and event.target):
            # A bounded belief sample must never discard public alternatives
            # merely because one sampled world already happened to match.
            # Rehydrate every still-hidden enemy Ghost at the newly public
            # source, validate the UPN, and apply the revealed attack. This is
            # triggered only by public information.
            for position in self.positions:
                fields = position.split(";")
                for index, field in enumerate(fields):
                    values = field.split(",")
                    if (len(values) < 9 or values[0] != "ghost" or
                            values[1] != "b" or values[8] != "0"):
                        continue
                    relocated = list(fields)
                    moved_values = list(values)
                    moved_values[2] = event.source
                    relocated[index] = ",".join(moved_values)
                    candidate_position = ";".join(relocated)
                    try:
                        legal = self.engine.legal_moves(candidate_position)
                    except ValueError:
                        continue
                    for move in legal:
                        if move_matches(move, event.source, event.target):
                            next_positions.append(
                                self.engine.apply(candidate_position, move))
        if not next_positions:
            privacy = "hidden Ghost" if conceal_ghost_coordinates else "visible"
            raise RuntimeError(f"{privacy} app move matches no belief: {event}")
        self.positions = self._bounded(next_positions)

    def observe_visible_ghost_destination(self, target: str) -> None:
        """Apply a Ghost move using only its newly public destination.

        The app renders the destination after ``Ghost:MakeVis``. Its release
        diagnostic also contains an origin that may have been private, so this
        update intentionally ignores the logged origin. If bounded sampling
        lost the real origin, rehydrate it over all adjacent legal origins.
        """
        next_positions: list[str] = []

        def append_matching(position: str) -> None:
            for move in self.engine.legal_moves(position):
                if move == "pass":
                    continue
                source, move_target, _separator = parse_engine_move(move)
                if move_target != target:
                    continue
                actor = upn_piece_at(position, source)
                if not actor or actor[0] != "ghost" or actor[1] != "b":
                    continue
                applied = self.engine.apply(position, move)
                revealed = upn_piece_at(applied, target)
                if revealed and revealed[0:2] == ("ghost", "b") and revealed[2]:
                    next_positions.append(applied)

        for position in self.positions:
            append_matching(position)

        target_file = ord(target[0]) - ord("a")
        target_rank = int(target[1:])
        origins = [
            f"{chr(ord('a') + target_file + dx)}{target_rank + dy}"
            for dx in (-1, 0, 1) for dy in (-1, 0, 1)
            if (dx or dy)
            and 0 <= target_file + dx < 8
            and 1 <= target_rank + dy <= 10
        ]
        for position in self.positions:
            fields = position.split(";")
            for index, field in enumerate(fields):
                values = field.split(",")
                if (len(values) < 9 or values[0] != "ghost" or
                        values[1] != "b" or values[8] != "0"):
                    continue
                for origin in origins:
                    relocated = list(fields)
                    moved_values = list(values)
                    moved_values[2] = origin
                    relocated[index] = ",".join(moved_values)
                    try:
                        append_matching(";".join(relocated))
                    except ValueError:
                        continue
        if not next_positions:
            raise RuntimeError(
                f"publicly visible Ghost destination {target} matches no belief"
            )
        self.positions = self._bounded(next_positions)

    def special_targets(self, piece: str, source: str,
                        separator: str) -> list[str]:
        """Return public candidate targets for a native stay-put action."""
        targets = set()
        for position in self.positions:
            for move in self.engine.legal_moves(position):
                if move == "pass":
                    continue
                move_source, move_target, marker = parse_engine_move(move)
                if move_source != source or marker != separator:
                    continue
                actor = upn_piece_at(position, move_source)
                if actor and actor[0] == piece:
                    targets.add(move_target)
        return sorted(targets, key=square_sort_key)

    def special_target_identities(
        self, piece: str, source: str, separator: str
    ) -> dict[str, set[str]]:
        """Return public target identities for each legal special action."""
        identities: dict[str, set[str]] = {}
        for position in self.positions:
            for move in self.engine.legal_moves(position):
                if move == "pass":
                    continue
                move_source, move_target, marker = parse_engine_move(move)
                if move_source != source or marker != separator:
                    continue
                actor = upn_piece_at(position, move_source)
                if not actor or actor[0] != piece:
                    continue
                target = upn_piece_covering(position, move_target)
                if target is not None:
                    identities.setdefault(move_target, set()).add(
                        public_probe_piece(target[0])
                    )
        return identities

    def choose(self, depth: int, nodes: int = 0,
               movetime_ms: int = 0,
               draw_moves: Sequence[str] = ()) -> tuple[str, str]:
        # The controller supplies factual public hypotheses but never scores,
        # filters, or overwrites a move. Root selection belongs to the native
        # information-set search so tactical sacrifices and Ghost risk are
        # evaluated by the same engine.
        legal_sets = [set(self.engine.legal_moves(position)) for position in self.positions]
        common = set.intersection(*legal_sets)
        if not common:
            raise RuntimeError("there is no move legal in every public belief")
        choice, _score, info = self.engine.search_beliefs(
            self.positions, depth, nodes, movetime_ms, draw_moves)
        if not choice:
            raise RuntimeError("Ultimate Fish returned no move for a nonterminal belief set")
        if choice not in common:
            raise RuntimeError(
                f"Ultimate Fish returned a move not legal in every belief: {choice}")
        return choice, info


def rewind_public_enemy_opening(
    current_enemy: Sequence[tuple[str, str]],
    opening_events: Sequence[AppEvent],
) -> list[list[tuple[str, str]]]:
    """Recover public initial deployments from a settled Ivory first turn.

    Onyx cannot reliably tap every opening cell before a fast opponent moves.
    Instead, the controller scans the now-stable public board and reverses only
    the opponent's public first-turn effects.  The resulting deployments are
    fed through ``initial_beliefs`` and the same journal is then replayed by
    ``play``.  That replay reconstructs captures, cooldowns, promotions and
    other lossless state in the native engine; this helper never guesses those
    state fields directly.

    Invisible Ghost coordinates remain absent.  If a Ghost became public, its
    current marker is removed here and the initial material total recreates the
    allowed hidden starting-square hypotheses before the public action filters
    them.
    """
    # None of these products is draft-placeable.  If present after the opening
    # turn, native replay will regenerate it from its Sludge/Devil/Angel actor.
    variants: list[list[tuple[str, str]]] = [[
        (piece, square) for piece, square in current_enemy
        if piece not in ("goop", "minion", "halo")
    ]]

    moves = [event for event in opening_events if event.kind == "move"]
    castle_companions: dict[int, tuple[int, str]] = {}
    companion_rooks: set[int] = set()
    for royal_index, royal in enumerate(moves):
        if (royal.piece not in ("king", "jester") or not royal.source or
                not royal.target or int(royal.source[1:]) != int(royal.target[1:]) or
                abs(ord(royal.source[0]) - ord(royal.target[0])) != 2):
            continue
        direction = 1 if royal.target[0] > royal.source[0] else -1
        rook_destination = (
            f"{chr(ord(royal.target[0]) - direction)}{royal.target[1:]}"
        )
        for rook_index, rook in enumerate(moves):
            if (rook_index == royal_index or rook.piece != "rook" or
                    not rook.source or rook.target != rook_destination or
                    int(rook.source[1:]) != int(royal.source[1:])):
                continue
            if ((ord(rook.source[0]) - ord(royal.source[0])) * direction > 2):
                castle_companions[royal_index] = (rook_index, rook.source)
                companion_rooks.add(rook_index)
                break

    for move_index in reversed(range(len(moves))):
        if move_index in companion_rooks:
            continue
        event = moves[move_index]
        if not event.piece or not event.source or not event.target:
            continue
        piece, source, target = event.piece, event.source, event.target

        # Generated Minion animations are synchronization records, while a
        # hidden Ghost has no public current or initial coordinate to reverse.
        if piece == "minion":
            continue
        if piece == "ghost":
            variants = [[
                (candidate, square) for candidate, square in variant
                if not (candidate == "ghost" and square == target)
            ] for variant in variants]
            continue

        if piece in ("devil", "fisherman", "sniper") and source == target:
            continue

        if (piece in ("king", "jester") and
                int(source[1:]) == int(target[1:]) and
                abs(ord(source[0]) - ord(target[0])) == 2):
            direction = 1 if target[0] > source[0] else -1
            rook_current = f"{chr(ord(target[0]) - direction)}{target[1:]}"
            recorded = castle_companions.get(move_index)
            rook_origins = (
                [recorded[1]] if recorded else [
                    f"{chr(ord(source[0]) + direction * distance)}{source[1:]}"
                    for distance in range(3, 8)
                    if 0 <= ord(source[0]) - ord("a") + direction * distance < 8
                ]
            )
            restored_variants = []
            for variant in variants:
                royal_indices = [
                    index for index, (candidate, square) in enumerate(variant)
                    if candidate == "king" and square == target
                ]
                rook_indices = [
                    index for index, (candidate, square) in enumerate(variant)
                    if candidate == "rook" and square == rook_current
                ]
                if not royal_indices or not rook_indices:
                    continue
                for royal_index in royal_indices:
                    for rook_index in rook_indices:
                        if royal_index == rook_index:
                            continue
                        base = [
                            item for index, item in enumerate(variant)
                            if index not in (royal_index, rook_index)
                        ]
                        occupied = {square for _candidate, square in base}
                        for rook_origin in rook_origins:
                            if rook_origin in occupied:
                                continue
                            restored_variants.append(
                                base + [("king", source), ("rook", rook_origin)]
                            )
            if not restored_variants:
                raise RuntimeError(
                    f"cannot rewind public castle {source}-{target}"
                )
            variants = restored_variants
            continue

        if piece == "angel":
            # Linking replaces the stationary Angel with a public Halo.
            variants = [variant + [("angel", source)] for variant in variants]
            continue

        if piece == "mage" and source == target:
            swapped: list[list[tuple[str, str]]] = []
            for variant in variants:
                mage_indices = [
                    index for index, (candidate, square) in enumerate(variant)
                    if candidate == "mage" and square != source
                ]
                partner_indices = [
                    index for index, (_candidate, square) in enumerate(variant)
                    if square == source
                ]
                for mage_index in mage_indices:
                    mage_target = variant[mage_index][1]
                    if partner_indices:
                        for partner_index in partner_indices:
                            restored = [
                                item for index, item in enumerate(variant)
                                if index not in (mage_index, partner_index)
                            ]
                            restored.extend((
                                ("mage", source),
                                (variant[partner_index][0], mage_target),
                            ))
                            swapped.append(restored)
                    else:
                        restored = [
                            item for index, item in enumerate(variant)
                            if index != mage_index
                        ]
                        restored.append(("mage", source))
                        swapped.append(restored)
                if not mage_indices:
                    swapped.append(variant + [("mage", source)])
            variants = swapped
            continue

        if piece in ("copycat", "copycatClone"):
            restored_variants = []
            source_mirror = (
                f"{chr(ord('h') - (ord(source[0]) - ord('a')))}{source[1:]}"
            )
            first, second = sorted(
                (source, source_mirror), key=square_sort_key
            )
            for variant in variants:
                restored = [
                    item for item in variant
                    if item[0] not in ("copycat", "copycatClone")
                ]
                restored.extend((("copycat", first), ("copycatClone", second)))
                restored_variants.append(restored)
            variants = restored_variants
            continue

        if piece == "giant":
            restored_variants = []
            source_file, source_rank = ord(source[0]) - ord("a"), int(source[1:])
            initial_anchors = []
            for file_offset in (0, -1):
                for rank_offset in (0, -1):
                    file_index = source_file + file_offset
                    rank = source_rank + rank_offset
                    if not (0 <= file_index < 7 and rank in (8, 9)):
                        continue
                    anchor = f"{chr(ord('a') + file_index)}{rank}"
                    if source in giant_footprint(anchor):
                        initial_anchors.append(anchor)
            for variant in variants:
                current_indices = [
                    index for index, (candidate, anchor) in enumerate(variant)
                    if candidate == "giant" and target in giant_footprint(anchor)
                ]
                bases = []
                if current_indices:
                    for current_index in current_indices:
                        bases.append([
                            item for index, item in enumerate(variant)
                            if index != current_index
                        ])
                else:
                    bases.append(list(variant))
                for base in bases:
                    for anchor in initial_anchors:
                        restored_variants.append(base + [("giant", anchor)])
            if not restored_variants:
                raise RuntimeError(
                    f"cannot recover initial Giant anchor from public source {source}"
                )
            variants = restored_variants
            continue

        public_piece = "king" if piece in ("king", "jester") else piece
        accepted = {public_piece}
        if public_piece == "checker":
            accepted.add("checkerKing")
        restored_variants = []
        for variant in variants:
            target_indices = [
                index for index, (candidate, square) in enumerate(variant)
                if square == target and candidate in accepted
            ]
            # A Bomb action can remove its actor during the public explosion,
            # so an absent target is still reversible from the logged source.
            if not target_indices:
                restored_variants.append(variant + [(public_piece, source)])
                continue
            for target_index in target_indices:
                restored = [
                    item for index, item in enumerate(variant)
                    if index != target_index
                ]
                restored.append((public_piece, source))
                restored_variants.append(restored)
        variants = restored_variants

    unique: dict[tuple[tuple[str, str], ...], list[tuple[str, str]]] = {}
    for variant in variants:
        ordered = sorted(variant, key=lambda item: (square_sort_key(item[1]), item[0]))
        unique[tuple(ordered)] = ordered
    if not unique:
        raise RuntimeError("opening rewind produced no public deployment")
    return list(unique.values())


def ranked_spawn_public(
    spawns: Sequence[AppEvent], local_ivory: bool,
) -> list[tuple[str, str]]:
    """Sanitize one public committed-group journal into local coordinates.

    The opening group also redraws the two fixed Kings; later callbacks contain
    only their newly locked group. Onyx sees native coordinates rotated 180
    degrees. Ghost coordinates were discarded by the parser and are therefore
    absent here; their count is recovered later from public material only.
    """
    public: list[tuple[str, str]] = []
    saw_spawn = False
    for event in spawns:
        if event.kind != "draft_piece_spawn" or not event.piece:
            continue
        saw_spawn = True
        if event.piece == "ghost" or event.source is None:
            continue
        x_text, y_text = event.source.split(":", 1)
        x, y = int(x_text), int(y_text)
        record = ModelPieceRecord(event.piece, 1, x, y)
        square = _online_square(record, not local_ivory)
        if int(square[1:]) < 8:
            continue
        if event.piece == "giant":
            public.extend(
                ("giant", cell)
                for cell in sorted(giant_footprint(square), key=square_sort_key)
            )
        else:
            public.append((public_probe_piece(event.piece), square))
    if not saw_spawn:
        raise RuntimeError("Ranked public spawn journal contains no piece records")
    return normalize_copycat_probes(
        sorted(public, key=lambda item: square_sort_key(item[1]))
    )


def ranked_spawn_local(
    spawns: Sequence[AppEvent], local_ivory: bool,
) -> list[tuple[str, str]]:
    """Recover the local player's exact newly locked Ranked group.

    Unlike the opponent sanitizer, local Ghost coordinates would be public to
    their owner (although the current release parser conservatively masks all
    Ghost spawn coordinates). Giant records are returned as their one native
    anchor rather than expanded footprint cells so DraftDeployment can reserve
    them directly.
    """
    local: list[tuple[str, str]] = []
    saw_spawn = False
    for event in spawns:
        if event.kind != "draft_piece_spawn" or not event.piece:
            continue
        saw_spawn = True
        if event.source is None:
            continue
        x_text, y_text = event.source.split(":", 1)
        record = ModelPieceRecord(event.piece, 0, int(x_text), int(y_text))
        square = _online_square(record, not local_ivory)
        if int(square[1:]) > 3:
            continue
        if event.piece == "copycatClone":
            continue
        piece = "checker" if event.piece == "checkerKing" else event.piece
        local.append((piece, square))
    if not saw_spawn:
        raise RuntimeError("Ranked local spawn journal contains no piece records")
    return sorted(local, key=lambda item: square_sort_key(item[1]))


def ranked_public_roster(
    probed_enemy: Sequence[tuple[str, str]], material: int,
) -> Counter[str]:
    """Recover a Ranked roster from a public locked deployment and counter.

    The first royal silhouette is the zero-point native King and every
    additional silhouette is a ten-point Jester. Invisible Ghosts are the only
    drafted pieces absent from the public board, so their exact count follows
    from the public material total without exposing any coordinate.
    """
    roster: Counter[str] = Counter()
    royal_count = 0
    giant_cells = {
        square for piece, square in probed_enemy if piece == "giant"
    }
    roster["giant"] += len(giant_anchors(giant_cells))
    for piece, _square in probed_enemy:
        if piece in ("king", "jester"):
            royal_count += 1
        elif piece == "giant":
            continue
        elif piece == "copycatClone":
            # One selectable CopyCat creates both public board models.
            continue
        else:
            roster[piece] += 1
    if royal_count < 1:
        raise RuntimeError("Ranked public deployment has no royal silhouette")
    roster["jester"] += royal_count - 1
    visible = sum(PIECE_COST[piece] * count for piece, count in roster.items())
    hidden = material - visible
    if hidden >= 0 and hidden % PIECE_COST["ghost"] == 0:
        ghost_count = hidden // PIECE_COST["ghost"]
    else:
        # Duplicate-heavy live armies can adjust the displayed total by at most
        # two points. Accept only a unique Ghost count in that measured bound.
        candidates = [
            count for count in range(8)
            if abs(visible + count * PIECE_COST["ghost"] - material) <= 2
        ]
        if len(candidates) != 1:
            raise RuntimeError(
                f"Ranked material {material} is inconsistent with visible cost {visible}"
            )
        ghost_count = candidates[0]
    roster["ghost"] += ghost_count
    return +roster


def initial_beliefs(own_team: Sequence[tuple[str, str]],
                    probed_enemy: Sequence[tuple[str, str]],
                    enemy_material: int | None, limit: int = 64,
                    side: str = "w",
                    piece_states: dict[
                        tuple[str, str], ModelPieceRecord
                    ] | None = None,
                    enemy_king_candidates: Iterable[str] | None = None,
                    ) -> list[str]:
    """Build public-information hypotheses for royals and hidden Ghosts."""
    collapsed: list[tuple[str, str]] = []
    giant_squares = {square for piece, square in probed_enemy if piece == "giant"}
    collapsed.extend(("giant", anchor) for anchor in giant_anchors(giant_squares))
    collapsed.extend((piece, square) for piece, square in probed_enemy if piece != "giant")

    royal_indices = [i for i, (piece, _) in enumerate(collapsed)
                     if piece in ("king", "jester")]
    allowed_king_squares = (
        None if enemy_king_candidates is None else set(enemy_king_candidates)
    )
    candidate_indices = [
        index for index in royal_indices
        if allowed_king_squares is None
        or collapsed[index][1] in allowed_king_squares
    ]
    royal_variants: list[list[tuple[str, str]]] = []
    if candidate_indices:
        for king_index in candidate_indices:
            variant = list(collapsed)
            for index in royal_indices:
                variant[index] = ("king" if index == king_index else "jester", variant[index][1])
            royal_variants.append(variant)
    else:
        if allowed_king_squares is None:
            raise RuntimeError("no visible enemy royal was found")
        raise RuntimeError(
            "no visible enemy royal matches the public first-pick chronology"
        )

    # Every additional royal silhouette is a Jester even though its concrete
    # identity is masked above.  Jesters still contribute ten public material
    # points, so include them before inferring hidden Ghost count.
    visible_cost = (sum(PIECE_COST[piece] for piece, _ in collapsed)
                    + max(0, len(royal_indices) - 1) * PIECE_COST["jester"])
    if enemy_material is None:
        ghost_count = 0
    else:
        hidden_cost = enemy_material - visible_cost
        if hidden_cost >= 0 and hidden_cost % PIECE_COST["ghost"] == 0:
            ghost_count = hidden_cost // PIECE_COST["ghost"]
        else:
            # Live 5.731 duplicate-heavy armies expose a bounded dynamic
            # adjustment in the public counter.  A six-Rook/Bishop/hidden-
            # Ghost army reads 100 although the static Character.value entries
            # total 102.  Reconcile only a unique count within those observed
            # two points; larger discrepancies remain hard failures.
            available = 24 - len(collapsed)
            candidates = [
                count for count in range(available + 1)
                if abs(visible_cost + count * PIECE_COST["ghost"] - enemy_material) <= 2
            ]
            if len(candidates) != 1:
                raise RuntimeError(
                    f"enemy material {enemy_material} is inconsistent with visible cost "
                    f"{visible_cost}"
                )
            ghost_count = candidates[0]

    occupied = {square for _, square in collapsed}
    for piece, anchor in collapsed:
        if piece != "giant":
            continue
        file_index = ord(anchor[0]) - ord("a")
        rank = int(anchor[1:])
        occupied.update(
            f"{chr(ord('a') + file_index + df)}{rank + dr}"
            for df in (0, 1) for dr in (0, 1)
        )
    ghost_squares = [f"{file}{rank}" for rank in (8, 9, 10) for file in "abcdefgh"
                     if f"{file}{rank}" not in occupied]
    combinations: Iterable[tuple[str, ...]]
    combinations = itertools.combinations(ghost_squares, ghost_count)
    positions = []
    for variant in royal_variants:
        for ghosts in combinations:
            enemy = variant + [("ghost", square) for square in ghosts]
            upn = make_upn(own_team, enemy, side)
            # ``visible`` is visibility to the Ghost's opponent, not to the
            # local phone user. Both armies' newly deployed Ghosts begin
            # hidden from the other side. Marking our own Ghost visible made
            # engine attack maps treat it as a shield even though a native
            # enemy Sniper shoots through it to the first visible target.
            upn = re.sub(r";ghost,([wb]),([a-h](?:10|[1-9]))(?=;|$)",
                         r";ghost,\1,\2,0,0,0,0,0,0,-1,1,-1,0", upn)
            if piece_states:
                fields = upn.split(";")
                for index, field in enumerate(fields[1:], 1):
                    values = field.split(",")
                    if len(values) != 3:
                        continue
                    record = piece_states.get((values[1], values[2]))
                    if record is None:
                        continue
                    piece, action, power, moved, visible = model_piece_upn_state(
                        values[0], record.action
                    )
                    fields[index] = (
                        f"{piece},{values[1]},{values[2]}"
                        f",{action},{record.cooldown},{record.freeze_count}"
                        f",{power},{int(moved)},{int(visible)},-1,1,-1,0"
                    )
                upn = ";".join(fields)
            positions.append(upn)
            if len(positions) >= limit * 8:
                break
        if len(positions) >= limit * 8:
            break
        # itertools iterators are exhausted; rebuild for the next royal branch.
        combinations = itertools.combinations(ghost_squares, ghost_count)
    return positions


def model_piece_upn_state(
    public_piece: str, native_action: int,
) -> tuple[str, int, int, bool, bool]:
    """Decode the polymorphic native ``Model_Piece.action`` byte.

    ``SimulatedPiece.SetAction`` treats one as ``pieceMoved``, but several
    subclasses replace that meaning entirely.  UPN deliberately stores those
    concepts in separate fields, so copying the byte into ``PieceState.action``
    corrupts ordinary castling/Pawn state and loses Berserker power.
    """
    if not 0 <= native_action <= 255:
        raise ValueError("Model_Piece action must fit in one byte")
    if public_piece == "berserker":
        # Native construction normalizes action 0/1 to powerLevel 1. UPN power
        # counts captures beyond the base 15-point level.
        return public_piece, 0, max(1, native_action) - 1, False, True
    if public_piece == "penguin":
        # SimulatedFreeze serializes its eight currently frozen directions.
        return public_piece, native_action, 0, False, True
    if public_piece == "ghost":
        return public_piece, 0, 0, False, native_action == 1
    if public_piece == "pawn":
        if native_action == 2:
            # Native SimulatedPawn retains an upgraded flag and delegates to
            # Queen movement. Ultimate Fish represents the same live piece as
            # a Queen after promotion.
            return "queen", 0, 0, True, True
        return public_piece, 0, 0, native_action == 1, True
    if public_piece == "checker":
        return (
            "checkerKing" if native_action == 1 else "checker",
            0, 0, False, True,
        )
    if public_piece in ("angel", "halo", "copycat", "copycatClone", "prince"):
        # These actions encode a linked square or an in-progress continuation,
        # not generic moved state. Initial OnStartGame/replay.initState records
        # have no such continuation; relationships are rebuilt from their
        # explicit companion records by Position::set_upn.
        return public_piece, 0, 0, False, True
    return public_piece, 0, 0, native_action == 1, True


def _online_square(record: ModelPieceRecord, flipped: bool) -> str:
    raw = f"{chr(ord('a') + record.x)}{record.y + 1}"
    if not flipped:
        return raw
    if record.piece != "giant":
        return rotate_square(raw)
    # Giant stores one lower-left 2x2 anchor. Rotating only that one cell would
    # turn it into the upper-right cell, so rotate the footprint and collapse
    # it back to Ultimate Fish's lower-left anchor.
    cells = [rotate_square(square) for square in giant_footprint(raw)]
    return min(cells, key=square_sort_key)


def infer_online_local_team(
    state: OnlineStartState, expected_team: Sequence[tuple[str, str]]
) -> int:
    """Identify the local network team by its exact known saved deployment."""
    expected = Counter(expected_team)
    matches = []
    for team in (0, 1):
        observed = Counter(
            (record.piece, _online_square(record, team == 1))
            for record in state.pieces if record.team == team
        )
        if observed == expected:
            matches.append(team)
    if len(matches) != 1:
        raise RuntimeError(
            "authoritative online state does not uniquely identify the local team"
        )
    return matches[0]


def sanitize_online_start(
    state: OnlineStartState,
    local_team: int,
    belief_limit: int = 64,
    side: str = "w",
) -> tuple[list[tuple[str, str]], list[str]]:
    """Apply the public-information boundary before creating engine UPN.

    Local records remain exact. Enemy Ghost coordinates are never copied;
    royal identities become identical silhouettes, and hidden Ghost beliefs
    are reconstructed from the public starting material limit.
    """
    if local_team not in (0, 1):
        raise ValueError("local online team must be 0 or 1")
    flipped = local_team == 1
    own: list[tuple[str, str]] = []
    public_enemy: list[tuple[str, str]] = []
    public_states: dict[tuple[str, str], ModelPieceRecord] = {}
    for record in state.pieces:
        square = _online_square(record, flipped)
        if record.team == local_team:
            own.append((record.piece, square))
            public_states[("w", square)] = record
            continue
        if record.piece == "ghost":
            continue
        public_piece = (
            "king" if record.piece in ("king", "jester") else record.piece
        )
        public_states[("b", square)] = record
        if public_piece == "giant":
            public_enemy.extend(
                ("giant", cell)
                for cell in sorted(giant_footprint(square), key=square_sort_key)
            )
        else:
            public_enemy.append((public_piece, square))

    own.sort(key=lambda item: square_sort_key(item[1]))
    if not any(piece == "king" for piece, _square in own):
        raise RuntimeError("authoritative online state has no local King")
    public_material = state.max_points
    if public_material is None:
        raise RuntimeError(
            "authoritative online state lacks the public material limit"
        )
    positions = initial_beliefs(
        own, public_enemy, public_material, belief_limit, side, public_states
    )
    return own, positions


def complete_ranked_start(state: OnlineStartState) -> OnlineStartState:
    """Add the two fixed Kings omitted from Ranked pick-group callbacks."""
    pieces = list(state.pieces)
    king_teams = {record.team for record in pieces if record.piece == "king"}
    if 0 not in king_teams:
        pieces.append(ModelPieceRecord("king", 0, 0, 0))
    if 1 not in king_teams:
        pieces.append(ModelPieceRecord("king", 1, 7, 9))
    return OnlineStartState(tuple(pieces), state.max_points or 100)


class PhoneGame:
    def __init__(self, adb: str, device: str, engine_path: str,
                 geometry: BoardGeometry, own_team: Sequence[tuple[str, str]],
                 depth: int, nodes: int, movetime_ms: int, belief_limit: int,
                 verbose: bool = False, configure_army: bool | None = None):
        self.geometry = geometry
        self.own_team = list(own_team)
        custom_team = tuple(self.own_team) != DEFAULT_OWN_TEAM
        self.configure_army = custom_team if configure_army is None else configure_army
        self.depth = depth
        self.nodes = nodes
        self.movetime_ms = movetime_ms
        self.belief_limit = belief_limit
        self.verbose = verbose
        self.adb = AdbDevice(adb, device)
        self.events = EventStream(adb, device)
        self.engine = EngineClient(engine_path)
        self.beliefs: BeliefSet | None = None
        self.perspective_flipped = False
        # Online positions are normalized to the local player's fixed camera,
        # so their tap coordinates never rotate. Local pass-and-play instead
        # flips the camera after every completed turn; its fixture runner sets
        # this independent input transform alongside event canonicalization.
        self.rotate_taps = False
        self.online_local_team: int | None = None
        self.draft_pots: dict[str, tuple[int, int]] = {}
        self.ranked_enemy_roster: Counter[str] = Counter()
        self.ranked_enemy_king_candidates: set[str] | None = None
        self.ranked_enemy_snapshots: list[tuple[tuple[str, str], ...]] = []
        self.ranked_local_points = 0
        self.ranked_opponent_points: int | None = None
        self.army_drag_offsets: dict[tuple[str, str], tuple[float, float]] = {}
        self.local_army_drag_offsets: dict[
            tuple[tuple[str, str], ...],
            dict[tuple[str, str], tuple[float, float]],
        ] = {}
        self.local_army_pot_slots: dict[
            tuple[int, tuple[tuple[str, str], ...]], dict[str, str]
        ] = {}
        self.army_pot_slots: dict[str, str] = {}
        self.army_verified_pre_ready = False

    def log(self, message: str) -> None:
        print(message, flush=True)

    def wait_screen(self, name: str, predicate, timeout: float = 15.0):
        deadline = time.monotonic() + timeout
        last = None
        while time.monotonic() < deadline:
            last = self.adb.screenshot()
            if predicate(last):
                return last
            time.sleep(0.15)
        raise TimeoutError(f"timed out waiting for {name}")

    def wait_connected_main(self, timeout: float = 60.0):
        """Wait for account data, restoring the existing login when needed.

        Several splash/menu states reuse the same cyan button artwork.  In
        particular, the disconnected ``Log In`` button used to satisfy a
        generic menu-color check and let navigation continue offline.  Require
        both the loaded profile card and the main Play control instead.
        """
        deadline = time.monotonic() + timeout
        next_login = 0.0
        next_maintenance = 0.0
        next_reconnect = 0.0
        next_foreground = time.monotonic() + 10.0
        login_failures = 0
        while time.monotonic() < deadline:
            image = self.adb.screenshot()
            if self._connected_main(image):
                return image
            maintenance = self._maintenance_ack_point(image)
            if maintenance:
                now = time.monotonic()
                if now >= next_maintenance:
                    self.log("server unavailable; dismissing maintenance notice")
                    self.adb.tap(*maintenance)
                    next_maintenance = now + 3.0
                    next_reconnect = now + 1.0
                time.sleep(0.25)
                continue
            login_failure = self._login_failure_ack_point(image)
            if login_failure:
                login_failures += 1
                self.log(
                    "dismissing failed Google login notice "
                    f"({login_failures}/2)"
                )
                self.adb.tap(*login_failure)
                if login_failures >= 2:
                    raise OnlineAuthenticationRequired(
                        "Google Play Games rejected the restored Chess Ultimate "
                        "session twice; sign in interactively before starting an "
                        "online run"
                    )
                time.sleep(0.5)
                continue
            decline = self._reconnect_decline_point(image)
            if decline:
                self.log("discarding stale reconnect prompt")
                self.adb.tap(*decline)
                time.sleep(0.5)
                continue
            acknowledgement = self._unlock_ack_point(image)
            if acknowledgement:
                self.log("dismissing unlock acknowledgement")
                self.adb.tap(*acknowledgement)
                time.sleep(0.5)
                continue
            login = self._login_point(image)
            now = time.monotonic()
            if login and now >= next_login:
                self.log("restoring the existing app login")
                self.adb.tap(*login)
                next_login = now + 2.0
                time.sleep(0.5)
                continue
            back = find_text_center(image, "BACK")
            if (back and back[0] > image.width * 0.65 and
                    back[1] < image.height * 0.20):
                self.log("returning from restored app submenu")
                self.adb.tap(*back)
                time.sleep(0.5)
                continue
            if now >= next_foreground:
                # Google Play Games/login can briefly leave its own activity in
                # front.  Relaunch only our package; never interact with the
                # foreign screen.
                self.adb.launch_app("com.JesseLugassy.ChessUltimate")
                next_foreground = now + 10.0
                time.sleep(0.5)
                continue
            if now >= next_reconnect:
                self.adb.tap(170, 2280)
                next_reconnect = now + 2.0
            time.sleep(0.20)
        raise TimeoutError("timed out waiting for connected main menu")

    def wait_local_main(self, timeout: float = 30.0):
        """Reach the main menu without requiring an online account session."""
        deadline = time.monotonic() + timeout
        next_foreground = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            image = self.adb.screenshot()
            if self._local_main(image):
                return image
            acknowledgement = (
                self._maintenance_ack_point(image)
                or self._login_failure_ack_point(image)
                or self._unlock_ack_point(image)
            )
            if acknowledgement:
                self.log("dismissing offline startup notice")
                self.adb.tap(*acknowledgement)
                time.sleep(0.5)
                continue
            play_offline = self._play_offline_point(image)
            if play_offline:
                self.log("entering Play Offline")
                self.adb.tap(*play_offline)
                time.sleep(0.7)
                continue
            back = find_text_center(image, "BACK")
            if (back and back[0] > image.width * 0.65 and
                    back[1] < image.height * 0.20):
                self.adb.tap(*back)
                time.sleep(0.5)
                continue
            now = time.monotonic()
            if now >= next_foreground:
                self.adb.launch_app("com.JesseLugassy.ChessUltimate")
                next_foreground = now + 10.0
            time.sleep(0.20)
        raise TimeoutError("timed out waiting for offline-capable main menu")

    @staticmethod
    def _connected_main(image) -> bool:
        """Recognize the online main menu after account data has populated."""
        return (
            PhoneGame._color_count(
                image, (0.02, 0.04, 0.30, 0.18), "purple") > 8000
            and PhoneGame._color_count(
                image, (0.20, 0.35, 0.80, 0.48), "yellow") > 20000
            # Shop, Settings, and other restored submenus can contain the same
            # profile-like purple art and large yellow purchase banners. Every
            # such page has a red Back control in this safe top-right crop; the
            # connected main menu does not.
            and PhoneGame._color_count(
                image, (0.66, 0.02, 0.99, 0.11), "red") < 3000
        )

    @staticmethod
    def _local_main(image) -> bool:
        """Recognize the main Play control even while the account is offline."""
        return (
            PhoneGame._color_count(
                image, (0.20, 0.35, 0.80, 0.48), "yellow"
            ) > 20_000
            and PhoneGame._color_count(
                image, (0.66, 0.02, 0.99, 0.11), "red"
            ) < 3_000
        )

    @staticmethod
    def _mode_menu_visible(image) -> bool:
        """Recognize the central Unranked button, not main-menu Shop."""
        return PhoneGame._color_count(
            image, (0.20, 0.31, 0.80, 0.40), "light_cyan") > 10000

    @staticmethod
    def _maintenance_ack_point(image) -> tuple[int, int] | None:
        """Return the harmless Okay control on the server-unavailable modal."""
        if PhoneGame._color_count(
                image, (0.02, 0.35, 0.98, 0.66), "red") > 200000:
            return image.width // 2, round(image.height * 0.59)
        return None

    @staticmethod
    def _reconnect_decline_point(image) -> tuple[int, int] | None:
        """Find the large red No button on the stale-game reconnect prompt."""
        import numpy as np

        if PhoneGame._maintenance_ack_point(image) is not None:
            return None
        rgb = np.asarray(image.convert("RGB"))
        red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
        mask = (red > 205) & (red > green * 1.35) & (red > blue * 1.15)
        for component in _components(mask):
            if not component:
                continue
            ys = [point[0] for point in component]
            xs = [point[1] for point in component]
            width = max(xs) - min(xs) + 1
            height = max(ys) - min(ys) + 1
            center_y = (min(ys) + max(ys)) // 2
            if (width > image.width * 0.40 and height > image.height * 0.04
                    and image.height * 0.48 < center_y < image.height * 0.68):
                return (min(xs) + max(xs)) // 2, center_y
        return None

    @staticmethod
    def _login_failure_ack_point(image) -> tuple[int, int] | None:
        """Find Okay on the login-error modal before generic red dialogs.

        The wide red error panel resembles the stale-game reconnect dialog,
        but tapping that component's center misses its inset pink Okay control
        and loops forever. The inset control has a unique settled size/color
        in the login screen; use that actual hit target instead of its label,
        whose outlined shipping font is unreliable under full-screen OCR.
        """
        import numpy as np

        rgb = np.asarray(image.convert("RGB"))
        red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
        pink = (
            (red > 235) & (green > 90) & (green < 180)
            & (blue > 90) & (blue < 180)
            & (np.abs(green.astype(int) - blue.astype(int)) < 30)
        )
        candidates = []
        for component in _components(pink):
            if not component:
                continue
            ys = [point[0] for point in component]
            xs = [point[1] for point in component]
            width = max(xs) - min(xs) + 1
            height = max(ys) - min(ys) + 1
            center_x = (min(xs) + max(xs)) // 2
            center_y = (min(ys) + max(ys)) // 2
            if (image.width * 0.18 < width < image.width * 0.30
                    and image.height * 0.02 < height < image.height * 0.05
                    and image.width * 0.38 < center_x < image.width * 0.62
                    and image.height * 0.50 < center_y < image.height * 0.60):
                candidates.append((len(component), center_x, center_y))
        if candidates:
            _area, center_x, center_y = max(candidates)
            return center_x, center_y
        return None

    @staticmethod
    def _unlock_ack_point(image) -> tuple[int, int] | None:
        """Find the cyan Okay button left by a successful key unlock."""
        import numpy as np

        rgb = np.asarray(image.convert("RGB"))
        red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
        mask = (red > 115) & (green > 170) & (blue > 185)
        for component in _components(mask):
            if not component:
                continue
            ys = [point[0] for point in component]
            xs = [point[1] for point in component]
            width = max(xs) - min(xs) + 1
            height = max(ys) - min(ys) + 1
            center_x = (min(xs) + max(xs)) // 2
            center_y = (min(ys) + max(ys)) // 2
            if (image.width * 0.20 < width < image.width * 0.35
                    and image.height * 0.02 < height < image.height * 0.06
                    and image.width * 0.35 < center_x < image.width * 0.65
                    and image.height * 0.48 < center_y < image.height * 0.60):
                # Several informational overlays contain a similarly-sized
                # cyan panel at the same height.  Only an actual OK/OKAY label
                # is safe to dismiss as an unlock acknowledgement.
                text = find_text_center(image, "OKAY") or find_text_center(
                    image, "OK", exact=True)
                if text and abs(text[0] - center_x) < width * 0.45:
                    return center_x, center_y
        return None

    @staticmethod
    def _login_point(image) -> tuple[int, int] | None:
        """Find the large cyan Log In control on the disconnected splash."""
        import numpy as np

        rgb = np.asarray(image.convert("RGB"))
        red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
        mask = (red > 115) & (green > 170) & (blue > 185)
        for component in _components(mask):
            if not component:
                continue
            ys = [point[0] for point in component]
            xs = [point[1] for point in component]
            width = max(xs) - min(xs) + 1
            height = max(ys) - min(ys) + 1
            center_x = (min(xs) + max(xs)) // 2
            center_y = (min(ys) + max(ys)) // 2
            if (width > image.width * 0.45 and height > image.height * 0.05
                    and image.width * 0.30 < center_x < image.width * 0.70
                    and image.height * 0.48 < center_y < image.height * 0.64):
                return center_x, center_y
        return None

    @staticmethod
    def _play_offline_point(image) -> tuple[int, int] | None:
        """Find Play Offline without confusing it with the adjacent Log In.

        Both disconnected-splash controls share the same wide cyan border, so
        component color alone cannot identify which one was found first.
        ``OFFLINE`` is large public UI text and gives an unambiguous safe tap.
        """
        point = find_text_center(image, "OFFLINE", exact=True)
        if point is None:
            return None
        if (image.width * 0.25 < point[0] < image.width * 0.75 and
                image.height * 0.45 < point[1] < image.height * 0.66):
            return point
        return None

    @staticmethod
    def _accept_point(image) -> tuple[int, int] | None:
        """Find the *settled* Accept button on the timed Game Found prompt.

        The modal scales upward over unrelated menu controls.  Require the
        final-position yellow Accept and its same-row red Decline partner so a
        partially animated button cannot pass a touch through to Puzzles.
        """
        import numpy as np

        rgb = np.asarray(image.convert("RGB"))
        red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
        masks = {
            "accept": (red > 220) & (green > 135) & (blue < 100),
            "decline": (red > 205) & (red > green * 1.35) & (red > blue * 1.15),
        }
        candidates: dict[str, list[tuple[int, int, int, int, int]]] = {
            "accept": [], "decline": [],
        }
        for kind, mask in masks.items():
            for component in _components(mask):
                if not component:
                    continue
                ys = [point[0] for point in component]
                xs = [point[1] for point in component]
                width = max(xs) - min(xs) + 1
                height = max(ys) - min(ys) + 1
                center_x = (min(xs) + max(xs)) // 2
                center_y = (min(ys) + max(ys)) // 2
                if (image.width * 0.20 < width < image.width * 0.34
                        and image.height * 0.025 < height < image.height * 0.070
                        and image.height * 0.55 < center_y < image.height * 0.64):
                    candidates[kind].append(
                        (len(component), center_x, center_y, width, height))
        paired = []
        for accept in candidates["accept"]:
            if not image.width * 0.58 < accept[1] < image.width * 0.72:
                continue
            if any(
                image.width * 0.27 < decline[1] < image.width * 0.49
                and abs(decline[2] - accept[2]) < image.height * 0.025
                for decline in candidates["decline"]
            ):
                paired.append(accept)
        if not paired:
            return None
        _, x, y, _, _ = max(paired)
        return x, y

    @staticmethod
    def _army_clear_confirmation_point(image) -> tuple[int, int] | None:
        """Find the large red Clear control in the builder confirmation."""
        import numpy as np

        rgb = np.asarray(image.convert("RGB"))
        # With all characters unlocked, the button's lower red rim can touch a
        # tall saved-army outline below it. Label only the modal band so those
        # unrelated pixels cannot merge into one implausibly tall component.
        crop_left = round(image.width * 0.18)
        # The confirmation shifts upward on taller/unlocked builder layouts.
        # Keep both observed layouts inside the crop while still excluding the
        # character grid below the modal.
        crop_top = round(image.height * 0.42)
        crop = rgb[
            crop_top:round(image.height * 0.60),
            crop_left:round(image.width * 0.82),
        ]
        red, green, blue = crop[:, :, 0], crop[:, :, 1], crop[:, :, 2]
        mask = (red > 205) & (red > green * 1.35) & (red > blue * 1.15)
        candidates = []
        for component in _components(mask):
            if not component:
                continue
            ys = [point[0] for point in component]
            xs = [point[1] for point in component]
            width = max(xs) - min(xs) + 1
            height = max(ys) - min(ys) + 1
            center_x = crop_left + (min(xs) + max(xs)) // 2
            center_y = crop_top + (min(ys) + max(ys)) // 2
            if (image.width * 0.42 < width < image.width * 0.62
                    and image.height * 0.045 < height < image.height * 0.085
                    and image.width * 0.40 < center_x < image.width * 0.60
                    and image.height * 0.44 < center_y < image.height * 0.59):
                candidates.append((len(component), center_x, center_y))
        if not candidates:
            return None
        _area, x, y = max(candidates)
        return x, y

    @staticmethod
    def _color_count(image, box: tuple[float, float, float, float], color: str) -> int:
        import numpy as np

        rgb = np.asarray(image.convert("RGB"))
        height, width = rgb.shape[:2]
        x0, y0, x1, y1 = box
        crop = rgb[round(y0 * height):round(y1 * height),
                   round(x0 * width):round(x1 * width)]
        red, green, blue = crop[:, :, 0], crop[:, :, 1], crop[:, :, 2]
        if color == "yellow":
            mask = (red > 220) & (green > 135) & (blue < 100)
        elif color == "red":
            mask = (red > 205) & (red > green * 1.35) & (red > blue * 1.15)
        elif color == "cyan":
            mask = (blue > 150) & (green > 130) & (blue > red * 1.08)
        elif color == "light_cyan":
            mask = (red > 115) & (green > 170) & (blue > 185)
        elif color == "purple":
            mask = ((blue > 100) & (red > 50) & (blue > green * 1.25)
                    & (red > green * 0.70))
        elif color == "green":
            mask = ((green > 180) & (green > red * 1.20)
                    & (green > blue * 1.20))
        else:
            raise ValueError(color)
        return int(mask.sum())

    @staticmethod
    def _opening_board_obscured(image) -> bool:
        """Detect the large white chat/emote bubble over enemy deployment."""
        import numpy as np

        rgb = np.asarray(image.convert("RGB"))
        height, width = rgb.shape[:2]
        crop = rgb[
            round(height * 0.205):round(height * 0.405),
            round(width * 0.065):round(width * 0.94),
        ]
        red, green, blue = crop[:, :, 0], crop[:, :, 1], crop[:, :, 2]
        white = (red > 235) & (green > 235) & (blue > 235)
        area_scale = width * height / (1080 * 2400)
        minimum_area = round(8000 * area_scale)
        minimum_width = round(85 * width / 1080)
        minimum_height = round(90 * height / 2400)
        for component in _components(white):
            if len(component) < minimum_area:
                continue
            ys = [point[0] for point in component]
            xs = [point[1] for point in component]
            if (max(xs) - min(xs) + 1 >= minimum_width and
                    max(ys) - min(ys) + 1 >= minimum_height):
                return True
        return False

    def start_very_hard_cpu(self) -> None:
        """Navigate from a cold launch to a settled Very Hard CPU board."""
        for build_attempt in itertools.count(1):
            try:
                was_configuring = self.configure_army
                self._start_very_hard_cpu_once()
                # The builder saves the verified army. Reuse it for later CPU
                # games in this run instead of clearing/rebuilding each time.
                if was_configuring:
                    self.configure_army = False
                return
            except ArmyPlacementRetry as exc:
                # A miscalibrated drop can replace a cheaper occupied piece.
                # The material delta then proves the saved army is corrupt,
                # but its reported destination also teaches the exact drag
                # correction. Restart the scene and reuse that correction.
                self.log(
                    f"{exc}; restarting a clean corrected CPU build "
                    f"(attempt {build_attempt + 1})"
                )
            except TimeoutError as exc:
                # Unity occasionally exposes artwork one animation frame
                # before the corresponding CPU-menu raycaster is live.
                self.log(
                    f"CPU navigation did not settle ({exc}); restarting "
                    f"(attempt {build_attempt + 1})"
                )

    def _start_very_hard_cpu_once(self) -> None:
        """Perform one CPU navigation/build attempt."""
        self.log("starting Very Hard CPU game")
        self.adb.restart_app("com.JesseLugassy.ChessUltimate")
        # CPU is available offline. This accepts an already-connected main
        # menu too, but avoids repeatedly invoking Google login when the Chess
        # Ultimate service is under maintenance.
        self.wait_local_main()
        # Mirror the manually calibrated sequence: let the connected main menu
        # settle, then send exactly one Play tap.  Repeated asynchronous taps
        # can arrive after the mode transition and activate another control.
        time.sleep(3.0)
        # Several of this Unity UI's button hitboxes are vertically below the
        # artwork.  These are the measured native hitbox coordinates, not the
        # apparent centers in a screenshot.
        self.adb.tap_sync(540, 1090)  # Play
        self.wait_screen("mode menu", self._mode_menu_visible)
        time.sleep(0.35)
        self.adb.tap_sync(540, 1480)  # Vs. CPU
        self.wait_screen(
            "difficulty menu",
            lambda image: self._color_count(image, (0.23, 0.51, 0.78, 0.61), "red") > 25000,
        )
        time.sleep(0.35)
        self.adb.tap_sync(540, 1570)  # Very Hard
        builder = self.wait_screen(
            "army builder",
            lambda image: self._color_count(image, (0.45, 0.80, 0.98, 0.96), "yellow") > 20000,
        )
        if self.configure_army or not self.army_verified_pre_ready:
            builder = self.wait_screen(
                "settled unlocked army builder",
                lambda image: image if len(detect_pot_centers(image)) == len(POT_SORT_ORDER)
                else False,
                8.0,
            )
            time.sleep(1.0)
            self._prepare_army_builder(builder)
        else:
            time.sleep(0.35)
        self.adb.tap_sync(820, 2120)  # Ready

        self.wait_for_board(20.0)
        self.events.drain()

    def start_local(
        self,
        player1: Sequence[tuple[str, str]],
        player2: Sequence[tuple[str, str]],
    ) -> None:
        """Build two exact armies and launch an offline pass-and-play game.

        Local Play is the native conformance laboratory: both turns are under
        controller control, no matchmaking state is touched, and each army is
        independently rebuilt and verified before the board loads.
        """
        teams = (list(player1), list(player2))
        for number, team in enumerate(teams, 1):
            if sum(PIECE_COST.get(piece, -1000) for piece, _ in team) != 100:
                raise ValueError(f"Local player {number} army must cost exactly 100 points")
            kings = [square for piece, square in team if piece == "king"]
            if len(kings) != 1 or not re.fullmatch(r"[a-h][1-3]", kings[0]):
                raise ValueError(
                    f"Local player {number} army must have one King in its home zone"
                )

        self.log("starting offline Local conformance game")
        self.events.reset_network_state()
        self.perspective_flipped = False
        self.adb.restart_app("com.JesseLugassy.ChessUltimate")
        self.wait_local_main()
        time.sleep(3.0)
        self.adb.tap_sync(540, 1090)  # Play
        self.wait_screen("mode menu", self._mode_menu_visible)
        time.sleep(0.45)
        self.adb.tap_sync(540, 1348)  # Local Play
        self.wait_screen(
            "Local game options",
            lambda image: self._color_count(
                image, (0.30, 0.86, 0.70, 0.98), "green"
            ) > 8000,
            20.0,
        )
        self.adb.tap_sync(540, 2225)  # Continue, default 100 / one minute per turn

        for number, team in enumerate(teams, 1):
            builder = self.wait_screen(
                f"Local player {number} army builder",
                lambda image: image
                if len(detect_pot_centers(image)) == len(POT_SORT_ORDER)
                else False,
                20.0,
            )
            time.sleep(0.7)
            self.own_team = list(team)
            self.configure_army = True
            self.army_verified_pre_ready = False
            # Drop bias is formation-specific: an oversized neighbour can
            # shift the safe in-cell point for the same piece/square between
            # Player 1 and Player 2. Cache it by exact formation so a clean
            # full-Local retry retains proven corrections without leaking
            # them into the independently configured opposing army.
            team_key = tuple(team)
            self.army_drag_offsets = dict(
                self.local_army_drag_offsets.get(team_key, {})
            )
            # Local's second builder presents the same pot artwork through a
            # different pass-and-play camera/layout.  Keep authoritative
            # ArmyMove identity corrections separate by player and formation;
            # a Player-2 Copycat/Giant pot swap must survive a full retry but
            # must never contaminate Player 1's freshly detected pot map.
            pot_key = (number, team_key)
            self.army_pot_slots = dict(
                self.local_army_pot_slots.get(pot_key, {})
            )
            try:
                self._configure_army_builder(
                    builder, force_clear=True, clear_point=(165, 315),
                    deployment=LOCAL_DEPLOYMENT_GEOMETRY,
                    points_box=(0.30, 0.15, 0.58, 0.22),
                )
            finally:
                self.local_army_drag_offsets[team_key] = dict(
                    self.army_drag_offsets
                )
                self.local_army_pot_slots[pot_key] = dict(
                    self.army_pot_slots
                )
            if number == 1:
                confirmation = None
                for ready_attempt in range(1, 5):
                    self.adb.tap_sync(820, 2120)  # Ready
                    try:
                        confirmation = self.wait_screen(
                            "Local player 1 confirmation",
                            lambda image: self._color_count(
                                image, (0.10, 0.50, 0.90, 0.61), "light_cyan"
                            ) > 50000,
                            3.0,
                        )
                        break
                    except TimeoutError:
                        current = self.adb.screenshot()
                        if len(detect_pot_centers(current)) != len(POT_SORT_ORDER):
                            # The builder has begun transitioning; do not tap
                            # the old Ready coordinate on the next screen.
                            confirmation = self.wait_screen(
                                "Local player 1 confirmation",
                                lambda image: self._color_count(
                                    image, (0.10, 0.50, 0.90, 0.61),
                                    "light_cyan",
                                ) > 50000,
                                7.0,
                            )
                            break
                        if self.verbose:
                            self.log(
                                "Local player 1 Ready tap was swallowed; "
                                f"retrying ({ready_attempt}/4)"
                            )
                        time.sleep(0.65)
                if confirmation is None:
                    raise TimeoutError(
                        "Local player 1 Ready did not open its confirmation"
                    )
                # The centered Okay hitbox is stable after the modal's cyan
                # panel has reached full size.
                del confirmation
                self.adb.tap_sync(540, 1365)
            else:
                board = None
                for ready_attempt in range(1, 5):
                    self.adb.tap_sync(820, 2120)  # Ready
                    try:
                        board = self.wait_for_board(6.0, local=True)
                        break
                    except TimeoutError:
                        current = self.adb.screenshot()
                        if len(detect_pot_centers(current)) != len(POT_SORT_ORDER):
                            # A tornado/load animation can outlast the short
                            # probe. Once the builder is gone, wait without
                            # risking a tap on the live board.
                            board = self.wait_for_board(24.0, local=True)
                            break
                        if self.verbose:
                            self.log(
                                "Local player 2 Ready tap was swallowed; "
                                f"retrying ({ready_attempt}/4)"
                            )
                        time.sleep(0.65)
                if board is None:
                    raise TimeoutError(
                        "Local player 2 Ready did not launch the game"
                    )

        self.events.drain()
        # Controller-side ownership remains Player 1; Player 2's deployment is
        # transformed by the fixture runner into the board's top orientation.
        self.own_team = list(player1)
        self.configure_army = False

    def wait_for_board(self, timeout: float = 180.0, local: bool = False):
        def board_ready(image) -> bool:
            home_ranks = (1, 2, 3, 8, 9, 10)
            red = detect_outline_squares(
                image, self.geometry, "red", home_ranks
            )
            blue = detect_outline_squares(
                image, self.geometry, "blue", home_ranks
            )
            if local:
                # Pass-and-play rotates its three material digit tiles with
                # the camera; the generic upright OCR can therefore miss a
                # fully live board. Both teams' outlines plus disappearance
                # of the 24-pot builder are a stronger Local-only boundary.
                return bool(
                    red and blue and
                    len(detect_pot_centers(image)) != len(POT_SORT_ORDER)
                )
            try:
                # Requiring both the board outlines and its public three-tile
                # material counter avoids false positives from red menu tags
                # crossing the board-coordinate sampling region. Local
                # pass-and-play can settle with either player's camera/color
                # on either home zone, so do not hard-code red at the top.
                read_material_counter(image)
                return bool(red or blue)
            except RuntimeError:
                return False
        return self.wait_screen("settled game board", board_ready, timeout)

    def _army_drag_result(
        self, timeout: float = 1.4, idle_timeout: float = 0.22
    ) -> tuple[tuple[int, int] | None, list[int], str | None]:
        """Collect native piece identity, destination, and point totals."""
        deadline = time.monotonic() + timeout
        coordinate = None
        points: list[int] = []
        identity = None
        received = False
        last_received = time.monotonic()
        while time.monotonic() < deadline:
            try:
                event = self.events.wait(
                    ("army_drop", "army_points", "army_piece", "selected"),
                    min(idle_timeout, deadline - time.monotonic()),
                )
            except TimeoutError:
                if received and time.monotonic() - last_received >= idle_timeout:
                    break
                continue
            received = True
            last_received = time.monotonic()
            if event.kind == "army_points":
                points.append(int(event.source or "-1"))
            elif event.kind in ("army_piece", "selected"):
                # Moving an already persisted model within the builder emits
                # its ordinary GetAvailableMoves selection callback, but not
                # the ArmyMove creation diagnostic used for pot-to-board
                # drags. Both records carry the same native prefab identity.
                identity = event.piece
            elif event.source:
                x_text, y_text = event.source.split(":", 1)
                coordinate = int(x_text), int(y_text)
        return coordinate, points, identity

    def _learn_army_pot_identity(self, expected: str, actual: str) -> bool:
        """Swap mislabeled native pot slots using authoritative ArmyMove."""
        if expected == actual:
            return False
        expected_slot = self.army_pot_slots.get(expected, expected)
        actual_slot = self.army_pot_slots.get(actual, actual)
        self.army_pot_slots[expected] = actual_slot
        self.army_pot_slots[actual] = expected_slot
        self.log(
            f"learned native pot swap: {expected} slot spawned {actual}; "
            "swapping assignments"
        )
        return True

    @staticmethod
    def _deployment_coordinate(square: str) -> tuple[int, int]:
        return ord(square[0]) - ord("a"), int(square[1:]) - 1

    @staticmethod
    def _deployment_square(coordinate: tuple[int, int]) -> str | None:
        x, y = coordinate
        if 0 <= x < 8 and 0 <= y < 3:
            return f"{chr(ord('a') + x)}{y + 1}"
        return None

    def _learn_army_drag_offset(
        self,
        piece: str,
        square: str,
        landed: tuple[int, int],
        deployment: DeploymentGeometry,
    ) -> tuple[int, int]:
        desired = self._deployment_coordinate(square)
        old_x, old_y = self.army_drag_offsets.get((piece, square), (0.0, 0.0))
        proposed = (
            old_x + (desired[0] - landed[0]) * deployment.cell_width,
            old_y + (landed[1] - desired[1]) * deployment.cell_height,
        )
        # A full-cell correction at an edge (h-file or rank one) points
        # outside the deployment collider and makes every subsequent drop get
        # rejected. Keep the learned bias inside the intended board while
        # still moving as far away as possible from an overlapping model.
        center_x, center_y = deployment.point(square)
        margin = 4.0
        learned = (
            min(deployment.right - margin - center_x,
                max(deployment.left + margin - center_x, proposed[0])),
            min(deployment.bottom - margin - center_y,
                max(deployment.top + margin - center_y, proposed[1])),
        )
        self.army_drag_offsets[(piece, square)] = learned
        if getattr(self, "verbose", False):
            self.log(
                f"learned native drag offset for {piece}@{square}: "
                f"{learned[0]:+.0f}px,{learned[1]:+.0f}px"
            )
        return learned

    def _correct_army_drop(
        self,
        piece: str,
        actual: tuple[int, int],
        desired_square: str,
        expected_points: int,
        deployment: DeploymentGeometry,
    ) -> None:
        """Move a misplaced, non-colliding deployment piece to its exact cell."""
        desired = self._deployment_coordinate(desired_square)
        current = actual
        correction_x = 0.0
        correction_y = 0.0
        for attempt in range(1, 7):
            current_square = self._deployment_square(current)
            if current_square is None:
                break
            source = deployment.point(current_square)
            base_target = deployment.point(desired_square)
            target = (
                round(base_target[0] + correction_x),
                round(base_target[1] + correction_y),
            )
            self.events.drain()
            self.adb.drag_sync(source, target, 260)
            landed, point_events, _identity = self._army_drag_result()
            final_points = point_events[-1] if point_events else expected_points
            if final_points != expected_points:
                raise RuntimeError(
                    f"correcting {piece}@{desired_square} changed native army "
                    f"total from {expected_points} to {final_points}"
                )
            if landed == desired:
                self.log(
                    f"corrected native {piece} drop "
                    f"{current_square}->{desired_square}"
                )
                return desired
            if landed is None or self._deployment_square(landed) is None:
                time.sleep(0.35)
                continue
            # The piece is now at ``landed``.  Compensate the next pointer
            # destination by the exact file/rank error Unity reported.
            correction_x += (desired[0] - landed[0]) * deployment.cell_width
            correction_y += (landed[1] - desired[1]) * deployment.cell_height
            current = landed
            if self.verbose:
                self.log(
                    f"native {piece} correction landed at "
                    f"{self._deployment_square(landed)}; retrying ({attempt}/6)"
                )
            time.sleep(0.35)
        raise TimeoutError(
            f"could not correct native {piece} placement to {desired_square}"
        )

    def _configure_army_builder(
        self,
        image,
        force_clear: bool = False,
        clear_point: tuple[int, int] = (150, 165),
        deployment: DeploymentGeometry = DeploymentGeometry(),
        points_box: tuple[float, float, float, float] = (0.27, 0.075, 0.58, 0.17),
    ) -> None:
        """Install ``--own-team`` into the native 8x3 saved-army builder."""
        if not self.configure_army:
            return
        if sum(PIECE_COST.get(piece, 0) for piece, _square in self.own_team) != 100:
            raise ValueError("a configured Unranked/CPU army must cost exactly 100 points")
        kings = [item for item in self.own_team if item[0] == "king"]
        if len(kings) != 1 or not re.fullmatch(r"[a-h][1-3]", kings[0][1]):
            raise ValueError(
                "the configured army must contain one King in the deployment zone")
        # The engine parser is the single source of truth for Giant/Copycat
        # footprints and all deployment overlaps.
        self.engine.set_position(make_upn(
            self.own_team, (("king", "a10"),), "w"))

        self.log("installing optimized 100-point army")
        points_are_zero = (
            self._color_count(image, points_box, "green") < 1000
            and self._color_count(image, points_box, "yellow") < 1000
        )
        if force_clear or not points_are_zero:
            confirmation = None
            for clear_attempt in range(1, 5):
                # The Clear artwork becomes visible before its Unity
                # raycaster finishes sliding into place. Retry the same safe
                # control rather than restarting an otherwise settled scene.
                self.adb.tap_sync(*clear_point)
                try:
                    confirmation = self.wait_screen(
                        "army clear confirmation",
                        self._army_clear_confirmation_point,
                        2.5,
                    )
                    break
                except TimeoutError:
                    if self.verbose:
                        self.log(
                            "builder Clear tap was swallowed; retrying "
                            f"({clear_attempt}/4)"
                        )
                    time.sleep(0.75)
            if confirmation is None:
                raise TimeoutError("timed out waiting for army clear confirmation")
            clear = self._army_clear_confirmation_point(confirmation)
            if clear is None:  # Defensive: wait_screen already required this.
                raise TimeoutError("army clear confirmation disappeared")
            self.adb.tap_sync(*clear)
            self.wait_screen(
                "zero-point army builder",
                lambda frame: (
                    self._color_count(frame, points_box, "green") < 1000
                    and self._color_count(frame, points_box, "yellow") < 1000
                ),
                4.0,
            )
        # Clear recreates the scene's pot character models. Their drag handlers
        # are live only after all 24 orange bodies have returned; remap those
        # fresh models rather than retaining pre-Clear centers.
        rebuilt = self.wait_screen(
            "rebuilt unlocked army pots",
            lambda frame: len(detect_pot_centers(frame)) == len(POT_SORT_ORDER),
            12.0,
        )
        pots = map_ranked_pots(rebuilt)
        time.sleep(1.0)
        king_square = kings[0][1]
        if king_square != "a1":
            # Clear leaves the required zero-cost King at a1. It is the only
            # deployment model at this point, so its source identity is exact
            # even on app builds which omit ArmyMove identity for board-to-
            # board drags.
            self.events.drain()
            self.adb.drag_sync(
                deployment.point("a1"), deployment.point(king_square), 260)
            landed, point_events, actual_piece = self._army_drag_result()
            if actual_piece not in (None, "king"):
                raise ArmyPlacementRetry(
                    f"native King relocation selected {actual_piece}")
            final_points = point_events[-1] if point_events else 0
            if final_points != 0:
                raise ArmyPlacementRetry(
                    f"native King relocation changed total to {final_points}")
            desired = self._deployment_coordinate(king_square)
            if landed != desired:
                if landed is None or self._deployment_square(landed) is None:
                    raise ArmyPlacementRetry(
                        f"native King did not relocate to {king_square}")
                landed = self._correct_army_drop(
                    "king", landed, king_square, 0, deployment)
            if landed != desired:
                raise ArmyPlacementRetry(
                    f"native King relocation missed {king_square}")
            self.log(f"relocated native King a1->{king_square}")
        pieces = [item for item in self.own_team if item[0] != "king"]
        # Place wide footprints first so later single-cell models cannot block
        # a Giant or the mirrored Copycat clone.
        pieces.sort(key=lambda item: 4 if item[0] == "giant"
                    else 2 if item[0] == "copycat" else 1, reverse=True)
        wide = [item for item in pieces if item[0] in ("giant", "copycat")]
        ordinary: dict[str, list[str]] = {}
        for piece, square in pieces:
            if piece not in ("giant", "copycat"):
                ordinary.setdefault(piece, []).append(square)
        interleaved: list[tuple[str, str]] = []
        while any(ordinary.values()):
            for piece in tuple(ordinary):
                if ordinary[piece]:
                    interleaved.append((piece, ordinary[piece].pop(0)))
        # Giants' oversized 3D raycasters extend over neighbouring logical
        # cells in the standalone builder.  Place single-cell characters first
        # and the already-validated non-overlapping Giant footprints last.
        # Multiple adjacent Giants are logically legal, but their oversized
        # builder raycasters make placement order observable. Approach from
        # left to right so an existing model cannot cover a later drop target.
        wide.sort(key=lambda item: square_sort_key(item[1]))
        pieces = interleaved + wide
        confirmed_points = 0
        native_confirmed: Counter[tuple[str, str]] = Counter()
        for piece, square in pieces:
            if piece not in pots:
                raise RuntimeError(f"no unlocked army-builder pot for {piece}")
            expected = confirmed_points + PIECE_COST[piece]
            for attempt in range(1, 9):
                self.events.drain()
                base_target = deployment.point(square)
                default_offset = (
                    (0.0, deployment.cell_height * 0.41)
                    if piece == "giant" else (0.0, 0.0)
                )
                offset_x, offset_y = self.army_drag_offsets.get(
                    (piece, square), default_offset
                )
                target = (
                    round(base_target[0] + offset_x),
                    round(base_target[1] + offset_y),
                )
                pot_slot = self.army_pot_slots.get(piece, piece)
                self.adb.drag_sync(pots[pot_slot], target, 220)
                landed, point_events, actual_piece = self._army_drag_result()
                actual_piece = resolve_army_drag_identity(
                    piece, actual_piece, confirmed_points, point_events
                )
                if (actual_piece is not None
                        and self._learn_army_pot_identity(piece, actual_piece)):
                    raise ArmyPlacementRetry(
                        f"native {piece} pot spawned {actual_piece}"
                    )
                observed = point_events[-1] if point_events else confirmed_points
                if observed == expected:
                    desired = self._deployment_coordinate(square)
                    if piece != "giant" and landed is None:
                        raise ArmyPlacementRetry(
                            f"native {piece}@{square} changed material without "
                            "reporting a destination"
                        )
                    if landed is not None and landed != desired:
                        actual_square = self._deployment_square(landed)
                        if actual_square is None:
                            raise RuntimeError(
                                f"native {piece} drop reported invalid deployment "
                                f"coordinate {landed[0]}:{landed[1]}"
                            )
                        self._learn_army_drag_offset(
                            piece, square, landed, deployment
                        )
                        landed = self._correct_army_drop(
                            piece, landed, square, expected, deployment
                        )
                    if piece != "giant":
                        if landed != desired:
                            raise ArmyPlacementRetry(
                                f"native {piece}@{square} did not finish on its "
                                "requested destination"
                            )
                        native_confirmed[(piece, square)] += 1
                    confirmed_points = observed
                    break
                if observed != confirmed_points:
                    if landed is not None and self._deployment_square(landed):
                        self._learn_army_drag_offset(
                            piece, square, landed, deployment
                        )
                    raise ArmyPlacementRetry(
                        f"native army total jumped from {confirmed_points} to {observed} "
                        f"while placing {piece}@{square}")
                if self.verbose:
                    self.log(
                        f"builder rejected {piece}@{square}; retrying ({attempt}/8)")
                time.sleep(0.45)
            else:
                raise TimeoutError(
                    f"builder rejected {piece}@{square} eight times at {confirmed_points}/100")
            time.sleep(0.25)
        if confirmed_points != 100:
            raise RuntimeError(f"native army total stopped at {confirmed_points}/100")
        complete = self.wait_screen(
            "complete optimized army",
            lambda frame: self._color_count(
                frame, points_box, "green") > 5000,
            8.0,
        )
        # The point counter alone cannot distinguish a correctly placed army
        # from a full-cost misplacement. Ordinary models must have emitted
        # their exact native coordinates, and Giant's otherwise-unlogged 2x2
        # anchor must agree with the settled screenshot.
        verify_builder_placement(
            complete, self.own_team, native_confirmed, deployment
        )
        self.army_verified_pre_ready = True
        self.log(
            "pre-Ready placement verified: "
            + " ".join(f"{piece}@{square}" for piece, square in self.own_team))

    def _verify_saved_army_builder(self, image) -> None:
        """Round-trip persisted models before queueing to prove identity/layout.

        The builder's 100-point label cannot distinguish equal-cost pieces or
        a full-cost misplacement. Moving each ordinary model to a known-empty
        scratch cell and immediately back produces authoritative ``ArmyMove``
        identity and coordinate diagnostics without changing the saved army.
        Giant anchors are verified from their distinctive 2x2 footprints.
        """
        if sum(PIECE_COST.get(piece, 0) for piece, _ in self.own_team) != 100:
            raise ValueError("a saved Unranked army must cost exactly 100 points")
        self.engine.set_position(make_upn(
            self.own_team, (("king", "a10"),), "w"))
        if self._color_count(image, (0.27, 0.075, 0.58, 0.17), "green") <= 5000:
            raise ArmyPlacementRetry("saved army does not show a complete 100-point total")

        occupied: set[str] = set()
        for piece, square in self.own_team:
            if piece == "giant":
                occupied.update(giant_footprint(square))
            elif piece == "copycat":
                occupied.add(square)
                occupied.add(DraftDeployment._mirror(square))
            else:
                occupied.add(square)
        scratch_cells = [
            f"{file}{rank}" for rank in (3, 2, 1) for file in "abcdefgh"
            if f"{file}{rank}" not in occupied
        ]
        if not scratch_cells:
            raise ArmyPlacementRetry("saved army has no empty verification scratch cell")

        deployment = DeploymentGeometry()
        native_confirmed: Counter[tuple[str, str]] = Counter()
        ordinary = [
            item for item in self.own_team if item[0] not in ("king", "giant")
        ]
        for piece, square in ordinary:
            scratch = next((candidate for candidate in scratch_cells
                            if piece != "copycat" or
                            DraftDeployment._mirror(candidate) not in occupied), None)
            if scratch is None:
                raise ArmyPlacementRetry(
                    f"saved {piece}@{square} has no safe verification scratch pair")
            desired_coordinate = self._deployment_coordinate(square)
            scratch_coordinate = self._deployment_coordinate(scratch)
            verified = False
            for attempt in range(1, 5):
                self.events.drain()
                self.adb.drag_sync(
                    deployment.point(square), deployment.point(scratch), 240)
                landed, points, outward_piece = self._army_drag_result(1.4, 0.55)
                if landed is None:
                    if self.verbose:
                        self.log(
                            f"saved {piece}@{square} verification drag was swallowed; "
                            f"retrying ({attempt}/4)")
                    continue
                landed_square = self._deployment_square(landed)
                if landed_square is None:
                    raise ArmyPlacementRetry(
                        f"saved {piece}@{square} landed outside deployment at {landed}")
                if outward_piece != piece:
                    # Do not drag an unexpected model onto the still-occupied
                    # expected source. The caller will clear and rebuild this
                    # now-mutated saved army before it can queue.
                    raise ArmyPlacementRetry(
                        f"saved {piece}@{square} selected "
                        f"{outward_piece or 'unknown'}")

                # Restore first, even when identity/destination evidence is
                # wrong, so a failed coordinate audit does not leave a moved
                # copy of the correctly identified model.
                self.events.drain()
                self.adb.drag_sync(
                    deployment.point(landed_square), deployment.point(square), 240)
                restored, restored_points, inward_piece = self._army_drag_result(1.4, 0.55)
                final_points = (restored_points or points)
                if final_points and final_points[-1] != 100:
                    raise ArmyPlacementRetry(
                        f"saved {piece}@{square} round trip changed total to "
                        f"{final_points[-1]}")
                if (landed == scratch_coordinate and restored == desired_coordinate
                        and outward_piece == piece and inward_piece == piece):
                    native_confirmed[(piece, square)] += 1
                    verified = True
                    break
                if self.verbose:
                    self.log(
                        f"saved {piece}@{square} round trip reported "
                        f"{outward_piece}@{landed_square} -> "
                        f"{inward_piece}@{self._deployment_square(restored) if restored else '?'}; "
                        f"retrying ({attempt}/4)")
                time.sleep(0.25)
            if not verified:
                raise ArmyPlacementRetry(
                    f"saved {piece}@{square} identity/coordinate audit failed")

        settled = self.adb.screenshot()
        verify_builder_placement(settled, self.own_team, native_confirmed)
        self.army_verified_pre_ready = True
        self.log(
            "pre-Ready saved army verified: "
            + " ".join(f"{piece}@{square}" for piece, square in self.own_team))

    def _prepare_army_builder(self, image) -> None:
        """Verify an automatic saved roster or rebuild the requested roster.

        ``configure_army is None`` means automatic selection, not permission to
        trust any complete 100-point save. Local conformance can legitimately
        replace that save between CPU/online runs, so verify identities while
        the builder is still editable and request a clean rebuild on failure.
        """
        if self.configure_army:
            self._configure_army_builder(image)
            return
        if self.army_verified_pre_ready:
            return
        try:
            self._verify_saved_army_builder(image)
        except (ArmyPlacementRetry, TimeoutError):
            self.configure_army = True
            raise

    def start_unranked(self) -> None:
        """Enter the explicitly authorized blind-pick Unranked queue."""
        self.log("starting Unranked game")
        self.events.reset_network_state()
        match_already_found = False
        cached_accept: tuple[int, int] | None = None
        for navigation_attempt in itertools.count(1):
            self.adb.restart_app("com.JesseLugassy.ChessUltimate")
            # Matchmaking is an unattended workflow and the backend
            # occasionally posts a short maintenance window.  Keep restoring
            # the existing session long enough to ride through it.
            self.wait_connected_main(900.0)
            time.sleep(3.0)
            pre_navigation = self.adb.screenshot()
            cached_accept = self._accept_point(pre_navigation)
            if cached_accept:
                match_already_found = True
                break
            self.adb.tap_sync(540, 1090)  # Play exactly once
            try:
                navigation = self.wait_screen(
                    "mode menu",
                    lambda image: (
                        self._mode_menu_visible(image)
                        or self._accept_point(image) is not None
                    ),
                )
            except TimeoutError:
                self.log(
                    f"Play navigation was swallowed; retrying "
                    f"(attempt {navigation_attempt})"
                )
                continue
            cached_accept = self._accept_point(navigation)
            if cached_accept:
                match_already_found = True
                break
            # The mode artwork and OCR become stable before Unity finishes
            # sliding its raycast targets.  Direct calibration shows that the
            # final Unranked hitbox is reliable after this short settle window.
            time.sleep(3.0)
            try:
                self.adb.tap_sync(540, 850)  # settled Unranked hitbox
                self.wait_screen(
                    "army builder",
                    lambda image: self._color_count(
                        image, (0.45, 0.80, 0.98, 0.96), "yellow") > 20000,
                )
            except TimeoutError:
                self.log(
                    f"Unranked navigation was swallowed; retrying "
                    f"(attempt {navigation_attempt})"
                )
                continue
            # The Ready artwork/raycaster appears before ArmyBuilder.Init has
            # finished instantiating the unlocked roster.  Tapping in that
            # interval invokes ArmyBuilder.Ready but does not dispatch the
            # network JoinQueue call.  All 24 pots are a native, observable
            # completion barrier; allow one final animation beat after it.
            try:
                builder = self.wait_screen(
                    "settled unlocked army builder",
                    lambda image: len(detect_pot_centers(image)) == len(POT_SORT_ORDER),
                    8.0,
                )
            except TimeoutError:
                last = self.adb.screenshot()
                self.log(
                    "army builder did not settle "
                    f"({len(detect_pot_centers(last))} pots visible); "
                    f"retrying navigation (attempt {navigation_attempt})"
                )
                continue
            # Empty pot bodies are created before addressable character models
            # and Ready's queue callback finish initializing.  The user-visible
            # roster needs a separate settle window beyond the 24-pot barrier.
            time.sleep(3.0)
            if self.configure_army:
                try:
                    self._prepare_army_builder(builder)
                    # The native builder persists this exact roster. Queue
                    # retries and subsequent games in the same run must reuse
                    # it instead of clearing and rebuilding it again.
                    self.configure_army = False
                except ArmyPlacementRetry as exc:
                    self.log(f"{exc}; restarting a clean corrected build")
                    continue
                except TimeoutError as exc:
                    self.log(f"army scene did not settle ({exc}); retrying navigation")
                    continue
            elif not self.army_verified_pre_ready:
                try:
                    self._prepare_army_builder(builder)
                except (ArmyPlacementRetry, TimeoutError) as exc:
                    # A persisted roster that cannot prove its exact identity
                    # and coordinates must never reach matchmaking. Rebuild it
                    # locally on the next navigation attempt.
                    self.log(
                        f"saved army verification failed ({exc}); "
                        "rebuilding before queue"
                    )
                    continue
            self.events.drain()
            self.adb.tap_sync(817, 2160)
            # Fresh accounts default to an often-empty newcomer pool.  Wait
            # until the server has joined it before requesting the ordinary
            # full pool, otherwise the tap can land while the builder closes.
            try:
                initial_queue_state = self.events.wait(
                    ("queue_joined", "match_found"), 5.0)
            except TimeoutError:
                initial_queue_state = None
                time.sleep(0.5)
            if initial_queue_state and initial_queue_state.kind == "match_found":
                # A populated newcomer pool can match immediately.  Preserve
                # this time-critical event instead of discarding it while
                # waiting only for the ordinary queue acknowledgement.
                match_already_found = True
                break
            # The queue acknowledgement precedes the top banner's slide-in.
            # Wait for its Join Full Queue raycast to reach the calibrated
            # position before issuing the one upgrade tap.
            time.sleep(3.0)
            # Discard the newcomer-pool acknowledgement before issuing the
            # distinct full-pool request so only the latter can confirm it.
            self.events.drain()
            self.adb.tap_sync(850, 290)  # Join Full Queue
            try:
                queue_state = self.events.wait(
                    ("queue_joined", "match_found"), 8.0)
                match_already_found = queue_state.kind == "match_found"
                break
            except TimeoutError:
                # The backend occasionally displays Game Found without a
                # corresponding queue acknowledgement reaching logcat.  Check
                # for that exact safe prompt before restarting the app, or an
                # immediate match would be discarded as a failed join.
                prompt = self.adb.screenshot()
                cached_accept = self._accept_point(prompt)
                if cached_accept:
                    match_already_found = True
                    break
                # Ready already produced the initial queue acknowledgement.
                # The full-pool upgrade does not reliably emit a second one,
                # while the UI can still show that the queue is joined. Never
                # discard that active queue (or rebuild its army) merely
                # because the redundant diagnostic was absent.
                visible = find_text_center(prompt, "QUEUE") is not None
                self.log(
                    "full queue joined via visible state; waiting for match"
                    if visible else
                    "full-queue acknowledgement absent; preserving active queue"
                )
                break

        # GameFound is dispatched before the ten-second prompt finishes its
        # opening animation.  OCR just the one prominent word and tap its live
        # bounding box; this also survives aspect-ratio and layout changes.
        while not match_already_found:
            try:
                self.events.wait("match_found", 3.0)
                break
            except TimeoutError:
                prompt = self.adb.screenshot()
                cached_accept = self._accept_point(prompt)
                if cached_accept:
                    match_already_found = True
                    break
                self.log("still waiting in the full Unranked queue")
        # Synchronize on the app's own completed-board lifecycle rather than
        # OCR.  In particular, a material counter such as 099 can be visible
        # while an OCR retry loop consumes the first action clock.  An accepted
        # opponent can still fail to accept their side of the offer; in that
        # case the app silently returns us to the same queue.  Keep consuming
        # subsequent GameFound offers instead of turning that normal requeue
        # into a fatal 90-second board timeout.
        self._wait_for_accepted_match(
            cached_accept, ("board_loaded",), "Unranked board")
        # Let the short camera easing finish before tapping the visual a1
        # perspective probe.
        time.sleep(0.45)
        self.events.drain()

    @staticmethod
    def _draft_control(image, label: str) -> tuple[int, int] | None:
        """Locate a prominent draft action button in the lower half."""
        if label.upper() == "LOCK":
            import numpy as np

            rgb = np.asarray(image.convert("RGB"))
            red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
            mask = (
                (green > 145)
                & (green > red * 1.20)
                & (green > blue * 1.10)
            )
            candidates = []
            for component in _components(mask):
                if len(component) < 800:
                    continue
                ys = [point[0] for point in component]
                xs = [point[1] for point in component]
                width = max(xs) - min(xs) + 1
                height = max(ys) - min(ys) + 1
                center = (
                    (min(xs) + max(xs)) // 2,
                    (min(ys) + max(ys)) // 2,
                )
                if (image.width * 0.12 < width < image.width * 0.28
                        and image.height * 0.05 < height < image.height * 0.12
                        and image.width * 0.15 < center[0] < image.width * 0.50
                        and image.height * 0.78 < center[1] < image.height * 0.93):
                    candidates.append((len(component), center))
            if candidates:
                return max(candidates)[1]
        top = round(image.height * 0.58)
        crop = image.crop((0, top, image.width, image.height))
        point = find_text_center(crop, label)
        return (point[0], point[1] + top) if point else None

    def _tap_draft_control(self, labels: Sequence[str], timeout: float = 4.0) -> str:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            image = self.adb.screenshot()
            for label in labels:
                point = self._draft_control(image, label)
                if point:
                    self.adb.tap_sync(*point)
                    return label
            time.sleep(0.12)
        raise TimeoutError("draft control was not visible: " + "/".join(labels))

    def _accept_match_prompt(self, cached: tuple[int, int] | None = None) -> None:
        self.log("match found; accepting")
        # GameFound is authoritative and this final Android hitbox has been
        # repeatedly measured.  Wait out the modal animation, then use a
        # blocking tap rather than consuming its ten-second lifetime on OCR.
        time.sleep(1.5)
        point = cached or (687, 1425)
        self.log(f"accept control at {point[0]},{point[1]}")
        # If the prompt was discovered visually, its corresponding GameFound
        # record may have arrived while its scale animation was settling.  It
        # describes this offer, not a second one, and is safe to discard before
        # the acceptance tap.  A board cannot start until this tap is sent.
        self.events.drain()
        self.adb.tap_sync(*point)
        # The prompt can remain visible until the opponent accepts; the caller
        # requires OnStartGame/Board.LoadBoard as confirmation.

    def _wait_for_accepted_match(
        self,
        initial_accept: tuple[int, int] | None,
        started_kinds: Sequence[str],
        description: str,
    ) -> AppEvent:
        """Accept offers until one opponent also accepts and a scene starts.

        Chess Ultimate automatically requeues a player when the other side of
        an accepted offer expires.  There is no dependable terminal callback
        for that transition, but the next offer always has a fresh GameFound
        event.  Periodic visual checks cover the release build's occasional
        missing log record without repeatedly tapping the still-settling offer.
        """
        self._accept_match_prompt(initial_accept)
        accepted_at = time.monotonic()
        last_status = accepted_at
        awaited = set(started_kinds)
        awaited.update(("match_found", "queue_joined", "game_over", "out_of_time"))

        while True:
            try:
                event = self.events.wait(awaited, 3.0)
            except TimeoutError:
                now = time.monotonic()
                # The timed modal lives for ten seconds.  Before then a visible
                # Accept can still be the just-accepted offer's closing UI;
                # after then it must be a new offer (or a missed first tap).
                if now - accepted_at >= 10.0:
                    point = self._accept_point(self.adb.screenshot())
                    if point:
                        self.log("new visible match offer; accepting")
                        self._accept_match_prompt(point)
                        accepted_at = time.monotonic()
                        last_status = accepted_at
                        continue
                if now - last_status >= 15.0:
                    self.log(
                        f"waiting for opponent acceptance or the next {description} offer")
                    last_status = now
                continue

            if event.kind in started_kinds:
                return event
            if event.kind == "match_found":
                self.log("previous opponent did not start; accepting next match offer")
                self._accept_match_prompt()
                accepted_at = time.monotonic()
                last_status = accepted_at
            elif event.kind == "queue_joined":
                self.log("match offer expired; matchmaking requeued automatically")
            else:
                self.log(
                    f"match ended before {description} ({event.kind}); awaiting requeue")

    def start_ranked(self) -> None:
        """Enter the explicitly authorized Ranked draft queue."""
        self.log("starting Ranked game")
        self.events.reset_network_state()
        self.ranked_enemy_roster.clear()
        self.ranked_enemy_king_candidates = None
        self.ranked_enemy_snapshots.clear()
        self.ranked_local_points = 0
        self.ranked_opponent_points = None
        cached_accept = None
        match_found = False
        for attempt in itertools.count(1):
            self.adb.restart_app("com.JesseLugassy.ChessUltimate")
            self.wait_connected_main(900.0)
            time.sleep(3.0)
            image = self.adb.screenshot()
            if self._ranked_queue_visible(image):
                self.log("resuming the visibly joined Ranked queue")
            else:
                self.adb.tap_sync(540, 1090)  # Play exactly once
                try:
                    self.wait_screen("mode menu", self._mode_menu_visible)
                except TimeoutError:
                    self.log(
                        f"Play navigation was swallowed; retrying (attempt {attempt})")
                    continue
                # Artwork becomes visible before Unity finishes sliding the
                # mode buttons and their raycasters into place.
                time.sleep(3.0)
                self.events.drain()
                # The purple Ranked-info collider overlaps the visual center
                # of the yellow button. Its lower-left interior is the proven
                # queue hitbox and avoids that collider.
                self.adb.tap_sync(350, 1060)
                try:
                    state = self.events.wait(("queue_joined", "match_found"), 8.0)
                    match_found = state.kind == "match_found"
                except TimeoutError:
                    image = self.adb.screenshot()
                    cached_accept = self._accept_point(image)
                    match_found = cached_accept is not None
                    if (not match_found and
                            not self._ranked_queue_visible(image)):
                        if find_text_center(image, "CHARACTERS"):
                            raise RuntimeError(
                                "Ranked rejected the verified full character roster")
                        self.log(
                            f"Ranked queue did not acknowledge; retrying "
                            f"(attempt {attempt})")
                        continue
            if match_found:
                break

            missing_queue_checks = 0
            while not match_found:
                try:
                    self.events.wait("match_found", 3.0)
                    match_found = True
                    break
                except TimeoutError:
                    image = self.adb.screenshot()
                    cached_accept = self._accept_point(image)
                    if cached_accept:
                        match_found = True
                        break
                    if self._ranked_queue_visible(image):
                        missing_queue_checks = 0
                        self.log("still waiting in the visibly joined Ranked queue")
                        continue
                    missing_queue_checks += 1
                    if missing_queue_checks < 3:
                        self.log("Ranked queue banner unsettled; confirming state")
                        continue
                    self.log("Ranked queue ended without a match; rejoining")
                    break
            if match_found:
                break
        self._wait_for_accepted_match(
            cached_accept, ("start_game", "draft_board_loaded"), "Ranked draft")

        # Character addressables finish asynchronously after OnStartGame.  Do
        # not begin the clock-sensitive draft until all 24 source-mapped pots
        # are visible and stable.
        draft_image = self.wait_screen(
            "full Ranked character pot layout",
            lambda image: len(detect_pot_centers(image)) == len(POT_SORT_ORDER),
            20.0,
        )
        self.draft_pots = map_ranked_pots(draft_image)
        self.log("calibrated all 24 Ranked character pots")

    def _ranked_is_ivory(self) -> bool:
        """Determine whether phase zero belongs to the local player."""
        # IsCharacterUsable logs the exact native turn predicate for a touched
        # local pot. Bind that answer to the number of opening Bans already
        # completed: addressable/pot calibration can outlast a fast remote
        # phase zero. This is more reliable than overlapping comic pot art and
        # the short-lived Ban speech bubble.
        probe = "ninja"
        self.events.drain()
        self.adb.tap_sync(*self.draft_pots[probe])
        try:
            event = self.events.wait(
                ("draft_turn_probe", "game_over", "out_of_time"), 1.5
            )
        except TimeoutError:
            event = None
        if event is not None:
            if event.kind != "draft_turn_probe":
                raise RuntimeError(
                    f"Ranked game ended during side detection: {event.kind}"
                )
            completed_bans = (
                event.payload if isinstance(event.payload, int)
                else self.events.ranked_bans_completed()
            )
            if completed_bans not in (0, 1):
                raise RuntimeError(
                    "Ranked side detection occurred after both opening bans"
                )
            if event.source == "local":
                return completed_bans % 2 == 0
            if event.source == "opponent":
                return completed_bans % 2 == 1
            raise RuntimeError("native Ranked turn probe had no side")

        # Compatibility fallback for a release without the predicate log.
        completed_bans = self.events.ranked_bans_completed()
        if completed_bans not in (0, 1):
            raise RuntimeError("Ranked side detection occurred after both opening bans")
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline:
            if self._pot_ban_control(self.adb.screenshot(), probe):
                return completed_bans % 2 == 0
            time.sleep(0.12)
        return completed_bans % 2 == 1

    @classmethod
    def _ranked_queue_visible(cls, image) -> bool:
        """Detect the persistent top Ranked queue banner without comic OCR."""
        return (
            cls._color_count(image, (0.12, 0.045, 0.88, 0.14), "cyan") > 40_000
            and cls._color_count(image, (0.35, 0.105, 0.65, 0.15), "red") > 2_000
        )

    @staticmethod
    def _visual_ban_control(
        image, source: tuple[int, int]
    ) -> tuple[int, int] | None:
        """Find the native red BAN speech bubble above a selected draft pot.

        OCR is fragile on the outlined comic font and consumed an opening
        Ranked clock. The action bubble is the only broad saturated-red
        component immediately above the selected pot; red character artwork
        is narrower and centered on or below the pot.
        """
        import numpy as np

        rgb = np.asarray(image.convert("RGB"))
        x0 = max(0, round(source[0] - image.width * 0.26))
        x1 = min(image.width, round(source[0] + image.width * 0.26))
        y0 = max(0, round(source[1] - image.height * 0.16))
        y1 = min(image.height, round(source[1] + image.height * 0.03))
        crop = rgb[y0:y1, x0:x1]
        red, green, blue = crop[:, :, 0], crop[:, :, 1], crop[:, :, 2]
        mask = (red > 180) & (red > green * 1.35) & (red > blue * 1.15)
        candidates: list[tuple[int, int, int]] = []
        for component in _components(mask):
            if not component:
                continue
            ys = [point[0] for point in component]
            xs = [point[1] for point in component]
            width = max(xs) - min(xs) + 1
            height = max(ys) - min(ys) + 1
            center = (
                x0 + (min(xs) + max(xs)) // 2,
                y0 + (min(ys) + max(ys)) // 2,
            )
            rise = source[1] - center[1]
            if (image.width * 0.10 < width < image.width * 0.25
                    and image.height * 0.02 < height < image.height * 0.07
                    and abs(center[0] - source[0]) < image.width * 0.18
                    and image.height * 0.005 < rise < image.height * 0.14):
                candidates.append((len(component), center[0], center[1]))
        if not candidates:
            return None
        _pixels, x, y = max(candidates)
        return x, y

    @staticmethod
    def _fixed_ban_control(image) -> tuple[int, int] | None:
        """Find the actionable red Ban button in the lower-left inspector.

        The blue top-row ``BAN`` is only the current phase label. Selected
        pots also render a red speech bubble, but that bubble is not the
        reliable pointer target in the shipping Android layout.
        """
        import numpy as np

        rgb = np.asarray(image.convert("RGB"))
        red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
        mask = (
            (red > 180) & (red > green * 1.35) & (red > blue * 1.15)
        )
        candidates: list[tuple[int, int, int]] = []
        for component in _components(mask):
            if len(component) < 6000:
                continue
            ys = [point[0] for point in component]
            xs = [point[1] for point in component]
            width = max(xs) - min(xs) + 1
            height = max(ys) - min(ys) + 1
            center = (
                (min(xs) + max(xs)) // 2,
                (min(ys) + max(ys)) // 2,
            )
            if (image.width * 0.18 < width < image.width * 0.32
                    and image.height * 0.025 < height < image.height * 0.055
                    and image.width * 0.05 < center[0] < image.width * 0.25
                    and image.height * 0.77 < center[1] < image.height * 0.86):
                candidates.append((len(component), center[0], center[1]))
        if not candidates:
            return None
        _pixels, x, y = max(candidates)
        return x, y

    def _pot_ban_control(self, image, piece: str) -> tuple[int, int] | None:
        # The selected-pot speech bubble and the top phase label are both
        # visual-only in the shipping build. Waiting a frame for the fixed
        # inspector control is cheaper and safer than tapping either decoy.
        del piece
        return self._fixed_ban_control(image)

    def _ban_ranked_piece(self, piece: str, timeout: float = 3.0) -> None:
        # A fixed Ban button can remain visible for the pot touched during
        # side detection. Never click that stale control before selecting the
        # engine's actual choice. Wait for IsCharacterUsable's native turn
        # predicate so Unity has processed the new pot before confirming it.
        self.events.drain()
        self.adb.tap_sync(*self.draft_pots[piece])
        try:
            selected = self.events.wait(
                ("draft_turn_probe", "game_over", "out_of_time"),
                min(1.0, timeout),
            )
            if selected.kind != "draft_turn_probe":
                raise RuntimeError(
                    f"Ranked game ended while selecting {piece}: {selected.kind}"
                )
            if selected.source != "local":
                raise RuntimeError(
                    f"Ranked {piece} pot was processed outside the local Ban turn"
                )
        except TimeoutError:
            # Compatibility fallback for builds without the predicate log.
            # A short UI frame still prevents confirming the previously
            # selected calibration pot.
            time.sleep(0.20)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            point = self._pot_ban_control(self.adb.screenshot(), piece)
            if point:
                self.adb.tap_sync(*point)
                return
            time.sleep(0.10)
        raise TimeoutError(f"native Ban action did not appear for {piece}")

    def _place_ranked_piece(
        self, piece: str, square: str, local_ivory: bool,
    ) -> RankedPlacementResult:
        if piece not in self.draft_pots:
            raise RuntimeError(f"no calibrated Ranked pot for {piece}")
        # LoadBoardDraft retains the live Board model, but the pick camera
        # projects the local home zone into a compact 8x3 rectangle. Drag to
        # that visible grid rather than the later settled-gameplay geometry;
        # relying on Giant.getClosestIntersection to rescue off-board drops is
        # both slow and dependent on whichever model collider is nearest.
        deployment_geometry = RANKED_DEPLOYMENT_GEOMETRY.scaled(
            self.geometry.width, self.geometry.height,
        )
        def collect_drag_events():
            deadline = time.monotonic() + 1.5
            last_received = time.monotonic()
            received = False
            landed: tuple[int, int] | None = None
            identity: str | None = None
            points: list[int] = []
            while time.monotonic() < deadline:
                try:
                    event = self.events.wait(
                        ("army_drop", "army_piece", "army_points",
                         "game_over", "out_of_time"),
                        min(0.18, deadline - time.monotonic()),
                    )
                except TimeoutError:
                    if received and time.monotonic() - last_received >= 0.18:
                        break
                    continue
                if event.kind in ("game_over", "out_of_time"):
                    raise RuntimeError(
                        f"Ranked draft ended while placing {piece}: {event.kind}"
                    )
                received = True
                last_received = time.monotonic()
                if event.kind == "army_drop" and event.source:
                    x_text, y_text = event.source.split(":", 1)
                    landed = int(x_text), int(y_text)
                elif event.kind == "army_piece":
                    identity = event.piece
                elif event.kind == "army_points":
                    player1 = int(event.source or -1)
                    player2 = int(event.target or -1)
                    points.append(player1 if local_ivory else player2)
            return landed, identity, points

        def local_square(coordinate: tuple[int, int]) -> str:
            record = ModelPieceRecord(piece, 1, coordinate[0], coordinate[1])
            return _online_square(record, not local_ivory)

        source = self.draft_pots[piece]
        # PointerDown grabs the pot model and PointerUp on the square performs
        # the native placement. Giant and other tall models can cover one
        # pixel of an otherwise legal target cell. Sweep several points inside
        # that same Square collider before rejecting the logical cell: a human
        # drag succeeds for exactly the same reason, and repeatedly using one
        # intercepted pixel can otherwise consume the whole Ranked clock.
        base_target = deployment_geometry.drop_point(piece, square)
        center = deployment_geometry.point(square)
        offsets = (
            (0.0, 0.0),
            (0.0, -0.30),
            (-0.28, -0.12),
            (0.28, -0.12),
            (0.0, 0.28),
        )
        landed_coordinate = None
        actual_piece = None
        point_history: list[int] = []
        for target_attempt, (x_offset, y_offset) in enumerate(offsets, 1):
            target = base_target if target_attempt == 1 else (
                round(center[0] + x_offset * deployment_geometry.cell_width),
                round(center[1] + y_offset * deployment_geometry.cell_height),
            )
            if target_attempt > 1:
                self.events.drain()
            self.adb.drag_sync(source, target, 180)
            attempt_landed, attempt_piece, attempt_points = collect_drag_events()
            landed_coordinate = attempt_landed
            actual_piece = attempt_piece
            point_history.extend(attempt_points)
            if (attempt_points
                    and attempt_points[-1] != self.ranked_local_points):
                break
            if self.verbose and target_attempt < len(offsets):
                self.log(
                    f"Ranked {piece}@{square} target pixel was intercepted; "
                    f"trying in-cell point {target_attempt + 1}/{len(offsets)}"
                )

        if landed_coordinate is None:
            if piece == "giant" and point_history:
                # Giant's recovered override logs no coordinate pair. Its 2x2
                # footprint is confirmed by the point transition. ArmyMove
                # first needs a raycastable anchor cell; Giant then centers its
                # model by averaging that cell and its three neighbors.
                actual_square = square
            elif not point_history or point_history[-1] == self.ranked_local_points:
                raise TimeoutError(
                    f"Ranked {piece} drag produced no native landing coordinate"
                )
            else:
                raise RuntimeError(
                    f"Ranked {piece} changed material without a landing coordinate"
                )
        else:
            actual_square = local_square(landed_coordinate)

        final_points = (
            point_history[-1] if point_history else self.ranked_local_points
        )
        # CopyCat's override can share Giant's blank ArmyMove diagnostic. Use
        # the transient removal/addition point sequence to retain the requested
        # identity in that one ambiguous case. Comparing only the net increase
        # is insufficient when a Giant replaces another pending model.
        actual_piece = resolve_army_drag_identity(
            piece, actual_piece, self.ranked_local_points, point_history
        )
        if actual_piece is None:
            actual_piece = piece

        # Do not merely accept a public ordinary-piece misdrop into the logical
        # planner. Move that already placed model to the requested cell using
        # the same native-feedback correction loop proven by the Local builder.
        # This preserves the intended complete packing and prevents a stray
        # Prince/Queen from making a later Giant group appear impossible.
        if (landed_coordinate is not None and actual_piece == piece
                and final_points != self.ranked_local_points
                and piece != "giant" and actual_square != square):
            desired = self._deployment_coordinate(square)
            current_square = actual_square
            correction_x = 0.0
            correction_y = 0.0
            for attempt in range(1, 7):
                source = deployment_geometry.point(current_square)
                base_target = deployment_geometry.point(square)
                target = (
                    round(base_target[0] + correction_x),
                    round(base_target[1] + correction_y),
                )
                self.events.drain()
                self.adb.drag_sync(source, target, 260)
                corrected, corrected_piece, corrected_points = collect_drag_events()
                point_history.extend(corrected_points)
                if corrected_piece not in (None, piece):
                    raise RuntimeError(
                        f"correcting Ranked {piece}@{square} selected "
                        f"{corrected_piece}"
                    )
                if corrected_points and corrected_points[-1] != final_points:
                    raise RuntimeError(
                        f"correcting Ranked {piece}@{square} changed material "
                        f"from {final_points} to {corrected_points[-1]}"
                    )
                if corrected is None:
                    time.sleep(0.12)
                    continue
                corrected_square = local_square(corrected)
                if corrected_square == square:
                    self.log(
                        f"corrected native Ranked {piece} drop "
                        f"{actual_square}->{square}"
                    )
                    actual_square = square
                    break
                current = self._deployment_coordinate(corrected_square)
                correction_x += (desired[0] - current[0]) * deployment_geometry.cell_width
                correction_y += (current[1] - desired[1]) * deployment_geometry.cell_height
                current_square = corrected_square
                if self.verbose:
                    self.log(
                        f"Ranked {piece} correction landed at {corrected_square}; "
                        f"retrying ({attempt}/6)"
                    )
                time.sleep(0.12)
            else:
                raise TimeoutError(
                    f"could not correct Ranked {piece} placement to {square}"
                )
        return RankedPlacementResult(
            actual_square, actual_piece, final_points, tuple(point_history)
        )

    def _ranked_committed_points(self, local_ivory: bool) -> tuple[int, int]:
        """Read cumulative draft totals from the non-consuming native ledger.

        The phase-transition wait may consume every queued ``army_points``
        event before this method runs. EventStream journals those public totals
        independently, just like Ranked spawn groups, so observation never
        depends on queue timing.
        """
        deadline = time.monotonic() + 1.5
        previous_opponent = self.ranked_opponent_points or 0
        while time.monotonic() < deadline:
            snapshotter = getattr(self.events, "points_snapshot", None)
            points = snapshotter() if snapshotter is not None else None
            if points is not None:
                player1, player2 = points
                local = player1 if local_ivory else player2
                opponent = player2 if local_ivory else player1
                if opponent > previous_opponent:
                    return local, opponent
            time.sleep(0.01)
        raise TimeoutError("Ranked committed group produced no new native points")

    def _observe_ranked_local_pick(
        self,
        local_ivory: bool,
        choices: Sequence[str],
        previous_generation: int,
        locked_team: Sequence[tuple[str, str]],
        assumed_group: Sequence[tuple[str, str]],
    ) -> list[tuple[str, str]]:
        """Read back one immutable local group from its public spawn journal.

        Giant's ArmyMove override does not report a landing coordinate, so a
        successful point transition alone cannot prove its anchor. Once Lock
        commits, OnSpawnPieceGroup publishes the exact local coordinates. Use
        that record before planning the next window instead of carrying an
        assumed Giant footprint forward.
        """
        deadline = time.monotonic() + 2.5
        last_error: RuntimeError | None = None
        while time.monotonic() < deadline:
            generation, complete, spawns = self.events.ranked_spawn_snapshot()
            if generation <= previous_generation or not complete:
                time.sleep(0.02)
                continue
            try:
                public = ranked_spawn_local(spawns, local_ivory)
                # The opening callback redraws the fixed King; later callbacks
                # normally contain only the newly committed group. Remove any
                # already locked records by exact multiplicity in either case.
                additions = Counter(public)
                additions.subtract(Counter(locked_team))
                additions = +additions
                group = sorted(additions.elements(), key=lambda item: square_sort_key(item[1]))
                # The parser masks every Ghost coordinate before knowing which
                # side owns it. A local Ghost is genuinely known to us, so
                # retain only that already-verified assumed anchor while every
                # unmasked model comes from the authoritative spawn journal.
                missing_ghosts = (Counter(choices)["ghost"]
                                  - Counter(piece for piece, _square in group)["ghost"])
                if missing_ghosts > 0:
                    group.extend(
                        item for item in assumed_group
                        if item[0] == "ghost"
                    )
                    group = sorted(group, key=lambda item: square_sort_key(item[1]))
                if Counter(piece for piece, _square in group) != Counter(choices):
                    raise RuntimeError(
                        "local Ranked spawn roster differs from locked choices: "
                        + " ".join(f"{piece}@{square}" for piece, square in group)
                    )
                return group
            except RuntimeError as exc:
                last_error = exc
            time.sleep(0.02)
        raise RuntimeError(
            "local Ranked spawn journal did not reconcile the locked group"
        ) from last_error

    def _observe_ranked_opponent_pick(self, local_ivory: bool) -> list[str]:
        """Apply the newly public, locked opponent group to draft knowledge."""
        generation, _complete, _spawns = self.events.ranked_spawn_snapshot()
        _local_points, opponent_points = self._ranked_committed_points(local_ivory)
        self.ranked_opponent_points = opponent_points
        last_error: RuntimeError | None = None
        group_public: list[tuple[str, str]] | None = None
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            current_generation, complete, spawns = self.events.ranked_spawn_snapshot()
            if current_generation != generation:
                raise RuntimeError("Ranked spawn journal advanced across pick phases")
            snapshotter = getattr(self.events, "points_snapshot", None)
            points = snapshotter() if snapshotter is not None else None
            if points is not None:
                player1, player2 = points
                current_opponent = player2 if local_ivory else player1
                if current_opponent >= opponent_points:
                    opponent_points = current_opponent
                    self.ranked_opponent_points = current_opponent
            if complete:
                try:
                    candidate_group = ranked_spawn_public(spawns, local_ivory)
                    candidate_public = (
                        list(self.ranked_enemy_snapshots[-1]) + candidate_group
                        if self.ranked_enemy_snapshots else candidate_group
                    )
                    # OnSanityCheck can precede the delayed Square.Spawn
                    # coroutines by hundreds of milliseconds. Do not accept a
                    # syntactically complete marker until the records reconcile
                    # exactly with the already-public cumulative material.
                    ranked_public_roster(candidate_public, opponent_points)
                    if (not self.ranked_enemy_snapshots and not any(
                            piece == "king" for piece, _square in candidate_public)):
                        raise RuntimeError(
                            "opening Ranked journal has not spawned its royal yet"
                        )
                    group_public = candidate_group
                except RuntimeError as exc:
                    last_error = exc
                if group_public is not None:
                    break
            time.sleep(0.02)

        # The stock 5.73 app emits the public Square.Spawn group journal. Do not
        # fall back to taps: character pots overlap the shallow deployment and
        # a missed Queen can otherwise look like a material-inferred Ghost.
        if group_public is None:
            raise RuntimeError(
                "public Ranked spawn journal did not complete"
            ) from last_error

        if self.ranked_enemy_snapshots:
            previous = list(self.ranked_enemy_snapshots[-1])
            duplicate_cells = Counter(previous) & Counter(group_public)
            if duplicate_cells:
                raise RuntimeError(
                    "a newly committed Ranked group overlaps locked public cells: "
                    + ", ".join(
                        f"{piece}@{square}" for (piece, square) in duplicate_cells
                    )
                )
            public = previous + group_public
        else:
            public = group_public
        snapshot = tuple(sorted(public, key=lambda item: square_sort_key(item[1])))

        royal_squares = {
            square for piece, square in public if piece in ("king", "jester")
        }
        if self.ranked_enemy_king_candidates is None:
            if not royal_squares:
                raise RuntimeError(
                    "the first public Ranked group contains no royal silhouette"
                )
            # Any royal in the first revealed group may be the fixed King.
            # Royals first appearing in later immutable groups are known
            # Jesters and must never expand this candidate set.
            self.ranked_enemy_king_candidates = set(royal_squares)
        elif not self.ranked_enemy_king_candidates <= royal_squares:
            raise RuntimeError(
                "a first-group royal candidate vanished from a locked Ranked setup"
            )

        roster = ranked_public_roster(public, opponent_points)
        removed_types = self.ranked_enemy_roster - roster
        if removed_types:
            raise RuntimeError(
                "the public Ranked roster lost locked character types: "
                + ", ".join(removed_types.elements())
            )
        additions = roster - self.ranked_enemy_roster
        order = {piece: index for index, piece in enumerate(POT_SORT_ORDER)}
        choices = sorted(additions.elements(), key=lambda piece: order[piece])
        for piece in choices:
            self.engine.draft_choose(piece)
        self.engine.draft_commit()
        self.ranked_enemy_roster = roster
        self.ranked_enemy_snapshots.append(snapshot)
        self.log(
            f"public opponent group: {opponent_points} points; "
            + (" ".join(choices) if choices else "no new visible-cost pieces")
        )
        return choices

    def _observe_ranked_opponent_ban(
        self,
        previous_count: int,
    ) -> str:
        """Recover the exact ban from its non-consuming native journal.

        Unity does not consistently order the texture diagnostic and the
        OnBanCharacter method frame. Waiting for the latter can therefore
        consume and discard the former. The journal is authoritative across
        either ordering; never substitute an overlapping-pot image guess.
        """
        deadline = time.monotonic() + 1.5
        event: AppEvent | None = None
        while time.monotonic() < deadline:
            snapshot = self.events.ranked_ban_snapshot()
            if len(snapshot) > previous_count:
                if len(snapshot) != previous_count + 1:
                    raise RuntimeError(
                        "multiple native Ban identities appeared in one phase"
                    )
                return snapshot[-1]
            try:
                event = self.events.wait(
                    ("draft_ban_piece", "game_over", "out_of_time"),
                    min(0.10, deadline - time.monotonic()),
                )
            except TimeoutError:
                continue
            if event.kind != "draft_ban_piece" or not event.piece:
                raise RuntimeError(
                    f"Ranked draft stopped while reading opponent ban: {event.kind}"
                )
            # EventStream journals before queueing. Test/replay streams may
            # expose only the event itself, which is equally exact.
            snapshot = self.events.ranked_ban_snapshot()
            if len(snapshot) > previous_count:
                return snapshot[-1]
            return event.piece
        raise RuntimeError("native opponent Ban produced no public piece identity")

    def _wait_ranked_opponent_phase(
        self, event_kind: str, action: str, phase: int,
    ) -> AppEvent:
        """Wait for native remote completion without inventing a shorter clock."""
        last_status = time.monotonic()
        while True:
            try:
                return self.events.wait(
                    (event_kind, "game_over", "out_of_time"), 5.0,
                )
            except TimeoutError:
                now = time.monotonic()
                if now - last_status >= 15.0:
                    self.log(
                        f"still waiting for opponent Ranked {action} "
                        f"phase {phase}"
                    )
                    last_status = now

    def _commit_ranked_local_ban(self, piece: str, phase: int) -> None:
        """Submit and positively acknowledge a local ban before its clock."""
        deadline = time.monotonic() + 50.0
        self.events.drain()
        for attempt in itertools.count(1):
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(
                    f"Ranked ban phase {phase} never acknowledged {piece}"
                )
            try:
                self._ban_ranked_piece(piece, min(3.0, remaining))
            except TimeoutError:
                self.log(
                    f"Ranked ban control for {piece} was not ready; retrying"
                )
            try:
                committed = self.events.wait(
                    ("draft_ban_committed", "game_over", "out_of_time"),
                    min(3.0, max(0.01, deadline - time.monotonic())),
                )
            except TimeoutError:
                self.log(
                    f"Ranked ban {piece} lacked native acknowledgement; "
                    f"retrying tap {attempt + 1}"
                )
                continue
            if committed.kind != "draft_ban_committed":
                raise RuntimeError(
                    f"Ranked draft stopped during local phase {phase}: "
                    f"{committed.kind}"
                )
            return

    def _commit_ranked_local_pick(
        self, choices: Sequence[str], deployment: DraftDeployment, phase: int,
        local_ivory: bool,
    ) -> None:
        """Place and verify one immutable group, then acknowledge its Lock."""
        placements = []
        locked_team = tuple(deployment.team)
        spawn_generation = self.events.ranked_spawn_snapshot()[0]
        self.events.drain()
        starting_points = self.ranked_local_points
        # The native Local builder proved that ordinary models must be placed
        # before wide models. A later Prince drag can otherwise hit a Giant's
        # oversized collider and replace it despite targeting a disjoint
        # logical cell. The complete backtracking plan still reserves enough
        # room for every Giant/CopyCat before the first ordinary drop.
        ordered_choices = sorted(
            enumerate(choices),
            key=lambda item: (
                item[1] in ("giant", "copycat"),
                -PIECE_COST[item[1]],
                item[0],
            ),
        )
        desired = Counter(choices)
        ordered_piece_names = [piece for _index, piece in ordered_choices]
        excluded: dict[tuple[str, int], set[str]] = {
            (piece, ordinal): set()
            for piece, count in desired.items() for ordinal in range(count)
        }
        placement_deadline = time.monotonic() + 45.0
        last_packing_log = 0.0

        def current_roster() -> Counter[str]:
            return Counter(piece for piece, _square in placements)

        def removed_placements(
            incoming_piece: str, incoming_square: str, removed_cost: int,
        ) -> list[tuple[str, str]]:
            if removed_cost == 0:
                return []
            incoming_cells = deployment.cells(incoming_piece, incoming_square)
            exact: list[tuple[int, int, tuple[tuple[str, str], ...]]] = []
            for count in range(1, len(placements) + 1):
                for subset in itertools.combinations(placements, count):
                    if sum(PIECE_COST[piece] for piece, _square in subset) != removed_cost:
                        continue
                    overlap = sum(
                        len(deployment.cells(piece, square) & incoming_cells)
                        for piece, square in subset
                    )
                    exact.append((overlap, -count, subset))
            if not exact:
                raise RuntimeError(
                    f"native Ranked replacement removed {removed_cost} "
                    "unreconciled points from the pending group"
                )
            # Native replacement occurs at the reported landing. Prefer the
            # exact-cost subset occupying that footprint, then the smallest
            # subset. Equal-cost duplicates are interchangeable for roster
            # recovery and retain deterministic placement order.
            return list(max(exact, key=lambda item: (item[0], item[1]))[2])

        while current_roster() != desired:
            if time.monotonic() >= placement_deadline:
                # Do not abandon the process and donate a timeout. Keep the
                # fastest legal recovery running until the native phase itself
                # acknowledges or terminates.
                placement_deadline = time.monotonic() + 15.0
                self.log("Ranked placement recovery is continuing past its soft deadline")

            roster = current_roster()
            remaining = []
            remaining_keys: list[tuple[str, int]] = []
            remaining_counts = Counter(roster)
            for candidate in ordered_piece_names:
                if remaining_counts[candidate] < desired[candidate]:
                    remaining_keys.append((candidate, remaining_counts[candidate]))
                    remaining.append(candidate)
                    remaining_counts[candidate] += 1
            piece = remaining[0]
            piece_key = remaining_keys[0]
            extras = Counter(roster)
            extras.subtract(desired)
            replace = next(
                (placement for placement in placements
                 if extras[placement[0]] > 0),
                None,
            )
            if replace is not None:
                requested = replace[1]
                extras[replace[0]] -= 1
            else:
                try:
                    requested = deployment.plan(
                        remaining, [excluded[key] for key in remaining_keys],
                    )[0]
                except RuntimeError:
                    # A native miss is target-instance evidence, not proof that
                    # the group itself is unpackable. Reset this occurrence's
                    # retry cycle and remain in-phase rather than donating a
                    # Ranked timeout by terminating the controller.
                    excluded[piece_key].clear()
                    now = time.monotonic()
                    if now - last_packing_log >= 1.0:
                        self.log(
                            f"Ranked {piece} packing exhausted transient targets; "
                            "restarting its in-phase placement cycle"
                        )
                        last_packing_log = now
                    # A malformed recovery state previously emitted millions
                    # of retries per second while the native phase clock ran.
                    # Yield to Unity and the event reader before replanning.
                    time.sleep(0.10)
                    continue
            before_points = self.ranked_local_points
            try:
                result = self._place_ranked_piece(
                    piece, requested, local_ivory,
                )
            except TimeoutError:
                if replace is None:
                    excluded[piece_key].add(requested)
                self.log(
                    f"Ranked {piece}@{requested} did not materialize; "
                    "retrying in-phase"
                )
                continue

            local_points = result.local_points
            if local_points == before_points:
                if replace is None:
                    excluded[piece_key].add(requested)
                self.log(
                    f"Ranked {piece}@{requested} did not increase material; "
                    "retrying in-phase"
                )
                continue

            actual_piece = result.piece
            removed_cost = (
                before_points + PIECE_COST[actual_piece] - local_points
            )
            if removed_cost < 0:
                raise RuntimeError(
                    f"Ranked {piece}@{requested} produced an impossible "
                    f"material increase to {local_points}"
                )
            removed = removed_placements(
                actual_piece, result.square, removed_cost
            )
            for removed_piece, removed_square in removed:
                placements.remove((removed_piece, removed_square))
                deployment.release(removed_piece, removed_square)

            deployment.reserve(actual_piece, result.square)
            placements.append((actual_piece, result.square))
            self.ranked_local_points = local_points
            if removed:
                self.log(
                    f"recovered Ranked replacement at {result.square}: "
                    + " ".join(
                        f"{removed_piece}@{removed_square}"
                        for removed_piece, removed_square in removed
                    )
                    + f" -> {actual_piece}; re-adding the missing roster"
                )
            elif actual_piece != piece:
                self.log(
                    f"Ranked {piece} pot produced {actual_piece}@{result.square}; "
                    "reconciling the pending roster in-phase"
                )
            elif result.square != requested:
                self.log(
                    f"native Ranked landing corrected {piece}: "
                    f"{requested} -> {result.square}"
                )
        expected_points = starting_points + sum(PIECE_COST[piece] for piece in choices)
        if self.ranked_local_points != expected_points:
            raise RuntimeError(
                "native Ranked placement total disagrees with engine draft: "
                f"app {self.ranked_local_points}, engine {expected_points}"
            )
        deadline = time.monotonic() + 45.0
        for attempt in itertools.count(1):
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(
                    f"Ranked pick phase {phase} never locked its group"
                )
            try:
                self._tap_draft_control(("LOCK",), min(4.0, remaining))
            except TimeoutError:
                self.log("Ranked Lock control was not ready; retrying")
            try:
                committed = self.events.wait(
                    ("draft_pick_committed", "game_over", "out_of_time"),
                    min(4.0, max(0.01, deadline - time.monotonic())),
                )
            except TimeoutError:
                self.log(
                    f"Ranked group lacked native acknowledgement; retrying "
                    f"Lock tap {attempt + 1}"
                )
                continue
            if committed.kind != "draft_pick_committed":
                raise RuntimeError(
                    f"Ranked draft stopped during local phase {phase}: "
                    f"{committed.kind}"
                )
            exact_placements = self._observe_ranked_local_pick(
                local_ivory, choices, spawn_generation, locked_team, placements,
            )
            for placed_piece, placed_square in reversed(placements):
                deployment.release(placed_piece, placed_square)
            for placed_piece, placed_square in exact_placements:
                deployment.reserve(placed_piece, placed_square)
            placements = exact_placements
            self.log(
                "locked local Ranked group: "
                + " ".join(f"{piece}@{square}" for piece, square in placements)
            )
            return

    def run_ranked_draft(
        self,
    ) -> list[tuple[str, str]] | OpeningTerminal:
        """Play all twelve public Ranked draft windows without private leaks."""
        if len(self.draft_pots) != len(POT_SORT_ORDER):
            raise RuntimeError("call start_ranked before drafting")
        self.engine.draft_new()
        deployment = DraftDeployment()
        # Preserve the pristine phase-zero locks before spending time on side
        # detection; a very fast Ivory opponent can otherwise finish its ban
        # while that detection is running.
        local_ivory = self._ranked_is_ivory()
        self.online_local_team = 0 if local_ivory else 1
        self.log("Ranked draft side: " + ("Ivory" if local_ivory else "Onyx"))
        precompleted_count = self.events.ranked_bans_completed()
        deadline = time.monotonic() + 1.5
        precompleted_bans = self.events.ranked_ban_snapshot()
        while (len(precompleted_bans) < precompleted_count
               and time.monotonic() < deadline):
            time.sleep(0.02)
            precompleted_bans = self.events.ranked_ban_snapshot()
        if len(precompleted_bans) != precompleted_count:
            raise RuntimeError(
                "Ranked calibration saw a Ban callback without its public piece name"
            )

        for phase in range(12):
            status = self.engine.draft_status()
            if status["phase"] != phase:
                raise RuntimeError(f"engine draft desynchronized at phase {phase}: {status}")
            action = str(status["action"])
            local = (phase % 2 == 0) == local_ivory
            event_kind = "draft_ban_committed" if action == "ban" else "draft_pick_committed"
            if action == "ban" and phase < precompleted_count:
                if local:
                    raise RuntimeError(
                        "a local Ranked Ban completed before controller input"
                    )
                piece = precompleted_bans[phase]
                self.log(f"recovered pre-calibration opponent ban: {piece}")
                self.engine.draft_choose(piece)
                self.engine.draft_commit()
                time.sleep(0.18)
                continue
            if local:
                choices = self.engine.draft_auto()
                self.log(
                    f"draft phase {phase}: {action} "
                    + (" ".join(choices) if choices else "(none)"))
                if action == "ban":
                    piece = choices[0]
                    self._commit_ranked_local_ban(piece, phase)
                else:
                    self._commit_ranked_local_pick(
                        choices, deployment, phase, local_ivory,
                    )
            else:
                self.log(f"draft phase {phase}: waiting for opponent {action}")
                ban_generation = (
                    len(self.events.ranked_ban_snapshot())
                    if action == "ban" else -1
                )
                committed = self._wait_ranked_opponent_phase(
                    event_kind, action, phase,
                )
                if committed.kind != event_kind:
                    # Before board reveal, a terminal event during the remote
                    # player's clock is necessarily their timeout/forfeit. Do
                    # not abort a multi-game climb after earning the result;
                    # classify the visible overlay and let the outer loop
                    # requeue normally.
                    result = (
                        "win" if committed.kind == "out_of_time" else
                        self.classify_game_over(decisive_result="win")
                    )
                    self.log(
                        f"result: {result} ({committed.kind} during opponent "
                        f"Ranked draft phase {phase})"
                    )
                    return OpeningTerminal(result, committed.kind)
                if action == "pick":
                    # The opponent's just-committed group is now public and
                    # immutable. Scan that public board, infer only the count
                    # of coordinate-hidden Ghosts from material, and apply the
                    # exact roster additions to the native draft state.
                    self._observe_ranked_opponent_pick(local_ivory)
                else:
                    piece = self._observe_ranked_opponent_ban(ban_generation)
                    self.log(f"public opponent ban: {piece}")
                    self.engine.draft_choose(piece)
                    self.engine.draft_commit()
            time.sleep(0.18)

        if self.engine.draft_status()["action"] != "complete":
            raise RuntimeError("engine draft did not complete after twelve phases")
        self.own_team = deployment.team
        self.log(
            "completed Ranked deployment: "
            + " ".join(f"{piece}@{square}" for piece, square in self.own_team))
        # Unlike Unranked, the shipping Ranked transition does not invoke
        # Board.LoadBoard again after the twelfth phase. The final
        # OnSpawnPieceGroup acknowledgement above is the reveal barrier, then
        # the existing board spends about ten seconds in its intro before it
        # becomes interactive. Perspective calibration below waits through
        # that intro. Do not drain here: a quick Ivory opening is retained by
        # the independent gameplay journal and replayed after Onyx initializes.
        self.log("final Ranked group acknowledged; awaiting interactive reveal")
        return list(self.own_team)

    def calibrate_perspective(self, timeout: float = 4.0) -> bool:
        """Use our known local King square to detect a rotated Onyx board."""
        king_squares = [square for piece, square in self.own_team if piece == "king"]
        if len(king_squares) != 1:
            raise RuntimeError("perspective calibration requires one known local King")
        king_square = king_squares[0]
        rotated_king = rotate_square(king_square)
        deadline = time.monotonic() + timeout
        selected = None
        dot = None
        while time.monotonic() < deadline:
            self.events.drain()
            self.adb.tap_square(self.geometry, king_square)
            try:
                candidate = self.events.wait("selected", 0.35)
            except TimeoutError:
                continue
            if candidate.piece != "king":
                # During the intro easing, the eventual King pixel can still
                # overlap a neighbouring pawn.  Only a King acknowledgement
                # is calibration evidence; other selections are transient.
                continue
            try:
                candidate_dot = self.events.wait("dot_ready", 0.45)
            except TimeoutError:
                continue
            selected, dot = candidate, candidate_dot
            break
        if selected is None or dot is None:
            raise TimeoutError("board camera did not settle for perspective probe")
        if dot.source not in (king_square, rotated_king):
            raise RuntimeError(f"unexpected raw King square during perspective probe: {dot.source}")
        self.perspective_flipped = dot.source == rotated_king
        try:
            self.events.wait("touch_end", 0.5)
        except TimeoutError:
            pass
        self.log("player side: " + ("Onyx" if self.perspective_flipped else "Ivory"))
        return self.perspective_flipped

    def await_onyx_opening(
        self, timeout: float = 75.0
    ) -> tuple[AppEvent, ...] | OpeningTerminal:
        """Wait for Ivory's public first action before probing as Onyx.

        The app starts Ivory's clock as soon as the board appears.  On an Onyx
        game the opponent can therefore move while camera calibration and army
        verification are running.  EventStream's non-consuming journal lets us
        wait for the complete action even if ordinary tap waits drained its log
        records.  Probing only the settled *current* board also captures public
        products such as Sludge Goop without learning anything private.
        """
        if not self.perspective_flipped:
            return ()
        self.log("waiting for Ivory opening action before Onyx initialization")
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            _generation, events = self.events.gameplay_snapshot()
            for event in events:
                if event.kind in ("terminal_label", "game_over", "out_of_time"):
                    if event.kind == "terminal_label":
                        result = "draw" if event.source == "draw" else "loss"
                    elif event.kind == "out_of_time":
                        result = "win"
                    else:
                        # The server's GameOver can precede the visible attack
                        # by several seconds. Give the public menu time to say
                        # VICTORY/DEFEAT/DRAW; a still-unlabelled result with an
                        # opponent action in the journal is a local knockout.
                        result = self.classify_game_over(4.5)
                        if result == "unknown":
                            _latest_generation, latest = (
                                self.events.gameplay_snapshot()
                            )
                            opponent_acted = any(
                                candidate.kind in (
                                    "move", "attack", "dead",
                                    "ghost_visible", "ghost_hidden",
                                )
                                for candidate in latest
                            )
                            # With no public opponent action, only Ivory's
                            # opening clock could have expired. Conversely, an
                            # action followed by GameOver before our reply is a
                            # local timeout/knockout and therefore a loss.
                            result = "loss" if opponent_acted else "win"
                    self.log(
                        f"result: {result} ({event.kind} before Onyx initialization)"
                    )
                    return OpeningTerminal(result, event.kind)
            action_indices = [
                index for index, event in enumerate(events)
                if event.kind in (
                    "move", "attack", "dead", "ghost_visible", "ghost_hidden"
                )
            ]
            if action_indices:
                start = action_indices[0]
                for end in range(start + 1, len(events)):
                    if events[end].kind != "turn_end":
                        continue
                    prefix = events[start:end + 1]
                    bomb_involved = any(
                        (event.kind == "move" and event.piece == "bomb")
                        or (event.kind == "attack" and event.piece == "bomb")
                        for event in prefix
                    )
                    if bomb_involved:
                        bomb_deaths = [
                            index for index in range(start, end + 1)
                            if (events[index].kind == "dead"
                                and events[index].piece == "bomb")
                        ]
                        # SignalR can announce ChangeTurn before the local Bomb
                        # death/explosion animation.  A settled board requires
                        # the later native turn barrier, never that early event.
                        if not bomb_deaths or bomb_deaths[-1] >= end:
                            continue
                    time.sleep(0.25)
                    opening = tuple(
                        self.canonical_event(event)
                        for event in events[start:end + 1]
                    )
                    self.log("Ivory opening action settled")
                    return opening
            time.sleep(0.05)
        raise TimeoutError("timed out waiting for Ivory opening action")

    def canonical_event(self, event: AppEvent) -> AppEvent:
        if not self.perspective_flipped:
            return event
        return AppEvent(event.kind, event.piece,
                        rotate_square(event.source) if event.source else None,
                        rotate_square(event.target) if event.target else None,
                        event.raw)

    def device_square(self, square: str) -> str:
        """Map a canonical square to the current rendered input coordinate."""
        return rotate_square(square) if getattr(self, "rotate_taps", False) else square

    @staticmethod
    def _unlock_badges(image) -> list[tuple[int, int]]:
        """Return visible golden one-key badge centers in the character grid."""
        import numpy as np

        rgb = np.asarray(image.convert("RGB"))
        red, green, blue = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
        yellow = (red > 220) & (green > 120) & (green < 220) & (blue < 100)
        badges = []
        for component in _components(yellow):
            if not component:
                continue
            ys = [point[0] for point in component]
            xs = [point[1] for point in component]
            width, height = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
            # The fully scrolled four-item tail starts at 36% height. Prince's
            # crown touches its one-key badge, widening that connected gold
            # component to roughly 158 px; both shapes remain well separated
            # from sale headers and currency counters.
            if (115 <= width <= 175 and 75 <= height <= 105 and
                    min(ys) > image.height * 0.30):
                badges.append(((min(xs) + max(xs)) // 2,
                               (min(ys) + max(ys)) // 2))
        return sorted(badges, key=lambda point: (point[1], point[0]))

    def unlock_characters(self, count: int) -> int:
        """Spend already-earned character keys; never gems or cash items.

        Returns the number unlocked.  An already-complete roster is a clean
        no-op, which lets the unattended farm loop continue playing without
        ever falling through to gems, cash, cosmetics, or battle-pass items.
        """
        self.adb.restart_app("com.JesseLugassy.ChessUltimate")
        self.wait_connected_main()
        time.sleep(0.8)
        unlocked = 0
        for index in range(count):
            if index:
                # Successful key purchases return to the main menu with an OK
                # acknowledgement over the Shop button.
                self.adb.tap(540, 1300)
                time.sleep(0.35)
            self.adb.tap(540, 1280)  # Shop

            try:
                screenshot = self.wait_screen(
                    "character shop",
                    lambda image: bool(self._unlock_badges(image)),
                    4.0,
                )
                badges = self._unlock_badges(screenshot)
            except TimeoutError:
                # Once the first five rows are owned, reveal the remaining
                # character grid before concluding there is nothing to buy.
                self.adb.command("input swipe 540 2050 540 750 300")
                try:
                    screenshot = self.wait_screen(
                        "scrolled locked characters",
                        lambda image: bool(self._unlock_badges(image)), 5.0)
                    badges = self._unlock_badges(screenshot)
                except TimeoutError:
                    self.log("all key-unlockable characters are owned")
                    return unlocked

            badge_x, badge_y = badges[0]
            self.events.drain()
            card_clicked = False
            for _ in range(5):
                self.adb.tap(badge_x - 95, badge_y + 125)
                try:
                    self.events.wait("shop_item", 0.50)
                    card_clicked = True
                    break
                except TimeoutError:
                    continue
            if not card_clicked:
                raise TimeoutError("locked character card rejected five taps")
            # Some newly encountered 3D previews stream their addressable model
            # for ten-plus seconds before the detail controls are rendered.
            # Retapping during that load only creates ambiguous pointer input.
            self.wait_screen(
                "locked character detail",
                lambda image: self._color_count(
                    image, (0.38, 0.84, 0.98, 0.94), "yellow") > 20000,
                30.0,
            )
            self.adb.tap(650, 2210)  # Buy Item (one character key)
            self.wait_screen(
                "key confirmation",
                lambda image: self._color_count(
                    image, (0.47, 0.50, 0.78, 0.62), "yellow") > 5000,
                5.0,
            )
            self.adb.tap(690, 1350)  # Buy (confirmation explicitly shows key -1)
            self.wait_screen(
                "unlock acknowledgement",
                lambda image: self._color_count(
                    image, (0.15, 0.34, 0.82, 0.45), "yellow") > 6000,
                8.0,
            )
            unlocked += 1
            self.log(f"unlocked character {unlocked}/{count}")
        return unlocked

    def probe_enemy(self, image=None,
                    outlined: Iterable[str] | None = None,
                    fast: bool = False,
                    minimum_confirmations: int = 2) -> list[tuple[str, str]]:
        started_at = time.monotonic()
        image = image or self.adb.screenshot()
        outlined = set(outlined or detect_outline_squares(
            image, self.geometry, "red", (8, 9, 10)))
        if not outlined:
            raise RuntimeError("no red-outlined enemy pieces found; wait for the board to settle")
        self.log("outlined enemy squares: " + " ".join(
            sorted(outlined, key=square_sort_key)))
        probed = []
        # Probe only cells with public visual occupancy.  Tall character meshes
        # can intercept a tap in a neighbouring empty cell and report their own
        # prefab, which previously assigned that piece to the touched empty
        # square.  Colored outlines plus the dark-body fallback above cover
        # every visible character while hidden Ghosts remain intentionally
        # absent and are reconstructed only from public material.
        squares = sorted(outlined, key=square_sort_key)
        for square in squares:
            observations: list[str] = []
            # Dense linked formations can defer a selected-character log while
            # their paired outline/animation settles. More importantly, a tall
            # neighbour's collider can cover one part of this square. Sample the
            # center and all four cardinal offsets, then require one unambiguous
            # public prefab identity instead of trusting the first collider hit.
            center_x, center_y = self.geometry.point(square)
            offsets = ((0.0, 0.0), (0.0, 0.18)) if fast else (
                (0.0, 0.0), (0.0, 0.24), (0.0, -0.24),
                (-0.20, 0.0), (0.20, 0.0),
                (-0.18, -0.18), (0.18, -0.18),
                (-0.18, 0.18), (0.18, 0.18),
            )
            for attempt, (x_offset, y_offset) in enumerate(offsets, 1):
                self.events.drain()
                self.adb.tap(
                    round(center_x + x_offset * self.geometry.cell_width),
                    round(center_y + y_offset * self.geometry.cell_height),
                )
                try:
                    candidate = self.events.wait(
                        ("selected", "game_over", "out_of_time"),
                        0.10 if fast else 0.12)
                except TimeoutError:
                    continue
                if candidate.kind != "selected":
                    raise RuntimeError(
                        f"game ended while probing {square}: {candidate.kind}")
                assert candidate.piece
                observations.append(public_probe_piece(candidate.piece))
                # Two matching samples are enough to avoid spending
                # the online action clock on the four diagonal fallbacks. Sparse
                # or empty candidates still receive the full 3x3 probe.
                if ((fast or attempt >= 3)
                        and len(observations) >= minimum_confirmations
                        and len(set(observations)) == 1):
                    break
            if not observations:
                if self.verbose:
                    self.log(f"probe {square}: stable outline spill ignored")
                continue
            if len(observations) < minimum_confirmations:
                # A stable outline plus only one collider remains ambiguous.
                # Accepting it creates a plausible but wrong
                # engine position, so abort before the first move.
                raise RuntimeError(
                    f"outlined enemy square {square} produced only "
                    f"{len(observations)} native selection acknowledgement(s)")
            identities = set(observations)
            if len(identities) != 1:
                raise RuntimeError(
                    f"outlined enemy square {square} has conflicting collider hits: "
                    + ", ".join(
                        f"{piece}={observations.count(piece)}"
                        for piece in sorted(identities)
                    )
                )
            piece = observations[0]
            if piece == "ghost":
                # Release diagnostics disclose the private Ghost coordinate.
                # Knowing that the army contains a Ghost comes solely from the
                # public material total, never from this tap result.
                if self.verbose:
                    self.log(f"probe {square}: hidden piece masked")
                continue
            probed.append((piece, square))
            if self.verbose:
                self.log(f"probe {square}: {piece}")
            # ``input tap`` itself is synchronous, but give logcat a brief
            # chance to consume the matching pointer-up before the next tap.
            try:
                self.events.wait("touch_end", 0.12)
            except TimeoutError:
                pass
        if not probed:
            raise RuntimeError("no native-confirmed enemy pieces found")
        normalized = normalize_copycat_probes(probed)
        self.log(f"verified {len(normalized)} enemy deployment cells in "
                 f"{time.monotonic() - started_at:.2f}s")
        return normalized

    def probe_enemy_grid(self, fast: bool = False) -> list[tuple[str, str]]:
        """Recover every selectable public enemy directly from board logs.

        This is the fail-closed fallback for a public material mismatch.  It
        does not guess that an opponent spent the 100-point maximum: the exact
        native counter may describe any legal under-budget army.  Instead, it
        touches every enemy home-zone cell and accepts a piece only when the
        native Square.OnPointerDown coordinate agrees with the cell being
        tested.  That coordinate prevents a tall neighbouring mesh from being
        assigned to an empty square.  Invisible Ghost callbacks are masked in
        exactly the same way as the outline-led fast path.
        """
        started_at = time.monotonic()
        probed: list[tuple[str, str]] = []
        offsets = ((0.0, 0.0), (0.0, 0.18)) if fast else (
            (0.0, 0.0), (0.0, 0.22), (0.0, -0.22),
            (-0.18, 0.0), (0.18, 0.0),
        )
        for square in (
            f"{file_name}{rank}"
            for rank in (8, 9, 10) for file_name in "abcdefgh"
        ):
            observations: list[str] = []
            center_x, center_y = self.geometry.point(square)
            for x_offset, y_offset in offsets:
                self.events.drain()
                self.adb.tap(
                    round(center_x + x_offset * self.geometry.cell_width),
                    round(center_y + y_offset * self.geometry.cell_height),
                )
                pointer_source = None
                deadline = time.monotonic() + (0.10 if fast else 0.14)
                while time.monotonic() < deadline:
                    try:
                        event = self.events.wait(
                            ("pointer_square", "selected", "game_over",
                             "out_of_time"),
                            deadline - time.monotonic(),
                        )
                    except TimeoutError:
                        break
                    if event.kind == "pointer_square":
                        pointer_source = self.canonical_event(event).source
                        continue
                    if event.kind != "selected":
                        raise RuntimeError(
                            f"game ended during exact enemy scan: {event.kind}"
                        )
                    if pointer_source != square:
                        # No pointer means this build did not prove the public
                        # coordinate; a different pointer proves collider spill.
                        break
                    assert event.piece
                    observations.append(public_probe_piece(event.piece))
                    break
                if len(observations) >= 2 and len(set(observations)) == 1:
                    break
            if not observations:
                continue
            if len(observations) < 2 or len(set(observations)) != 1:
                raise RuntimeError(
                    f"exact enemy scan could not confirm {square}: "
                    + ", ".join(observations or ("no matching native hit",))
                )
            piece = observations[0]
            if piece != "ghost":
                probed.append((piece, square))
                if self.verbose:
                    self.log(f"exact probe {square}: {piece}")
        normalized = normalize_copycat_probes(probed)
        self.log(
            f"exactly scanned {len(normalized)} public enemy cells in "
            f"{time.monotonic() - started_at:.2f}s"
        )
        return normalized

    def verify_own_team(self) -> None:
        """Verify local identities, with outline fallback for disabled pieces.

        During check the app does not dispatch GetAvailableMoves for every
        occupied local cell.  A wrong native identity is still a hard failure;
        a cell that emits no selection record is accepted only when its blue
        ownership outline is visibly present at the expected coordinate.
        """
        started_at = time.monotonic()
        outlined = set(detect_outline_squares(
            self.adb.screenshot(), self.geometry, "blue", (1, 2, 3)
        ))
        expected: list[tuple[str, str, tuple[str, ...]]] = []
        for piece, square in self.own_team:
            if piece == "king":
                # Perspective calibration has already selected the known King.
                continue
            if piece == "giant":
                # The native deployment validator guarantees the other three
                # footprint cells once the public Giant anchor is confirmed.
                # Probing empty footprint corners wastes several seconds because
                # the single Giant collider does not cover every skin equally.
                expected.append((piece, square, ("giant",)))
            elif piece == "copycat":
                mirror = DraftDeployment._mirror(square)
                expected.append((piece, square, ("copycat", "copycatClone")))
                expected.append((piece, mirror, ("copycat", "copycatClone")))
            else:
                expected.append((piece, square, (piece,)))

        failures = []
        for piece, square, accepted in expected:
            observations: list[str] = []
            center_x, center_y = self.geometry.point(square)
            offsets = (
                (0.0, 0.0), (0.0, -0.22), (0.0, 0.0), (0.0, 0.22),
                (-0.18, 0.0), (0.18, 0.0),
                (-0.16, -0.16), (0.16, -0.16),
                (-0.16, 0.16), (0.16, 0.16),
            )
            for x_offset, y_offset in offsets:
                self.events.drain()
                self.adb.tap(
                    round(center_x + x_offset * self.geometry.cell_width),
                    round(center_y + y_offset * self.geometry.cell_height),
                )
                try:
                    event = self.events.wait(
                        ("selected", "game_over", "out_of_time"), 0.12
                    )
                except TimeoutError:
                    continue
                if event.kind != "selected":
                    raise RuntimeError(
                        f"game ended while verifying own army: {event.kind}"
                    )
                if event.piece:
                    observations.append(event.piece)
                accepted_hits = sum(
                    observed in accepted for observed in observations
                )
                if (accepted_hits >= 2 or
                        (accepted_hits >= 1 and square in outlined)):
                    break
            accepted_hits = sum(observed in accepted for observed in observations)
            corroborated = (
                accepted_hits >= 2 or
                (accepted_hits >= 1 and square in outlined)
            )
            if not corroborated:
                if observations or square not in outlined:
                    failures.append(
                        f"{piece}@{square} selected "
                        + (",".join(observations) if observations else "nothing")
                    )
                elif self.verbose:
                    self.log(
                        f"verify {piece}@{square}: occupied but unavailable "
                        "under the current check/turn state"
                    )
        if failures:
            raise RuntimeError(
                "saved own army does not match Ultimate Fish: " + "; ".join(failures)
            )
        self.events.drain()
        self.log(f"verified {len(expected) + 1} own deployment cells in "
                 f"{time.monotonic() - started_at:.2f}s")

    def initialize_online(
        self, state: OnlineStartState, local_team: int | None = None,
        side: str = "w",
    ) -> None:
        """Initialize from sanitized authoritative online records without taps."""
        if local_team is None:
            local_team = infer_online_local_team(state, self.own_team)
        own, positions = sanitize_online_start(
            state, local_team, self.belief_limit, side
        )
        if Counter(own) != Counter(self.own_team):
            raise RuntimeError(
                "authoritative online local army differs from the expected deployment"
            )
        valid = []
        for position in positions:
            try:
                self.engine.set_position(position)
                valid.append(position)
            except ValueError:
                continue
        if not valid:
            raise RuntimeError(
                "no sanitized authoritative online hypothesis is engine-valid"
            )
        self.adb.keep_awake()
        self.perspective_flipped = local_team == 1
        self.own_team = own
        self.events.drain()
        self.beliefs = BeliefSet(self.engine, valid, self.belief_limit)
        self.log(
            f"initialized {len(valid)} sanitized authoritative online belief(s)"
        )

    def initialize(self, enemy_material: int | None, side: str = "w",
                   fast: bool = False, require_pristine: bool = False,
                   opening_events: Sequence[AppEvent] = ()) -> None:
        self.adb.keep_awake()
        if require_pristine and self.events.opening_action_started():
            raise RuntimeError(
                "opponent opening action began before initial public scan"
            )
        opening = self.wait_screen(
            "unobscured opening board",
            lambda frame: not self._opening_board_obscured(frame),
            10.0,
        )
        observed_material = enemy_material
        material_reads: list[int] = []
        used_native_material = False

        def native_enemy_material() -> int | None:
            snapshotter = getattr(self.events, "initial_points_snapshot", None)
            points = snapshotter() if snapshotter is not None else None
            if points is None:
                return None
            return points[1 if side == "w" else 0]

        opening_frames = []
        opening_outlines: list[set[str]] = []
        opening_error: Exception | None = None
        for attempt in range(12):
            try:
                if self._opening_board_obscured(opening):
                    raise RuntimeError("enemy deployment is covered by an emote")
                frame_outlines = detect_outline_squares(
                    opening, self.geometry, "red", (8, 9, 10))
                if not frame_outlines:
                    raise RuntimeError("enemy outlines are not rendered yet")
                opening_frames.append(opening)
                opening_outlines.append(set(frame_outlines))
                if enemy_material is None:
                    native_material = native_enemy_material()
                    if native_material is not None:
                        observed_material = native_material
                        used_native_material = True
                    else:
                        candidate = read_material_counter(opening)
                        material_reads.append(candidate)
                if len(opening_frames) < 3:
                    raise RuntimeError("opening video consensus is incomplete")
                outlined = consensus_square_sets(opening_outlines[-3:])
                if not outlined:
                    raise RuntimeError("enemy outline consensus is empty")
                recent_union = set().union(*opening_outlines[-3:])
                if set(outlined) != recent_union:
                    # Do not silently reinterpret a flickering 15-point visible
                    # piece as a hidden Ghost. Wait until every public occupied
                    # cell agrees across all three frames or fail closed.
                    raise RuntimeError("enemy outline cells have not stabilized")
                if enemy_material is None:
                    native_material = native_enemy_material()
                    if native_material is not None:
                        observed_material = native_material
                        used_native_material = True
                    else:
                        # Require three consecutive fully rendered counter
                        # reads only when the exact native ledger is absent.
                        # This rejects transient 011/099 digit easing.
                        if (len(material_reads) < 3
                                or len(set(material_reads[-3:])) != 1):
                            raise RuntimeError("material counter has not stabilized")
                        observed_material = material_reads[-1]
                break
            except RuntimeError as exc:
                opening_error = exc
                if attempt == 11:
                    raise
                time.sleep(0.15)
                opening = self.adb.screenshot()
        else:  # pragma: no cover - loop either breaks or raises above
            raise RuntimeError("opening board did not settle") from opening_error
        if enemy_material is None:
            assert observed_material is not None
            source = "native" if used_native_material else "counter OCR"
            self.log(f"public enemy material: {observed_material} ({source})")
        if require_pristine and self.events.opening_action_started():
            raise RuntimeError(
                "opponent opening action began during outline consensus"
            )
        probed = self.probe_enemy(opening_frames[-1], outlined, fast)
        if require_pristine and self.events.opening_action_started():
            raise RuntimeError(
                "opponent opening action began during initial public scan"
            )
        native_material = native_enemy_material()
        if native_material is not None:
            if observed_material != native_material:
                self.log(
                    f"native enemy material {native_material} overrides "
                    f"counter OCR {observed_material}"
                )
            observed_material = native_material
        probed_variants = (
            rewind_public_enemy_opening(probed, opening_events)
            if opening_events else [probed]
        )
        def build_positions(
            variants: Sequence[Sequence[tuple[str, str]]],
        ) -> tuple[list[str], RuntimeError | None]:
            built: list[str] = []
            error: RuntimeError | None = None
            for initial_enemy in variants:
                try:
                    built.extend(initial_beliefs(
                        self.own_team, initial_enemy, observed_material,
                        self.belief_limit, side,
                        enemy_king_candidates=self.ranked_enemy_king_candidates,
                    ))
                except RuntimeError as exc:
                    error = exc
            return built, error

        positions, initial_error = build_positions(probed_variants)
        if not positions and initial_error is not None:
            # A native total that does not equal visible public cost plus an
            # integral number of hidden Ghosts means vision missed public
            # knowledge. Never turn that discrepancy into a belief or fill an
            # under-budget roster toward 100. Recover the exact public cells
            # from pointer/selection logs and reconcile against the same total.
            self.log(
                f"public deployment did not reconcile ({initial_error}); "
                "running exact native grid scan"
            )
            exact_probed = self.probe_enemy_grid(fast)
            probed_variants = (
                rewind_public_enemy_opening(exact_probed, opening_events)
                if opening_events else [exact_probed]
            )
            positions, initial_error = build_positions(probed_variants)
        if not positions and initial_error is not None:
            raise initial_error
        # Parsing in the engine catches outline false positives immediately.
        valid = []
        for position in positions:
            try:
                self.engine.set_position(position)
                valid.append(position)
            except ValueError:
                continue
        if not valid:
            raise RuntimeError("no initial public-information hypothesis is engine-valid")
        self.beliefs = BeliefSet(self.engine, valid, self.belief_limit)
        if opening_events:
            self.log(
                f"rewound {len(probed_variants)} public opening deployment "
                "candidate(s); replaying settled Ivory turn"
            )
        self.log(f"initialized {len(self.beliefs.positions)} public-information belief(s)")

    def initialize_position(self, positions: Sequence[str]) -> None:
        """Resume an already calibrated live game from lossless UPN states."""
        valid = []
        for position in positions:
            self.engine.set_position(position)
            valid.append(position)
        self.adb.keep_awake()
        self.events.drain()
        self.beliefs = BeliefSet(self.engine, valid, self.belief_limit)
        self.log(f"resumed {len(valid)} public-information belief(s)")

    def initialize_ranked_public(self, side: str) -> None:
        """Initialize from the final public locked-group journal without taps."""
        if not self.ranked_enemy_snapshots:
            raise RuntimeError("Ranked draft has no public opponent snapshot")
        if self.ranked_opponent_points is None:
            raise RuntimeError("Ranked draft has no public opponent material total")
        positions = initial_beliefs(
            self.own_team,
            self.ranked_enemy_snapshots[-1],
            self.ranked_opponent_points,
            self.belief_limit,
            side,
            enemy_king_candidates=self.ranked_enemy_king_candidates,
        )
        self.initialize_position(positions)
        self.log("initialized from the public Ranked spawn journal")

    def await_special_source_ready(
        self, source: str, display_source: str, timeout: float = 8.0
    ) -> AppEvent | None:
        """Wait until a drag-only special actor can receive board input.

        Local's opening royal/Ghost reveal can continue after the preceding
        turn callback. A drag delivered during that animation is swallowed
        without a Character diagnostic, which looks exactly like a native
        rules rejection. A harmless source selection is the authoritative
        input-ready probe.
        """
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.adb.tap_square(self.geometry, display_source)
            pointer_source = None
            attempt_deadline = min(deadline, time.monotonic() + 0.50)
            while time.monotonic() < attempt_deadline:
                try:
                    event = self.events.wait(
                        ("pointer_square", "selected", "terminal_label",
                         "game_over", "out_of_time"),
                        attempt_deadline - time.monotonic(),
                    )
                except TimeoutError:
                    break
                if event.kind == "pointer_square":
                    pointer_source = self.canonical_event(event).source
                    continue
                if event.kind != "selected":
                    return event
                if pointer_source is None or pointer_source == source:
                    try:
                        release = self.events.wait(
                            ("touch_end", "terminal_label", "game_over",
                             "out_of_time"),
                            0.60,
                        )
                        if release.kind != "touch_end":
                            return release
                    except TimeoutError:
                        pass
                    return None
                break
            time.sleep(0.08)
        raise TimeoutError(f"special-action source {source} never became selectable")

    def confirm_forced_timeout_castle(
        self, probe: str, source: str, target: str, timeout: float = 20.0
    ) -> AppEvent | None:
        """Confirm the silent terminal state after an enemy-Rook castle.

        That native path emits no ordinary move or ChangeTurn barrier. Both
        models visibly relocate, board.turn changes to the Rook's team, and
        playerTeam remains the castling side. Touching the relocated Rook logs
        both predicates without making another action, giving an authoritative
        confirmation that the opponent can only time out.
        """
        pending = self.events.drain()
        if pending is not None:
            return AppEvent(
                "forced_timeout", source=source, target=target,
                raw=f"completed:{pending.kind}",
            )
        if getattr(self, "verbose", False):
            self.log(
                f"probing silent enemy-Rook castle at {probe} for "
                "board.turn/playerTeam mismatch"
            )
        board_matches = False
        player_mismatches = False
        last_probe_state = None
        deadline = time.monotonic() + timeout
        next_probe = 0.0
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_probe:
                self.adb.tap_square(self.geometry, self.device_square(probe))
                next_probe = now + 0.65
            try:
                event = self.events.wait(
                    ("draft_turn_probe", "player_team_probe",
                     "terminal_label", "game_over", "out_of_time"),
                    min(deadline, next_probe) - time.monotonic(),
                )
            except TimeoutError:
                continue
            if event.kind in ("terminal_label", "game_over", "out_of_time"):
                return AppEvent(
                    "forced_timeout", source=source, target=target,
                    raw=f"completed:{event.kind}",
                )
            if event.kind == "draft_turn_probe":
                board_matches = event.source == "local"
            elif event.kind == "player_team_probe":
                player_mismatches = event.source == "opponent"
            probe_state = (board_matches, player_mismatches)
            if getattr(self, "verbose", False) and probe_state != last_probe_state:
                self.log(
                    f"forced-timeout probe {event.kind}={event.source}; "
                    f"board={board_matches} player-mismatch={player_mismatches}"
                )
                last_probe_state = probe_state
            if board_matches and player_mismatches:
                return AppEvent(
                    "forced_timeout", source=source, target=target,
                    raw="enemy-owned Rook castle advanced board.turn without playerTeam",
                )
        return None

    def execute(
        self,
        move: str,
        expect_bomb_resolution: bool = False,
        _destination_attempt: int = 0,
        _retry_destination: bool = True,
    ) -> AppEvent:
        source, target, separator = parse_engine_move(move)
        verbose = getattr(self, "verbose", False)

        def raise_if_native_app_exited() -> None:
            package = "com.JesseLugassy.ChessUltimate"
            wait_for_exit = getattr(self.adb, "wait_for_package_exit", None)
            if callable(wait_for_exit):
                exited = wait_for_exit(package, 1.0)
            else:
                running = getattr(self.adb, "is_package_running", None)
                exited = callable(running) and not running(package)
            if exited:
                raise RuntimeError(
                    "Chess Ultimate process exited while executing " + move
                )
        if source == "pass":
            raise RuntimeError("the phone location of the pass/end-turn control is not calibrated")
        # The opponent may resign or flag while Ultimate Fish is searching.
        # Preserve that result instead of tapping the game-over screen.
        pending_terminal = self.events.drain()
        if pending_terminal is not None:
            return pending_terminal
        beliefs = getattr(self, "beliefs", None)
        castling = bool(
            beliefs and any(
                is_castling_move(position, move)
                for position in beliefs.positions
            )
        )
        forced_timeout_probe = None
        if beliefs:
            probes = [
                enemy_rook_castle_probe_square(position, move)
                for position in beliefs.positions
            ]
            if probes and all(probe is not None for probe in probes):
                unique_probes = set(probes)
                if len(unique_probes) == 1:
                    forced_timeout_probe = next(iter(unique_probes))
                    if verbose:
                        self.log(
                            "enemy-owned Rook castle requires silent terminal "
                            f"probe at {forced_timeout_probe}"
                        )
        linked_copycat = bool(
            beliefs and any(
                is_copycat_square(position, source)
                for position in beliefs.positions
            )
        )
        copycat_selection_markers = {
            source,
            f"{chr(ord('h') - (ord(source[0]) - ord('a')))}{source[1:]}",
        }
        display_source = self.device_square(source)
        display_target = self.device_square(target)
        destination_offsets = (
            (0.0, 0.0),
            (-0.24, 0.0), (0.24, 0.0),
            (0.0, -0.24), (0.0, 0.24),
            (-0.24, -0.24), (0.24, -0.24),
            (-0.24, 0.24), (0.24, 0.24),
        )

        if separator in ("~", "!", "&"):
            # Mage swaps, Fisherman hooks, and Angel links are presented as
            # legal dots like ordinary moves, but their live handlers commit
            # only from one continuous pointer drag. A source tap followed by
            # a destination tap selects the allied target instead of acting.
            # CompareDragDisplacement logs its native source/hover cells.  If
            # Unity sampled no displacement, retry once with a slightly deeper
            # endpoint instead of waiting for a turn event that cannot arrive.
            terminal = self.await_special_source_ready(source, display_source)
            if terminal is not None:
                return terminal
            # The source readiness probe is a complete tap.  Give Unity one
            # rendered frame after its pointer-up/DestroyDots path before a
            # new synthetic ACTION_DOWN; otherwise Android can merge the two
            # gestures and the subsequent swipe produces no pointer event.
            time.sleep(0.12)
            pending = self.events.drain()
            if pending is not None:
                return pending
            overshoots = special_drag_overshoots(display_source, display_target)

            def canonical_drag_square(raw: str | None) -> str | None:
                if raw is None:
                    return None
                match = re.fullmatch(r"(\d+):(\d+)", raw)
                if match is None:
                    return None
                square = scene_index_to_square(
                    int(match.group(1)) * 10 + int(match.group(2))
                )
                return (rotate_square(square)
                        if getattr(self, "perspective_flipped", False)
                        else square)

            for attempt, overshoot in enumerate(overshoots, 1):
                self.adb.drag_sync(
                    self.geometry.point(display_source),
                    self.geometry.drag_destination(
                        display_source, display_target, overshoot
                    ),
                    # The dragged 3D model trails Android's pointer.  On a
                    # seven-rank Fisherman gesture, 320 ms left Unity's final
                    # hover one full cell short and committed an ordinary move
                    # instead.  Special actions are rare; an 800 ms gesture is
                    # cheap insurance that the requested Dot/collider wins.
                    800,
                )
                deadline = time.monotonic() + 8.0
                while True:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        break
                    try:
                        terminal = self.events.wait(
                            ("army_drop", "move", "attack", "dead", "turn_end",
                             "terminal_label", "game_over", "out_of_time"),
                            remaining,
                        )
                    except TimeoutError:
                        break
                    if terminal.kind == "army_drop":
                        # CompareDragDisplacement logs ``hover - origin``.
                        # Do not accept an arbitrary nonzero displacement: a
                        # short swipe can legally commit a different ordinary
                        # move and its later ChangeTurn used to masquerade as
                        # success for the requested special action.
                        native_hover = canonical_drag_square(terminal.source)
                        native_origin = canonical_drag_square(terminal.target)
                        if native_origin is not None and native_origin != source:
                            raise RuntimeError(
                                f"special drag selected {native_origin}, expected "
                                f"{source}"
                            )
                        if native_hover == source:
                            break
                        if native_hover is not None and native_hover != target:
                            raise RuntimeError(
                                f"special drag {source}{separator}{target} landed "
                                f"on {native_hover}"
                            )
                        # A nonzero native displacement means the special Dot
                        # was reached. Allow its grapple/swap animation time to
                        # finish before considering a retry.
                        deadline = time.monotonic() + 12.0
                        continue
                    if terminal.kind in ("move", "attack", "dead"):
                        deadline = time.monotonic() + 12.0
                        continue
                    if terminal.kind == "turn_end":
                        return AppEvent(
                            "move", source=source, target=target,
                            raw="turn-end confirmed special drag",
                        )
                    return terminal
                if verbose:
                    self.log(
                        f"special drag {source}{separator}{target} was not "
                        f"accepted; retrying ({attempt}/{len(overshoots)})"
                    )
            raise TimeoutError(
                f"special drag {source}{separator}{target} was not accepted"
            )

        selected = None
        release_consumed = False
        # Online scenes occasionally retain an opponent-move animation or
        # latency veil for a few frames after ChangeTurn End.  Retry selection
        # for a bounded interval instead of turning one swallowed tap into a
        # controller failure.  Wide CopyCat/Giant renderers can also intercept
        # the center of a distant logical square.  Require Unity's preceding
        # Square.OnPointerDown coordinate to match the engine source, then try
        # bounded offsets wholly inside the source cell.
        selection_deadline = time.monotonic() + 5.0
        center_x, center_y = self.geometry.point(display_source)
        offsets = (
            (0.0, 0.0),
            (-0.24, 0.0), (0.24, 0.0),
            (0.0, -0.24), (0.0, 0.24),
            (-0.24, -0.24), (0.24, -0.24),
            (-0.24, 0.24), (0.24, 0.24),
        )
        attempt = 0
        while time.monotonic() < selection_deadline:
            x_offset, y_offset = offsets[attempt % len(offsets)]
            attempt += 1
            if x_offset == 0.0 and y_offset == 0.0:
                # Preserve the simple fake-device interface used by unit tests
                # and the normal fast path used by unobstructed pieces.
                self.adb.tap_square(self.geometry, display_source)
            else:
                self.adb.tap(
                    round(center_x + x_offset * self.geometry.cell_width),
                    round(center_y + y_offset * self.geometry.cell_height),
                )
            pointer_source = None
            attempt_deadline = min(selection_deadline, time.monotonic() + 0.45)
            while time.monotonic() < attempt_deadline:
                try:
                    event = self.events.wait(
                        ("pointer_square", "selected", "terminal_label",
                         "game_over", "out_of_time"),
                        attempt_deadline - time.monotonic(),
                    )
                except TimeoutError:
                    break
                if event.kind == "pointer_square":
                    pointer_source = self.canonical_event(event).source
                    continue
                if event.kind != "selected":
                    return event
                if pointer_source is not None and pointer_source != source:
                    if verbose:
                        self.log(
                            f"selection for {source} intercepted by "
                            f"{pointer_source}; retrying inside source cell"
                        )
                    break
                selected = event
                if linked_copycat:
                    # Both wide CopyCat pairs can receive one physical tap and
                    # emit several indistinguishable ``selected`` callbacks.
                    # SetUpMyDot logs each linked half's current source square,
                    # not its destination. Bind selection to both expected
                    # pair markers rather than a prefab name or stale callback.
                    observed_markers = set()
                    dot_deadline = min(selection_deadline, time.monotonic() + 0.55)
                    while time.monotonic() < dot_deadline:
                        try:
                            detail = self.events.wait(
                                ("dot_ready", "touch_end", "terminal_label",
                                 "game_over", "out_of_time"),
                                dot_deadline - time.monotonic(),
                            )
                        except TimeoutError:
                            break
                        if detail.kind not in ("dot_ready", "touch_end"):
                            return detail
                        if detail.kind == "dot_ready" and detail.source:
                            dot = self.canonical_event(detail).source
                            observed_markers.add(dot)
                        elif detail.kind == "touch_end":
                            release_consumed = True
                            break
                    if not copycat_selection_markers <= observed_markers:
                        if verbose:
                            self.log(
                                f"CopyCat selection for {source} did not "
                                "advertise both linked halves; retrying inside "
                                "source cell"
                            )
                        selected = None
                        release_consumed = False
                        break
                break
            if selected is not None:
                break
        if selected is None:
            raise_if_native_app_exited()
            raise TimeoutError(f"could not select {source} within five seconds")
        if not release_consumed:
            try:
                release = self.events.wait(
                    ("touch_end", "terminal_label", "game_over", "out_of_time"), 0.50)
                if release.kind != "touch_end":
                    return release
            except TimeoutError:
                # Some special-action controls do not use Character pointer-up.
                time.sleep(0.10)
        target_x_offset, target_y_offset = destination_offsets[
            min(_destination_attempt, len(destination_offsets) - 1)
        ]
        if target_x_offset == 0.0 and target_y_offset == 0.0:
            self.adb.tap_square(self.geometry, display_target)
        else:
            target_x, target_y = self.geometry.point(display_target)
            self.adb.tap(
                round(target_x + target_x_offset * self.geometry.cell_width),
                round(target_y + target_y_offset * self.geometry.cell_height),
            )
        expected = lambda event: (
            event.kind != "move" or
            (self.canonical_event(event).source == source and
             self.canonical_event(event).target == target))

        def completed(event: AppEvent) -> AppEvent:
            event = self.canonical_event(event)
            if event.kind == "turn_end":
                # Capturing a Bomb resolves its explosion and changes turn but
                # intentionally skips Character's ordinary "moves X --> Y"
                # diagnostic.  The selected legal destination plus native turn
                # completion is an unambiguous successful action.
                return AppEvent("move", source=source, target=target,
                                raw="turn-end confirmed special action")
            return event

        action_observed = False
        saw_bomb_death = False

        def wait_for_completion(initial_timeout: float) -> AppEvent:
            """Wait through the final post-explosion turn barrier when needed."""
            nonlocal action_observed, saw_bomb_death
            deadline = time.monotonic() + initial_timeout
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    if action_observed and expect_bomb_resolution:
                        raise TimeoutError(
                            "Bomb action was accepted but its death/turn animation "
                            "did not finish"
                        )
                    raise TimeoutError("move action was not accepted")
                event = self.canonical_event(self.events.wait(
                    ("move", "dead", "turn_end", "terminal_label",
                     "game_over", "out_of_time"),
                    remaining, expected,
                ))
                if event.kind in ("terminal_label", "game_over", "out_of_time"):
                    return event
                if event.kind == "dead":
                    if expect_bomb_resolution and event.piece == "bomb":
                        action_observed = True
                        saw_bomb_death = True
                        deadline = time.monotonic() + 8.0
                    continue
                if event.kind == "move":
                    action_observed = True
                    if (not expect_bomb_resolution and not castling and
                            not linked_copycat):
                        return event
                    deadline = time.monotonic() + 15.0
                    continue
                if event.kind == "turn_end":
                    action_observed = True
                    if (castling or linked_copycat or
                            not expect_bomb_resolution or saw_bomb_death):
                        return completed(event)
                    # Network turn dispatch precedes Bomb movement/death by
                    # several seconds. It is not yet an input-ready barrier.
                    deadline = time.monotonic() + 15.0

        try:
            # Native movement is not acknowledged until MoveTowards finishes.
            # Penguin's freeze update (and some crowded-board animations) can
            # take more than two seconds even though the destination tap was
            # accepted immediately.  Retrying before that callback changes
            # the Local perspective and then taps the next player's board.
            # This is only a maximum: ordinary move callbacks still return as
            # soon as they arrive.
            # Both CopyCat halves animate serially and the primary half's
            # coordinate callback can arrive after the ordinary five-second
            # action window.  Retrying at that point taps the next player's
            # board even though Unity already committed both relocations.
            return wait_for_completion(15.0 if linked_copycat else 5.0)
        except TimeoutError:
            if action_observed:
                raise
            raise_if_native_app_exited()
            if forced_timeout_probe is not None:
                confirmed = self.confirm_forced_timeout_castle(
                    forced_timeout_probe, source, target
                )
                if confirmed is not None:
                    return confirmed
            next_attempt = _destination_attempt + 1
            if _retry_destination and next_attempt < len(destination_offsets):
                if verbose:
                    self.log(
                        f"destination {target} did not commit; reselecting "
                        f"{source} with in-cell target retry "
                        f"({next_attempt + 1}/{len(destination_offsets)})"
                    )
                return self.execute(
                    move, expect_bomb_resolution,
                    _destination_attempt=next_attempt,
                    _retry_destination=True,
                )
            raise TimeoutError(f"move destination {target} was not accepted")

    def locate_public_piece(self, piece: str,
                            candidates: Sequence[str]) -> str:
        """Locate an engine-predicted visible piece by native selection identity.

        Unlike broad opening discovery, callers have already checked the
        predicted type and candidate squares against the lossless engine
        position.  One matching GetAvailableMoves callback is authoritative.
        Requiring two taps is incorrect because Unity leaves the character
        selected and intentionally emits no second selection callback.
        """
        self.events.drain()
        time.sleep(0.12)
        for square in candidates:
            display_square = self.device_square(square)
            center_x, center_y = self.geometry.point(display_square)
            hits = 0
            for x_offset, y_offset in (
                (0.0, 0.0), (0.0, -0.20), (0.0, 0.20),
                (-0.16, 0.0), (0.16, 0.0),
            ):
                self.events.drain()
                self.adb.tap(
                    round(center_x + x_offset * self.geometry.cell_width),
                    round(center_y + y_offset * self.geometry.cell_height),
                )
                try:
                    selected = self.events.wait(
                        ("selected", "game_over", "out_of_time"), 0.16)
                except TimeoutError:
                    continue
                if selected.kind != "selected":
                    raise RuntimeError(
                        f"game ended while locating {piece}: {selected.kind}")
                if (selected.piece and
                        public_probe_piece(selected.piece) ==
                        public_probe_piece(piece)):
                    hits += 1
                    if hits >= 1:
                        return square
        raise RuntimeError(
            f"could not locate visible {piece} on candidate squares: "
            + " ".join(candidates))

    def locate_generated_piece_by_outline(
        self, piece: str, candidates: Sequence[str]
    ) -> str:
        """Locate a newly spawned enemy without sending any board input.

        Devil destinations were empty before the action, so the new persistent
        red ownership outline uniquely identifies the visible Minion. This is
        a safe fallback for releases that omit Bot.RecordAiMove diagnostics.
        """
        if not candidates:
            raise RuntimeError(f"no legal candidate square for generated {piece}")
        ranks = sorted({int(square[1:]) for square in candidates})
        frames = []
        for _attempt in range(3):
            frames.append(self.adb.screenshot())
            time.sleep(0.10)
        outlined = set(consensus_outline_squares(
            frames, self.geometry, "red", ranks
        ))
        matches = sorted(outlined & set(candidates), key=square_sort_key)
        if len(matches) == 1:
            return matches[0]
        _generation, journal = self.events.gameplay_snapshot()
        recent_deaths = [event.piece for event in journal[-12:]
                         if event.kind == "dead" and event.piece]
        suffix = (
            "; recent public deaths: " + " ".join(recent_deaths)
            if recent_deaths else ""
        )
        raise RuntimeError(
            f"could not visually locate generated {piece}; outline candidates: "
            + (" ".join(matches) if matches else "none") + suffix
        )

    @staticmethod
    def diagnostic_special_target(
        piece: str, source: str, candidates: Sequence[str],
        diagnostic: AppEvent | None,
    ) -> str | None:
        """Validate a public Bot.RecordAiMove target against engine legality."""
        if diagnostic is None:
            return None
        if (diagnostic.kind != "bot_action" or diagnostic.piece != piece or
                diagnostic.source != source or diagnostic.target not in candidates):
            return None
        return diagnostic.target

    def locate_fisherman_target(
        self, source: str, targets: Sequence[str],
        identities: dict[str, set[str]],
    ) -> str:
        """Identify a completed public hook from its occupied landing cell.

        Every legal hook pulls its first visible ray target onto the adjacent
        cell. Candidate directions therefore have distinct landing cells that
        were publicly empty before the action. Probe toward each cell's outer
        edge to avoid the Fisherman's own collider, and accept only the public
        identity that occupied that candidate target before the pull.
        """
        self.events.drain()
        time.sleep(0.12)
        source_x, source_y = self.geometry.point(source)
        matches = []
        for target in targets:
            expected = identities.get(target, set())
            if not expected:
                continue
            landing = adjacent_toward(source, target)
            center_x, center_y = self.geometry.point(landing)
            dx = (center_x > source_x) - (center_x < source_x)
            dy = (center_y > source_y) - (center_y < source_y)
            hits = 0
            for outward, sideways in (
                (0.24, 0.0), (0.12, 0.0), (0.20, -0.12),
                (0.20, 0.12), (0.0, 0.0),
            ):
                # A perpendicular unit vector is (-dy, dx).
                x_offset = outward * dx - sideways * dy
                y_offset = outward * dy + sideways * dx
                self.events.drain()
                self.adb.tap(
                    round(center_x + x_offset * self.geometry.cell_width),
                    round(center_y + y_offset * self.geometry.cell_height),
                )
                try:
                    selected = self.events.wait(
                        ("selected", "game_over", "out_of_time"), 0.16)
                except TimeoutError:
                    continue
                if selected.kind != "selected":
                    raise RuntimeError(
                        "game ended while locating Fisherman target: "
                        + selected.kind
                    )
                if (selected.piece and
                        public_probe_piece(selected.piece) in expected):
                    hits += 1
                    if hits >= 2:
                        matches.append(target)
                        break
        if len(matches) != 1:
            raise RuntimeError(
                "could not uniquely locate public Fisherman hook among: "
                + " ".join(targets)
            )
        return matches[0]

    def await_opponent_bomb_resolution(self, timeout: float = 15.0) -> AppEvent:
        """Ignore the early network turn event and wait for Bomb death + turn.

        In online play ``ChangeTurn End`` is logged as soon as the move is
        dispatched. A Bomb can still be travelling and resolving its blast for
        several seconds. Acting on that early event lets two animations overlap
        and can leave the local clock/board one turn ahead of engine state.
        """
        deadline = time.monotonic() + timeout
        saw_bomb_death = False
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(
                    "opponent Bomb action did not reach its death/turn barrier"
                )
            event = self.events.wait(
                ("dead", "turn_end", "terminal_label", "game_over",
                 "out_of_time"),
                remaining,
            )
            if event.kind in ("terminal_label", "game_over", "out_of_time"):
                return event
            if event.kind == "dead":
                if event.piece == "bomb":
                    saw_bomb_death = True
                continue
            if event.kind == "turn_end" and saw_bomb_death:
                return event

    def classify_game_over(
        self, timeout: float = 4.0, decisive_result: str | None = None
    ) -> str:
        """Classify a menu, resolving condition-only text from move context."""
        deadline = time.monotonic() + timeout
        while True:
            try:
                result = read_game_over_result(self.adb.screenshot())
            except (RuntimeError, subprocess.SubprocessError):
                result = "unknown"
            if result in ("checkmate", "knockout", "terminal"):
                return decisive_result or "unknown"
            if result != "unknown":
                return result
            if time.monotonic() >= deadline:
                # The native game-over event already establishes which side
                # just ended the game in decisive call sites. Overlay text can
                # animate too late for OCR, but must not erase that fact.
                return decisive_result or "unknown"
            time.sleep(0.25)

    def play(self) -> str:
        if self.beliefs is None:
            raise RuntimeError("call initialize first")
        enemy_attack_target: str | None = None
        enemy_bomb_died_without_move = False
        ghost_became_visible = False
        enemy_bot_action: AppEvent | None = None
        enemy_deaths: set[str] = set()
        repetition_counts: Counter[str] = Counter()
        last_repetition_state: frozenset[str] | None = None

        def record_repetition_state() -> None:
            nonlocal last_repetition_state
            current = frozenset(
                upn_repetition_key(position)
                for position in self.beliefs.positions
            )
            if current != last_repetition_state:
                repetition_counts.update(current)
                last_repetition_state = current

        def move_completes_threefold(move: str) -> bool:
            next_keys = {
                upn_repetition_key(self.engine.apply(position, move))
                for position in self.beliefs.positions
                if move in self.engine.legal_moves(position)
            }
            return bool(next_keys) and all(
                repetition_counts[key] >= 2 for key in next_keys
            )

        while True:
            record_repetition_state()
            if self.beliefs.side == "w":
                repetition_draw_moves: list[str] = []
                if repetition_counts and max(repetition_counts.values()) >= 2:
                    legal_sets = [
                        set(self.engine.legal_moves(position))
                        for position in self.beliefs.positions
                    ]
                    for candidate in sorted(set.intersection(*legal_sets)):
                        if move_completes_threefold(candidate):
                            repetition_draw_moves.append(candidate)
                started = time.monotonic()
                move, info = self.beliefs.choose(
                    self.depth, self.nodes, self.movetime_ms,
                    repetition_draw_moves)
                elapsed = time.monotonic() - started
                self.log(f"Ultimate Fish {move} ({elapsed:.3f}s; {info})")
                bomb_resolution = self.beliefs.move_causes_bomb_detonation(move)
                repetition_draw = move in repetition_draw_moves
                event = self.execute(move, bomb_resolution)
                if event.kind == "terminal_label":
                    result = "draw" if event.source == "draw" else "win"
                    self.log(f"result: {result} ({event.source})")
                    return result
                if event.kind == "game_over":
                    result = ("draw" if repetition_draw else
                              self.classify_game_over(decisive_result="win"))
                    self.log(
                        f"result: {result} (game-over screen after Ultimate Fish move)"
                    )
                    return result
                if event.kind == "out_of_time":
                    self.log("result: loss (Ultimate Fish out of time)")
                    return "loss"
                if event.kind == "forced_timeout":
                    self.beliefs.apply_known(move)
                    if event.raw.startswith("completed:"):
                        self.log("result: win (forced opponent timeout)")
                        return "win"
                    self.log(
                        "enemy-owned Rook castle committed; waiting for the "
                        "trapped opponent clock"
                    )
                    terminal = self.events.wait(
                        ("out_of_time", "game_over", "terminal_label"), 90.0
                    )
                    if terminal.kind == "out_of_time":
                        self.log("result: win (forced opponent timeout)")
                        return "win"
                    if terminal.kind == "terminal_label":
                        result = "draw" if terminal.source == "draw" else "win"
                        self.log(f"result: {result} ({terminal.source})")
                        return result
                    result = self.classify_game_over(decisive_result="win")
                    self.log(f"result: {result} (forced-timeout game over)")
                    return result
                if event.kind != "move":
                    self.log(f"game stopped: {event.kind}")
                    return "unknown"
                self.beliefs.apply_known(move)
                enemy_deaths.clear()
                if not any(self.engine.legal_moves(position)
                           for position in self.beliefs.positions):
                    try:
                        terminal = self.events.wait(
                            ("terminal_label", "game_over", "out_of_time"), 5.0)
                        if terminal.kind == "terminal_label":
                            result = "draw" if terminal.source == "draw" else "win"
                            self.log(f"result: {result} ({terminal.source})")
                            return result
                        if terminal.kind == "game_over":
                            result = self.classify_game_over(decisive_result="win")
                            self.log(
                                f"result: {result} (terminal engine position)"
                            )
                            return result
                        self.log("result: loss (Ultimate Fish out of time)")
                        return "loss"
                    except TimeoutError:
                        # CPU resignation/checkmate overlays can finish after
                        # OpenGameOverMenu's diagnostic has already gone by.
                        # Read the public overlay before abandoning a proven
                        # terminal engine state as an unknown result.
                        result = self.classify_game_over(
                            timeout=10.0, decisive_result="win"
                        )
                        if result != "unknown":
                            self.log(
                                f"result: {result} "
                                "(late terminal engine overlay)"
                            )
                            return result
                        self.log("game stopped: terminal engine position")
                        return "unknown"
                enemy_attack_target = None
                continue

            event = self.events.wait(
                ("move", "attack", "dead", "ghost_visible", "ghost_hidden",
                 "bot_action", "turn_end", "terminal_label", "game_over", "out_of_time"),
                75.0)
            event = self.canonical_event(event)
            if event.kind == "bot_action":
                # The release diagnostic is public for these visible actions.
                # Never use it to reveal a quiet invisible-Ghost coordinate.
                enemy_bot_action = event
                continue
            if event.kind == "terminal_label":
                # No opponent move preceded this label.  It therefore resolves
                # our previous action (or an opponent resignation), not an
                # attack by the opponent.
                result = "draw" if event.source == "draw" else "win"
                self.log(f"result: {result} ({event.source})")
                return result
            if event.kind == "out_of_time":
                self.log("result: win (opponent out of time before moving)")
                return "win"
            if event.kind == "game_over":
                result = self.classify_game_over(decisive_result="win")
                self.log(f"result: {result} (public game-over screen)")
                return result
            if event.kind == "turn_end":
                if enemy_attack_target == "bomb":
                    if not enemy_bomb_died_without_move:
                        terminal = self.await_opponent_bomb_resolution()
                        if terminal.kind != "turn_end":
                            result = (
                                self.classify_game_over(decisive_result="loss")
                                if terminal.kind == "game_over"
                                else "win" if terminal.kind == "out_of_time"
                                else "loss"
                            )
                            self.log(
                                f"result: {result} ({terminal.kind} during "
                                "unlogged Bomb capture)"
                            )
                            return result
                    inferred = self.beliefs.observe_unlogged_bomb_capture()
                    self.log(
                        "opponent Bomb capture inferred after attacker death: "
                        + " ".join(inferred)
                    )
                    self.beliefs.observe_continuation()
                    enemy_deaths.clear()
                    enemy_attack_target = None
                    enemy_bomb_died_without_move = False
                    ghost_became_visible = False
                    continue
                if (enemy_bot_action is not None and
                        enemy_bot_action.piece == "parasite"):
                    # Parasite possession leaves the victim on its square with
                    # a new team and kills the Parasite before Character.Move
                    # can print a coordinate-bearing callback.  The public
                    # release diagnostic is therefore the authoritative move.
                    self.beliefs.observe_unlogged_parasite_possession(
                        enemy_bot_action
                    )
                    self.log(
                        "opponent Parasite possession: "
                        f"{enemy_bot_action.source}-{enemy_bot_action.target}"
                    )
                    self.beliefs.observe_continuation()
                    enemy_deaths.clear()
                    enemy_bot_action = None
                    enemy_attack_target = None
                    enemy_bomb_died_without_move = False
                    ghost_became_visible = False
                    continue
                if (enemy_bot_action is not None and
                        enemy_bot_action.piece != "ghost"):
                    # Some blind collisions kill the visible moving model
                    # before HasMovedHandler can emit its ordinary record. A
                    # captured Ranked trace had Sludge c5-c4 collide with our
                    # known hidden Ghost: both died, then ChangeTurn ended.
                    # The release action is public for a non-Ghost actor and
                    # identifies the exact legal engine transition. Never use
                    # this fallback for a quiet invisible enemy Ghost.
                    self.beliefs.observe_unlogged_public_action(
                        enemy_bot_action
                    )
                    self.log(
                        f"opponent unlogged {enemy_bot_action.piece} action: "
                        f"{enemy_bot_action.source}-{enemy_bot_action.target}"
                    )
                    self.beliefs.observe_continuation()
                    enemy_deaths.clear()
                    enemy_bot_action = None
                    enemy_attack_target = None
                    enemy_bomb_died_without_move = False
                    ghost_became_visible = False
                    continue
                if enemy_deaths:
                    inferred = self.beliefs.observe_unlogged_death_action(
                        enemy_deaths, enemy_attack_target
                    )
                    self.log(
                        "opponent action inferred from public deaths "
                        f"{','.join(sorted(enemy_deaths))}: "
                        + " ".join(inferred)
                    )
                    self.beliefs.observe_continuation()
                    enemy_deaths.clear()
                    enemy_bot_action = None
                    enemy_attack_target = None
                    enemy_bomb_died_without_move = False
                    ghost_became_visible = False
                    continue
                # A continuing turn is public proof that a captured ambiguous
                # royal was the Jester rather than the real King.
                self.beliefs.observe_continuation()
                continue
            if event.kind == "attack":
                # Native ``attacking <prefab>`` identifies the public target.
                # Keep the identity as well as the fact of an attack so a Bomb
                # attacking, or being attacked, can use the longer resolution
                # barrier below.
                enemy_attack_target = event.piece
                continue
            if event.kind == "dead":
                if event.piece:
                    enemy_deaths.add(public_probe_piece(event.piece))
                if enemy_attack_target == "bomb" and event.piece == "bomb":
                    enemy_bomb_died_without_move = True
                continue
            if event.kind == "ghost_visible":
                ghost_became_visible = True
                continue
            if event.kind == "ghost_hidden":
                continue
            if event.kind != "move":
                continue
            if event.piece == "minion":
                # Minions advance automatically at their side's turn start.
                # The preceding engine move already applied this transition,
                # so the native animation/log is synchronization only.
                if self.verbose:
                    self.log("opponent minion auto-advance observed")
                continue
            if (event.piece == "devil" and event.source == event.target and
                    event.source is not None):
                # Native Character.Move logs a Devil spawn as source->source;
                # the generated Minion is public but its destination is not in
                # that log.  Wait for the completed action, then inspect only
                # engine-legal spawn cells for the visible Minion.
                terminal = self.events.wait(
                    ("turn_end", "game_over", "out_of_time"), 5.0)
                if terminal.kind != "turn_end":
                    result = (self.classify_game_over(decisive_result="loss")
                              if terminal.kind == "game_over" else "win")
                    self.log(f"result: {result} ({terminal.kind} during Devil spawn)")
                    return result
                candidates = self.beliefs.special_targets(
                    "devil", event.source, "@")
                target = self.diagnostic_special_target(
                    "devil", event.source, candidates, enemy_bot_action
                )
                if target is None:
                    target = self.locate_generated_piece_by_outline(
                        "minion", candidates
                    )
                if self.verbose:
                    self.log(f"opponent devil spawn: {event.source}@{target}")
                self.beliefs.observe_move(
                    AppEvent("move", "devil", event.source, target, event.raw),
                    False,
                )
                self.beliefs.observe_continuation()
                enemy_deaths.clear()
                enemy_attack_target = None
                enemy_bot_action = None
                ghost_became_visible = False
                continue
            if (event.piece == "fisherman" and event.source == event.target and
                    event.source is not None):
                # A hook returns the Fisherman to its source and emits no
                # coordinate-bearing move for the pulled character. The moved
                # character is nevertheless public at the adjacent ray cell.
                # Resolve a unique legal hook directly; otherwise identify it
                # from that public landing and its already-public piece class.
                terminal = self.events.wait(
                    ("turn_end", "game_over", "out_of_time"), 5.0)
                if terminal.kind != "turn_end":
                    result = (self.classify_game_over(decisive_result="loss")
                              if terminal.kind == "game_over" else "win")
                    self.log(f"result: {result} ({terminal.kind} during Fisherman hook)")
                    return result
                candidates = self.beliefs.special_targets(
                    "fisherman", event.source, "!")
                if not candidates:
                    raise RuntimeError(
                        "stay-put Fisherman action has no legal hook target"
                    )
                target = self.diagnostic_special_target(
                    "fisherman", event.source, candidates, enemy_bot_action
                )
                if target is None and len(candidates) == 1:
                    target = candidates[0]
                if target is None:
                    raise RuntimeError(
                        "public Fisherman target diagnostic is absent or illegal"
                    )
                if self.verbose:
                    self.log(f"opponent fisherman hook: {event.source}!{target}")
                self.beliefs.observe_move(
                    AppEvent(
                        "move", "fisherman", event.source, target, event.raw
                    ),
                    False,
                )
                self.beliefs.observe_continuation()
                enemy_deaths.clear()
                enemy_attack_target = None
                enemy_bot_action = None
                ghost_became_visible = False
                continue
            if (event.piece == "mage" and event.source == event.target and
                    event.source is not None):
                # Mage swaps use the same native stay-put diagnostic as other
                # special actions.  After the animation, the Mage itself is a
                # public marker at the engine's ``~`` target, so locate it only
                # among legal swap destinations.
                terminal = self.events.wait(
                    ("turn_end", "game_over", "out_of_time"), 5.0)
                if terminal.kind != "turn_end":
                    result = (self.classify_game_over(decisive_result="loss")
                              if terminal.kind == "game_over" else "win")
                    self.log(f"result: {result} ({terminal.kind} during Mage swap)")
                    return result
                candidates = self.beliefs.special_targets(
                    "mage", event.source, "~")
                target = self.diagnostic_special_target(
                    "mage", event.source, candidates, enemy_bot_action
                )
                if target is None and len(candidates) == 1:
                    target = candidates[0]
                if target is None:
                    raise RuntimeError(
                        "public Mage target diagnostic is absent or illegal"
                    )
                if self.verbose:
                    self.log(f"opponent mage swap: {event.source}~{target}")
                self.beliefs.observe_move(
                    AppEvent("move", "mage", event.source, target, event.raw),
                    False,
                )
                self.beliefs.observe_continuation()
                enemy_deaths.clear()
                enemy_attack_target = None
                enemy_bot_action = None
                ghost_became_visible = False
                continue
            if (event.piece == "sniper" and event.source == event.target and
                    event.source is not None):
                # A forward shot also leaves the Sniper in place, but unlike a
                # Devil spawn its first visible forward victim is unique.
                targets = self.beliefs.special_targets(
                    "sniper", event.source, "x")
                if len(targets) != 1:
                    if is_delayed_local_sniper_callback(
                            event, targets, enemy_bot_action):
                        # Bomb death can invoke Sniper.HasMovedHandler a second
                        # time just after our completed turn barrier. The
                        # duplicate has no preceding remote Bot.RecordAiMove
                        # diagnostic and no legal target on the opponent turn.
                        # Ignore only that proven-stale combination; an actual
                        # opponent Sniper action always supplies its public
                        # release diagnostic before the stay-put callback.
                        if self.verbose:
                            self.log(
                                "ignored delayed local Sniper stay-put callback"
                            )
                        continue
                    raise RuntimeError(
                        f"stay-put Sniper shot has ambiguous targets: {targets}")
                event = AppEvent(
                    "move", "sniper", event.source, targets[0], event.raw)
            consumed_turn_end = False
            if event.piece == "ghost" and enemy_attack_target is None:
                # Ghost visibility is resolved during its animation, after the
                # ordinary move record. Buffer through ChangeTurn so a newly
                # rendered destination is incorporated in the same update.
                while True:
                    terminal = self.events.wait(
                        ("ghost_visible", "ghost_hidden", "turn_end",
                         "terminal_label", "game_over", "out_of_time"),
                        5.0,
                    )
                    if terminal.kind == "ghost_visible":
                        ghost_became_visible = True
                        continue
                    if terminal.kind == "ghost_hidden":
                        continue
                    if terminal.kind == "turn_end":
                        consumed_turn_end = True
                        break
                    if terminal.kind == "terminal_label":
                        result = "draw" if terminal.source == "draw" else "loss"
                        self.log(f"result: {result} ({terminal.source})")
                        return result
                    result = (self.classify_game_over(decisive_result="loss")
                              if terminal.kind == "game_over"
                              else "win" if terminal.kind == "out_of_time" else "loss")
                    self.log(f"result: {result} ({terminal.kind} after opponent move)")
                    return result

            reveal_destination = (
                event.piece == "ghost" and ghost_became_visible
                and enemy_attack_target is None
            )
            conceal = (event.piece == "ghost" and enemy_attack_target is None
                       and not reveal_destination)
            bomb_resolution = (
                self.beliefs.observation_causes_bomb_detonation(event)
                or enemy_attack_target == "bomb"
                or (event.piece == "bomb" and enemy_attack_target is not None)
            )
            if self.verbose:
                if reveal_destination:
                    public = f"public destination {event.target}"
                else:
                    public = ("hidden origin/destination" if conceal
                              else f"{event.source}-{event.target}")
                self.log(f"opponent {event.piece}: {public}")
            if reveal_destination:
                assert event.target
                self.beliefs.observe_visible_ghost_destination(event.target)
            else:
                self.beliefs.observe_move(event, conceal)
            enemy_bot_action = None
            enemy_deaths.clear()
            enemy_attack_target = None
            enemy_bomb_died_without_move = False
            ghost_became_visible = False
            if self.beliefs.side == "b":
                # Prince double-steps and compulsory Checker chains retain the
                # same native turn until their queued continuation completes.
                # Consume the next move record instead of waiting for a turn
                # event that cannot be emitted yet.
                continue
            if consumed_turn_end:
                self.beliefs.observe_continuation()
                continue
            if bomb_resolution:
                terminal = self.await_opponent_bomb_resolution()
                if terminal.kind == "terminal_label":
                    result = "draw" if terminal.source == "draw" else "loss"
                    self.log(f"result: {result} ({terminal.source})")
                    return result
                if terminal.kind != "turn_end":
                    result = (self.classify_game_over(decisive_result="loss")
                              if terminal.kind == "game_over"
                              else "win" if terminal.kind == "out_of_time" else "loss")
                    self.log(
                        f"result: {result} ({terminal.kind} during Bomb resolution)"
                    )
                    return result
                self.beliefs.observe_continuation()
                continue
            # The move log is emitted before Unity finishes its board animation
            # and hands input to the next player.  Synchronizing on the native
            # turn event is both faster and more reliable than a fixed delay.
            terminal = self.events.wait(
                ("turn_end", "terminal_label", "game_over", "out_of_time"), 5.0)
            if terminal.kind == "terminal_label":
                result = "draw" if terminal.source == "draw" else "loss"
                self.log(f"result: {result} ({terminal.source})")
                return result
            if terminal.kind != "turn_end":
                result = (self.classify_game_over(decisive_result="loss")
                          if terminal.kind == "game_over"
                          else "win" if terminal.kind == "out_of_time" else "loss")
                self.log(f"result: {result} ({terminal.kind} after opponent move)")
                return result
            self.beliefs.observe_continuation()

    def close(self) -> None:
        self.engine.close()
        self.events.close()
        self.adb.close()


def parse_team(text: str) -> list[tuple[str, str]]:
    team = []
    for entry in text.split(";"):
        if not entry.strip():
            continue
        try:
            piece, square = (part.strip() for part in entry.split(",", 1))
        except ValueError as exc:
            raise argparse.ArgumentTypeError(f"invalid team entry: {entry}") from exc
        if piece not in PIECE_COST:
            raise argparse.ArgumentTypeError(f"unknown piece: {piece}")
        square_to_scene_index(square)
        team.append((piece, square))
    return team


def find_adb(explicit: str | None) -> str:
    if explicit:
        return explicit
    candidates = [
        os.environ.get("ADB"),
        str(Path.home() / "Library/Android/sdk/platform-tools/adb"),
        "adb",
    ]
    for candidate in candidates:
        if not candidate:
            continue
        if candidate == "adb" or Path(candidate).is_file():
            return candidate
    raise FileNotFoundError("adb was not found; pass --adb")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("probe", "play", "cpu", "unranked",
                                             "ranked", "queued", "farm", "unlock"),
                        help=("probe/play, start Very Hard CPU, Unranked, or Ranked, "
                              "farm Unranked keys, or spend earned character keys"))
    parser.add_argument("--device", required=True, help="adb device serial")
    parser.add_argument("--adb", help="path to adb")
    parser.add_argument("--engine", default=str(Path(__file__).parents[1] / "src/ultimatefish"))
    parser.add_argument("--depth", type=int, default=6)
    parser.add_argument("--nodes", type=int, default=0)
    parser.add_argument(
        "--movetime-ms", type=int,
        help=("hard iterative-deepening budget per move; omitted or zero is "
              "unlimited"),
    )
    parser.add_argument("--belief-limit", type=int, default=64)
    parser.add_argument("--enemy-material", type=int,
                        help="public red material total; enables hidden-Ghost beliefs")
    parser.add_argument("--position", action="append", default=[],
                        help="resume from a lossless UPN belief instead of probing")
    parser.add_argument(
        "--structured-state", action="store_true",
        help=("opt into a supplied serialized OnStartGame/replay diagnostic; "
              "stock app play uses the public tap verifier by default"),
    )
    parser.add_argument("--own-team", type=parse_team,
                        default=list(DEFAULT_OWN_TEAM),
                        help=("semicolon-separated piece,square entries; CPU/Unranked "
                              "installs a non-default team into the native builder"))
    parser.add_argument(
        "--use-saved-army", action="store_true",
        help=("use the app's already-saved army instead of rebuilding it; "
              "the live board is still verified before search"),
    )
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--games", type=int, default=1,
                        help="number of games for the cpu, unranked, ranked, or farm command")
    parser.add_argument("--keys", type=int, default=1,
                        help="earned character keys to spend for the unlock command")
    args = parser.parse_args()
    try:
        args.movetime_ms = controller_movetime(args.command, args.movetime_ms)
    except ValueError as exc:
        parser.error(str(exc))

    game = PhoneGame(find_adb(args.adb), args.device, args.engine,
                     BoardGeometry(), args.own_team, args.depth, args.nodes,
                     args.movetime_ms, args.belief_limit, args.verbose,
                     configure_army=(
                         False if args.use_saved_army else None
                     ))
    stopping = False

    def stop(_signum, _frame):
        nonlocal stopping
        if stopping:
            raise KeyboardInterrupt
        stopping = True
        game.close()
        raise SystemExit(130)

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    try:
        if args.command == "unlock":
            game.unlock_characters(args.keys)
        elif args.command == "ranked":
            for number in range(args.games):
                if args.games > 1:
                    print(f"ranked game {number + 1}/{args.games}", flush=True)
                game.start_ranked()
                draft_result = game.run_ranked_draft()
                if isinstance(draft_result, OpeningTerminal):
                    continue
                structured = game.events.ranked_state() if args.structured_state else None
                if structured is not None:
                    if game.online_local_team is None:
                        raise RuntimeError("Ranked draft side was not recorded")
                    structured = complete_ranked_start(structured)
                    # Buffered groups are first inspected after the final
                    # Ranked spawn acknowledgement publicly reveals both
                    # armies. Enemy private identities and Ghost coordinates
                    # are sanitized before engine UPN.
                    side = "w" if game.online_local_team == 0 else "b"
                    game.initialize_online(
                        structured, game.online_local_team, side
                    )
                else:
                    game.calibrate_perspective(15.0)
                    # The local army is public to its owner on both sides.
                    # Verify it before trusting drafted/saved identities; the
                    # gameplay journal remains non-consuming if Ivory moves
                    # during this short Onyx scan.
                    game.verify_own_team()
                    # Each committed group supplied a public Square.Spawn
                    # snapshot. Use the final one directly; draft pots overlap
                    # the shallow board and make tap reconstruction needlessly
                    # ambiguous.
                    if game.online_local_team is None:
                        raise RuntimeError("Ranked draft side was not recorded")
                    if game.perspective_flipped:
                        # Let the public first turn settle, initialize the
                        # pre-move locked snapshot, then replay that action.
                        opening = game.await_onyx_opening()
                        if isinstance(opening, OpeningTerminal):
                            continue
                        game.initialize_ranked_public("b")
                        game.events.replay_gameplay_journal()
                    else:
                        game.initialize_ranked_public("w")
                game.play()
        elif args.command in ("unranked", "queued", "farm"):
            games = args.games if args.command in ("unranked", "farm") else 1
            for number in range(games):
                result: str | None = None
                if games > 1:
                    print(f"unranked game {number + 1}/{games}", flush=True)
                if args.command == "queued" and number == 0:
                    game.log("waiting for queued Unranked game")
                    # This command is explicitly for an already-visible live
                    # board.  Native-acknowledged perspective calibration below
                    # is a faster and stricter readiness test than screenshot
                    # OCR, and avoids burning the current action clock.
                    game.events.drain()
                else:
                    game.start_unranked()
                structured = game.events.start_state() if args.structured_state else None
                if args.position:
                    game.calibrate_perspective()
                    game.initialize_position(args.position)
                elif structured is not None:
                    local_team = infer_online_local_team(
                        structured, game.own_team
                    )
                    game.initialize_online(
                        structured, local_team,
                        "w" if local_team == 0 else "b",
                    )
                else:
                    game.calibrate_perspective()
                    if game.army_verified_pre_ready:
                        game.log(
                            "using setup-editor own-army verification; "
                            "skipping mutable live-board probe"
                        )
                    else:
                        game.verify_own_team()
                    if game.perspective_flipped:
                        opening = game.await_onyx_opening()
                        if isinstance(opening, OpeningTerminal):
                            result = opening.result
                        else:
                            game.initialize(
                                args.enemy_material, "b", opening_events=opening,
                            )
                            game.events.replay_gameplay_journal()
                    else:
                        game.initialize(args.enemy_material, "w")
                if result is None:
                    result = game.play()
                if args.command == "farm":
                    # Match-made results award one character key.  Give the
                    # result RPC/animation a moment to settle, then use only
                    # the guarded one-key shop flow before re-queueing.
                    time.sleep(1.0)
                    game.unlock_characters(1)
        elif args.command == "cpu":
            for number in range(args.games):
                if args.games > 1:
                    print(f"game {number + 1}/{args.games}", flush=True)
                game.start_very_hard_cpu()
                game.calibrate_perspective()
                game.verify_own_team()
                if game.perspective_flipped:
                    opening = game.await_onyx_opening()
                    if isinstance(opening, OpeningTerminal):
                        continue
                    game.initialize(
                        args.enemy_material, "b", opening_events=opening,
                    )
                    game.events.replay_gameplay_journal()
                else:
                    game.initialize(args.enemy_material)
                game.play()
        else:
            if args.position:
                game.initialize_position(args.position)
            else:
                game.initialize(args.enemy_material)
            if args.command == "probe":
                assert game.beliefs
                for position in game.beliefs.positions:
                    print(position)
            else:
                game.play()
    finally:
        if not stopping:
            game.close()


if __name__ == "__main__":
    main()
