#!/usr/bin/env python3
# Run from the repository root with:
#   python3 tools/tablebases/plot_ultimate_tablebases.py
# Optional: choose a destination or resolution with --output PATH and --scale N.
"""Render the README tablebase summary as three endgame outcome grids."""

from __future__ import annotations

import argparse
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
    COPYCAT_SEPARATORS,
    DEFERRED_DYNAMIC_K2,
    PIECES,
    inventory,
    mirror_copycat_candidates,
    stateful_candidates,
    sufficient_pair,
)

Kind = Literal[
    "win_star", "draw", "mixed", "loss_star", "computing", "unknown",
    "duplicate",
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
    "win_star": "#D8EDB2",
    "draw": "#DCE2E8",
    "mixed": "#FFE3A1",
    "loss_star": "#F9CFB0",
    "computing": "#C9D8E6",
    "unknown": "#FFFFFF",
    "duplicate": "#FFFFFF",
}

PIECE_LABELS = {piece.name: piece.name.title() for piece in PIECES}
PIECE_BY_NAME = {piece.name: piece for piece in PIECES}
PIECE_INDEX = {piece.name: index for index, piece in enumerate(PIECES)}
BERSERKER_RADIUS_ROWS = tuple(
    f"berserker_radius_{radius}" for radius in range(1, 4))
PIECE_LABELS.update({
    row: f"Berserker (radius {radius})"
    for radius, row in enumerate(BERSERKER_RADIUS_ROWS, 1)
})


def parse_wdl(text: str) -> WDL:
    """Parse legal W/L/D counts, deliberately ignoring `(illegal)` counts."""
    values: list[int] = []
    for component in text.split("/"):
        match = re.match(r"\s*([0-9][0-9,]*)", component)
        if not match:
            raise ValueError(f"invalid W/L/D value: {text!r}")
        values.append(int(match.group(1).replace(",", "")))
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
            status = fields[4].strip("*").lower()
            # The computation ledger is canonical.  In particular, certified
            # S3-only payloads no longer have to appear in the legacy generated
            # local-file summary, but their exact W/L/D values still belong in
            # the plot.
            if fields[7] != "—" and fields[8] != "—":
                raw = ReadmeResult(
                    parse_wdl(fields[7]), parse_wdl(fields[8]), status)
            else:
                previous = results.get(filename)
                if previous is None:
                    raw = ReadmeResult(WDL(0, 0, 0), WDL(0, 0, 0), status)
                else:
                    raw = ReadmeResult(
                        previous.first_starts, previous.second_starts, status)
            results[filename] = raw
    return results


def read_berserker_radii(path: Path) -> dict[tuple[str, int], ReadmeResult]:
    """Read exact, reachability-filtered Berserker power slices."""
    document = json.loads(path.read_text(encoding="utf-8"))
    if document.get("schema") != 1:
        raise ValueError(f"unsupported Berserker radius summary schema: {path}")
    results: dict[tuple[str, int], ReadmeResult] = {}
    for filename, record in document.get("files", {}).items():
        for radius_text, raw in record.get("radii", {}).items():
            radius = int(radius_text)
            if radius not in (1, 2, 3):
                continue
            first = raw["first_starts"]
            second = raw["second_starts"]
            results[(filename, radius)] = ReadmeResult(
                WDL(first["wins"], first["losses"], first["draws"]),
                WDL(second["wins"], second["losses"], second["draws"]),
            )
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
        return Cell("unknown")
    if first.wins == first.total and second.wins != second.total:
        return Cell("win_star", first, second)
    if first.draws == first.total and second.draws == second.total:
        return Cell("draw", first, second)
    if allow_loss and first.losses != first.total and second.losses == second.total:
        return Cell("loss_star", first, second)
    return Cell("mixed", first, second)


def known_draw() -> Cell:
    draw = WDL(0, 0, 1)
    return Cell("draw", draw, draw)


