#!/usr/bin/env python3
# Run from the repository root with:
#   python3 tools/tablebases/plot_ultimate_tablebases.py
# Optional: choose a destination or resolution with --output PATH and --scale N.
"""Render the README tablebase summary as three endgame outcome grids."""

from __future__ import annotations

import argparse
from html import escape
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Literal

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:  # pragma: no cover - depends on the local environment
    raise SystemExit("Pillow is required: python3 -m pip install Pillow") from exc


ROOT = Path(__file__).resolve().parents[2]
TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

from plan_ultimate_tablebases import (  # noqa: E402
    DEFERRED_DYNAMIC_K2,
    PIECES,
    angel_candidates,
    devil_candidates,
    deferred_material,
    inventory,
    mirror_copycat_candidates,
    stateful_candidates,
    sufficient_pair,
)

Kind = Literal[
    "win",
    "win_star",
    "win_mostly",
    "no_forced_loss",
    "draw",
    "mixed",
    "no_forced_win",
    "loss_mostly",
    "loss_star",
    "loss",
    "computing",
    "unknown",
]


@dataclass(frozen=True)
class WDL:
    wins: int
    losses: int
    draws: int

    @property
    def total(self) -> int:
        return self.wins + self.losses + self.draws


@dataclass(frozen=True)
class ReadmeResult:
    first_starts: WDL
    second_starts: WDL
    status: str = "certified"


@dataclass(frozen=True)
class Cell:
    kind: Kind
    first: WDL | None = None
    second: WDL | None = None


COLORS = {
    "page": "#F4F7FA",
    "ink": "#172536",
    "muted": "#5C6B7A",
    "header": "#173B5E",
    "row_header": "#E5EEF6",
    "grid": "#AAB8C5",
    "win": "#5FAF32",
    "win_star": "#91C655",
    "win_mostly": "#B8DA86",
    "no_forced_loss": "#E0EFC4",
    "draw": "#DCE2E8",
    "mixed": "#FFE3A1",
    "no_forced_win": "#FCE2CE",
    "loss_mostly": "#F5BE98",
    "loss_star": "#E98B58",
    "loss": "#D15B3B",
    "computing": "#C9D8E6",
    "unknown": "#FFFFFF",
}

PLOT_SCOPE_CAPTION = (
    "Reachable positions only · Immediate stalemates and forced one-ply/tactical "
    "material simplifications are excluded · Prince: cont=0 starting boundaries "
    "only · Copycat: starts with one mirrored pair · Devil: own spawned Minions "
    "only; starts on ranks 1-3"
)

PIECE_LABELS = {piece.name: piece.name.title() for piece in PIECES}
PIECE_BY_NAME = {piece.name: piece for piece in PIECES}
PIECE_INDEX = {piece.name: index for index, piece in enumerate(PIECES)}
BERSERKER_RADII = tuple(range(1, 11))
BERSERKER_PLOTTED_RADII = tuple(range(1, 9))
BERSERKER_RADIUS_ROWS = tuple(
    f"berserker_radius_{radius}" for radius in BERSERKER_PLOTTED_RADII
)
GIANT_START_ROWS = tuple(f"giant_start_{size}" for size in (20, 16, 15, 12))
DEVIL_MINION_ROWS = tuple(f"devil_minions_{count}" for count in range(6))
CHECKER_START_ROWS = ("checker_normal", "checker_king")
SNIPER_START_ROWS = tuple(f"sniper_rank_{rank}" for rank in range(1, 4))
ANGEL_MIRROR_FILES = {"a": "h", "b": "g", "c": "f", "d": "e"}
ANGEL_START_SQUARES = tuple(
    f"{file}{rank}" for rank in range(1, 4) for file in "abcd"
)
ANGEL_START_ROWS = tuple(
    f"angel_square_{square}" for square in ANGEL_START_SQUARES
)
PIECE_LABELS.update(
    {
        row: f"Berserker (radius {radius})"
        for radius, row in zip(BERSERKER_PLOTTED_RADII, BERSERKER_RADIUS_ROWS)
    }
)
PIECE_LABELS.update(
    {
        row: f"Angel ({square}/{ANGEL_MIRROR_FILES[square[0]]}{square[1:]})"
        for row, square in zip(ANGEL_START_ROWS, ANGEL_START_SQUARES)
    }
)
PIECE_LABELS.update(
    {"checker_normal": "Checker (normal)", "checker_king": "Checker King"}
)
PIECE_LABELS.update(
    {
        row: f"Sniper (rank {rank})"
        for row, rank in zip(SNIPER_START_ROWS, range(1, 4))
    }
)
PIECE_LABELS.update(
    {row: f"Giant-{size}" for row, size in zip(GIANT_START_ROWS, (20, 16, 15, 12))}
)
PIECE_LABELS.update(
    {
        row: f"Devil + {count} Minion{'s' if count != 1 else ''}"
        for row, count in zip(DEVIL_MINION_ROWS, range(6))
    }
)


def parse_wdl(text: str, *, require_trivial: bool = False) -> WDL:
    """Parse plotted W/L/D after subtracting `[trivial]` positions.

    Parenthesized unreachable counts are outside the admitted count and remain
    excluded as before. Square-bracketed counts are an authenticated subset of
    the admitted count and are deliberately removed from the visualization.
    """
    values: list[int] = []
    for component in text.split("/"):
        match = re.fullmatch(
            r"\s*([0-9][0-9,]*)(?: \[([0-9][0-9,]*)\])?" r"(?: \(([0-9][0-9,]*)\))?\s*",
            component,
        )
        if not match:
            raise ValueError(f"invalid W/L/D value: {text!r}")
        if require_trivial and match.group(2) is None:
            raise ValueError(f"W/L/D lacks explicit trivial count: {text!r}")
        admitted = int(match.group(1).replace(",", ""))
        trivial = int(match.group(2).replace(",", "")) if match.group(2) else 0
        if trivial > admitted:
            raise ValueError(f"trivial W/L/D exceeds admitted count: {text!r}")
        values.append(admitted - trivial)
    if len(values) != 3:
        raise ValueError(f"expected three W/L/D values: {text!r}")
    return WDL(*values)


def read_summary(path: Path) -> dict[str, ReadmeResult]:
    results: dict[str, ReadmeResult] = {}
    text = path.read_text(encoding="utf-8")
    start_marker = "<!-- GENERATED_TABLE_START -->"
    end_marker = "<!-- GENERATED_TABLE_END -->"
    if start_marker not in text or end_marker not in text:
        raise ValueError(f"generated table markers missing from {path}")
    generated = text.split(start_marker, 1)[1].split(end_marker, 1)[0]
    for line in generated.splitlines():
        if not line.startswith("|") or ".uftb`" not in line:
            continue
        fields = [field.strip() for field in line.strip().strip("|").split("|")]
        if len(fields) < 6:
            continue
        filename = fields[0].strip("`")
        results[filename] = ReadmeResult(parse_wdl(fields[3]), parse_wdl(fields[4]))
    if not results:
        raise ValueError(f"no generated tablebase summary rows found in {path}")
    ledger_start = "<!-- COMPUTATION_LEDGER_START -->"
    ledger_end = "<!-- COMPUTATION_LEDGER_END -->"
    if ledger_start in text and ledger_end in text:
        ledger = text.split(ledger_start, 1)[1].split(ledger_end, 1)[0]
        for line in ledger.splitlines():
            if not line.startswith("| `"):
                continue
            fields = [field.strip() for field in line.strip().strip("|").split("|")]
            if len(fields) != 11 or fields[3] == "—":
                continue
            filename = fields[3].strip("`")
            key = fields[0].strip("`")
            status = fields[4].strip("*").lower()
            # Every certified result-bearing plot cell must explicitly carry
            # authenticated trivial counts.  Do not key this invariant to a
            # list of known result-domain labels: a new exact solver domain
            # must not silently fall back to plotting its full closure census.
            if status == "certified" and fields[7] != "—" and fields[8] != "—":
                try:
                    parse_wdl(fields[7], require_trivial=True)
                    parse_wdl(fields[8], require_trivial=True)
                except ValueError as exc:
                    raise ValueError(
                        f"certified row lacks trivial counts: {filename}"
                    ) from exc
            # The computation ledger is canonical.  In particular, certified
            # S3-only payloads no longer have to appear in the legacy generated
            # local-file summary, but their exact W/L/D values still belong in
            # the plot.
            if fields[7] != "—" and fields[8] != "—":
                raw = ReadmeResult(parse_wdl(fields[7]), parse_wdl(fields[8]), status)
            else:
                previous = results.get(filename)
                if previous is None:
                    raw = ReadmeResult(WDL(0, 0, 0), WDL(0, 0, 0), status)
                else:
                    raw = ReadmeResult(
                        previous.first_starts, previous.second_starts, status
                    )
            results[filename] = raw
            # The complete stateful Devil runtime is twelve canonical UFDS
            # partitions bound by a class certificate.  Its logical planner
            # record retains the historical kdevilk.uftb name, which denotes
            # only an excluded minion-free entry-root projection.  Alias the
            # certificate's audited, root-filtered ledger result solely for
            # catalog lookup; never read the projection's old outcome cells.
            if key == "single:devil":
                results["kdevilk.uftb"] = raw
    return results


