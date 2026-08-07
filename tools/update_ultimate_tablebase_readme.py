#!/usr/bin/env python3
"""Regenerate the checked-in tablebase summary table from packed files."""

from __future__ import annotations

import hashlib
from pathlib import Path
import struct

import plan_ultimate_tablebases as plan
import summarize_ultimate_tablebases as summarize


ROOT = Path(__file__).resolve().parents[1]
README = ROOT / "tablebases" / "README.md"
START = "<!-- GENERATED_TABLE_START -->"
END = "<!-- GENERATED_TABLE_END -->"


def display_name(record: dict[str, object]) -> str:
    first = str(record["primary"]).replace("copycat", "Copycat").title()
    second = str(record["secondary"]).title()
    if not second:
        return f"King+{first} vs King"
    if record["opposing"]:
        return f"King+{first} vs King+{second}"
    if first == second:
        return f"King+2 {first}s vs King"
    return f"King+{first}+{second} vs King"


def main() -> None:
    records = {str(record["filename"]): record for record in plan.inventory()}
    # Copycat uses v5 because its one deployable character has a linked clone,
    # but remains a K+K+1 inventory class.
    ordered = [record for record in plan.inventory()
               if (ROOT / "tablebases" / str(record["filename"])).exists()]
    lines = [
        START,
        "| File | Class | In-class edges | First material owner starts W / L / D | "
        "Second material owner / bare King starts W / L / D | SHA-256 |",
        "| --- | --- | ---: | ---: | ---: | --- |",
    ]
    for record in ordered:
        path = ROOT / "tablebases" / str(record["filename"])
        data = path.read_bytes()
        _magic, _version, _piece, _count, edges = struct.unpack_from("<8sIIII", data)
        totals, illegal = summarize.summary(path)
        digest = hashlib.sha256(data).hexdigest()
        lines.append(
            f"| `{path.name}` | {display_name(record)} | {edges:,} | "
            f"{summarize.cell(totals[0], illegal[0])} | "
            f"{summarize.cell(totals[1], illegal[1])} | `{digest}` |")
    lines.append(END)

    text = README.read_text()
    begin = text.index(START)
    end = text.index(END, begin) + len(END)
    README.write_text(text[:begin] + "\n".join(lines) + text[end:])
    print(f"updated {README} with {len(ordered)} tables")


if __name__ == "__main__":
    main()