class OutcomeCatalog:
    def __init__(
        self,
        summary: dict[str, ReadmeResult],
        berserker_radii: dict[tuple[str, int], ReadmeResult] | None = None,
    ) -> None:
        self.summary = summary
        self.berserker_radii = berserker_radii or {}
        # Match the canonical ledger's record precedence.  The broad stateful
        # catalog may contain a normalized duplicate for an already generated
        # requested class (for example kpenguinkdragon versus the authenticated
        # kdragonkpenguin header).  The exact inventory record must win so a
        # certified result cannot be hidden or interpreted with reversed owners.
        records_by_filename: dict[str, dict[str, object]] = {}
        for record in (*stateful_candidates(), *mirror_copycat_candidates(),
                       *inventory()):
            records_by_filename[str(record["filename"])] = record
        records = list(records_by_filename.values())
        self.singles = {
            str(record["primary"]): record
            for record in records
            if record["phase"] == "kings+1"
        }
        self.same_team = {
            tuple(sorted(
                (str(record["primary"]), str(record["secondary"])),
                key=PIECE_INDEX.__getitem__,
            )): record
            for record in records
            if record["secondary"] and not record["opposing"]
        }
        self.opposing = {
            tuple(sorted(
                (str(record["primary"]), str(record["secondary"])),
                key=PIECE_INDEX.__getitem__,
            )): record
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
        if raw.status == "computing":
            return Cell("computing")
        if raw.status not in {"certified", "preserving", "draw"}:
            return Cell("unknown")
        first, second = row_side_result(raw, row_is_primary)
        return classify(first, second, allow_loss)

    def single(self, name: str) -> Cell:
        if name in DEFERRED_DYNAMIC_K2:
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
        if aggregate is not None and aggregate.status == "computing":
            return Cell("computing")
        raw = self.berserker_radii.get((filename, radius))
        if raw is None:
            return Cell("unknown")
        first, second = row_side_result(raw, row_is_primary)
        return classify(first, second, allow_loss)

    def single_row(self, row: str) -> Cell:
        radius = self._radius(row)
        if radius is None:
            return self.single(row)
        return self._radius_cell_for_record(self.singles.get("berserker"), radius)

    def together_row(self, row: str, column: str) -> Cell:
        radius = self._radius(row)
        if radius is None:
            return self.together(row, column)
        if column == "berserker":
            # Two same-team Berserkers are exchange-folded; there is no
            # distinguished row Berserker whose radius can be sliced.
            return Cell("unknown")
        names = {"berserker", column}
        if (names & DEFERRED_DYNAMIC_K2 or
                "copycat" in names and bool(names & COPYCAT_SEPARATORS)):
            return Cell("unknown")
        first, second = sorted(names, key=PIECE_INDEX.__getitem__)
        if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], True):
            return known_draw()
        return self._radius_cell_for_record(
            self.same_team.get((first, second)), radius)

    def opposed_row(self, row: str, column: str) -> Cell:
        radius = self._radius(row)
        if radius is None:
            return self.opposed(row, column)
        if column == "berserker":
            return self._radius_cell_for_record(
                self.opposing.get(("berserker", "berserker")),
                radius,
                allow_loss=True,
            )
        names = {"berserker", column}
        if (names & DEFERRED_DYNAMIC_K2 or
                "copycat" in names and bool(names & COPYCAT_SEPARATORS)):
            return Cell("unknown")
        first, second = sorted(names, key=PIECE_INDEX.__getitem__)
        if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], False):
            return known_draw()
        record = self.opposing.get((first, second))
        return self._radius_cell_for_record(
            record,
            radius,
            row_is_primary=(record is not None and
                            str(record["primary"]) == "berserker"),
            allow_loss=True,
        )

    def together(self, row: str, column: str) -> Cell:
        if PIECE_INDEX[column] > PIECE_INDEX[row]:
            return Cell("duplicate")
        names = {row, column}
        if (names & DEFERRED_DYNAMIC_K2 or
                "copycat" in names and bool(names & COPYCAT_SEPARATORS)):
            return Cell("unknown")
        first, second = sorted((row, column), key=PIECE_INDEX.__getitem__)
        if not sufficient_pair(PIECE_BY_NAME[first], PIECE_BY_NAME[second], True):
            return known_draw()
        return self._cell_for_record(self.same_team.get((first, second)))

    def opposed(self, row: str, column: str) -> Cell:
        names = {row, column}
        if (names & DEFERRED_DYNAMIC_K2 or
                "copycat" in names and bool(names & COPYCAT_SEPARATORS)):
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