def read_berserker_radii(path: Path) -> dict[tuple[str, int], ReadmeResult]:
    """Read exact, reachability-filtered Berserker power slices."""
    document = json.loads(path.read_text(encoding="utf-8"))
    if (
        document.get("schema") != 3
        or document.get("semantics") != "reachability-admitted-minus-trivial-v3"
        or document.get("radius_to_power_substate") != {
            str(radius): radius - 1 for radius in BERSERKER_RADII
        }
        or document.get("saturated_radius") != 10
    ):
        raise ValueError(f"unsupported Berserker radius summary schema: {path}")
    results: dict[tuple[str, int], ReadmeResult] = {}
    for filename, record in document.get("files", {}).items():
        if record.get("excluded"):
            continue
        rows = record.get("radii", {})
        if set(rows) != {str(radius) for radius in BERSERKER_RADII}:
            raise ValueError(f"incomplete Berserker radius coverage: {filename}")
        for radius_text, raw in rows.items():
            radius = int(radius_text)
            if raw.get("power_substate") != radius - 1:
                raise ValueError(
                    f"invalid Berserker radius mapping: {filename} radius {radius}"
                )
            sides = []
            for key in ("first_starts", "second_starts"):
                side = raw[key]
                admitted = side["admitted"]
                trivial = side["trivial"]
                display = side["display"]
                if not {"total", "excluded"} <= set(side):
                    raise ValueError(
                        f"missing Berserker total/excluded counts for "
                        f"{filename} radius {radius} {key}"
                    )
                total = side["total"]
                excluded = side["excluded"]
                for field in ("wins", "losses", "draws"):
                    if (
                        trivial[field] > admitted[field]
                        or display[field] != admitted[field] - trivial[field]
                        or excluded[field] > total[field]
                        or admitted[field] != total[field] - excluded[field]
                    ):
                        raise ValueError(
                            f"invalid Berserker radius conservation for "
                            f"{filename} radius {radius} {key} {field}"
                        )
                sides.append(WDL(display["wins"], display["losses"], display["draws"]))
            results[(filename, radius)] = ReadmeResult(
                sides[0],
                sides[1],
            )
    return results


def read_giant_start_classes(path: Path) -> dict[tuple[str, int], ReadmeResult]:
    """Read exact, reachability-filtered Giant root-anchor parity slices."""
    document = json.loads(path.read_text(encoding="utf-8"))
    if (
        document.get("schema") != 1
        or document.get("semantics") != "reachability-admitted-minus-trivial-v3"
        or tuple(document.get("class_sizes", ())) != (20, 16, 15, 12)
    ):
        raise ValueError(f"unsupported Giant start-class summary schema: {path}")
    results: dict[tuple[str, int], ReadmeResult] = {}
    for filename, record in document.get("files", {}).items():
        if record.get("excluded"):
            continue
        seen: set[int] = set()
        for class_text, raw in record.get("classes", {}).items():
            giant_class = int(class_text)
            if giant_class not in (20, 16, 15, 12):
                raise ValueError(
                    f"unsupported Giant start class {giant_class}: {filename}"
                )
            if giant_class in seen:
                raise ValueError(
                    f"duplicate Giant start class {giant_class}: {filename}"
                )
            seen.add(giant_class)
            sides = []
            for key in ("first_starts", "second_starts"):
                side = raw[key]
                admitted = side["admitted"]
                trivial = side["trivial"]
                display = side["display"]
                for field in ("wins", "losses", "draws"):
                    if (
                        trivial[field] > admitted[field]
                        or display[field] != admitted[field] - trivial[field]
                    ):
                        raise ValueError(
                            f"invalid Giant start-class conservation for "
                            f"{filename} class {giant_class} {key} {field}"
                        )
                sides.append(WDL(display["wins"], display["losses"], display["draws"]))
            results[(filename, giant_class)] = ReadmeResult(sides[0], sides[1])
        if seen != {20, 16, 15, 12}:
            raise ValueError(f"incomplete Giant start-class coverage: {filename}")
    return results


def read_devil_minion_starts(path: Path) -> dict[tuple[str, int], ReadmeResult]:
    """Read exact alive-Devil root cohorts by current spawned Minion count."""
    document = json.loads(path.read_text(encoding="utf-8"))
    expected_counts = tuple(range(6))
    if (
        document.get("schema") != 2
        or document.get("semantics")
        != "stateful-reachability-admitted-minus-trivial-v1"
        or tuple(document.get("minion_counts", ())) != expected_counts
        or set(document.get("files", {})) != {"kdevilk.uftb"}
    ):
        raise ValueError(f"unsupported Devil Minion-start summary schema: {path}")
    results: dict[tuple[str, int], ReadmeResult] = {}
    for filename, record in document["files"].items():
        rows = record.get("minion_counts", {})
        if set(rows) != {str(count) for count in expected_counts}:
            raise ValueError(f"incomplete Devil Minion-start coverage: {filename}")
        for count in expected_counts:
            raw = rows[str(count)]
            sides = []
            for key in ("first_starts", "second_starts"):
                side = raw[key]
                total = side["total"]
                excluded = side["excluded"]
                admitted = side["admitted"]
                trivial = side["trivial"]
                display = side["display"]
                for field in ("wins", "losses", "draws"):
                    if (
                        excluded[field] > total[field]
                        or admitted[field] != total[field] - excluded[field]
                        or trivial[field] > admitted[field]
                        or display[field] != admitted[field] - trivial[field]
                    ):
                        raise ValueError(
                            f"invalid Devil Minion-start conservation for "
                            f"{filename} count {count} {key} {field}"
                        )
                sides.append(WDL(display["wins"], display["losses"], display["draws"]))
            results[(filename, count)] = ReadmeResult(sides[0], sides[1])
    return results


def read_checker_start_states(path: Path) -> dict[tuple[str, str], ReadmeResult]:
    """Read exact normal-Checker and Checker-King root-state slices."""
    document = json.loads(path.read_text(encoding="utf-8"))
    expected_states = ("normal", "king")
    if (
        document.get("schema") != 1
        or document.get("semantics") != "reachability-admitted-minus-trivial-v3"
        or tuple(document.get("start_states", ())) != expected_states
        or document.get("substate_groups") != {"normal": [0, 1], "king": [2, 3]}
    ):
        raise ValueError(f"unsupported Checker start-state summary schema: {path}")
    results: dict[tuple[str, str], ReadmeResult] = {}
    for filename, record in document.get("files", {}).items():
        if record.get("excluded"):
            continue
        rows = record.get("start_states", {})
        if set(rows) != set(expected_states):
            raise ValueError(f"incomplete Checker start-state coverage: {filename}")
        for state in expected_states:
            raw = rows[state]
            sides = []
            for key in ("first_starts", "second_starts"):
                side = raw[key]
                total = side["total"]
                excluded = side["excluded"]
                admitted = side["admitted"]
                trivial = side["trivial"]
                display = side["display"]
                for field in ("wins", "losses", "draws"):
                    if (
                        excluded[field] > total[field]
                        or admitted[field] != total[field] - excluded[field]
                        or trivial[field] > admitted[field]
                        or display[field] != admitted[field] - trivial[field]
                    ):
                        raise ValueError(
                            f"invalid Checker start-state conservation for "
                            f"{filename} {state} {key} {field}"
                        )
                sides.append(WDL(display["wins"], display["losses"],
                                 display["draws"]))
            results[(filename, state)] = ReadmeResult(sides[0], sides[1])
    return results


