#!/usr/bin/env python3
"""Merge disjoint authenticated Berserker-radius audit shards."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


INVARIANTS = (
    "schema",
    "description",
    "semantics",
    "radius_to_power_substate",
    "audit_binary_sha256",
)


def merge(
    manifest_path: Path,
    shard_paths: list[Path],
    *,
    allow_partial: bool = False,
) -> dict[str, object]:
    if not shard_paths:
        raise ValueError("at least one radius-audit shard is required")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    expected = set(manifest["files"])
    documents = [
        json.loads(path.read_text(encoding="utf-8")) for path in shard_paths
    ]
    baseline = documents[0]
    files: dict[str, object] = {}
    for path, document in zip(shard_paths, documents):
        for field in INVARIANTS:
            if document.get(field) != baseline.get(field):
                raise ValueError(f"inconsistent {field} in {path}")
        for filename, record in document.get("files", {}).items():
            expected_record = manifest["files"].get(filename)
            if expected_record is None:
                raise ValueError(f"unexpected radius record: {filename}")
            if bool(record.get("excluded")) != bool(
                expected_record.get("excluded")
            ):
                raise ValueError(
                    f"radius exclusion-orientation mismatch: {filename}"
                )
            if filename in files:
                raise ValueError(f"duplicate radius record: {filename}")
            files[filename] = record
    missing = expected - set(files)
    extra = set(files) - expected
    if extra or (missing and not allow_partial):
        raise ValueError(
            f"radius shard coverage mismatch: missing={sorted(missing)}, "
            f"extra={sorted(extra)}"
        )
    return {
        field: baseline[field] for field in INVARIANTS
    } | {"files": dict(sorted(files.items()))}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allow-partial", action="store_true")
    parser.add_argument("shards", type=Path, nargs="+")
    args = parser.parse_args()
    output = merge(
        args.manifest, args.shards, allow_partial=args.allow_partial
    )
    args.output.write_text(
        json.dumps(output, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
