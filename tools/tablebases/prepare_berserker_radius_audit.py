#!/usr/bin/env python3
"""Build the exact input manifest for Berserker radius-sliced audits."""

from __future__ import annotations

import argparse
import importlib.util
import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "tablebases" / "README.md"
PLOT_PATH = Path(__file__).with_name("plot_ultimate_tablebases.py")
DEPENDENCIES = Path(__file__).with_name(
    "ultimate_concrete_remaining_wave0_dependencies.json")


def load_plot():
    spec = importlib.util.spec_from_file_location("ultimate_tablebase_plot", PLOT_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import {PLOT_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def ledger_rows(text: str) -> dict[str, list[str]]:
    block = text.split("<!-- COMPUTATION_LEDGER_START -->", 1)[1].split(
        "<!-- COMPUTATION_LEDGER_END -->", 1)[0]
    rows: dict[str, list[str]] = {}
    for line in block.splitlines():
        if not line.startswith("| `"):
            continue
        fields = [field.strip() for field in line.strip().strip("|").split("|")]
        if len(fields) == 11 and fields[3] != "—":
            rows[fields[3].strip("`")] = fields
    return rows


def artifact_version(storage: str) -> str:
    """Extract the table/archive VersionId across old and new ledger prose."""
    abbreviated = re.search(
        r"S3\s+`[0-9a-f]+(?:…|\.\.\.)?`\s*/\s*`([^`]+)`", storage)
    if abbreviated:
        return abbreviated.group(1)
    version = re.search(r"(?:S3\s+)?VersionId\s+`?([^`; )]+)", storage)
    if version:
        return version.group(1)
    raise ValueError("missing table/archive VersionId")


def build(readme: Path) -> dict[str, object]:
    plot = load_plot()
    text = readme.read_text(encoding="utf-8")
    rows = ledger_rows(text)
    summary = plot.read_summary(readme)
    catalog = plot.OutcomeCatalog(summary)
    dependency_hashes = {
        item["filename"]: item["sha256"]
        for item in json.loads(DEPENDENCIES.read_text(encoding="utf-8"))["files"]
    }
    records: dict[str, dict[str, object]] = {}
    sources = [catalog.singles["berserker"]]
    sources.extend(record for key, record in catalog.same_team.items()
                   if "berserker" in key)
    sources.extend(record for key, record in catalog.opposing.items()
                   if "berserker" in key)
    for record in sources:
        filename = str(record["filename"])
        fields = rows[filename]
        status = fields[4].strip("*").lower()
        if status not in {"certified", "preserving"}:
            continue
        kind = fields[6]
        hashes = re.findall(r"sha256:([0-9a-f]{64})", fields[10])
        if not hashes:
            raise ValueError(f"missing exact storage bindings for {filename}")
        try:
            version = artifact_version(fields[10])
        except ValueError as error:
            raise ValueError(f"{error} for {filename}") from error
        raw = summary[filename]
        primary = str(record["primary"])
        secondary = str(record["secondary"])
        slot = "primary" if primary == "berserker" else "secondary"
        excluded = (primary == secondary == "berserker")
        records[filename] = {
            "filename": filename,
            "primary": primary,
            "secondary": secondary,
            "opposing": bool(record["opposing"]),
            "result_kind": kind,
            "berserker_slot": slot,
            "expected_sha256": None if kind.startswith("information") else hashes[0],
            "expected_payload_sha256": dependency_hashes.get(filename),
            "expected_archive_sha256": hashes[0] if kind.startswith("information") else None,
            "expected_archive_version_id": version,
            "aggregate": {
                "first_starts": vars(raw.first_starts),
                "second_starts": vars(raw.second_starts),
            },
            "excluded": excluded,
            "exclusion_reason": (
                "same-team identical Berserkers are exchange-folded and have no "
                "distinguished row piece" if excluded else None),
        }
    return {
        "schema": 1,
        "bucket": "ultimatefish-info-20260808-a4e679c6-831688117652",
        "region": "us-west-2",
        "radii": {"1": 0, "2": 1, "3": 2},
        "files": dict(sorted(records.items())),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--readme", type=Path, default=README)
    args = parser.parse_args()
    print(json.dumps(build(args.readme.resolve()), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