def read_sniper_start_ranks(path: Path) -> dict[tuple[str, int], ReadmeResult]:
    """Read exact Sniper color-relative root-rank slices."""
    document = json.loads(path.read_text(encoding="utf-8"))
    all_ranks = tuple(range(1, 11))
    plotted_ranks = tuple(range(1, 4))
    if (
        document.get("schema") != 1
        or document.get("semantics") != "reachability-admitted-minus-trivial-v3"
        or tuple(document.get("all_rank_buckets", ())) != all_ranks
        or tuple(document.get("plotted_start_ranks", ())) != plotted_ranks
    ):
        raise ValueError(f"unsupported Sniper start-rank summary schema: {path}")
    results: dict[tuple[str, int], ReadmeResult] = {}
    for filename, record in document.get("files", {}).items():
        if record.get("excluded"):
            continue
        rows = record.get("ranks", {})
        if set(rows) != {str(rank) for rank in all_ranks}:
            raise ValueError(f"incomplete Sniper root-rank coverage: {filename}")
        for rank in all_ranks:
            raw = rows[str(rank)]
            sides = []
            for key in ("first_starts", "second_starts"):
                side = raw[key]
                admitted = side["admitted"]
                trivial = side["trivial"]
                display = side["display"]
                for field in ("wins", "losses", "draws"):
                    if (
                        trivial[field] > admitted[field]
                        or display[field] != admitted[field] - trivial[field]
                    ):
                        raise ValueError(
                            f"invalid Sniper root-rank conservation for "
                            f"{filename} rank {rank} {key} {field}"
                        )
                    if "total" in side or "excluded" in side:
                        if not {"total", "excluded"} <= set(side):
                            raise ValueError(
                                f"partial Sniper total/excluded data for "
                                f"{filename} rank {rank} {key}"
                            )
                        if (side["excluded"][field] > side["total"][field] or
                                admitted[field] != side["total"][field] -
                                side["excluded"][field]):
                            raise ValueError(
                                f"invalid Sniper total/excluded conservation for "
                                f"{filename} rank {rank} {key} {field}"
                            )
                sides.append(WDL(display["wins"], display["losses"],
                                 display["draws"]))
            if rank in plotted_ranks:
                results[(filename, rank)] = ReadmeResult(sides[0], sides[1])
    return results


def read_angel_start_squares(path: Path) -> dict[tuple[str, str], ReadmeResult]:
    """Read exact color-relative, horizontally mirrored Angel root squares."""
    document = json.loads(path.read_text(encoding="utf-8"))
    all_squares = tuple(
        f"{file}{rank}" for rank in range(1, 11) for file in "abcd"
    )
    plotted_squares = ANGEL_START_SQUARES
    expected_symmetry = {
        file: f"{file}/{mirror}" for file, mirror in ANGEL_MIRROR_FILES.items()
    }
    if (
        document.get("schema") != 1
        or document.get("semantics") != "reachability-admitted-minus-trivial-v3"
        or tuple(document.get("all_square_buckets", ())) != all_squares
        or tuple(document.get("plotted_start_squares", ())) != plotted_squares
        or document.get("file_symmetry") != expected_symmetry
    ):
        raise ValueError(f"unsupported Angel start-square summary schema: {path}")
    results: dict[tuple[str, str], ReadmeResult] = {}
    for filename, record in document.get("files", {}).items():
        if record.get("excluded"):
            continue
        rows = record.get("squares", {})
        if set(rows) != set(all_squares):
            raise ValueError(f"incomplete Angel root-square coverage: {filename}")
        for square in all_squares:
            raw = rows[square]
            sides = []
            for key in ("first_starts", "second_starts"):
                side = raw[key]
                admitted = side["admitted"]
                trivial = side["trivial"]
                display = side["display"]
                for field in ("wins", "losses", "draws"):
                    if (
                        trivial[field] > admitted[field]
                        or display[field] != admitted[field] - trivial[field]
                    ):
                        raise ValueError(
                            f"invalid Angel root-square conservation for "
                            f"{filename} square {square} {key} {field}"
                        )
                    if "total" in side or "excluded" in side:
                        if not {"total", "excluded"} <= set(side):
                            raise ValueError(
                                f"partial Angel total/excluded data for "
                                f"{filename} square {square} {key}"
                            )
                        if (side["excluded"][field] > side["total"][field] or
                                admitted[field] != side["total"][field] -
                                side["excluded"][field]):
                            raise ValueError(
                                f"invalid Angel total/excluded conservation for "
                                f"{filename} square {square} {key} {field}"
                            )
                sides.append(WDL(display["wins"], display["losses"],
                                 display["draws"]))
            if square in plotted_squares:
                results[(filename, square)] = ReadmeResult(sides[0], sides[1])
    return results


def row_side_result(raw: ReadmeResult, row_is_primary: bool = True) -> tuple[WDL, WDL]:
    """Return row-side W/L/D for row-to-move, then opponent-to-move.

    README W/L/D is always written from the side-to-move perspective. When the
    other material owner (or bare King) starts, its wins are therefore losses
    for the row side and vice versa.
    """
    first, second = raw.first_starts, raw.second_starts
    if row_is_primary:
        return first, WDL(second.losses, second.wins, second.draws)
    return second, WDL(first.losses, first.wins, first.draws)


def classify(first: WDL, second: WDL, allow_loss: bool) -> Cell:
    if first.total == 0 or second.total == 0:
        # An authenticated slice can legitimately have every position for one
        # starting side removed by the reachability/triviality filters.  Keep
        # that information visible instead of making the cell look uncomputed;
        # cell_text renders the empty cohort as an explicit zero triplet.
        return Cell("mixed", first, second)
    if first.wins == first.total and second.wins == second.total:
        return Cell("win", first, second)
    if first.wins == first.total and second.wins != second.total:
        return Cell("win_star", first, second)
    if first.draws == first.total and second.draws == second.total:
        return Cell("draw", first, second)
    if allow_loss and first.losses == first.total and second.losses == second.total:
        return Cell("loss", first, second)
    if allow_loss and first.losses != first.total and second.losses == second.total:
        return Cell("loss_star", first, second)
    # Keep the exact Win*/Loss* tiers visually distinct while highlighting
    # near-forced outcomes that never cross into the opposite result.  Compare
    # integer products so the 98.5% boundary is deterministic for large tables.
    if (
        first.wins * 1000 >= first.total * 985
        and first.losses == 0
        and second.losses == 0
    ):
        return Cell("win_mostly", first, second)
    if (
        allow_loss
        and second.losses * 1000 >= second.total * 985
        and first.wins == 0
        and second.wins == 0
    ):
        return Cell("loss_mostly", first, second)
    if first.losses == 0 and second.losses == 0:
        return Cell("no_forced_loss", first, second)
    if allow_loss and first.wins == 0 and second.wins == 0:
        return Cell("no_forced_win", first, second)
    return Cell("mixed", first, second)


def known_draw() -> Cell:
    draw = WDL(0, 0, 1)
    return Cell("draw", draw, draw)


