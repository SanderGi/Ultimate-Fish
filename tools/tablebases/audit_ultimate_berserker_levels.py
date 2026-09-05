#!/usr/bin/env python3
"""Build exact Berserker root-radius plot data from public artifacts.

The tablebase codec has ten Berserker power substates: exact power 0 through 8,
then one board-saturating power 9+ bucket.  These become plot radii 1 through 9
and radius 10+, respectively.  Only the root is partitioned; successor play is
unrestricted, so every row retains the original tablebase W/L/D result.

Each certified material group is downloaded and SHA-256 authenticated
separately, audited with a bounded worker count, and removed before the next
group is fetched.  This bounds live disk usage to one table plus, for hidden
information material, its one observation overlay.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
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
POWER_SUBSTATES = tuple(range(10))
RADII = tuple(range(1, 11))


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
        if record["primary"] == "berserker"
        or record.get("secondary") == "berserker"
    }


def information_material(record: dict[str, Any]) -> bool:
    return bool({record["primary"], record.get("secondary")} & {"jester", "ghost"})


def exchange_folded(record: dict[str, Any]) -> bool:
    return (
        record["primary"] == "berserker"
        and record.get("secondary") == "berserker"
        and not record.get("opposing")
    )


def requires_turn_boundary(record: dict[str, Any]) -> bool:
    return "prince" in {record["primary"], record.get("secondary")}


def preserve_reporting_scope(
    output: dict[str, Any], record: dict[str, Any],
) -> None:
    if requires_turn_boundary(record):
        output["reporting_scope"] = "turn-boundary-continuation-none"


def empty_counts() -> list[int]:
    return [0, 0, 0, 0]


def add_counts(target: list[int], source: list[int]) -> None:
    for index, value in enumerate(source):
        target[index] += value


def named(values: list[int]) -> dict[str, int]:
    return dict(zip(RESULT_NAMES[1:], values[1:]))


def finish_rows(
    buckets: dict[tuple[int, int], dict[str, list[int]]],
    expected_kinds: set[str],
) -> dict[str, Any]:
    radii: dict[str, Any] = {}
    for substate in POWER_SUBSTATES:
        sides = []
        for side in range(2):
            raw = buckets.get((substate, side), {})
            if set(raw) != expected_kinds:
                raise RuntimeError(
                    f"incomplete Berserker substate {substate} audit for "
                    f"side {side}: {sorted(raw)}"
                )
            if "total" in raw:
                total = raw["total"]
                excluded = raw["excluded"]
                if any(excluded[index] > total[index] for index in range(4)):
                    raise RuntimeError(
                        f"Berserker substate {substate} excluded count exceeds total"
                    )
                admitted = [
                    total[index] - excluded[index] for index in range(4)
                ]
            else:
                admitted = raw["admitted"]
                excluded = raw["excluded"]
                total = [
                    admitted[index] + excluded[index] for index in range(4)
                ]
            trivial = raw["trivial"]
            if (
                admitted[0]
                or trivial[0]
                or any(trivial[index] > admitted[index] for index in range(4))
            ):
                raise RuntimeError(
                    f"invalid Berserker substate {substate} conservation for "
                    f"side {side}"
                )
            display = [
                admitted[index] - trivial[index] for index in range(4)
            ]
            sides.append({
                "total": named(total),
                "excluded": named(excluded),
                "admitted": named(admitted),
                "trivial": named(trivial),
                "display": named(display),
            })
        radius = substate + 1
        radii[str(radius)] = {
            "power_substate": substate,
            "first_starts": sides[0],
            "second_starts": sides[1],
        }
    return radii


def parse_counts(
    text: str, record: dict[str, Any], information: bool,
) -> dict[str, Any]:
    berserker_slot = (
        "primary" if record["primary"] == "berserker" else "secondary"
    )
    buckets: dict[tuple[int, int], dict[str, list[int]]] = {}
    if not information:
        for match in common.CONCRETE_LINE.finditer(text):
            if match.group(1) != berserker_slot:
                continue
            kind = {None: "excluded", "_total": "total",
                    "_trivial": "trivial"}[match.group(2)]
            substate, side = int(match.group(3)), int(match.group(4))
            if substate not in POWER_SUBSTATES:
                raise RuntimeError(f"invalid Berserker substate {substate}")
            key = (substate, side)
            if kind in buckets.setdefault(key, {}):
                raise RuntimeError("duplicate Berserker audit row")
            buckets[key][kind] = [
                int(match.group(index)) for index in range(5, 9)
            ]
        return finish_rows(buckets, {"total", "excluded", "trivial"})

    primary_factor = plot.PIECE_BY_NAME[str(record["primary"])].state_factor
    secondary_factor = plot.PIECE_BY_NAME[str(record["secondary"])].state_factor
    combined_factor = primary_factor * secondary_factor
    combined_rows: dict[tuple[int, int], dict[str, list[int]]] = {}
    for match in common.INFORMATION_LINE.finditer(text):
        kind = match.group(1)
        combined, side = int(match.group(2)), int(match.group(3))
        if not 0 <= combined < combined_factor:
            raise RuntimeError(f"invalid combined substate {combined}")
        key = (combined, side)
        if kind in combined_rows.setdefault(key, {}):
            raise RuntimeError("duplicate Berserker information audit row")
        combined_rows[key][kind] = [
            int(match.group(index)) for index in range(4, 8)
        ]
    for combined in range(combined_factor):
        substate = (
            combined // secondary_factor
            if berserker_slot == "primary"
            else combined % secondary_factor
        )
        if substate not in POWER_SUBSTATES:
            raise RuntimeError("information substate does not encode a Berserker")
        for side in range(2):
            raw = combined_rows.get((combined, side), {})
            if set(raw) != {"admitted", "excluded", "trivial"}:
                raise RuntimeError(
                    f"incomplete combined substate {combined} audit for side {side}"
                )
            target = buckets.setdefault((substate, side), {
                kind: empty_counts() for kind in raw
            })
            for kind, counts in raw.items():
                add_counts(target[kind], counts)
    return finish_rows(buckets, {"admitted", "excluded", "trivial"})


def normalize_and_validate_aggregate(
    filename: str,
    radii: dict[str, Any],
    aggregate: plot.ReadmeResult,
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
                name: sum(radii[str(radius)][key]["display"][name]
                          for radius in RADII)
                for name in RESULT_NAMES[1:]
            }

        actual = aggregate_side()
        reversed_actual = {
            "wins": actual["losses"],
            "losses": actual["wins"],
            "draws": actual["draws"],
        }
        if actual != expected and allow_role_flip and reversed_actual == expected:
            for radius in RADII:
                row = radii[str(radius)][key]
                for bucket in (
                    "total", "excluded", "admitted", "trivial", "display"
                ):
                    row[bucket]["wins"], row[bucket]["losses"] = (
                        row[bucket]["losses"], row[bucket]["wins"]
                    )
            actual = aggregate_side()
            flipped.append(side)
        if actual != expected:
            raise RuntimeError(
                f"Berserker radius slices do not reproduce {filename} {key}: "
                f"{actual} != {expected}"
            )
    return flipped


def validate_existing_prefix(
    filename: str, radii: dict[str, Any], existing: dict[str, Any],
) -> None:
    old = existing.get(filename, {}).get("radii", {})
    if not {"1", "2", "3"} <= set(old):
        raise RuntimeError(f"missing authenticated radius 1-3 baseline: {filename}")
    for radius in (1, 2, 3):
        for key in ("first_starts", "second_starts"):
            for bucket in ("admitted", "trivial", "display"):
                if radii[str(radius)][key][bucket] != old[str(radius)][key][bucket]:
                    raise RuntimeError(
                        f"Berserker radius baseline residual: {filename} "
                        f"radius {radius} {key} {bucket}"
                    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path,
                        default=ROOT / "src/ultimate_tablebase")
    parser.add_argument("--readme", type=Path,
                        default=ROOT / "tablebases/README.md")
    parser.add_argument("--baseline", type=Path,
                        default=ROOT / "tablebases/berserker-radius-summary.json")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "tablebases/berserker-radius-summary.json")
    parser.add_argument("--work-root", type=Path,
                        default=Path(tempfile.gettempdir()) /
                        "ultimatefish-berserker-level-audit")
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
        if exchange_folded(record):
            return 0
        size = int(remote[filename]["bytes"])
        if information_material(record):
            size += int(remote[f"{Path(filename).stem}.ufiw"]["bytes"])
        return size

    auditable = [item for item in certified_items if not exchange_folded(item[1])]
    if args.plan_only:
        print(json.dumps({
            "certified_records": len(certified_items),
            "audited_records": len(auditable),
            "exchange_folded_records": len(certified_items) - len(auditable),
            "total_transfer_bytes": sum(map(transfer_size, auditable)),
            "maximum_live_bytes": max(map(transfer_size, auditable), default=0),
            "dataset_revision": revision,
        }, indent=2, sort_keys=True))
        return

    baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
    if (
        baseline.get("schema") not in (2, 3)
        or baseline.get("semantics") != "reachability-admitted-minus-trivial-v3"
    ):
        raise RuntimeError("expected an authenticated Berserker-radius baseline")
    existing = baseline["files"]

    args.work_root.mkdir(parents=True, exist_ok=True)
    progress_path = args.work_root / "progress.json"
    previous_progress: dict[str, Any] = {}
    if args.output.exists():
        previous = json.loads(args.output.read_text(encoding="utf-8"))
        previous_progress = (
            previous.get("files", {})
            if previous.get("schema") == 3
            and previous.get("semantics") ==
            "reachability-admitted-minus-trivial-v3"
            else {}
        )
    progress = previous_progress
    if progress_path.exists():
        # The durable completed summary is a valid cache too. Merge a partial
        # interruption checkpoint over it rather than discarding already
        # published records on a reproducibility rerun.
        progress |= json.loads(progress_path.read_text(encoding="utf-8"))

    for filename, record in sorted(
            certified_items, key=lambda item: (transfer_size(item), item[0])):
        if exchange_folded(record):
            progress[filename] = {
                "excluded": True,
                "reason": (
                    "same-team identical Berserkers are exchange-folded and "
                    "have no distinguished row piece"
                ),
            }
            common.write_json(progress_path, progress)
            continue
        aggregate = summaries[filename]
        table_entry = remote[filename]
        information = information_material(record)
        overlay_name = f"{Path(filename).stem}.ufiw"
        overlay_entry = remote.get(overlay_name) if information else None
        table = args.work_root / filename
        overlay = args.work_root / overlay_name if overlay_entry else None
        cached = progress.get(filename)
        if (
            cached
            and cached.get("audit_binary_sha256") == binary_sha
            and cached.get("tablebase_sha256") == table_entry["sha256"]
            and cached.get("information_overlay_sha256") ==
            (overlay_entry["sha256"] if overlay_entry else None)
        ):
            validate_existing_prefix(filename, cached["radii"], existing)
            preserve_reporting_scope(cached, record)
            common.write_json(progress_path, progress)
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
            radii = parse_counts(completed.stdout, record, information)
            flipped = normalize_and_validate_aggregate(
                filename, radii, aggregate, information
            )
            validate_existing_prefix(filename, radii, existing)
            progress[filename] = {
                "audit_binary_sha256": binary_sha,
                "tablebase_sha256": table_entry["sha256"],
                "information_overlay_sha256": (
                    overlay_entry["sha256"] if overlay_entry else None
                ),
                "berserker_slot": (
                    "primary" if record["primary"] == "berserker"
                    else "secondary"
                ),
                "result_kind": "information-v2" if information else "concrete",
                "role_normalized_sides": flipped,
                "trivial_semantics": "authenticated-per-substate-v3",
                "radii": radii,
            }
            preserve_reporting_scope(progress[filename], record)
            common.write_json(progress_path, progress)
            print(f"audited {filename}", flush=True)
            audited = True
        finally:
            # Keep a payload only after audit/parser failure, making retries
            # cheap. Successful groups are removed before the next download.
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
            "Berserker summary coverage residual: "
            f"missing={sorted(certified - set(progress))} "
            f"extra={sorted(set(progress) - certified)}"
        )
    output = {
        "schema": 3,
        "description": (
            "Exact reachability-admitted Berserker root-radius slices with "
            "authenticated trivial positions removed; radius 10 represents "
            "the board-saturating power 9+ bucket and successor play remains "
            "unrestricted"
        ),
        "semantics": "reachability-admitted-minus-trivial-v3",
        "radius_to_power_substate": {
            str(radius): radius - 1 for radius in RADII
        },
        "saturated_radius": 10,
        "dataset": args.dataset,
        "dataset_revision": revision,
        "audit_binary_sha256": binary_sha,
        "files": {
            filename: progress[filename] for filename in sorted(progress)
        },
    }
    common.write_json(args.output, output)
    progress_path.unlink(missing_ok=True)
    print(args.output)


if __name__ == "__main__":
    main()
