#!/usr/bin/env python3
"""Build the fail-closed certificate for the complete stateful Devil class."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = ROOT / "tools/tablebases/ultimate_devil_stateful_recovery.json"
FIXED_SQUARES = (0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19)
WIDTH_BY_VERSION = {1: 16, 2: 16, 3: 14, 4: 7}
HASH_FIELDS = (
    "key_sha256", "node_sha256", "sidecar_sha256", "census_sha256",
)
VERSION_FIELDS = (
    "key_version_id", "node_version_id", "receipt_version_id",
    "sidecar_version_id", "sidecar_receipt_version_id", "census_version_id",
)


def _sha256(value: object, name: str) -> str:
    text = str(value)
    if len(text) != 64:
        raise ValueError(f"{name} is not a sha256")
    try:
        int(text, 16)
    except ValueError as error:
        raise ValueError(f"{name} is not a sha256") from error
    return text


def build_certificate(manifest: dict[str, Any]) -> dict[str, Any]:
    if manifest.get("schema") != "ultimate-devil-stateful-recovery-v1":
        raise ValueError("wrong recovery schema")
    if manifest.get("entry_slice_certifies_stateful_material") is not False:
        raise ValueError("entry slice must be explicitly excluded")
    if manifest.get("required_partitions") != len(FIXED_SQUARES):
        raise ValueError("wrong required partition count")
    rows = manifest.get("partitions")
    if not isinstance(rows, list) or len(rows) != len(FIXED_SQUARES):
        raise ValueError("incomplete partition list")
    by_square = {row.get("square"): row for row in rows}
    if len(by_square) != len(rows) or tuple(sorted(by_square)) != FIXED_SQUARES:
        raise ValueError("fixed-square coverage mismatch")

    certified_rows: list[dict[str, Any]] = []
    totals = {"states": 0, "wins": 0, "losses": 0, "draws": 0}
    max_dtw = 0
    for square in FIXED_SQUARES:
        row = by_square[square]
        if row.get("status") != "PRESERVED":
            raise ValueError(f"square {square} is not preserved")
        version = int(row.get("checkpoint_version", 0))
        width = int(row.get("key_record_bytes", 0))
        if WIDTH_BY_VERSION.get(version) != width:
            raise ValueError(f"square {square} generation/key-width mismatch")
        states = int(row.get("states", 0))
        wins = int(row.get("census_wins", -1))
        losses = int(row.get("census_losses", -1))
        draws = int(row.get("census_draws", -1))
        dtw = int(row.get("census_max_dtw", -1))
        if min(states, wins, losses, draws, dtw) < 0:
            raise ValueError(f"square {square} lacks census counts")
        if wins + losses + draws != states:
            raise ValueError(f"square {square} census conservation failure")
        hashes = {name: _sha256(row.get(name), name) for name in HASH_FIELDS}
        versions: dict[str, str] = {}
        for name in VERSION_FIELDS:
            value = str(row.get(name, ""))
            if not value:
                raise ValueError(f"square {square} lacks {name}")
            versions[name] = value
        certified_rows.append({
            "label": str(row["label"]),
            "square": square,
            "states": states,
            "checkpoint_version": version,
            "key_record_bytes": width,
            **hashes,
            **versions,
            "outcomes": {"win": wins, "loss": losses, "draw": draws},
            "max_dtw": dtw,
            "conservation_residual": 0,
        })
        totals["states"] += states
        totals["wins"] += wins
        totals["losses"] += losses
        totals["draws"] += draws
        max_dtw = max(max_dtw, dtw)

    return {
        "schema": "ultimate-devil-stateful-class-certificate-v1",
        "material": "King+Devil+causally-spawned-Minions vs King",
        "entry_slice_filename": str(manifest["entry_slice_filename"]),
        "entry_slice_excluded_as_class_proof": True,
        "fixed_squares": list(FIXED_SQUARES),
        "fixed_square_coverage_residual": 0,
        "partitions": certified_rows,
        "aggregate": {
            **totals,
            "max_dtw": max_dtw,
            "conservation_residual": (
                totals["states"] - totals["wins"] - totals["losses"]
                - totals["draws"]
            ),
        },
        "never_delete": True,
    }


def canonical_bytes(certificate: dict[str, Any]) -> bytes:
    return (json.dumps(certificate, sort_keys=True, separators=(",", ":"))
            + "\n").encode()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    certificate = build_certificate(json.loads(args.manifest.read_text()))
    payload = canonical_bytes(certificate)
    if args.output:
        args.output.write_bytes(payload)
    print(json.dumps({
        "sha256": hashlib.sha256(payload).hexdigest(),
        "bytes": len(payload),
        "aggregate": certificate["aggregate"],
    }, sort_keys=True))


if __name__ == "__main__":
    main()
