#!/usr/bin/env python3
"""Build exact symmetry-folded Angel root-square plot data.

The native auditor partitions every admitted root into all 40 color-relative,
horizontally mirrored square classes.  For an attached Angel, the square is its
Halo origin rather than its host's current square.  The plot consumes only the
12 classes in legal deployment ranks 1-3.  Successors remain unrestricted, so
attachment, rescue, external displacement, and all later play retain the
selected root's unchanged tablebase W/L/D result.

Each certified material group is downloaded and SHA-256 authenticated
separately, audited with a bounded worker count, and removed before the next
group is fetched.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import audit_ultimate_sniper_start_ranks as shared  # noqa: E402


common = shared.common
plot = shared.plot
RESULT_NAMES = shared.RESULT_NAMES
ALL_SQUARES = tuple(
    f"{file}{rank}" for rank in range(1, 11) for file in "abcd"
)
PLOTTED_SQUARES = tuple(
    f"{file}{rank}" for rank in range(1, 4) for file in "abcd"
)
DEFAULT_REVISION = "3532f701f81f8ba190d932891818a16af13243b8"
HISTORICALLY_UNAVAILABLE_OVERLAYS_BY_REVISION = {
    common.DEFAULT_REVISION: frozenset({
        "kjesterangelk.uftb", "kjesterkangel.uftb",
    }),
}
CONCRETE_LINE = re.compile(
    r"reachability_(primary|secondary)_angel_square"
    r"_(total|excluded|trivial) square ([a-d](?:10|[1-9])) side ([01]) "
    r"unknown (\d+) win (\d+) loss (\d+) draw (\d+)"
)
INFORMATION_LINE = re.compile(
    r"information_reachability_(primary|secondary)_angel_square_"
    r"(admitted|trivial) square ([a-d](?:10|[1-9])) side ([01]) "
    r"unknown (\d+) win (\d+) loss (\d+) draw (\d+)"
)


def records() -> dict[str, dict[str, Any]]:
    by_filename: dict[str, dict[str, Any]] = {}
    for record in (
        *plot.stateful_candidates(), *plot.angel_candidates(),
        *plot.mirror_copycat_candidates(), *plot.devil_candidates(),
        *plot.inventory(),
    ):
        by_filename[str(record["filename"])] = record
    return {
        filename: record for filename, record in by_filename.items()
        if record["primary"] == "angel" or record.get("secondary") == "angel"
    }


def information_material(record: dict[str, Any]) -> bool:
    return bool({record["primary"], record.get("secondary")} & {"jester", "ghost"})


def empty_counts() -> list[int]:
    return [0, 0, 0, 0]


def named(values: list[int]) -> dict[str, int]:
    return dict(zip(RESULT_NAMES[1:], values[1:]))


def finish_rows(
    buckets: dict[tuple[str, int], dict[str, list[int]]],
    expected_kinds: set[str],
) -> dict[str, Any]:
    rows: dict[str, Any] = {}
    for square in ALL_SQUARES:
        sides = []
        for side in range(2):
            raw = buckets.get((square, side), {})
            if set(raw) != expected_kinds:
                raise RuntimeError(
                    f"incomplete Angel square {square} audit for side {side}: "
                    f"{sorted(raw)}"
                )
            if "total" in raw:
                total = raw["total"]
                excluded = raw["excluded"]
                if any(excluded[index] > total[index] for index in range(4)):
                    raise RuntimeError(
                        f"Angel square {square} excluded count exceeds total"
                    )
                admitted = [
                    total[index] - excluded[index] for index in range(4)
                ]
            else:
                admitted = raw["admitted"]
                total = None
                excluded = None
            trivial = raw["trivial"]
            if (
                admitted[0]
                or trivial[0]
                or any(trivial[index] > admitted[index] for index in range(4))
            ):
                raise RuntimeError(
                    f"invalid Angel square {square} conservation for side {side}"
                )
            display = [
                admitted[index] - trivial[index] for index in range(4)
            ]
            result: dict[str, Any] = {
                "admitted": named(admitted),
                "trivial": named(trivial),
                "display": named(display),
            }
            if total is not None and excluded is not None:
                result["total"] = named(total)
                result["excluded"] = named(excluded)
            sides.append(result)
        rows[square] = {
            "first_starts": sides[0], "second_starts": sides[1]
        }
    return rows


def parse_counts(text: str, record: dict[str, Any],
                 information: bool) -> dict[str, Any]:
    angel_slot = "primary" if record["primary"] == "angel" else "secondary"
    pattern = INFORMATION_LINE if information else CONCRETE_LINE
    expected_kinds = ({"admitted", "trivial"} if information
                      else {"total", "excluded", "trivial"})
    buckets: dict[tuple[str, int], dict[str, list[int]]] = {}
    for match in pattern.finditer(text):
        if match.group(1) != angel_slot:
            continue
        kind, square, side = match.group(2), match.group(3), int(match.group(4))
        if square not in ALL_SQUARES:
            raise RuntimeError(f"invalid Angel color-relative square {square}")
        key = (square, side)
        if kind in buckets.setdefault(key, {}):
            raise RuntimeError("duplicate Angel root-square audit row")
        buckets[key][kind] = [int(match.group(index)) for index in range(5, 9)]
    return finish_rows(buckets, expected_kinds)


def normalize_and_validate_aggregate(
    filename: str, squares: dict[str, Any], aggregate: plot.ReadmeResult,
    allow_role_flip: bool,
) -> list[int]:
    flipped: list[int] = []
    for side, key in enumerate(("first_starts", "second_starts")):
        expected_wdl = aggregate.first_starts if side == 0 else aggregate.second_starts
        expected = {
            "wins": expected_wdl.wins,
            "losses": expected_wdl.losses,
            "draws": expected_wdl.draws,
        }

        def aggregate_side() -> dict[str, int]:
            return {
                name: sum(squares[square][key]["display"][name]
                          for square in ALL_SQUARES)
                for name in RESULT_NAMES[1:]
            }

        actual = aggregate_side()
        reversed_actual = {
            "wins": actual["losses"],
            "losses": actual["wins"],
            "draws": actual["draws"],
        }
        if actual != expected and allow_role_flip and reversed_actual == expected:
            for square in ALL_SQUARES:
                row = squares[square][key]
                for bucket in ("total", "excluded", "admitted", "trivial", "display"):
                    if bucket not in row:
                        continue
                    row[bucket]["wins"], row[bucket]["losses"] = (
                        row[bucket]["losses"], row[bucket]["wins"]
                    )
            actual = aggregate_side()
            flipped.append(side)
        if actual != expected:
            raise RuntimeError(
                f"Angel root-square slices do not reproduce {filename} {key}: "
                f"{actual} != {expected}"
            )
    return flipped


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path,
                        default=ROOT / "src/ultimate_tablebase")
    parser.add_argument("--readme", type=Path,
                        default=ROOT / "tablebases/README.md")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "tablebases/angel-start-square-summary.json")
    parser.add_argument("--work-root", type=Path,
                        default=Path(tempfile.gettempdir()) /
                        "ultimatefish-angel-start-square-audit")
    parser.add_argument("--dataset", default=common.DEFAULT_DATASET)
    parser.add_argument("--origin", default=common.DEFAULT_ORIGIN)
    parser.add_argument("--revision", default=DEFAULT_REVISION)
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--max-retries", type=int, default=5)
    parser.add_argument("--plan-only", action="store_true",
                        help="authenticate inventory and report transfer bounds")
    args = parser.parse_args()
    if not 1 <= args.workers <= 4:
        parser.error("--workers must be between 1 and 4")
    if not 0 <= args.max_retries <= 10:
        parser.error("--max-retries must be between 0 and 10")

    summaries = plot.read_summary(args.readme)
    material = records()
    token = common.hf_token()
    revision, remote = common.catalog(
        args.origin, args.dataset, args.revision, token
    )
    binary_sha = common.sha256(args.binary)
    certified_items = [
        (filename, record) for filename, record in material.items()
        if summaries.get(filename) and summaries[filename].status == "certified"
    ]
    unavailable: set[str] = set()
    for filename, record in certified_items:
        if filename not in remote:
            raise RuntimeError(f"Hugging Face catalog lacks {filename}")
        if information_material(record):
            overlay_name = f"{Path(filename).stem}.ufiw"
            if overlay_name not in remote:
                if filename in HISTORICALLY_UNAVAILABLE_OVERLAYS_BY_REVISION.get(
                        revision, frozenset()):
                    unavailable.add(filename)
                    continue
                available = sorted(
                    name for name in remote
                    if "angel" in name and name.endswith(".ufiw")
                )
                raise RuntimeError(
                    f"Hugging Face catalog lacks {overlay_name}; available "
                    f"Angel overlays: {available}"
                )

    def transfer_size(item: tuple[str, dict[str, Any]]) -> int:
        filename, record = item
        if filename in unavailable:
            return 0
        size = int(remote[filename]["bytes"])
        if information_material(record):
            size += int(remote[f"{Path(filename).stem}.ufiw"]["bytes"])
        return size

    if args.plan_only:
        auditable = [item for item in certified_items if item[0] not in unavailable]
        plan = {
            "certified_records": len(certified_items),
            "audited_records": len(auditable),
            "unavailable_information_records": sorted(unavailable),
            "total_transfer_bytes": sum(map(transfer_size, auditable)),
            "maximum_live_bytes": max(map(transfer_size, auditable), default=0),
            "dataset_revision": revision,
        }
        print(json.dumps(plan, indent=2, sort_keys=True))
        return

    args.work_root.mkdir(parents=True, exist_ok=True)
    progress_path = args.work_root / "progress.json"
    if progress_path.exists():
        progress = json.loads(progress_path.read_text(encoding="utf-8"))
    elif args.output.exists():
        previous = json.loads(args.output.read_text(encoding="utf-8"))
        progress = (previous.get("files", {})
                    if previous.get("schema") == 1 and
                    previous.get("semantics") ==
                    "reachability-admitted-minus-trivial-v3" else {})
    else:
        progress = {}

    for filename, record in sorted(
            certified_items, key=lambda item: (transfer_size(item), item[0])):
        aggregate = summaries[filename]
        if filename in unavailable:
            progress[filename] = {
                "excluded": True,
                "reason": (
                    "certified information overlay is absent from the public "
                    "Hugging Face dataset and its ledger-bound S3 version is "
                    "no longer available"
                ),
            }
            common.write_json(progress_path, progress)
            continue
        table_entry = remote[filename]
        information = information_material(record)
        overlay_name = f"{Path(filename).stem}.ufiw"
        overlay_entry = remote.get(overlay_name) if information else None
        table = args.work_root / filename
        overlay = args.work_root / overlay_name if overlay_entry else None
        cached = progress.get(filename)
        if (cached and cached.get("audit_binary_sha256") == binary_sha and
                cached.get("tablebase_sha256") == table_entry["sha256"] and
                cached.get("information_overlay_sha256") ==
                (overlay_entry["sha256"] if overlay_entry else None)):
            # A process may have been interrupted after atomically recording
            # the result but before its finally block removed the payload.
            # Reuse the authenticated result without retaining that stale
            # table or partial download.
            for payload in (table, overlay):
                if payload is None:
                    continue
                payload.unlink(missing_ok=True)
                payload.with_suffix(payload.suffix + ".download").unlink(
                    missing_ok=True
                )
            print(f"reusing {filename}", flush=True)
            continue

        audited = False
        try:
            print(f"downloading {filename} ({table_entry['bytes'] / 1e9:.3f} GB)",
                  flush=True)
            common.download(table_entry, table, args.origin, args.dataset,
                            revision, token, args.max_retries)
            if overlay_entry and overlay:
                print(f"downloading {overlay_name} "
                      f"({overlay_entry['bytes'] / 1e9:.3f} GB)", flush=True)
                common.download(overlay_entry, overlay, args.origin, args.dataset,
                                revision, token, args.max_retries)
            completed = subprocess.run(
                common.command(args.binary, record, table, args.workers, overlay),
                check=True, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            squares = parse_counts(completed.stdout, record, information)
            flipped = normalize_and_validate_aggregate(
                filename, squares, aggregate, information
            )
            progress[filename] = {
                "audit_binary_sha256": binary_sha,
                "tablebase_sha256": table_entry["sha256"],
                "information_overlay_sha256": (
                    overlay_entry["sha256"] if overlay_entry else None
                ),
                "angel_slot": ("primary" if record["primary"] == "angel"
                               else "secondary"),
                "result_kind": "information-v2" if information else "concrete",
                "role_normalized_sides": flipped,
                "squares": squares,
            }
            common.write_json(progress_path, progress)
            print(f"audited {filename}", flush=True)
            audited = True
        finally:
            if audited and overlay:
                overlay.unlink(missing_ok=True)
                overlay.with_suffix(overlay.suffix + ".download").unlink(
                    missing_ok=True
                )
            if audited:
                table.unlink(missing_ok=True)
                table.with_suffix(table.suffix + ".download").unlink(
                    missing_ok=True
                )

    certified = {filename for filename, _ in certified_items}
    if set(progress) != certified:
        raise RuntimeError(
            "Angel summary coverage residual: "
            f"missing={sorted(certified - set(progress))} "
            f"extra={sorted(set(progress) - certified)}"
        )
    output = {
        "schema": 1,
        "description": (
            "Exact reachability-admitted Angel color-relative, horizontally "
            "mirrored root squares with authenticated trivial positions "
            "removed; attached Angels use their Halo origin and successor play "
            "remains unrestricted"
        ),
        "semantics": "reachability-admitted-minus-trivial-v3",
        "all_square_buckets": list(ALL_SQUARES),
        "plotted_start_squares": list(PLOTTED_SQUARES),
        "file_symmetry": {"a": "a/h", "b": "b/g", "c": "c/f", "d": "d/e"},
        "dataset": args.dataset,
        "dataset_revision": revision,
        "audit_binary_sha256": binary_sha,
        "files": {filename: progress[filename] for filename in sorted(progress)},
    }
    common.write_json(args.output, output)
    progress_path.unlink(missing_ok=True)
    print(args.output)


if __name__ == "__main__":
    main()
