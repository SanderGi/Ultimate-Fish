#!/usr/bin/env python3
"""Execute an authenticated KGhostGhost transition/solve bundle on AWS."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess
from typing import Sequence


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1 << 20):
            digest.update(chunk)
    return digest.hexdigest()


def verify_inputs(root: Path, manifest: dict[str, object]) -> None:
    files = manifest["files"]
    if not isinstance(files, list):
        raise RuntimeError("bundle manifest files must be a list")
    for record in files:
        if not isinstance(record, dict):
            raise RuntimeError("bundle manifest file record must be an object")
        path = root / str(record["path"])
        if (not path.is_file() or path.stat().st_size != int(record["bytes"]) or
                sha256(path) != str(record["sha256"])):
            raise RuntimeError(f"bundle input authentication failed: {path}")


def run(command: Sequence[str], root: Path, log: Path) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("wb") as output:
        completed = subprocess.run(command, cwd=root, stdout=output,
                                   stderr=subprocess.STDOUT, check=False)
    if completed.returncode:
        raise RuntimeError(
            f"command failed ({completed.returncode}); inspect {log}")


def artifact_manifest(root: Path, manifest: dict[str, object]) -> None:
    expected = manifest["artifacts"]
    if not isinstance(expected, list):
        raise RuntimeError("bundle artifact inventory must be a list")
    records = []
    for relative in expected:
        path = root / str(relative)
        if path.exists():
            records.append({
                "path": str(relative),
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
            })
    destination = root / "work" / "artifact-manifest.json"
    destination.write_text(json.dumps({
        "source_sha256": manifest["source_sha256"],
        "model_sha256": manifest["model_sha256"],
        "observation_sha256": manifest["observation_sha256"],
        "lower_sidecar_sha256": manifest["lower_sidecar_sha256"],
        "artifacts": records,
    }, indent=2, sort_keys=True) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path,
                        default=Path("bundle-manifest.json"))
    parser.add_argument("--full", action="store_true",
                        help="continue from measurement to the full solve")
    args = parser.parse_args()
    root = args.manifest.resolve().parent
    manifest = json.loads(args.manifest.read_text())
    verify_inputs(root, manifest)
    work = root / "work"
    (work / "transitions").mkdir(parents=True, exist_ok=True)
    (work / "results").mkdir(parents=True, exist_ok=True)
    run(manifest["commands"]["build"], root, work / "logs" / "build.log")
    shards = manifest["commands"]["shards"]
    if not isinstance(shards, list) or len(shards) != 32:
        raise RuntimeError("bundle must contain exactly 32 transition shards")
    with ThreadPoolExecutor(max_workers=32) as executor:
        futures = [executor.submit(run, command, root,
                                   work / "logs" / f"shard-{index:02d}.log")
                   for index, command in enumerate(shards)]
        for future in futures:
            future.result()
    run(manifest["commands"]["merge"], root, work / "logs" / "merge.log")
    run(manifest["commands"]["measure"], root,
        work / "logs" / "measure.log")
    if args.full:
        run(manifest["commands"]["solve"], root,
            work / "logs" / "solve.log")
    artifact_manifest(root, manifest)


if __name__ == "__main__":
    main()
