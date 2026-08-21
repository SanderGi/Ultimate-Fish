#!/usr/bin/env python3
"""Import an authenticated information-v2 trivial audit into radius slices."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


KINDS = ("unknown", "win", "loss", "draw")
SUMMARY_FIELDS = {"win": "wins", "loss": "losses", "draw": "draws"}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_receipt(receipt: dict[str, object], sidecar: Path) -> None:
    filename = str(receipt.get("filename"))
    if (receipt.get("schema") != "ultimate-information-trivial-receipt-v2" or
            receipt.get("substates") != 10):
        raise RuntimeError(f"invalid information trivial receipt: {filename}")
    if sha256(sidecar) != receipt.get("sidecar_sha256"):
        raise RuntimeError(f"information trivial sidecar SHA residual: {filename}")
    text = sidecar.read_text(encoding="utf-8")
    required = (
        f"information_reachability_binding source_sha256 "
        f"{receipt['source_sha256']} model_sha256 {receipt['model_sha256']}",
        f"information_trivial_overlay_sha256 {receipt['overlay_sha256']}",
        f"information_trivial_binary_sha256 {receipt['binary_sha256']}",
        f"information_trivial_source_bundle_sha256 "
        f"{receipt['source_bundle_sha256']}",
    )
    if any(line not in text for line in required):
        raise RuntimeError(f"information trivial sidecar binding residual: {filename}")

    detail = receipt.get("substate_counts")
    totals = receipt.get("counts")
    if (not isinstance(detail, dict) or
            set(detail) != {str(value) for value in range(10)} or
            not isinstance(totals, dict) or set(totals) != {"0", "1"}):
        raise RuntimeError(f"information trivial coverage residual: {filename}")
    for side in range(2):
        for bucket in ("admitted", "excluded", "trivial"):
            summed = {kind: 0 for kind in KINDS}
            for substate in range(10):
                values = detail[str(substate)][str(side)][bucket]
                if set(values) != set(KINDS):
                    raise RuntimeError(
                        f"information trivial bucket residual: {filename}")
                for kind in KINDS:
                    summed[kind] += values[kind]
            if summed != totals[str(side)][bucket]:
                raise RuntimeError(
                    f"information trivial conservation residual: {filename} "
                    f"side {side} {bucket}")


def import_receipt(
    summary_path: Path,
    receipt_path: Path,
    sidecar_path: Path,
    receipt_s3_key: str,
    receipt_s3_version_id: str,
) -> None:
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    validate_receipt(receipt, sidecar_path)
    filename = str(receipt["filename"])
    record = summary.get("files", {}).get(filename)
    if (not isinstance(record, dict) or
            record.get("overlay_sha256") != receipt.get("overlay_sha256")):
        raise RuntimeError(f"radius summary overlay binding residual: {filename}")

    detail = receipt["substate_counts"]
    for radius, radius_row in record.get("radii", {}).items():
        substate = str(radius_row["power_substate"])
        for side, side_name in enumerate(("first_starts", "second_starts")):
            buckets = detail[substate][str(side)]
            admitted = radius_row[side_name]["admitted"]
            for kind, field in SUMMARY_FIELDS.items():
                if admitted[field] != buckets["admitted"][kind]:
                    raise RuntimeError(
                        f"radius admitted residual: {filename} radius {radius} "
                        f"side {side} {kind}")
                trivial = buckets["trivial"][kind]
                if trivial > admitted[field]:
                    raise RuntimeError(
                        f"radius trivial subset residual: {filename} radius "
                        f"{radius} side {side} {kind}")
                radius_row[side_name]["trivial"][field] = trivial
                radius_row[side_name]["display"][field] = admitted[field] - trivial

    record.update({
        "information_trivial_sha256": receipt["sidecar_sha256"],
        "information_trivial_s3_key": receipt["s3_key"],
        "information_trivial_s3_version_id": receipt["s3_version_id"],
        "information_trivial_binary_sha256": receipt["binary_sha256"],
        "information_trivial_source_bundle_sha256":
            receipt["source_bundle_sha256"],
        "information_trivial_receipt_sha256": sha256(receipt_path),
        "information_trivial_receipt_s3_key": receipt_s3_key,
        "information_trivial_receipt_s3_version_id": receipt_s3_version_id,
        "trivial_semantics": "authenticated-per-substate-v3",
    })
    summary_path.write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--sidecar", type=Path, required=True)
    parser.add_argument("--receipt-s3-key", required=True)
    parser.add_argument("--receipt-s3-version-id", required=True)
    args = parser.parse_args()
    import_receipt(
        args.summary, args.receipt, args.sidecar,
        args.receipt_s3_key, args.receipt_s3_version_id)


if __name__ == "__main__":
    main()
