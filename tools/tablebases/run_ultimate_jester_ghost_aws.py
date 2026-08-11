#!/usr/bin/env python3
"""Run an authenticated exact Jester+Ghost transition/solve bundle."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from typing import Sequence

SUFFIXES = (".header", ".meta", ".strata", ".index", ".blocks", ".verified")
RAW_GEOMETRIES = 38_450_880
RAW_PER_SHARD = 640_848
HALF_RAW = 320_424
BASE_SHARDS = 60
PARALLELISM = 29
ZERO_SHARDS = (
    2, 5, 8, 11, *range(14, 30), 32, 35, 38, 41, *range(44, 60),
)
HEAVY_SHARDS = tuple(index for index in range(BASE_SHARDS)
                     if index not in ZERO_SHARDS)


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


def option(command: Sequence[str], name: str) -> str:
    try: return command[command.index(name) + 1]
    except (ValueError, IndexError) as error:
        raise RuntimeError(f"transition command lacks {name}") from error


def validate_manifest(manifest: dict[str, object]) -> None:
    if (manifest.get("schema") != "ultimate-jester-ghost-aws-v2" or
            manifest.get("raw_geometries") != RAW_GEOMETRIES or
            manifest.get("base_shards") != BASE_SHARDS or
            manifest.get("raw_per_shard") != RAW_PER_SHARD or
            manifest.get("half_raw") != HALF_RAW or
            manifest.get("visibility_cycle") != 78 or
            manifest.get("bootstrap_inputs") != 40 or
            manifest.get("active_jobs") != 40 or
            manifest.get("merge_inputs") != 80 or
            manifest.get("parallelism") != PARALLELISM or
            manifest.get("zero_shards") != list(ZERO_SHARDS) or
            manifest.get("heavy_shards") != list(HEAVY_SHARDS)):
        raise RuntimeError("invalid Jester/Ghost V2 load-balanced manifest")
    expected_bootstrap = [
        [f"shard-{index:02d}", index * RAW_PER_SHARD, RAW_PER_SHARD]
        for index in ZERO_SHARDS]
    expected_active = [
        [f"shard-{index:02d}{suffix}",
         index * RAW_PER_SHARD + half * HALF_RAW, HALF_RAW]
        for index in HEAVY_SHARDS
        for half, suffix in enumerate(("a", "b"))]
    expected_merge = sorted(expected_bootstrap + expected_active,
                            key=lambda item: item[1])
    if (manifest.get("bootstrap_ranges") != expected_bootstrap or
            manifest.get("active_ranges") != expected_active or
            manifest.get("merge_ranges") != expected_merge):
        raise RuntimeError("Jester/Ghost V2 range inventory drift")
    commands = manifest.get("commands")
    if not isinstance(commands, dict):
        raise RuntimeError("Jester/Ghost V2 commands are absent")
    bootstrap = commands.get("bootstrap")
    active = commands.get("shards")
    merge = commands.get("merge")
    if (not isinstance(bootstrap, list) or len(bootstrap) != 40 or
            not isinstance(active, list) or len(active) != 40 or
            not isinstance(merge, list)):
        raise RuntimeError("Jester/Ghost V2 command count drift")
    for command, (name, begin, count) in zip(bootstrap, expected_bootstrap):
        if ("--verify-transitions" not in command or
                "--allow-partial-merge" not in command or
                prefix(command) != f"work/transitions/{name}" or
                int(option(command, "--raw-begin")) != begin or
                int(option(command, "--raw-count")) != count):
            raise RuntimeError("Jester/Ghost bootstrap authentication drift")
    for command, (name, begin, count) in zip(active, expected_active):
        if ("--compile-transitions" not in command or
                prefix(command) != f"work/transitions/{name}" or
                int(option(command, "--raw-begin")) != begin or
                int(option(command, "--raw-count")) != count):
            raise RuntimeError("Jester/Ghost active split command drift")
    shard_prefixes = [merge[index + 1] for index, value in enumerate(merge)
                      if value == "--shard" and index + 1 < len(merge)]
    if shard_prefixes != [f"work/transitions/{name}"
                          for name, _, _ in expected_merge]:
        raise RuntimeError("Jester/Ghost merge order/coverage drift")


def run(command: Sequence[str], root: Path, log: Path) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    active = log.with_name(f".{log.name}.active")
    if active.exists():
        archive_log(active, log, "interrupted")
    with active.open("xb") as output:
        result = subprocess.run(command, cwd=root, stdout=output,
                                stderr=subprocess.STDOUT, check=False)
    if result.returncode:
        failed = archive_log(active, log, "failed")
        raise RuntimeError(f"command failed; inspect {failed}")
    if log.exists():
        archive_log(log, log, "prior")
    active.replace(log)


def archive_log(source: Path, canonical: Path, label: str) -> Path:
    for attempt in range(1, 10_000):
        destination = canonical.with_name(
            f"{canonical.stem}.{label}-{attempt:04d}{canonical.suffix}")
        if not destination.exists():
            source.replace(destination)
            return destination
    raise RuntimeError(f"too many retained phase logs for {canonical}")


def merged_transition_is_complete(root: Path,
                                  command: Sequence[str]) -> bool:
    base = root / prefix(command)
    return (all(Path(f"{base}{suffix}").is_file()
                for suffix in SUFFIXES) and
            (root / "work/logs/merge.log").is_file())


def run_ranges(commands: list[list[str]], root: Path, workers: int) -> None:
    pending = [command for command in commands if not complete(root, command)]
    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = [executor.submit(run, command, root,
          root / "work/logs" / f"{Path(prefix(command)).name}.log")
                   for command in pending]
        for future in futures: future.result()


def authenticate_bootstrap(commands: list[list[str]], root: Path,
                           workers: int) -> None:
    missing = [prefix(command) for command in commands
               if not complete(root, command)]
    if missing:
        raise RuntimeError(
          "certified bootstrap transition ranges are missing: " +
          ", ".join(missing))
    for command in commands:
        header = (root / f"{prefix(command)}.header").read_bytes()
        if len(header) < 24:
            raise RuntimeError("certified bootstrap header is truncated")
        magic, version, _, begin, count = struct.unpack_from("<8sIIII", header)
        if (magic != b"UFJGT2\0\0" or version != 2 or
                begin != int(option(command, "--raw-begin")) or
                count != int(option(command, "--raw-count"))):
            raise RuntimeError(
              f"certified bootstrap range mismatch: {prefix(command)}")
    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = [executor.submit(run, command, root,
          root / "work/logs" /
          f"authenticate-{Path(prefix(command)).name}.log")
          for command in commands]
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
    validate_manifest(manifest)
    prepare(root); work = root / "work"
    run(manifest["commands"]["build"], root, work / "logs/build.log")
    authenticate_bootstrap(manifest["commands"]["bootstrap"], root,
                           PARALLELISM)
    run_ranges(manifest["commands"]["shards"], root, PARALLELISM)
    merge = manifest["commands"]["merge"]
    if not merged_transition_is_complete(root, merge):
        run(merge, root, work / "logs/merge.log")
    run(manifest["commands"]["measure"], root, work / "logs/measure.log")
    if args.full:
        run(manifest["commands"]["solve"], root, work / "logs/solve.log")
    artifact_manifest(root, manifest)


if __name__ == "__main__": main()
