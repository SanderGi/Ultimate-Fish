#!/usr/bin/env python3
"""Certify concrete legacy rows from the full-causal AWS audit receipt."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

import update_ultimate_tablebase_ledger as ledger


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "tablebases/README.md"
AUDIT_LINE = re.compile(
    r"reachability side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)")


def components(cell: str) -> list[int]:
    result: list[int] = []
    for part in cell.split("/"):
        match = re.fullmatch(
            r"\s*([0-9][0-9,]*)(?: \(([0-9][0-9,]*)\))?\s*", part)
        if match is None:
            raise ValueError(f"malformed existing W/L/D cell: {cell!r}")
        result.append(int(match.group(1).replace(",", "")) +
                      int((match.group(2) or "0").replace(",", "")))
    if len(result) != 3:
        raise ValueError(f"wrong W/L/D arity: {cell!r}")
    return result


def unreachable(text: str) -> list[list[int]]:
    result = [[0, 0, 0, 0] for _ in range(2)]
    matches = AUDIT_LINE.findall(text)
    if len(matches) != 2:
        raise ValueError("full-causal sidecar lacks two side rows")
    for side, unknown, win, loss, draw in matches:
        result[int(side)] = list(map(int, (unknown, win, loss, draw)))
    if any(side[0] for side in result):
        raise ValueError("full-causal sidecar contains unknown states")
    return result


def rendered(total: list[int], omitted: list[int]) -> str:
    values: list[str] = []
    for outcome, hidden in zip(total, omitted[1:]):
        if not 0 <= hidden <= outcome:
            raise ValueError("full-causal unreachable count exceeds WDL total")
        legal = outcome - hidden
        values.append(f"{legal:,}" + (f" ({hidden:,})" if hidden else ""))
    return " / ".join(values)


def storage(receipt: dict[str, object]) -> str:
    archive = receipt["archive"]
    certificate = receipt["preservation_certificate"]
    sidecar = receipt["s3"]
    return (
        f"S3 legacy archive sha256:{archive['sha256']} VersionId "
        f"{archive['version_id']}; preservation certificate "
        f"sha256:{certificate['sha256']} VersionId {certificate['version_id']}; "
        f"reachability v2 sha256:{sidecar['sha256']} VersionId "
        f"{sidecar['version_id']}"
    )


def import_receipt(receipt: dict[str, object], readme: Path) -> int:
    if (receipt.get("schema") != "ultimate-legacy-reachability-v2" or
            receipt.get("predicate") != "native-full-causal-reachability"):
        raise ValueError("legacy reachability receipt schema/predicate residual")
    audits = {str(item["filename"]): unreachable(str(item["text"]))
              for item in receipt["records"]}
    rows = ledger.entries(readme.read_text())
    values: list[str] = []
    for row in rows:
        legacy_row = (row.status == "preserving" or
                      (row.status == "certified" and
                       row.storage.startswith("S3 legacy archive")))
        if (not legacy_row or row.result_kind != "concrete" or
                row.filename not in audits):
            continue
        side_totals = (components(row.first), components(row.second))
        if row.states is None or any(sum(total) != row.states // 2
                                     for total in side_totals):
            raise ValueError(f"{row.filename}: existing WDL conservation residual")
        first = rendered(side_totals[0], audits[row.filename][0])
        second = rendered(side_totals[1], audits[row.filename][1])
        result = {
            "result_kind": "concrete", "first": first, "second": second,
            "reachability": ledger.reachability(first, second),
            "storage": storage(receipt),
        }
        values.append(row.filename + "=" +
                      json.dumps(result, separators=(",", ":")))
    if len(values) != 154:
        raise ValueError(f"expected 154 legacy concrete rows, got {len(values)}")
    ledger.update(readme, [], [], certified_values=values)
    return len(values)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipt", type=Path)
    parser.add_argument("--readme", type=Path, default=README)
    args = parser.parse_args()
    receipt = json.loads(args.receipt.read_text())
    print(json.dumps({"certified": import_receipt(receipt, args.readme)},
                     sort_keys=True))


if __name__ == "__main__":
    main()
