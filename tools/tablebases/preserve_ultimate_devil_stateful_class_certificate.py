#!/usr/bin/env python3
"""Preserve and exact-version restore the complete stateful Devil certificate."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

import finalize_ultimate_devil_stateful_recovery as finalizer


REGION = "us-west-2"
BUCKET = "ultimatefish-info-20260808-a4e679c6-831688117652"


def aws(*arguments: str) -> str:
    return subprocess.check_output(
        ["aws", *arguments], text=True).strip()


def preserve(manifest_path: Path, output_path: Path) -> dict[str, object]:
    manifest = json.loads(manifest_path.read_text())
    payload = finalizer.canonical_bytes(finalizer.build_certificate(manifest))
    digest = hashlib.sha256(payload).hexdigest()
    output_path.write_bytes(payload)
    key = ("results/devil-stateful-v1/class-certificates/sha256/" + digest
           + "/king-devil-causal-minions-v1.json")
    version = aws(
        "s3api", "put-object", "--region", REGION, "--bucket", BUCKET,
        "--key", key, "--body", str(output_path),
        "--metadata", (
            "schema=ultimate-devil-stateful-class-certificate-v1,"
            f"sha256={digest},fixed-square-coverage-residual=0"),
        "--query", "VersionId", "--output", "text")
    if not version or version == "None":
        raise RuntimeError("certificate upload lacks VersionId")
    with tempfile.TemporaryDirectory(prefix="devil-stateful-restore-") as temp:
        restored = Path(temp) / output_path.name
        aws(
            "s3api", "get-object", "--region", REGION, "--bucket", BUCKET,
            "--key", key, "--version-id", version, str(restored))
        restored_payload = restored.read_bytes()
    if restored_payload != payload:
        raise RuntimeError("exact-version certificate restore mismatch")
    return {
        "schema": "ultimate-devil-stateful-class-preservation-v1",
        "sha256": digest,
        "bytes": len(payload),
        "s3_key": key,
        "version_id": version,
        "restore_residual": 0,
        "fixed_square_coverage_residual": 0,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path,
                        default=finalizer.DEFAULT_MANIFEST)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(preserve(args.manifest, args.output), sort_keys=True))


if __name__ == "__main__":
    main()