def cell_text(cell: Cell) -> str:
    if cell.kind in {"unknown", "duplicate", "computing"}:
        return ""
    if cell.kind == "win_star":
        return "Win*"
    if cell.kind == "draw":
        return "Draw"
    if cell.kind == "loss_star":
        return "Loss*"
    assert cell.first is not None and cell.second is not None
    lines = []
    for label, field in (("W", "wins"), ("L", "losses"), ("D", "draws")):
        first_value = getattr(cell.first, field)
        second_value = getattr(cell.second, field)
        lines.append(
            f"{label} {percentage(first_value, cell.first.total)}–"
            f"{percentage(second_value, cell.second.total)}%"
        )
    return "\n".join(lines)


def diagonal_hatch(width: int, height: int, spacing: int,
                   line_width: int) -> "Image.Image":
    """Return a clipped overlay of parallel down-right hatch segments."""
    hatch = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    hatch_draw = ImageDraw.Draw(hatch)
    for offset in range(-height, width + height, spacing):
        hatch_draw.line(
            (offset, 0, offset + height, height),
            fill="#6E8FA9", width=line_width)
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
                    cell_width, cell_height, spacing, max(1, spacing // 5))
                image.alpha_composite(hatch, (left, top))
            text = cell_text(cell)
            if not text:
                continue
            selected_font = terminal_font if cell.kind != "mixed" else value_font
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


def render(readme: Path, radii: Path, output: Path, scale: int) -> None:
    catalog = OutcomeCatalog(read_summary(readme), read_berserker_radii(radii))
    names = [piece.name for piece in PIECES]
    rows = list(names)
    berserker_index = rows.index("berserker") + 1
    rows[berserker_index:berserker_index] = BERSERKER_RADIUS_ROWS

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
        "Only reachability-admitted states are counted · hatched cells are computing now · white cells are planned, deferred, or duplicates",
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
        [[catalog.together_row(row, column) for column in names] for row in rows],
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

    legend_y = height - 126 * scale
    legend_items = (
        ("win_star", "Forced win when row starts"),
        ("draw", "Forced draw"),
        ("mixed", "Outcome depends on state"),
        ("loss_star", "Forced loss when column starts"),
        ("computing", "Computing now"),
        ("unknown", "Not computed / duplicate"),
    )
    legend_font = font(21 * scale)
    item_width = 570 * scale
    legend_x = (width - item_width * len(legend_items)) // 2
    for index, (kind, label) in enumerate(legend_items):
        left = legend_x + index * item_width
        draw.rounded_rectangle(
            (left, legend_y, left + 46 * scale, legend_y + 32 * scale),
            radius=6 * scale,
            fill=COLORS[kind],
            outline=COLORS["grid"],
            width=1,
        )
        draw.text(
            (left + 60 * scale, legend_y + 16 * scale),
            label,
            fill=COLORS["ink"],
            font=legend_font,
            anchor="lm",
        )

    output.parent.mkdir(parents=True, exist_ok=True)
    image.convert("RGB").save(output, optimize=True)
    print(f"Wrote {output} ({width}×{height})")


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
        help="exact reachability-filtered radius 1/2/3 Berserker results",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "tablebases" / "ultimate-tablebase-grid.png",
    )
    parser.add_argument(
        "--scale",
        type=int,
        default=1,
        choices=(1, 2),
        help="render at 1× or 2× resolution (default: 1)",
    )
    args = parser.parse_args()
    render(
        args.readme.resolve(),
        args.berserker_radii.resolve(),
        args.output.resolve(),
        args.scale,
    )


if __name__ == "__main__":
    main()
