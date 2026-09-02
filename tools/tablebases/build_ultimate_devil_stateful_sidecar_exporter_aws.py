#!/usr/bin/env python3
"""Build, version-stage, and restore-authenticate the Devil sidecar exporter."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import launch_ultimate_devil_spawned_solver_aws as base


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tools/tablebases/export_ultimate_devil_stateful_sidecar.cpp"
PREFIX = "sources/devil-stateful-sidecar-exporter-v4"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def aws_json(arguments: list[str]) -> dict[str, object]:
    value = json.loads(subprocess.check_output(arguments, text=True))
    if not isinstance(value, dict):
        raise RuntimeError("AWS response is not an object")
    return value


def build(instance: str, volume: str) -> dict[str, str]:
    if not volume.startswith("/mnt/ultimatefish"):
        raise ValueError("build volume must be an Ultimate Fish mount")
    source_sha = sha256(SOURCE)
    source_key = f"{PREFIX}/source/sha256/{source_sha}/{SOURCE.name}"
    upload = aws_json([
        "aws", "s3api", "put-object", "--region", base.REGION,
        "--bucket", base.BUCKET, "--key", source_key,
        "--body", str(SOURCE), "--metadata", f"sha256={source_sha}",
        "--output", "json",
    ])
    source_version = str(upload.get("VersionId", ""))
    if not source_version:
        raise RuntimeError("source upload lacks VersionId")
    stage = f"{volume.rstrip('/')}/devil-stateful-sidecar-exporter-v4-{source_sha[:8]}"
    remote_source = f"{stage}/{SOURCE.name}"
    binary = f"{stage}/ultimate_devil_stateful_sidecar_exporter"
    output = base.send(instance, [
        "set -euo pipefail",
        f"install -d -m 0755 {stage}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {source_key} --version-id {source_version} {remote_source} >/dev/null",
        f"test \"$(sha256sum {remote_source} | cut -d ' ' -f1)\" = {source_sha}",
        f"g++ -std=c++17 -O3 -DNDEBUG -pthread -Wall -Wextra -Wpedantic -Werror "
        f"{remote_source} -o {binary}.tmp",
        f"mv {binary}.tmp {binary}",
        f"binary_sha=$(sha256sum {binary} | cut -d ' ' -f1)",
        f"binary_key={PREFIX}/binary/sha256/$binary_sha/ultimate_devil_stateful_sidecar_exporter",
        f"binary_version=$(aws s3api put-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key \"$binary_key\" --body {binary} --metadata sha256=\"$binary_sha\",source-sha256={source_sha} "
        "--query VersionId --output text)",
        f"restore={stage}/restore.bin",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        "--key \"$binary_key\" --version-id \"$binary_version\" \"$restore\" >/dev/null",
        "test \"$(sha256sum \"$restore\" | cut -d ' ' -f1)\" = \"$binary_sha\"",
        "rm -f \"$restore\"",
        "printf '%s\\n' binary_sha256=\"$binary_sha\" binary_key=\"$binary_key\" "
        "binary_version=\"$binary_version\"",
    ], timeout=900)
    result = {"source_sha256": source_sha, "source_key": source_key,
              "source_version": source_version, "stage": stage,
              "remote_output": output.strip()}
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--volume", default="/mnt/ultimatefish")
    args = parser.parse_args()
    print(json.dumps(build(args.instance, args.volume), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