class OutcomeCatalog:
    def __init__(
        self,
        summary: dict[str, ReadmeResult],
        berserker_radii: dict[tuple[str, int], ReadmeResult] | None = None,
        giant_start_classes: dict[tuple[str, int], ReadmeResult] | None = None,
        devil_minion_starts: dict[tuple[str, int], ReadmeResult] | None = None,
        checker_start_states: dict[tuple[str, str], ReadmeResult] | None = None,
        sniper_start_ranks: dict[tuple[str, int], ReadmeResult] | None = None,
        angel_start_squares: dict[tuple[str, str], ReadmeResult] | None = None,
    ) -> None:
        self.summary = summary
        self.berserker_radii = berserker_radii or {}
        self.giant_start_classes = giant_start_classes or {}
        self.devil_minion_starts = devil_minion_starts or {}
        self.checker_start_states = checker_start_states or {}
        self.sniper_start_ranks = sniper_start_ranks or {}
        self.angel_start_squares = angel_start_squares or {}
        # Match the canonical ledger's record precedence.  The broad stateful
        # catalog may contain a normalized duplicate for an already generated
        # requested class.  The exact inventory record must win so a certified
        # result cannot be hidden or interpreted with reversed owners.
        records_by_filename: dict[str, dict[str, object]] = {}
        for record in (
            *stateful_candidates(),
            *angel_candidates(),
            *mirror_copycat_candidates(),
            *devil_candidates(),
            *inventory(),
        ):
            records_by_filename[str(record["filename"])] = record
        records = list(records_by_filename.values())
        self.singles = {
            str(record["primary"]): record
            for record in records
            if record["phase"] in {"kings+1", "devil-spawned-closure-v3"}
        }
        self.same_team = {
            tuple(
                sorted(
                    (str(record["primary"]), str(record["secondary"])),
                    key=PIECE_INDEX.__getitem__,
                )
            ): record
            for record in records
            if record["secondary"] and not record["opposing"]
        }
        self.opposing = {
            tuple(
                sorted(
                    (str(record["primary"]), str(record["secondary"])),
                    key=PIECE_INDEX.__getitem__,
                )
            ): record
            for record in records
            if record["secondary"] and record["opposing"]
        }

    def _cell_for_record(
        self,
        record: dict[str, object] | None,
        row_is_primary: bool = True,
        allow_loss: bool = False,
    ) -> Cell:
        if record is None:
            return Cell("unknown")
        raw = self.summary.get(str(record["filename"]))
        if raw is None:
            return Cell("unknown")
        # PRESERVING is still an active, non-final pipeline state.  Its exact
        # result exists, but the authenticated import (and therefore the W/L/D
        # displayed here) is incomplete.  Render it with the same in-progress
        # hatch as COMPUTING instead of leaving a misleading blank cell.
        if raw.status in {"computing", "preserving"}:
            return Cell("computing")
        if raw.status not in {"certified", "preserving", "draw"}:
            return Cell("unknown")
        first, second = row_side_result(raw, row_is_primary)
        return classify(first, second, allow_loss)

    def single(self, name: str) -> Cell:
        if name in DEFERRED_DYNAMIC_K2 and name != "devil":
            return Cell("unknown")
        if not PIECE_BY_NAME[name].decisive:
            return known_draw()
        return self._cell_for_record(self.singles.get(name))

    @staticmethod
    def _radius(row: str) -> int | None:
        if row not in BERSERKER_RADIUS_ROWS:
            return None
        return int(row.rsplit("_", 1)[1])

    def _radius_cell_for_record(
        self,
        record: dict[str, object] | None,
        radius: int,
        row_is_primary: bool = True,
        allow_loss: bool = False,
    ) -> Cell:
        if record is None:
            return Cell("unknown")
        filename = str(record["filename"])
        aggregate = self.summary.get(filename)
        if aggregate is not None and aggregate.status in {"computing", "preserving"}:
            return Cell("computing")
        raw = self.berserker_radii.get((filename, radius))
        if raw is None:
            return Cell("unknown")
        first, second = row_side_result(raw, row_is_primary)
        return classify(first, second, allow_loss)

    @staticmethod
    def _giant_class(row: str) -> int | None:
        if row not in GIANT_START_ROWS:
            return None
        return int(row.rsplit("_", 1)[1])

    def _giant_cell_for_record(
        self,
        record: dict[str, object] | None,
        giant_class: int,
        row_is_primary: bool = True,
        allow_loss: bool = False,
    ) -> Cell:
        if record is None:
            return Cell("unknown")
        filename = str(record["filename"])
        aggregate = self.summary.get(filename)
        if aggregate is not None and aggregate.status in {"computing", "preserving"}:
            return Cell("computing")
        raw = self.giant_start_classes.get((filename, giant_class))
        if raw is None:
            return Cell("unknown")
        first, second = row_side_result(raw, row_is_primary)
        return classify(first, second, allow_loss)

    @staticmethod
    def _devil_minions(row: str) -> int | None:
        if row not in DEVIL_MINION_ROWS:
            return None
        return int(row.rsplit("_", 1)[1])

    def _devil_minion_cell(self, minions: int) -> Cell:
        record = self.singles.get("devil")
        if record is None:
            return Cell("unknown")
        aggregate = self.summary.get(str(record["filename"]))
        if aggregate is not None and aggregate.status in {"computing", "preserving"}:
            return Cell("computing")
        raw = self.devil_minion_starts.get((str(record["filename"]), minions))
        if raw is None:
            return Cell("unknown")
        first, second = row_side_result(raw)
        return classify(first, second, allow_loss=False)

    @staticmethod
    def _checker_state(row: str) -> str | None:
        if row not in CHECKER_START_ROWS:
            return None
        return row.removeprefix("checker_")

    def _checker_cell_for_record(
        self,
        record: dict[str, object] | None,
        state: str,
        row_is_primary: bool = True,
        allow_loss: bool = False,
    ) -> Cell:
        if record is None:
            return Cell("unknown")
        filename = str(record["filename"])
        aggregate = self.summary.get(filename)
        if aggregate is not None and aggregate.status in {"computing", "preserving"}:
            return Cell("computing")
        raw = self.checker_start_states.get((filename, state))
        if raw is None:
            return Cell("unknown")
        first, second = row_side_result(raw, row_is_primary)
        return classify(first, second, allow_loss)

    @staticmethod
    def _sniper_rank(row: str) -> int | None:
        if row not in SNIPER_START_ROWS:
            return None
        return int(row.rsplit("_", 1)[1])

    def _sniper_cell_for_record(
        self,
        record: dict[str, object] | None,
        rank: int,
        row_is_primary: bool = True,
        allow_loss: bool = False,
    ) -> Cell:
        if record is None:
            return Cell("unknown")
        filename = str(record["filename"])
        aggregate = self.summary.get(filename)
        if aggregate is not None and aggregate.status in {"computing", "preserving"}:
            return Cell("computing")
        raw = self.sniper_start_ranks.get((filename, rank))
        if raw is None:
            return Cell("unknown")
        first, second = row_side_result(raw, row_is_primary)
        return classify(first, second, allow_loss)

    @staticmethod
    def _angel_square(row: str) -> str | None:
        if row not in ANGEL_START_ROWS:
            return None
        return row.removeprefix("angel_square_")

    def _angel_cell_for_record(
        self,
        record: dict[str, object] | None,
        square: str,
        row_is_primary: bool = True,
        allow_loss: bool = False,
    ) -> Cell:
        if record is None:
            return Cell("unknown")
        filename = str(record["filename"])
        aggregate = self.summary.get(filename)
        if aggregate is not None and aggregate.status in {"computing", "preserving"}:
            return Cell("computing")
        raw = self.angel_start_squares.get((filename, square))
        if raw is None:
            return Cell("unknown")
        first, second = row_side_result(raw, row_is_primary)
        return classify(first, second, allow_loss)

    def single_row(self, row: str) -> Cell:
        radius = self._radius(row)
        if radius is not None:
            return self._radius_cell_for_record(self.singles.get("berserker"), radius)
        giant_class = self._giant_class(row)
        if giant_class is not None:
            return self._giant_cell_for_record(self.singles.get("giant"), giant_class)
        devil_minions = self._devil_minions(row)
        if devil_minions is not None:
            return self._devil_minion_cell(devil_minions)
        if self._checker_state(row) is not None:
            return known_draw()
        sniper_rank = self._sniper_rank(row)
        if sniper_rank is not None:
            return self._sniper_cell_for_record(
                self.singles.get("sniper"), sniper_rank
            )
        if self._angel_square(row) is not None:
            return known_draw()
        return self.single(row)

    def together_row(self, row: str, column: str) -> Cell:
        if self._devil_minions(row) is not None:
            # Only the certified lone-Devil closure has been sliced. Companion
            # classes deliberately remain blank until their own exact solves.
            return Cell("unknown")
        checker_state = self._checker_state(row)
        if checker_state is not None:
            if column == "checker":
                # Same-team Checkers are exchange-folded, so the row Checker
                # is not distinguishable from the column Checker.
                return self.together("checker", "checker")
            if deferred_material("checker", column):
                return Cell("unknown")
            first, second = sorted(("checker", column), key=PIECE_INDEX.__getitem__)
            if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], True):
                return known_draw()
            return self._checker_cell_for_record(
                self.same_team.get((first, second)), checker_state
            )
        sniper_rank = self._sniper_rank(row)
        if sniper_rank is not None:
            if column == "sniper":
                # Same-team Snipers are exchange-folded, so there is no
                # distinguished row Sniper whose rank can be selected.
                return self.together("sniper", "sniper")
            if deferred_material("sniper", column):
                return Cell("unknown")
            first, second = sorted(("sniper", column), key=PIECE_INDEX.__getitem__)
            if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], True):
                return known_draw()
            record = self.same_team.get((first, second))
            # Both material pieces have the same owner in this grid, so the
            # row side is the primary side even when Sniper occupies the
            # tablebase codec's secondary material slot.
            return self._sniper_cell_for_record(record, sniper_rank)
        angel_square = self._angel_square(row)
        if angel_square is not None:
            if column == "angel":
                return self.together("angel", "angel")
            if deferred_material("angel", column):
                return Cell("unknown")
            first, second = sorted(("angel", column), key=PIECE_INDEX.__getitem__)
            if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], True):
                return known_draw()
            record = self.same_team.get((first, second))
            # Both material pieces have the same owner in this grid, so the
            # row side is the primary side even when Angel occupies the
            # tablebase codec's secondary material slot.
            return self._angel_cell_for_record(record, angel_square)
        radius = self._radius(row)
        if radius is not None:
            if column == "berserker":
                # Two same-team Berserkers are exchange-folded, so there is no
                # distinguished row Berserker to slice.  Repeat the certified
                # aggregate outcome across the ten radius rows instead.
                return self.together("berserker", "berserker")
            if deferred_material("berserker", column):
                return Cell("unknown")
            first, second = sorted(("berserker", column), key=PIECE_INDEX.__getitem__)
            if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], True):
                return known_draw()
            return self._radius_cell_for_record(
                self.same_team.get((first, second)), radius
            )
        giant_class = self._giant_class(row)
        if giant_class is not None:
            if column == "giant":
                return self.together("giant", "giant")
            if deferred_material("giant", column):
                return Cell("unknown")
            first, second = sorted(("giant", column), key=PIECE_INDEX.__getitem__)
            if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], True):
                return known_draw()
            return self._giant_cell_for_record(
                self.same_team.get((first, second)), giant_class
            )
        return self.together(row, column)

    def opposed_row(self, row: str, column: str) -> Cell:
        if self._devil_minions(row) is not None:
            return Cell("unknown")
        checker_state = self._checker_state(row)
        if checker_state is not None:
            if deferred_material("checker", column, opposing=True):
                return Cell("unknown")
            first, second = sorted(("checker", column), key=PIECE_INDEX.__getitem__)
            if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], False):
                return known_draw()
            record = self.opposing.get((first, second))
            return self._checker_cell_for_record(
                record,
                checker_state,
                row_is_primary=(
                    record is not None and str(record["primary"]) == "checker"
                ),
                allow_loss=True,
            )
        sniper_rank = self._sniper_rank(row)
        if sniper_rank is not None:
            if deferred_material("sniper", column, opposing=True):
                return Cell("unknown")
            first, second = sorted(("sniper", column), key=PIECE_INDEX.__getitem__)
            if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], False):
                return known_draw()
            record = self.opposing.get((first, second))
            return self._sniper_cell_for_record(
                record,
                sniper_rank,
                row_is_primary=(
                    record is not None and str(record["primary"]) == "sniper"
                ),
                allow_loss=True,
            )
        angel_square = self._angel_square(row)
        if angel_square is not None:
            if column == "angel":
                return self.opposed("angel", "angel")
            if deferred_material("angel", column, opposing=True):
                return Cell("unknown")
            first, second = sorted(("angel", column), key=PIECE_INDEX.__getitem__)
            if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], False):
                return known_draw()
            record = self.opposing.get((first, second))
            return self._angel_cell_for_record(
                record,
                angel_square,
                row_is_primary=(
                    record is not None and str(record["primary"]) == "angel"
                ),
                allow_loss=True,
            )
        radius = self._radius(row)
        if radius is not None:
            if column == "berserker":
                return self._radius_cell_for_record(
                    self.opposing.get(("berserker", "berserker")),
                    radius,
                    allow_loss=True,
                )
            if deferred_material("berserker", column, opposing=True):
                return Cell("unknown")
            first, second = sorted(("berserker", column), key=PIECE_INDEX.__getitem__)
            if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], False):
                return known_draw()
            record = self.opposing.get((first, second))
            return self._radius_cell_for_record(
                record,
                radius,
                row_is_primary=(
                    record is not None and str(record["primary"]) == "berserker"
                ),
                allow_loss=True,
            )
        giant_class = self._giant_class(row)
        if giant_class is not None:
            if deferred_material("giant", column, opposing=True):
                return Cell("unknown")
            first, second = sorted(("giant", column), key=PIECE_INDEX.__getitem__)
            if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], False):
                return known_draw()
            record = self.opposing.get((first, second))
            return self._giant_cell_for_record(
                record,
                giant_class,
                row_is_primary=(
                    record is not None and str(record["primary"]) == "giant"
                ),
                allow_loss=True,
            )
        return self.opposed(row, column)

    def together(self, row: str, column: str) -> Cell:
        first, second = sorted((row, column), key=PIECE_INDEX.__getitem__)
        if deferred_material(first, second):
            return Cell("unknown")
        if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], True):
            return known_draw()
        return self._cell_for_record(self.same_team.get((first, second)))

    def opposed(self, row: str, column: str) -> Cell:
        if deferred_material(row, column, opposing=True):
            return Cell("unknown")
        first, second = sorted((row, column), key=PIECE_INDEX.__getitem__)
        if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], False):
            return known_draw()
        record = self.opposing.get((first, second))
        return self._cell_for_record(
            record,
            row_is_primary=(record is not None and row == str(record["primary"])),
            allow_loss=True,
        )


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    names = (
        (
            "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
            if bold
            else "/System/Library/Fonts/Supplemental/Arial.ttf"
        ),
        (
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
            if bold
            else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
        ),
    )
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default(size=size)


