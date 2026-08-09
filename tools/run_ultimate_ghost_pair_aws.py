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


TRANSITION_SUFFIXES = (
    ".header", ".meta", ".strata", ".actual", ".index", ".blocks",
    ".verified")


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


def transition_prefix(command: Sequence[str]) -> str:
    try:
        index = command.index("--transition-prefix")
        prefix = command[index + 1]
    except (ValueError, IndexError) as error:
        raise RuntimeError("transition command lacks a prefix") from error
    path = Path(prefix)
    if (path.is_absolute() or len(path.parts) != 3 or
            path.parts[:2] != ("work", "transitions") or
            path.parts[2] in ("", ".", "..")):
        raise RuntimeError("transition prefix escapes its authenticated root")
    return prefix


def transition_is_complete(root: Path, command: Sequence[str]) -> bool:
    prefix = root / transition_prefix(command)
    return all(Path(f"{prefix}{suffix}").is_file()
               for suffix in TRANSITION_SUFFIXES)


def run_ranges(commands: object, root: Path, parallelism: int) -> None:
    if not isinstance(commands, list):
        raise RuntimeError("transition command inventory must be a list")
    pending = []
    for command in commands:
        if (not isinstance(command, list) or
                not all(isinstance(argument, str) for argument in command)):
            raise RuntimeError("invalid transition command")
        if not transition_is_complete(root, command):
            pending.append(command)
    with ThreadPoolExecutor(max_workers=parallelism) as executor:
        futures = []
        for command in pending:
            name = Path(transition_prefix(command)).name
            futures.append(executor.submit(
                run, command, root, root / "work" / "logs" / f"{name}.log"))
        for future in futures:
            future.result()


def prepare_workdirs(root: Path) -> None:
    for name in ("transitions", "solve", "results", "logs"):
        (root / "work" / name).mkdir(parents=True, exist_ok=True)


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
    if (manifest.get("schema") != "ultimate-ghost-pair-aws-v3" or
            manifest.get("active_jobs") != 60 or
            manifest.get("long_jobs") != 30 or
            manifest.get("tail_jobs") != 30 or
            manifest.get("zero_bootstrap_jobs") != 24 or
            manifest.get("merge_inputs") != 84 or
            manifest.get("parallelism") != 30):
        raise RuntimeError("invalid load-balanced Ghost-pair manifest")
    work = root / "work"
    prepare_workdirs(root)
    run(manifest["commands"]["build"], root, work / "logs" / "build.log")
    parallelism = int(manifest["parallelism"])
    zero_shards = manifest["commands"].get("zero_shards")
    shards = manifest["commands"].get("shards")
    if (not isinstance(zero_shards, list) or len(zero_shards) != 24 or
            not isinstance(shards, list) or len(shards) != 60):
        raise RuntimeError("invalid load-balanced transition inventory")
    # Existing complete ranges are preserved.  The native merge later
    # authenticates every extent, payload, binding, and regeneration marker;
    # a stale or corrupt range can therefore never enter the merged proof.
    run_ranges(zero_shards, root, parallelism)
    run_ranges(shards, root, parallelism)
    run(manifest["commands"]["merge"], root, work / "logs" / "merge.log")
    run(manifest["commands"]["measure"], root,
        work / "logs" / "measure.log")
    if args.full:
        run(manifest["commands"]["solve"], root,
            work / "logs" / "solve.log")
    artifact_manifest(root, manifest)


if __name__ == "__main__":
    main()
