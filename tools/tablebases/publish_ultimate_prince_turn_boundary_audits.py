#!/usr/bin/env python3
"""Authenticate and publish locally generated Prince boundary audit receipts."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys
import tarfile
import tempfile
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import finalize_ultimate_aws_concrete_class as finalizer  # noqa: E402
import package_ultimate_prince_turn_boundary_auditor as package  # noqa: E402
import update_ultimate_tablebase_ledger as ledger  # noqa: E402


REGION = "us-west-2"
BUCKET = "ultimatefish-info-20260808-a4e679c6-831688117652"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def aws_json(arguments: list[str]) -> dict[str, Any]:
    value = json.loads(subprocess.check_output(
        ["aws", *arguments, "--output", "json"], text=True))
    if not isinstance(value, dict):
        raise RuntimeError("AWS command did not return a JSON object")
    return value


def publish(path: Path, key: str, metadata: dict[str, str],
            temporary: Path) -> tuple[str, str]:
    digest = sha256(path)
    response = aws_json([
        "s3api", "put-object", "--region", REGION, "--bucket", BUCKET,
        "--key", key, "--body", str(path), "--metadata",
        ",".join(f"{name}={value}" for name, value in metadata.items()),
    ])
    version = str(response.get("VersionId", ""))
    if not version:
        raise RuntimeError(f"S3 upload lacks VersionId: {key}")
    restored = temporary / (path.name + ".verify")
    aws_json([
        "s3api", "get-object", "--region", REGION, "--bucket", BUCKET,
        "--key", key, "--version-id", version, str(restored),
    ])
    if sha256(restored) != digest:
        raise RuntimeError(f"S3 restore SHA-256 residual: {key}")
    restored.unlink()
    return digest, version


def source_manifest(bundle: Path) -> dict[str, Any]:
    with tarfile.open(bundle, "r") as archive:
        member = archive.extractfile("bundle-manifest.json")
        if member is None:
            raise RuntimeError("source bundle lacks manifest")
        manifest = json.loads(member.read())
    if (manifest.get("schema") != package.SCHEMA or
            manifest.get("semantics") != package.SEMANTICS or
            len(manifest.get("files", [])) != 16):
        raise RuntimeError("source bundle manifest residual")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--audits", type=Path, required=True)
    parser.add_argument("--tablebases", type=Path, required=True)
    parser.add_argument("--source-bundle", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--receipt-json", type=Path, required=True)
    parser.add_argument("--only", action="append", default=[],
                        help="publish only this Prince filename (repeatable)")
    args = parser.parse_args()

    manifest = source_manifest(args.source_bundle)
    model_sha = str(manifest["generator_model_sha256"])
    inventory_sha = str(manifest["inventory_sha256"])
    bundle_sha = sha256(args.source_bundle)
    binary_sha = sha256(args.binary)
    accepted = {
        (
            "552b16528f17dc2c6bc4b816a5cb8c8eb13a88799830db71db1323a982570a6e",
            "ba8d13582359fcb16c0312e54fb0f23073cd910f0dbf173329c5cbb583a5e143",
            "4347b9593dba9b806a4cc0bbad7ab4aaf4118897ae59f40a017fd431e43016fc",
            "7e6513eb68b4e6a1fef30f55d50972a8271d7156c542ca5af427267ee5a2d1f9",
        ),
        (
            "68c329e8bd3d248468c176b6e2038c3e0285116250fa1d88459fe1069c9866c4",
            "ba8d13582359fcb16c0312e54fb0f23073cd910f0dbf173329c5cbb583a5e143",
            "da64848ffda3fda2256dcf4b9765b1437c979cf69e297083bd89684cf5164366",
            "56b2ce467815b3a3726f1b068fad91d627012e71fdc24adef3e6bf4ce4c723ff",
        ),
    }
    if (model_sha, inventory_sha, bundle_sha, binary_sha) not in accepted:
        raise RuntimeError("Prince boundary auditor provenance residual")

    readme = (ROOT / "tablebases/README.md").read_text()
    rows = [row for row in ledger.entries(readme)
            if row.status == "certified" and row.result_kind == "concrete" and
            "prince" in row.filename]
    if len(rows) != 41:
        raise RuntimeError(f"expected 41 certified concrete Prince rows, got {len(rows)}")
    if args.only:
        requested = set(args.only)
        rows = [row for row in rows if row.filename in requested]
        if {row.filename for row in rows} != requested:
            raise RuntimeError("--only includes a non-certified Prince filename")

    platform_key = f"{sys.platform}-{platform.machine().lower()}"
    with tempfile.TemporaryDirectory(prefix="ultimate-prince-publish-") as raw:
        temporary = Path(raw)
        source_key = ("sources/bundles/prince-turn-boundary-v1/sha256/"
                      f"{bundle_sha}/ultimatefish-prince-turn-boundary-v1-source.tar")
        publish(args.source_bundle, source_key, {
            "sha256": bundle_sha, "model-sha256": model_sha,
            "inventory-sha256": inventory_sha,
        }, temporary)
        binary_key = (f"sources/auditors/prince-turn-boundary-v1/{platform_key}/"
                      f"sha256/{binary_sha}/ultimate_tablebase")
        publish(args.binary, binary_key, {
            "sha256": binary_sha, "model-sha256": model_sha,
            "source-bundle-sha256": bundle_sha,
        }, temporary)

        receipts = []
        for row in rows:
            tablebase = args.tablebases / row.filename
            table_sha = sha256(tablebase)
            audit = args.audits / f"{Path(row.filename).stem}.txt"
            native = audit.read_text()
            if native.count("reachability_scope turn_boundary\n") != 1:
                raise RuntimeError(f"{row.filename}: boundary scope residual")
            binding = (
                f"reachability_binding filename {row.filename} "
                f"output_sha256 {table_sha}\n"
                f"trivial_audit_binding source_overlay_sha256 {bundle_sha} "
                f"model_sha256 {model_sha} inventory_sha256 {inventory_sha} "
                f"binary_sha256 {binary_sha}\n")
            sidecar = temporary / f"{Path(row.filename).stem}.txt"
            sidecar.write_text(binding + native)
            totals, _, _ = finalizer.reporting_counts(row.filename,
                                                       sidecar.read_text())
            record = finalizer.encoded_record_for(row.filename)
            prince_factor = 2 ** sum(
                name == "prince" for name in
                (str(record["primary"]),
                 str(record.get("secondary") or "")))
            if any(sum(side) != int(record["states"]) // (2 * prince_factor)
                   for side in totals):
                raise RuntimeError(f"{row.filename}: boundary conservation residual")
            side_sha = sha256(sidecar)
            stem = Path(row.filename).stem
            key = (f"results/trivial-reachability-v3/{row.filename}/sha256/"
                   f"{side_sha}/{stem}.reachability-trivial-v3.txt")
            _, version = publish(sidecar, key, {
                "sha256": side_sha, "output-sha256": table_sha,
                "model-sha256": model_sha,
            }, temporary)
            receipts.append({
                "filename": row.filename, "output_sha256": table_sha,
                "sidecar_sha256": side_sha,
                "sidecar_version_id": version, "sidecar_key": key,
            })
    args.receipt_json.write_text(json.dumps(receipts, indent=2) + "\n")
    print(json.dumps(receipts, indent=2))


if __name__ == "__main__":
    main()