def percentage(value: int, total: int) -> str:
    if total == 0:
        return "0"
    percent = 100.0 * value / total
    if value == 0:
        return "0"
    if value == total:
        return "100"
    if percent < 0.5:
        return "<1"
    if percent > 99.5:
        return ">99"
    return f"{percent:.0f}"


def legend_positions(
    widths: list[int],
    normal_gap: int,
    group_gap: int,
    group_starts: frozenset[int] = frozenset({3, 5, 8}),
) -> tuple[list[int], int]:
    """Place variable-width legend items with wider group boundaries."""
    positions: list[int] = []
    cursor = 0
    for index, width in enumerate(widths):
        if index:
            cursor += group_gap if index in group_starts else normal_gap
        positions.append(cursor)
        cursor += width
    return positions, cursor


def cell_text(cell: Cell) -> str:
    if cell.kind in {"unknown", "computing"}:
        return ""
    if cell.kind == "win":
        return "Win"
    if cell.kind == "win_star":
        return "Win*"
    if cell.kind == "draw":
        return "Draw"
    if cell.kind == "loss_star":
        return "Loss*"
    if cell.kind == "loss":
        return "Loss"
    assert cell.first is not None and cell.second is not None
    displayed: list[tuple[str, str, str]] = []
    for label, field in (("W", "wins"), ("L", "losses"), ("D", "draws")):
        first_value = getattr(cell.first, field)
        second_value = getattr(cell.second, field)
        displayed.append(
            (
                label,
                percentage(first_value, cell.first.total),
                percentage(second_value, cell.second.total),
            )
        )
    if (
        cell.first.total != 0
        and cell.second.total != 0
        and all(first == second for _, first, second in displayed)
    ):
        return "\n".join(f"{label} {first}%" for label, first, _ in displayed)
    lines = [f"{label} {first}–{second}%" for label, first, second in displayed]
    return "\n".join(lines)


