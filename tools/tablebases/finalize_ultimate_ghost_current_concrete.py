#!/usr/bin/env python3
"""Verify, preserve, and clean one fresh current-model Ghost concrete build."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def command(*args: str, capture: bool = False) -> str:
    result = subprocess.run(args, check=True, text=True,
                            capture_output=capture)
    return result.stdout if capture else ""


def put(path: Path, bucket: str, key: str,
        metadata: dict[str, str]) -> dict[str, object]:
    meta = ",".join(f"{key}={value}" for key, value in metadata.items())
    command("aws", "s3api", "put-object", "--region", "us-west-2",
            "--bucket", bucket, "--key", key, "--body", str(path),
            "--metadata", meta)
    head = json.loads(command(
        "aws", "s3api", "head-object", "--region", "us-west-2",
        "--bucket", bucket, "--key", key, capture=True))
    observed = {str(key).lower(): str(value)
                for key, value in head.get("Metadata", {}).items()}
    if (head.get("ContentLength") != path.stat().st_size or
            not head.get("VersionId") or
            any(observed.get(key) != value for key, value in metadata.items())):
        raise RuntimeError(f"S3 verification failed: {key}")
    return head


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--unit", required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--primary", required=True)
    parser.add_argument("--secondary", required=True)
    parser.add_argument("--opposing", action="store_true")
    parser.add_argument("--source-sha256", required=True)
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--prefix", required=True)
    args = parser.parse_args()

    while subprocess.run(
            ["systemctl", "is-active", "--quiet", args.unit],
            check=False).returncode == 0:
        time.sleep(10)
    result = command("systemctl", "show", args.unit, "-p", "Result",
                     "--value", capture=True).strip()
    if result != "success":
        raise RuntimeError(f"solver unit did not succeed: {args.unit}: {result}")

    table = args.root / "outputs" / args.name / f"{args.name}.uftb"
    scratch = args.root / "scratch" / args.name
    output = table.parent
    if not table.is_file():
        raise RuntimeError(f"solver output is missing: {table}")
    reachability = output / f"{args.name}.reachability-v2.txt"
    audit = [str(args.binary), "--piece", args.primary]
    if args.secondary:
        audit += ["--piece2", args.secondary]
    if args.opposing:
        audit.append("--opposing")
    audit += ["--checkpoint-every", "0", "--audit-reachability", str(table)]
    with reachability.open("xb") as stream:
        subprocess.run(audit, cwd=args.root, stdout=stream,
                       stderr=subprocess.STDOUT, check=True)

    table_sha = sha256(table)
    reach_sha = sha256(reachability)
    common = {"source-sha256": args.source_sha256}
    table_key = f"{args.prefix.rstrip('/')}/concrete/{table.name}"
    reach_key = (f"{args.prefix.rstrip('/')}/reachability/"
                 f"{reachability.name}")
    table_head = put(table, args.bucket, table_key,
                     {**common, "sha256": table_sha})
    reach_head = put(reachability, args.bucket, reach_key, {
        **common, "sha256": reach_sha,
        "predicate": "native-full-causal-reachability",
    })
    manifest = output / "artifact-manifest.json"
    manifest.write_text(json.dumps({
        "schema": "ultimate-ghost-current-concrete-v1",
        "name": args.name,
        "unit": args.unit,
        "source_sha256": args.source_sha256,
        "table": {"bytes": table.stat().st_size, "sha256": table_sha,
                  "key": table_key,
                  "version_id": table_head["VersionId"]},
        "reachability": {
            "bytes": reachability.stat().st_size, "sha256": reach_sha,
            "key": reach_key, "version_id": reach_head["VersionId"],
        },
    }, indent=2, sort_keys=True) + "\n")
    manifest_sha = sha256(manifest)
    manifest_key = (f"{args.prefix.rstrip('/')}/manifests/"
                    f"{args.name}.artifact-manifest.json")
    manifest_head = put(manifest, args.bucket, manifest_key,
                        {**common, "sha256": manifest_sha})
    document = json.loads(manifest.read_text())
    if scratch.exists():
        shutil.rmtree(scratch)
    print(json.dumps({
        **document,
        "manifest_receipt": {"key": manifest_key, "sha256": manifest_sha,
                             "version_id": manifest_head["VersionId"]},
    }, sort_keys=True), flush=True)


if __name__ == "__main__":
    main()
