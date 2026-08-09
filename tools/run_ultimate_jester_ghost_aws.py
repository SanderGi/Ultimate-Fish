#!/usr/bin/env python3
"""Run an authenticated exact Jester+Ghost transition/solve bundle."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess
from typing import Sequence

SUFFIXES = (".header", ".meta", ".strata", ".index", ".blocks", ".verified")


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1 << 20): digest.update(chunk)
    return digest.hexdigest()


def verify_inputs(root: Path, manifest: dict[str, object]) -> None:
    for record in manifest.get("files", []):
        path = root / str(record["path"])
        if (not path.is_file() or path.stat().st_size != int(record["bytes"]) or
                sha(path) != str(record["sha256"])):
            raise RuntimeError(f"bundle input authentication failed: {path}")


def prefix(command: Sequence[str]) -> str:
    try: value = command[command.index("--transition-prefix") + 1]
    except (ValueError, IndexError) as error:
        raise RuntimeError("transition command lacks prefix") from error
    path = Path(value)
    if path.is_absolute() or path.parts[:2] != ("work", "transitions") or len(path.parts) != 3:
        raise RuntimeError("transition prefix escapes bundle")
    return value


def complete(root: Path, command: Sequence[str]) -> bool:
    base = root / prefix(command)
    return all(Path(f"{base}{suffix}").is_file() for suffix in SUFFIXES)


def run(command: Sequence[str], root: Path, log: Path) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("wb") as output:
        result = subprocess.run(command, cwd=root, stdout=output,
                                stderr=subprocess.STDOUT, check=False)
    if result.returncode: raise RuntimeError(f"command failed; inspect {log}")


def run_ranges(commands: list[list[str]], root: Path, workers: int) -> None:
    pending = [command for command in commands if not complete(root, command)]
    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = [executor.submit(run, command, root,
          root / "work/logs" / f"{Path(prefix(command)).name}.log")
                   for command in pending]
        for future in futures: future.result()


def prepare(root: Path) -> None:
    for directory in ("transitions", "solve", "results", "logs"):
        (root / "work" / directory).mkdir(parents=True, exist_ok=True)


def artifact_manifest(root: Path, manifest: dict[str, object]) -> None:
    records = []
    for relative in manifest["artifacts"]:
        path = root / relative
        if path.exists():
            records.append({"path": relative, "bytes": path.stat().st_size,
                            "sha256": sha(path)})
    (root / "work/artifact-manifest.json").write_text(json.dumps({
        "source_sha256": manifest["source_sha256"],
        "model_sha256": manifest["model_sha256"],
        "observation_sha256": manifest["observation_sha256"],
        "lower_jester_overlay_sha256": manifest["lower_jester_overlay_sha256"],
        "lower_ghost_sidecar_sha256": manifest["lower_ghost_sidecar_sha256"],
        "artifacts": records}, indent=2, sort_keys=True) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=Path("bundle-manifest.json"))
    parser.add_argument("--full", action="store_true")
    args = parser.parse_args(); root = args.manifest.resolve().parent
    manifest = json.loads(args.manifest.read_text()); verify_inputs(root, manifest)
    if (manifest.get("schema") != "ultimate-jester-ghost-aws-v1" or
            manifest.get("active_jobs") != 60 or
            manifest.get("parallelism") != 30 or
            manifest.get("raw_per_shard") != 640_848 or
            manifest.get("visibility_cycle") != 78):
        raise RuntimeError("invalid Jester/Ghost load-balanced manifest")
    prepare(root); work = root / "work"
    run(manifest["commands"]["build"], root, work / "logs/build.log")
    run_ranges(manifest["commands"]["shards"], root, 30)
    run(manifest["commands"]["merge"], root, work / "logs/merge.log")
    run(manifest["commands"]["measure"], root, work / "logs/measure.log")
    if args.full:
        run(manifest["commands"]["solve"], root, work / "logs/solve.log")
    artifact_manifest(root, manifest)


if __name__ == "__main__": main()