def same_team_grid(
    catalog: OutcomeCatalog, rows: list[str], columns: list[str]
) -> list[list[Cell]]:
    """Build a full same-team grid with one source Cell per mirrored pair."""
    mirrored: dict[tuple[str, str], Cell] = {}
    for row_index, row in enumerate(columns):
        for column in columns[row_index:]:
            cell = catalog.together(row, column)
            mirrored[(row, column)] = cell
            mirrored[(column, row)] = cell
    return [
        [
            mirrored[(row, column)]
            if row in PIECE_INDEX
            else catalog.together_row(row, column)
            for column in columns
        ]
        for row in rows
    ]


def diagonal_hatch(
    width: int, height: int, spacing: int, line_width: int
) -> "Image.Image":
    """Return a clipped overlay of parallel down-right hatch segments."""
    hatch = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    hatch_draw = ImageDraw.Draw(hatch)
    for offset in range(-height, width + height, spacing):
        hatch_draw.line(
            (offset, 0, offset + height, height), fill="#6E8FA9", width=line_width
        )
    return hatch


def draw_rotated_text(
    image: Image.Image,
    xy: tuple[int, int],
    text: str,
    text_font: ImageFont.ImageFont,
    angle: float,
) -> None:
    probe = ImageDraw.Draw(image)
    box = probe.textbbox((0, 0), text, font=text_font)
    width, height = box[2] - box[0] + 20, box[3] - box[1] + 20
    layer = Image.new("RGBA", (width, height), (0, 0, 0, 0))  # type: ignore
    ImageDraw.Draw(layer).text(
        (10 - box[0], 10 - box[1]), text, fill="white", font=text_font
    )
    layer = layer.rotate(angle, expand=True, resample=Image.Resampling.BICUBIC)
    image.alpha_composite(layer, (xy[0] - layer.width // 2, xy[1] - layer.height // 2))


def draw_grid(
    image: Image.Image,
    origin: tuple[int, int],
    title: str,
    columns: list[str],
    rows: list[str],
    cells: list[list[Cell]],
    cell_width: int,
    cell_height: int,
    row_label_width: int,
    header_height: int,
) -> tuple[int, int]:
    draw = ImageDraw.Draw(image)
    x0, y0 = origin
    title_font = font(34, bold=True)
    header_font = font(20, bold=True)
    row_font = font(21, bold=False)
    value_font = font(17, bold=True)
    terminal_font = font(24, bold=True)

    total_width = row_label_width + len(columns) * cell_width
    draw.text(
        (x0 + total_width // 2, y0 - 56),
        title,
        fill=COLORS["ink"],
        font=title_font,
        anchor="mm",
    )
    draw.rectangle(
        (x0, y0, x0 + row_label_width, y0 + header_height), fill=COLORS["header"]
    )
    draw.text(
        (x0 + row_label_width // 2, y0 + header_height - 28),
        "Material",
        fill="white",
        font=header_font,
        anchor="mm",
    )

    for column_index, column in enumerate(columns):
        left = x0 + row_label_width + column_index * cell_width
        draw.rectangle(
            (left, y0, left + cell_width, y0 + header_height),
            fill=COLORS["header"],
            outline="#7F96AA",
            width=1,
        )
        draw_rotated_text(
            image,
            (left + cell_width // 2, y0 + header_height // 2 + 10),
            PIECE_LABELS.get(column, column),
            header_font,  # type: ignore
            55,
        )

    body_y = y0 + header_height
    for row_index, row in enumerate(rows):
        top = body_y + row_index * cell_height
        draw.rectangle(
            (x0, top, x0 + row_label_width, top + cell_height),
            fill=COLORS["row_header"],
            outline=COLORS["grid"],
            width=1,
        )
        draw.text(
            (x0 + 16, top + cell_height // 2),
            PIECE_LABELS.get(row, row),
            fill=COLORS["ink"],
            font=row_font,
            anchor="lm",
        )
        for column_index, cell in enumerate(cells[row_index]):
            left = x0 + row_label_width + column_index * cell_width
            draw.rectangle(
                (left, top, left + cell_width, top + cell_height),
                fill=COLORS[cell.kind],
                outline=COLORS["grid"],
                width=1,
            )
            if cell.kind == "computing":
                spacing = max(7, 10 * max(1, cell_width // 102))
                # Draw into a cell-sized overlay so clipping cannot bend the
                # end points or leak into neighbouring cells.  Every segment
                # has the same +1 slope in local coordinates.
                hatch = diagonal_hatch(
                    cell_width, cell_height, spacing, max(1, spacing // 5)
                )
                image.alpha_composite(hatch, (left, top))
            text = cell_text(cell)
            if not text:
                continue
            selected_font = (
                value_font
                if cell.kind
                in {
                    "mixed",
                    "win_mostly",
                    "no_forced_loss",
                    "no_forced_win",
                    "loss_mostly",
                }
                else terminal_font
            )
            draw.multiline_text(
                (left + cell_width // 2, top + cell_height // 2),
                text,
                fill=COLORS["ink"],
                font=selected_font,
                anchor="mm",
                align="center",
                spacing=1,
            )
    return total_width, header_height + len(rows) * cell_height


def render_png(
    readme: Path,
    radii: Path,
    giant_classes: Path,
    devil_minions: Path,
    checker_states: Path,
    sniper_ranks: Path,
    angel_squares: Path,
    output: Path,
    scale: int,
) -> None:
    catalog = OutcomeCatalog(
        read_summary(readme),
        read_berserker_radii(radii),
        read_giant_start_classes(giant_classes),
        read_devil_minion_starts(devil_minions),
        read_checker_start_states(checker_states),
        read_sniper_start_ranks(sniper_ranks),
        read_angel_start_squares(angel_squares),
    )
    names = [piece.name for piece in PIECES]
    rows = list(names)
    berserker_index = rows.index("berserker") + 1
    rows[berserker_index:berserker_index] = BERSERKER_RADIUS_ROWS
    giant_index = rows.index("giant") + 1
    rows[giant_index:giant_index] = GIANT_START_ROWS
    devil_index = rows.index("devil") + 1
    rows[devil_index:devil_index] = DEVIL_MINION_ROWS
    checker_index = rows.index("checker") + 1
    rows[checker_index:checker_index] = CHECKER_START_ROWS
    sniper_index = rows.index("sniper") + 1
    rows[sniper_index:sniper_index] = SNIPER_START_ROWS
    angel_index = rows.index("angel") + 1
    rows[angel_index:angel_index] = ANGEL_START_ROWS

    cell_width = 102 * scale
    cell_height = 82 * scale
    row_label_width = 250 * scale
    header_height = 225 * scale
    outer = 70 * scale
    gap = 60 * scale
    top = 255 * scale
    footer = 210 * scale
    single_cell_width = 220 * scale

    single_width = row_label_width + single_cell_width
    matrix_width = row_label_width + len(names) * cell_width
    width = outer * 2 + single_width + matrix_width * 2 + gap * 2
    height = top + header_height + len(rows) * cell_height + footer
    image = Image.new("RGBA", (width, height), COLORS["page"])
    draw = ImageDraw.Draw(image)

    draw.text(
        (width // 2, 45 * scale),
        "Ultimate Fish · Endgame Tablebase Grouped by Material",
        fill=COLORS["ink"],
        font=font(47 * scale, bold=True),
        anchor="ma",
    )
    draw.text(
        (width // 2, 113 * scale),
        "Percent ranges show the row side starting then the opponent starting · Win/Loss Cells indicate the result for the row",
        fill=COLORS["muted"],
        font=font(23 * scale),
        anchor="ma",
    )
    draw.text(
        (width // 2, 151 * scale),
        PLOT_SCOPE_CAPTION,
        fill=COLORS["muted"],
        font=font(20 * scale),
        anchor="ma",
    )

    grid_y = top
    x = outer
    draw_grid(
        image,
        (x, grid_y),
        "King + A  vs  King",
        ["Outcome"],
        rows,
        [[catalog.single_row(row)] for row in rows],
        single_cell_width,
        cell_height,
        row_label_width,
        header_height,
    )
    x += single_width + gap
    draw_grid(
        image,
        (x, grid_y),
        "King + A + B  vs  King",
        names,
        rows,
        same_team_grid(catalog, rows, names),
        cell_width,
        cell_height,
        row_label_width,
        header_height,
    )
    x += matrix_width + gap
    draw_grid(
        image,
        (x, grid_y),
        "King + A  vs  King + B",
        names,
        rows,
        [[catalog.opposed_row(row, column) for column in names] for row in rows],
        cell_width,
        cell_height,
        row_label_width,
        header_height,
    )

    legend_items = (
        ("win", "Forced win"),
        ("win_star", "Win when row starts"),
        ("win_mostly", "Mostly win / no forced loss"),
        ("no_forced_loss", "No forced loss"),
        ("draw", "Forced draw"),
        ("mixed", "State-dependent"),
        ("no_forced_win", "No forced win"),
        ("loss_mostly", "Mostly loss / no forced win"),
        ("loss_star", "Loss when column starts"),
        ("loss", "Forced loss"),
        ("computing", "Computing"),
        ("unknown", "Not computed"),
    )
    legend_y = height - 126 * scale
    legend_font = font(22 * scale)
    item_widths = []
    for _, label in legend_items:
        bounds = draw.textbbox((0, 0), label, font=legend_font)
        item_widths.append(52 * scale + bounds[2] - bounds[0])
    positions, legend_width = legend_positions(
        item_widths,
        normal_gap=18 * scale,
        group_gap=62 * scale,
        group_starts=frozenset({4, 6, 10}),
    )
    legend_x = (width - legend_width) // 2
    for index, (kind, label) in enumerate(legend_items):
        left = legend_x + positions[index]
        draw.rounded_rectangle(
            (left, legend_y, left + 42 * scale, legend_y + 30 * scale),
            radius=6 * scale,
            fill=COLORS[kind],
            outline=COLORS["grid"],
            width=1,
        )
        draw.text(
            (left + 52 * scale, legend_y + 15 * scale),
            label,
            fill=COLORS["ink"],
            font=legend_font,
            anchor="lm",
        )

    output.parent.mkdir(parents=True, exist_ok=True)
    image.convert("RGB").save(output, optimize=True)
    print(f"Wrote {output} ({width}×{height})")


SVG_FONT_FAMILY = "Arial, 'DejaVu Sans', sans-serif"


def svg_rect(
    elements: list[str],
    left: int,
    top: int,
    right: int,
    bottom: int,
    *,
    fill: str,
    outline: str | None = None,
    stroke_width: int = 1,
    radius: int = 0,
) -> None:
    stroke = (
        f' stroke="{outline}" stroke-width="{stroke_width}"'
        if outline else ""
    )
    rounded = f' rx="{radius}" ry="{radius}"' if radius else ""
    elements.append(
        f'<rect x="{left}" y="{top}" width="{right - left}" '
        f'height="{bottom - top}" fill="{fill}"{stroke}{rounded}/>'
    )


def svg_text(
    elements: list[str],
    xy: tuple[int, int],
    value: str,
    *,
    fill: str,
    size: int,
    bold: bool = False,
    anchor: Literal["start", "middle", "end"] = "middle",
    baseline: Literal["middle", "hanging"] = "middle",
    angle: int = 0,
    spacing: int = 1,
) -> None:
    x, y = xy
    transform = f' transform="rotate({angle} {x} {y})"' if angle else ""
    weight = 700 if bold else 400
    lines = value.splitlines() or [""]
    line_height = size + spacing
    first_y = y - (len(lines) - 1) * line_height / 2
    for index, line in enumerate(lines):
        line_y = first_y + index * line_height
        elements.append(
            f'<text x="{x}" y="{line_y:g}" fill="{fill}" '
            f'font-family="{SVG_FONT_FAMILY}" font-size="{size}" '
            f'font-weight="{weight}" text-anchor="{anchor}" '
            f'dominant-baseline="{baseline}"{transform}>'
            f'{escape(line)}</text>'
        )


def draw_grid_svg(
    elements: list[str],
    origin: tuple[int, int],
    title: str,
    columns: list[str],
    rows: list[str],
    cells: list[list[Cell]],
    cell_width: int,
    cell_height: int,
    row_label_width: int,
    header_height: int,
) -> tuple[int, int]:
    x0, y0 = origin
    total_width = row_label_width + len(columns) * cell_width
    svg_text(
        elements, (x0 + total_width // 2, y0 - 56), title,
        fill=COLORS["ink"], size=34, bold=True,
    )
    svg_rect(
        elements, x0, y0, x0 + row_label_width, y0 + header_height,
        fill=COLORS["header"],
    )
    svg_text(
        elements,
        (x0 + row_label_width // 2, y0 + header_height - 28),
        "Material", fill="white", size=20, bold=True,
    )

    for column_index, column in enumerate(columns):
        left = x0 + row_label_width + column_index * cell_width
        svg_rect(
            elements, left, y0, left + cell_width, y0 + header_height,
            fill=COLORS["header"], outline="#7F96AA",
        )
        svg_text(
            elements,
            (left + cell_width // 2, y0 + header_height // 2 + 10),
            PIECE_LABELS.get(column, column), fill="white", size=20,
            bold=True, angle=-55,
        )

    body_y = y0 + header_height
    for row_index, row in enumerate(rows):
        top = body_y + row_index * cell_height
        svg_rect(
            elements, x0, top, x0 + row_label_width, top + cell_height,
            fill=COLORS["row_header"], outline=COLORS["grid"],
        )
        svg_text(
            elements, (x0 + 16, top + cell_height // 2),
            PIECE_LABELS.get(row, row), fill=COLORS["ink"], size=21,
            anchor="start",
        )
        for column_index, cell in enumerate(cells[row_index]):
            left = x0 + row_label_width + column_index * cell_width
            svg_rect(
                elements, left, top, left + cell_width, top + cell_height,
                fill=COLORS[cell.kind], outline=COLORS["grid"],
            )
            if cell.kind == "computing":
                svg_rect(
                    elements, left, top, left + cell_width, top + cell_height,
                    fill="url(#computing-hatch)",
                )
            value = cell_text(cell)
            if not value:
                continue
            detailed = cell.kind in {
                "mixed", "win_mostly", "no_forced_loss", "no_forced_win",
                "loss_mostly",
            }
            svg_text(
                elements, (left + cell_width // 2, top + cell_height // 2),
                value, fill=COLORS["ink"], size=17 if detailed else 24,
                bold=True,
            )
    return total_width, header_height + len(rows) * cell_height


def render_svg(
    readme: Path,
    radii: Path,
    giant_classes: Path,
    devil_minions: Path,
    checker_states: Path,
    sniper_ranks: Path,
    angel_squares: Path,
    output: Path,
    scale: int,
) -> None:
    catalog = OutcomeCatalog(
        read_summary(readme),
        read_berserker_radii(radii),
        read_giant_start_classes(giant_classes),
        read_devil_minion_starts(devil_minions),
        read_checker_start_states(checker_states),
        read_sniper_start_ranks(sniper_ranks),
        read_angel_start_squares(angel_squares),
    )
    names = [piece.name for piece in PIECES]
    rows = list(names)
    rows[rows.index("berserker") + 1:rows.index("berserker") + 1] = (
        BERSERKER_RADIUS_ROWS
    )
    rows[rows.index("giant") + 1:rows.index("giant") + 1] = GIANT_START_ROWS
    rows[rows.index("devil") + 1:rows.index("devil") + 1] = DEVIL_MINION_ROWS
    rows[rows.index("checker") + 1:rows.index("checker") + 1] = CHECKER_START_ROWS
    rows[rows.index("sniper") + 1:rows.index("sniper") + 1] = SNIPER_START_ROWS
    rows[rows.index("angel") + 1:rows.index("angel") + 1] = ANGEL_START_ROWS

    cell_width = 102 * scale
    cell_height = 82 * scale
    row_label_width = 250 * scale
    header_height = 225 * scale
    outer = 70 * scale
    gap = 60 * scale
    top = 255 * scale
    footer = 210 * scale
    single_cell_width = 220 * scale
    single_width = row_label_width + single_cell_width
    matrix_width = row_label_width + len(names) * cell_width
    width = outer * 2 + single_width + matrix_width * 2 + gap * 2
    height = top + header_height + len(rows) * cell_height + footer

    spacing = 10 * scale
    line_width = max(1, spacing // 5)
    elements = [
        f'<rect width="{width}" height="{height}" fill="{COLORS["page"]}"/>',
    ]
    svg_text(
        elements, (width // 2, 45 * scale),
        "Ultimate Fish · Endgame Tablebase Grouped by Material",
        fill=COLORS["ink"], size=47 * scale, bold=True, baseline="hanging",
    )
    svg_text(
        elements, (width // 2, 113 * scale),
        "Percent ranges show the row side starting then the opponent starting · Win/Loss Cells indicate the result for the row",
        fill=COLORS["muted"], size=23 * scale, baseline="hanging",
    )
    svg_text(
        elements, (width // 2, 151 * scale),
        PLOT_SCOPE_CAPTION,
        fill=COLORS["muted"], size=20 * scale, baseline="hanging",
    )

    grid_y = top
    x = outer
    draw_grid_svg(
        elements, (x, grid_y), "King + A  vs  King", ["Outcome"], rows,
        [[catalog.single_row(row)] for row in rows], single_cell_width,
        cell_height, row_label_width, header_height,
    )
    x += single_width + gap
    draw_grid_svg(
        elements, (x, grid_y), "King + A + B  vs  King", names, rows,
        same_team_grid(catalog, rows, names),
        cell_width, cell_height, row_label_width, header_height,
    )
    x += matrix_width + gap
    draw_grid_svg(
        elements, (x, grid_y), "King + A  vs  King + B", names, rows,
        [[catalog.opposed_row(row, column) for column in names] for row in rows],
        cell_width, cell_height, row_label_width, header_height,
    )

    legend_items = (
        ("win", "Forced win"),
        ("win_star", "Win when row starts"),
        ("win_mostly", "Mostly win / no forced loss"),
        ("no_forced_loss", "No forced loss"),
        ("draw", "Forced draw"),
        ("mixed", "State-dependent"),
        ("no_forced_win", "No forced win"),
        ("loss_mostly", "Mostly loss / no forced win"),
        ("loss_star", "Loss when column starts"),
        ("loss", "Forced loss"),
        ("computing", "Computing"),
        ("unknown", "Not computed"),
    )
    legend_y = height - 126 * scale
    legend_size = 22 * scale
    legend_font = font(legend_size)
    item_widths = []
    probe = ImageDraw.Draw(Image.new("L", (1, 1)))
    for _, label in legend_items:
        bounds = probe.textbbox((0, 0), label, font=legend_font)
        item_widths.append(52 * scale + bounds[2] - bounds[0])
    positions, legend_width = legend_positions(
        item_widths, normal_gap=18 * scale, group_gap=62 * scale,
        group_starts=frozenset({4, 6, 10}),
    )
    legend_x = (width - legend_width) // 2
    for index, (kind, label) in enumerate(legend_items):
        left = legend_x + positions[index]
        svg_rect(
            elements, left, legend_y, left + 42 * scale,
            legend_y + 30 * scale, fill=COLORS[kind], outline=COLORS["grid"],
            radius=6 * scale,
        )
        if kind == "computing":
            svg_rect(
                elements, left, legend_y, left + 42 * scale,
                legend_y + 30 * scale, fill="url(#computing-hatch)",
                radius=6 * scale,
            )
        svg_text(
            elements, (left + 52 * scale, legend_y + 15 * scale), label,
            fill=COLORS["ink"], size=legend_size, anchor="start",
        )

    definitions = (
        f'<defs><pattern id="computing-hatch" width="{spacing}" '
        f'height="{spacing}" patternUnits="userSpaceOnUse">'
        f'<path d="M 0 0 L {spacing} {spacing}" fill="none" '
        f'stroke="#6E8FA9" stroke-width="{line_width}"/>'
        f'</pattern></defs>'
    )
    document = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
        f'height="{height}" viewBox="0 0 {width} {height}" '
        'role="img" aria-labelledby="plot-title plot-description" '
        'text-rendering="geometricPrecision">\n'
        '<title id="plot-title">Ultimate Fish endgame tablebase outcomes</title>\n'
        '<desc id="plot-description">Three grids summarize single-piece, '
        'same-team two-piece, and opposing two-piece endgames.</desc>\n'
        f'{definitions}\n' + "\n".join(elements) + "\n</svg>\n"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(document, encoding="utf-8")
    print(f"Wrote {output} ({width}×{height} viewBox)")


def render(
    readme: Path,
    radii: Path,
    giant_classes: Path,
    devil_minions: Path,
    checker_states: Path,
    sniper_ranks: Path,
    angel_squares: Path,
    output: Path,
    scale: int,
) -> None:
    suffix = output.suffix.lower()
    renderer = render_svg if suffix == ".svg" else render_png if suffix == ".png" else None
    if renderer is None:
        raise ValueError("plot output must use an .svg or .png extension")
    renderer(
        readme, radii, giant_classes, devil_minions, checker_states,
        sniper_ranks, angel_squares, output, scale,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--readme",
        type=Path,
        default=ROOT / "tablebases" / "README.md",
        help="README containing the generated tablebase summary",
    )
    parser.add_argument(
        "--berserker-radii",
        type=Path,
        default=ROOT / "tablebases" / "berserker-radius-summary.json",
        help="exact reachability-filtered radius 1-9/10+ Berserker results",
    )
    parser.add_argument(
        "--giant-classes",
        type=Path,
        default=ROOT / "tablebases" / "giant-start-class-summary.json",
        help="exact reachability-filtered Giant root-anchor class results",
    )
    parser.add_argument(
        "--devil-minions",
        type=Path,
        default=ROOT / "tablebases" / "devil-minion-start-summary.json",
        help="exact alive lone-Devil root results by current Minion count",
    )
    parser.add_argument(
        "--checker-states",
        type=Path,
        default=ROOT / "tablebases" / "checker-start-state-summary.json",
        help="exact normal-Checker and Checker-King root results",
    )
    parser.add_argument(
        "--sniper-ranks",
        type=Path,
        default=ROOT / "tablebases" / "sniper-start-rank-summary.json",
        help="exact color-relative Sniper root-rank results",
    )
    parser.add_argument(
        "--angel-squares",
        type=Path,
        default=ROOT / "tablebases" / "angel-start-square-summary.json",
        help="exact symmetry-folded Angel root-square results",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "tablebases" / "ultimate-tablebase-grid.svg",
    )
    parser.add_argument(
        "--scale",
        type=int,
        default=1,
        choices=(1, 2),
        help="render at 1× or 2× drawing scale (default: 1)",
    )
    args = parser.parse_args()
    render(
        args.readme.resolve(),
        args.berserker_radii.resolve(),
        args.giant_classes.resolve(),
        args.devil_minions.resolve(),
        args.checker_states.resolve(),
        args.sniper_ranks.resolve(),
        args.angel_squares.resolve(),
        args.output.resolve(),
        args.scale,
    )


if __name__ == "__main__":
    main()
