#!/usr/bin/env python3
"""Audit dense table states with Ultimate Fish's native legality semantics."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile

import plan_ultimate_tablebases as plan
import ultimate_tablebase_shards as shards


ROOT = Path(__file__).resolve().parents[2]
CATALOG = ROOT / "tablebases" / "reachability.json"
ENGINE = ROOT / "src" / "ultimate_tablebase"
LINE = re.compile(
    r"(?:predecessor_safety|reachability) side (\d) unknown (\d+) "
    r"win (\d+) loss (\d+) draw (\d+)")
STATEFUL = {"pawn", "berserker", "ghost", "penguin", "sniper", "prince", "checker"}


def audit(record: dict[str, object], causal: bool) -> list[list[int]]:
    path = ROOT / "tablebases" / str(record["filename"])
    command = [str(ENGINE), "--piece", str(record["primary"]),
               "--checkpoint-every", "0"]
    secondary = str(record["secondary"])
    if secondary:
        command += ["--piece2", secondary]
    if record["opposing"]:
        command.append("--opposing")
    temporary_name: str | None = None
    audit_path = path
    if shards.manifest(path):
        with tempfile.NamedTemporaryFile(prefix=path.stem + "-audit-", suffix=".uftb",
                                         delete=False) as temporary:
            temporary_name = temporary.name
            for chunk in shards.iter_logical(path):
                temporary.write(chunk)
        audit_path = Path(temporary_name)
    command += ["--audit-reachability" if causal else "--audit-predecessor-safety",
                str(audit_path)]
    try:
        output = subprocess.run(command, check=True, capture_output=True, text=True).stdout
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
    counts = [[0, 0, 0, 0] for _ in range(2)]
    matches = LINE.findall(output)
    if len(matches) != 2:
        raise RuntimeError(f"unexpected native audit output for {path.name}: {output!r}")
    for side, unknown, win, loss, draw in matches:
        counts[int(side)] = [int(unknown), int(win), int(loss), int(draw)]
    return counts


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--all", action="store_true",
                        help="audit every generated class (default: stateless only)")
    parser.add_argument("--causal", action="store_true",
                        help="union native state-causality checks (use with explicit files)")
    parser.add_argument("files", nargs="*")
    args = parser.parse_args()
    if not ENGINE.exists():
        subprocess.run(["make", "-C", str(ROOT / "src"), "ultimate-tablebase"],
                       check=True)
    requested = set(args.files)
    records = []
    for record in plan.inventory():
        path = ROOT / "tablebases" / str(record["filename"])
        if not path.exists() or (requested and path.name not in requested):
            continue
        stateful = bool({str(record["primary"]), str(record["secondary"])} & STATEFUL)
        if not args.all and stateful and not (args.causal and requested):
            continue
        records.append(record)
    document = {"version": 1, "predicate": "ordinary_previous_mover_king_safety",
                "files": {}}
    if CATALOG.exists():
        document = json.loads(CATALOG.read_text())
    files = document.setdefault("files", {})
    for number, record in enumerate(records, 1):
        filename = str(record["filename"])
        path = ROOT / "tablebases" / filename
        counts = audit(record, args.causal)
        entry = files.setdefault(filename, {})
        entry["sha256"] = shards.logical_sha256(path)
        entry["necessary_reachability" if args.causal else
              "ordinary_predecessor_safety"] = counts
        print(f"[{number}/{len(records)}] {filename}: {counts}", flush=True)
    CATALOG.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
