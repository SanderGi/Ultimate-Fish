#!/usr/bin/env python3
"""Build exact color-relative Sniper root-rank plot data.

The native auditor partitions every admitted root into all ten color-relative
ranks, which provides an aggregate conservation proof.  The plot consumes only
ranks 1-3, the legal deployment ranks for either color.  Successors remain
unrestricted, so Fisherman pulls, Mage swaps, and all later play are already
represented by the selected root's unchanged tablebase W/L/D result.

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

import audit_ultimate_checker_start_states as common  # noqa: E402


plot = common.plot
RESULT_NAMES = common.RESULT_NAMES
ALL_RANKS = tuple(range(1, 11))
PLOTTED_RANKS = tuple(range(1, 4))
CONCRETE_LINE = re.compile(
    r"reachability_(primary|secondary)_sniper_rank"
    r"_(total|excluded|trivial) rank (\d+) side ([01]) "
    r"unknown (\d+) win (\d+) loss (\d+) draw (\d+)"
)
INFORMATION_LINE = re.compile(
    r"information_reachability_(primary|secondary)_sniper_rank_"
    r"(admitted|trivial) rank (\d+) side ([01]) "
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
        if record["primary"] == "sniper" or record.get("secondary") == "sniper"
    }


def information_material(record: dict[str, Any]) -> bool:
    return bool({record["primary"], record.get("secondary")} & {"jester", "ghost"})


def exchange_folded(record: dict[str, Any]) -> bool:
    return (
        record["primary"] == "sniper"
        and record.get("secondary") == "sniper"
        and not record.get("opposing")
    )


def empty_counts() -> list[int]:
    return [0, 0, 0, 0]


def named(values: list[int]) -> dict[str, int]:
    return dict(zip(RESULT_NAMES[1:], values[1:]))


def finish_rows(
    buckets: dict[tuple[int, int], dict[str, list[int]]],
    expected_kinds: set[str],
) -> dict[str, Any]:
    rows: dict[str, Any] = {}
    for rank in ALL_RANKS:
        sides = []
        for side in range(2):
            raw = buckets.get((rank, side), {})
            if set(raw) != expected_kinds:
                raise RuntimeError(
                    f"incomplete Sniper rank {rank} audit for side {side}: "
                    f"{sorted(raw)}"
                )
            if "total" in raw:
                total = raw["total"]
                excluded = raw["excluded"]
                if any(excluded[index] > total[index] for index in range(4)):
                    raise RuntimeError(
                        f"Sniper rank {rank} excluded count exceeds total"
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
                    f"invalid Sniper rank {rank} conservation for side {side}"
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
        rows[str(rank)] = {
            "first_starts": sides[0], "second_starts": sides[1]
        }
    return rows


def parse_counts(text: str, record: dict[str, Any],
                 information: bool) -> dict[str, Any]:
    sniper_slot = "primary" if record["primary"] == "sniper" else "secondary"
    pattern = INFORMATION_LINE if information else CONCRETE_LINE
    expected_kinds = ({"admitted", "trivial"} if information
                      else {"total", "excluded", "trivial"})
    buckets: dict[tuple[int, int], dict[str, list[int]]] = {}
    for match in pattern.finditer(text):
        if match.group(1) != sniper_slot:
            continue
        kind = match.group(2)
        rank, side = int(match.group(3)), int(match.group(4))
        if rank not in ALL_RANKS:
            raise RuntimeError(f"invalid Sniper color-relative rank {rank}")
        key = (rank, side)
        if kind in buckets.setdefault(key, {}):
            raise RuntimeError("duplicate Sniper root-rank audit row")
        buckets[key][kind] = [int(match.group(index)) for index in range(5, 9)]
    return finish_rows(buckets, expected_kinds)


def normalize_and_validate_aggregate(
    filename: str, ranks: dict[str, Any], aggregate: plot.ReadmeResult,
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
                name: sum(ranks[str(rank)][key]["display"][name]
                          for rank in ALL_RANKS)
                for name in RESULT_NAMES[1:]
            }

        actual = aggregate_side()
        reversed_actual = {
            "wins": actual["losses"],
            "losses": actual["wins"],
            "draws": actual["draws"],
        }
        if actual != expected and allow_role_flip and reversed_actual == expected:
            for rank in ALL_RANKS:
                row = ranks[str(rank)][key]
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
                f"Sniper root-rank slices do not reproduce {filename} {key}: "
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
                        default=ROOT / "tablebases/sniper-start-rank-summary.json")
    parser.add_argument("--work-root", type=Path,
                        default=Path(tempfile.gettempdir()) /
                        "ultimatefish-sniper-start-rank-audit")
    parser.add_argument("--dataset", default=common.DEFAULT_DATASET)
    parser.add_argument("--origin", default=common.DEFAULT_ORIGIN)
    parser.add_argument("--revision", default=common.DEFAULT_REVISION)
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
    for filename, record in certified_items:
        if exchange_folded(record):
            continue
        if filename not in remote:
            raise RuntimeError(f"Hugging Face catalog lacks {filename}")
        if information_material(record):
            overlay_name = f"{Path(filename).stem}.ufiw"
            if overlay_name not in remote:
                raise RuntimeError(f"Hugging Face catalog lacks {overlay_name}")

    def transfer_size(item: tuple[str, dict[str, Any]]) -> int:
        filename, record = item
        if filename not in remote:
            return 0
        size = int(remote[filename]["bytes"])
        if information_material(record):
            size += int(remote[f"{Path(filename).stem}.ufiw"]["bytes"])
        return size

    auditable = [item for item in certified_items if not exchange_folded(item[1])]
    if args.plan_only:
        plan = {
            "certified_records": len(certified_items),
            "audited_records": len(auditable),
            "exchange_folded_records": len(certified_items) - len(auditable),
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
        if exchange_folded(record):
            progress[filename] = {
                "excluded": True,
                "reason": ("exchange-folded same-team Snipers have no "
                           "distinguished row Sniper"),
            }
            common.write_json(progress_path, progress)
            continue
        table_entry = remote[filename]
        information = information_material(record)
        overlay_name = f"{Path(filename).stem}.ufiw"
        overlay_entry = remote.get(overlay_name) if information else None
        cached = progress.get(filename)
        if (cached and cached.get("audit_binary_sha256") == binary_sha and
                cached.get("tablebase_sha256") == table_entry["sha256"] and
                cached.get("information_overlay_sha256") ==
                (overlay_entry["sha256"] if overlay_entry else None)):
            print(f"reusing {filename}", flush=True)
            continue

        table = args.work_root / filename
        overlay = args.work_root / overlay_name if overlay_entry else None
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
            ranks = parse_counts(completed.stdout, record, information)
            flipped = normalize_and_validate_aggregate(
                filename, ranks, aggregate, information
            )
            progress[filename] = {
                "audit_binary_sha256": binary_sha,
                "tablebase_sha256": table_entry["sha256"],
                "information_overlay_sha256": (
                    overlay_entry["sha256"] if overlay_entry else None
                ),
                "sniper_slot": ("primary" if record["primary"] == "sniper"
                                 else "secondary"),
                "result_kind": "information-v2" if information else "concrete",
                "role_normalized_sides": flipped,
                "ranks": ranks,
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
            "Sniper summary coverage residual: "
            f"missing={sorted(certified - set(progress))} "
            f"extra={sorted(set(progress) - certified)}"
        )
    output = {
        "schema": 1,
        "description": (
            "Exact reachability-admitted Sniper color-relative root ranks with "
            "authenticated trivial positions removed; successor play remains "
            "unrestricted"
        ),
        "semantics": "reachability-admitted-minus-trivial-v3",
        "all_rank_buckets": list(ALL_RANKS),
        "plotted_start_ranks": list(PLOTTED_RANKS),
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
