#!/usr/bin/env python3
"""Regenerate the checked-in tablebase summary table from packed files."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct

import plan_ultimate_tablebases as plan
import summarize_ultimate_tablebases as summarize
import ultimate_tablebase_shards as shards


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


def cached_rows(text: str) -> dict[str, str]:
    """Return previously generated rows keyed by their logical filename.

    Exact table files are immutable once generated. Reusing an older row when
    its file is no newer than the README avoids rereading gigabytes on every
    incremental batch. ``--full`` remains the authoritative end-to-end audit.
    """
    begin = text.index(START)
    end = text.index(END, begin)
    result: dict[str, str] = {}
    for line in text[begin:end].splitlines():
        if not line.startswith("| `"):
            continue
        filename, separator, _rest = line[3:].partition("` |")
        if separator:
            result[filename] = line
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--full", action="store_true",
        help="reread, validate, summarize, and hash every packed table")
    args = parser.parse_args()

    records = {str(record["filename"]): record for record in plan.inventory()}
    # Copycat uses v5 because its one deployable character has a linked clone,
    # but remains a K+K+1 inventory class.
    ordered = [record for record in plan.inventory()
               if (ROOT / "tablebases" / str(record["filename"])).exists()]
    text = README.read_text()
    old_rows = cached_rows(text)
    readme_mtime = README.stat().st_mtime_ns
    logic_mtime = max(Path(__file__).stat().st_mtime_ns,
                      Path(summarize.__file__).stat().st_mtime_ns,
                      Path(plan.__file__).stat().st_mtime_ns,
                      Path(shards.__file__).stat().st_mtime_ns,
                      summarize.REACHABILITY.stat().st_mtime_ns)
    reused = 0
    lines = [
        START,
        "| File | Class | In-class edges | First material owner starts W / L / D | "
        "Second material owner / bare King starts W / L / D | SHA-256 |",
        "| --- | --- | ---: | ---: | ---: | --- |",
    ]
    for record in ordered:
        path = ROOT / "tablebases" / str(record["filename"])
        cached = old_rows.get(path.name)
        if (not args.full and logic_mtime <= readme_mtime and cached is not None
                and path.stat().st_mtime_ns <= readme_mtime):
            lines.append(cached)
            reused += 1
            continue
        data = shards.read_logical(path)
        _magic, version, _piece, _count, edges = struct.unpack_from("<8sIIII", data)
        if version >= 6:
            edges = struct.unpack_from("<Q", data, 48)[0]
        digest = hashlib.sha256(data).hexdigest()
        totals, illegal = summarize.summary(path, data, digest)
        lines.append(
            f"| `{path.name}` | {display_name(record)} | {edges:,} | "
            f"{summarize.cell(totals[0], illegal[0])} | "
            f"{summarize.cell(totals[1], illegal[1])} | `{digest}` |")
    lines.append(END)

    begin = text.index(START)
    end = text.index(END, begin) + len(END)
    README.write_text(text[:begin] + "\n".join(lines) + text[end:])
    print(f"updated {README} with {len(ordered)} tables ({reused} cached)")


if __name__ == "__main__":
    main()
