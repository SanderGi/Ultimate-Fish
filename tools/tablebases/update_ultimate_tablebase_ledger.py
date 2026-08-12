#!/usr/bin/env python3
"""Maintain the README as the canonical Ultimate tablebase computation ledger.

The ledger is deliberately independent of files present in ``tablebases/``.
Local files are only caches; an entry remains visible after its bytes move to
S3, and an active current-model run overrides any older local result.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, replace
from pathlib import Path
import json
import re
import sys
from typing import Iterable


ROOT = Path(__file__).resolve().parents[2]
TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import plan_ultimate_tablebases as plan  # noqa: E402
import ultimate_information_tablebases as information  # noqa: E402


README = ROOT / "tablebases" / "README.md"
START = "<!-- COMPUTATION_LEDGER_START -->"
END = "<!-- COMPUTATION_LEDGER_END -->"
RESULT_START = "<!-- GENERATED_TABLE_START -->"
RESULT_END = "<!-- GENERATED_TABLE_END -->"
STATUSES = {
    "certified", "preserving", "computing", "planned", "draw", "deferred",
    "blocked",
}
DEFERRED = plan.DEFERRED_DYNAMIC_K2
SEPARATORS = plan.COPYCAT_SEPARATORS


@dataclass(frozen=True)
class Result:
    filename: str
    edges: str
    first: str
    second: str
    digest: str


@dataclass(frozen=True)
class Entry:
    key: str
    material: str
    domain: str
    filename: str
    status: str
    states: int | None
    result_kind: str
    first: str
    second: str
    reachability: str
    storage: str
    digest: str


def _fields(line: str) -> list[str]:
    return [field.strip() for field in line.strip().strip("|").split("|")]


def result_rows(text: str) -> dict[str, Result]:
    generated = text.split(RESULT_START, 1)[1].split(RESULT_END, 1)[0]
    rows: dict[str, Result] = {}
    for line in generated.splitlines():
        if not line.startswith("| `"):
            continue
        fields = _fields(line)
        if len(fields) < 6:
            continue
        filename = fields[0].strip("`")
        digest = fields[-1].strip("`")
        rows[filename] = Result(filename, fields[2], fields[3], fields[4], digest)
    return rows


def old_entries(text: str) -> dict[str, Entry]:
    if START not in text or END not in text:
        return {}
    generated = text.split(START, 1)[1].split(END, 1)[0]
    rows: dict[str, Entry] = {}
    for line in generated.splitlines():
        if not line.startswith("| `"):
            continue
        fields = _fields(line)
        if len(fields) != 11:
            continue
        key = fields[0].strip("`")
        state_text = fields[5].replace(",", "")
        rows[key] = Entry(
            key=key, material=fields[1], domain=fields[2],
            filename=fields[3].strip("`") if fields[3] != "—" else "",
            status=fields[4].strip("*").lower(),
            states=int(state_text) if state_text.isdigit() else None,
            result_kind=fields[6], first=fields[7], second=fields[8],
            reachability=fields[9], storage=fields[10], digest="",
        )
    return rows


def parse_wdl(cell: str) -> tuple[int, int]:
    """Return admitted and unreachable counts from a rendered W/L/D cell."""
    admitted = unreachable = 0
    for component in cell.split("/"):
        match = re.fullmatch(r"\s*([0-9][0-9,]*)(?: \(([0-9][0-9,]*)\))?\s*",
                             component)
        if match is None:
            raise ValueError(f"malformed W/L/D cell: {cell!r}")
        admitted += int(match.group(1).replace(",", ""))
        if match.group(2):
            unreachable += int(match.group(2).replace(",", ""))
    return admitted, unreachable


def reachability(first: str, second: str) -> str:
    first_live, first_unreachable = parse_wdl(first)
    second_live, second_unreachable = parse_wdl(second)
    return (f"{first_live:,} / {first_unreachable:,}; "
            f"{second_live:,} / {second_unreachable:,}")


def material_key(domain: str, first: str, second: str = "") -> str:
    names = [piece.name for piece in plan.PIECES]
    order = {name: index for index, name in enumerate(names)}
    if domain == "single":
        return f"single:{first}"
    a, b = sorted((first, second), key=order.__getitem__)
    return f"{domain}:{a}+{b}"


def display(domain: str, first: str, second: str = "") -> str:
    def label(name: str) -> str:
        return "Copycat" if name == "copycat" else name.title()
    if domain == "single":
        return f"King+{label(first)} vs King"
    if domain == "same":
        if first == second:
            return f"King+2 {label(first)}s vs King"
        return f"King+{label(first)}+{label(second)} vs King"
    return f"King+{label(first)} vs King+{label(second)}"


def record_catalog() -> dict[str, dict[str, object]]:
    """Return one canonical filename/codec record for each material cell."""
    result: dict[str, dict[str, object]] = {}
    for record in (*plan.stateful_candidates(),
                   *plan.mirror_copycat_candidates(), *plan.inventory()):
        primary = str(record["primary"])
        secondary = str(record["secondary"])
        domain = "single" if not secondary else (
            "opposed" if record["opposing"] else "same")
        key = material_key(domain, primary, secondary)
        # inventory() is last so already generated/requested filenames preserve
        # their authenticated header-owner order (for example Dragon/Penguin).
        result[key] = record
    return result


def _is_deferred(first: str, second: str = "") -> bool:
    names = {first, second} - {""}
    if names & DEFERRED:
        return True
    return "copycat" in names and bool(names & SEPARATORS)


def _requires_information(record: dict[str, object] | None) -> bool:
    """Return whether material contains public hidden information.

    The solver inventory is intentionally narrower than the complete K+K+2
    campaign.  Material truth, rather than membership in that implemented
    solver subset, decides whether concrete W/L/D may be published.
    """
    if record is None:
        return False
    return bool({str(record.get("primary", "")),
                 str(record.get("secondary", ""))} & {"jester", "ghost"})


def entries(text: str) -> list[Entry]:
    results = result_rows(text)
    previous = old_entries(text)
    records = record_catalog()
    pieces = list(plan.PIECES)
    result: list[Entry] = []

    def add(domain: str, first: str, second: str = "") -> None:
        key = material_key(domain, first, second)
        record = records.get(key)
        filename = str(record["filename"]) if record else ""
        exact = results.get(filename)
        if _is_deferred(first, second):
            status = "deferred"
        elif record is None:
            status = "draw"
        elif exact is not None:
            status = "preserving"
        else:
            status = "planned"
        old = previous.get(key)
        if old is not None and old.status in STATUSES:
            status = old.status
        states = int(record["states"]) if record else None
        hidden = _requires_information(record)
        if exact is not None and (not hidden or
                                  filename in information.AFFECTED_FILENAMES):
            kind = "information v2" if hidden else "concrete"
            first_cell, second_cell = exact.first, exact.second
            audited = reachability(first_cell, second_cell)
            digest = exact.digest
        elif (old is not None and old.status in {"certified", "preserving"} and
              old.first != "—" and old.second != "—" and
              old.reachability != "—"):
            # S3-only information results may not have a concrete UFTB row in
            # the legacy generated-details table.  Once their exact values
            # enter the canonical ledger, retain them across status-only
            # regeneration instead of silently reverting to placeholders.
            kind, first_cell, second_cell = (
                old.result_kind, old.first, old.second)
            audited, digest = old.reachability, ""
        elif status == "draw":
            kind, first_cell, second_cell = "insufficient material", "0 / 0 / 1", "0 / 0 / 1"
            audited, digest = "closed-form draw", ""
        else:
            kind = "information required" if hidden else "concrete"
            first_cell = second_cell = audited = "—"
            digest = ""
        storage = old.storage if old is not None else (
            "S3 preservation pending" if exact is not None else "—")
        if status == "certified" and storage == "S3 preservation pending":
            status = "preserving"
        result.append(Entry(
            key, display(domain, first, second), domain, filename, status,
            states, kind, first_cell, second_cell, audited, storage, digest,
        ))

    for piece in pieces:
        add("single", piece.name)
    for first_index, first in enumerate(pieces):
        for second in pieces[first_index:]:
            add("same", first.name, second.name)
    for first_index, first in enumerate(pieces):
        for second in pieces[first_index:]:
            add("opposed", first.name, second.name)
    if len(result) != 624 or len({row.key for row in result}) != 624:
        raise RuntimeError("canonical 24 + 300 + 300 material ledger residual")
    return result


def apply_overrides(rows: Iterable[Entry], statuses: dict[str, str],
                    storage: dict[str, str]) -> list[Entry]:
    result: list[Entry] = []
    seen_status: set[str] = set()
    seen_storage: set[str] = set()
    for row in rows:
        keys = {row.key, row.filename} - {""}
        status_matches = keys & statuses.keys()
        storage_matches = keys & storage.keys()
        if len(status_matches) > 1 or len(storage_matches) > 1:
            raise ValueError(f"ambiguous override for {row.key}")
        status = row.status
        if status_matches:
            name = next(iter(status_matches))
            status = statuses[name]
            seen_status.add(name)
        stored = row.storage
        if storage_matches:
            name = next(iter(storage_matches))
            stored = storage[name]
            seen_storage.add(name)
        # A current computation must never expose a previous result as current.
        if status in {"computing", "planned", "blocked", "deferred"}:
            result.append(replace(row, status=status, first="—", second="—",
                                  reachability="—", storage=stored))
        else:
            result.append(replace(row, status=status, storage=stored))
    missing = (set(statuses) - seen_status) | (set(storage) - seen_storage)
    if missing:
        raise ValueError(f"ledger override did not match: {sorted(missing)}")
    return result


def parse_certified(values: list[str]) -> dict[str, dict[str, str]]:
    result: dict[str, dict[str, str]] = {}
    for value in values:
        key, separator, encoded = value.partition("=")
        if not separator or not key:
            raise ValueError(f"expected KEY=JSON: {value!r}")
        record = json.loads(encoded)
        if not isinstance(record, dict):
            raise ValueError(f"certified result is not an object: {key}")
        fields = {name: str(record.get(name, "")) for name in
                  ("result_kind", "first", "second", "reachability", "storage")}
        if (fields["result_kind"] not in {"concrete", "information v2"} or
                not fields["storage"] or fields["storage"] == "—" or
                reachability(fields["first"], fields["second"]) !=
                fields["reachability"]):
            raise ValueError(f"invalid certified result: {key}")
        result[key] = fields
    return result


def apply_certified(rows: Iterable[Entry],
                    certified: dict[str, dict[str, str]]) -> list[Entry]:
    result: list[Entry] = []
    seen: set[str] = set()
    for row in rows:
        matches = ({row.key, row.filename} - {""}) & certified.keys()
        if len(matches) > 1:
            raise ValueError(f"ambiguous certified result for {row.key}")
        if not matches:
            result.append(row)
            continue
        name = next(iter(matches))
        record = certified[name]
        seen.add(name)
        result.append(replace(
            row, status="certified", result_kind=record["result_kind"],
            first=record["first"], second=record["second"],
            reachability=record["reachability"], storage=record["storage"]))
    missing = set(certified) - seen
    if missing:
        raise ValueError(f"certified result did not match: {sorted(missing)}")
    return result


def render(rows: list[Entry]) -> str:
    counts = {status: sum(row.status == status for row in rows)
              for status in sorted(STATUSES)}
    lines = [
        START,
        (f"Ledger totals: **{counts['certified']} certified**, "
         f"**{counts['preserving']} preserving**, "
         f"**{counts['computing']} computing**, **{counts['draw']} exact draws**, "
         f"**{counts['planned']} planned**, **{counts['blocked']} blocked**, and "
         f"**{counts['deferred']} deferred**; {len(rows)} unique material classes."),
        "",
        "| Key | Class | Domain | File | Status | Indexed states | Result domain | First starts W / L / D | Second starts W / L / D | Reachable / unreachable (first; second) | Canonical storage |",
        "| --- | --- | --- | --- | --- | ---: | --- | ---: | ---: | ---: | --- |",
    ]
    for row in rows:
        filename = f"`{row.filename}`" if row.filename else "—"
        states = f"{row.states:,}" if row.states is not None else "—"
        lines.append(
            f"| `{row.key}` | {row.material} | {row.domain} | {filename} | "
            f"**{row.status.upper()}** | {states} | {row.result_kind} | "
            f"{row.first} | {row.second} | {row.reachability} | {row.storage} |")
    lines.append(END)
    return "\n".join(lines)


def parse_assignments(values: list[str], *, statuses: bool) -> dict[str, str]:
    result: dict[str, str] = {}
    for value in values:
        key, separator, setting = value.partition("=")
        if not separator or not key or not setting:
            raise ValueError(f"expected KEY=VALUE: {value!r}")
        if statuses and setting not in STATUSES:
            raise ValueError(f"unsupported ledger status: {setting}")
        result[key] = setting
    return result


def update(path: Path, status_values: list[str], storage_values: list[str],
           result_storage: str | None = None,
           certified_values: list[str] | None = None) -> None:
    text = path.read_text(encoding="utf-8")
    rows = apply_overrides(
        entries(text), parse_assignments(status_values, statuses=True),
        parse_assignments(storage_values, statuses=False))
    rows = apply_certified(rows, parse_certified(certified_values or []))
    if result_storage is not None:
        rows = [replace(row, storage=result_storage)
                if row.digest and row.storage == "S3 preservation pending" else row
                for row in rows]
    block = render(rows)
    if START in text and END in text:
        begin = text.index(START)
        end = text.index(END, begin) + len(END)
        text = text[:begin] + block + text[end:]
    else:
        anchor = RESULT_START
        text = text.replace(anchor, block + "\n\n## Certified result details\n\n" + anchor,
                            1)
    path.write_text(text, encoding="utf-8")
    print(f"updated {path} with {len(rows)} canonical material classes")


def check_launch(path: Path, target: str, resume: bool) -> None:
    rows = entries(path.read_text(encoding="utf-8"))
    matches = [row for row in rows if target in {row.key, row.filename}]
    if len(matches) != 1:
        raise RuntimeError(f"ledger launch target is not unique: {target}")
    row = matches[0]
    required = "computing" if resume else "planned"
    if row.status != required:
        raise RuntimeError(
            f"{target}: ledger status {row.status}; expected {required}; "
            "refusing duplicate or untracked computation")
    print(f"LEDGER_LAUNCH_ALLOWED {row.key} {row.filename or '-'} {row.status}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--readme", type=Path, default=README)
    parser.add_argument("--set-status", action="append", default=[],
                        metavar="KEY=STATUS")
    parser.add_argument("--set-storage", action="append", default=[],
                        metavar="KEY=DESCRIPTION")
    parser.add_argument("--result-storage",
                        help="storage description for completed result rows still pending preservation")
    parser.add_argument("--set-certified", action="append", default=[],
                        metavar="KEY=JSON",
                        help="install exact certified result cells and storage")
    parser.add_argument("--check-launch", metavar="KEY_OR_FILE",
                        help="fail closed unless a new computation is PLANNED")
    parser.add_argument("--resume", action="store_true",
                        help="with --check-launch, require COMPUTING instead")
    args = parser.parse_args()
    if args.check_launch:
        if (args.set_status or args.set_storage or args.result_storage or
                args.set_certified):
            parser.error("--check-launch cannot be combined with ledger edits")
        check_launch(args.readme, args.check_launch, args.resume)
        return
    if args.resume:
        parser.error("--resume requires --check-launch")
    update(args.readme, args.set_status, args.set_storage, args.result_storage,
           args.set_certified)


if __name__ == "__main__":
    main()
