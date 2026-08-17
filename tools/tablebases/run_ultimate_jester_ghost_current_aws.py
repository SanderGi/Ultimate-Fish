#!/usr/bin/env python3
"""Run fresh current-source K+Jester+Ghost versus K information."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys


RAW_PER_SHARD = 640_848
HALF_RAW = 320_424
ZERO_SHARDS = (2, 5, 8, 11, *range(14, 30), 32, 35, 38, 41,
               *range(44, 60))
HEAVY_SHARDS = tuple(index for index in range(60)
                     if index not in ZERO_SHARDS)
SUFFIXES = (".header", ".meta", ".strata", ".index", ".blocks",
            ".verified")
LOWER_JESTER_STATES = 985_920


def ranges() -> tuple[list[tuple[str, int, int]],
                      list[tuple[str, int, int]],
                      list[tuple[str, int, int]]]:
    zero = [(f"shard-{index:02d}", index * RAW_PER_SHARD, RAW_PER_SHARD)
            for index in ZERO_SHARDS]
    active = [(f"shard-{index:02d}{suffix}",
               index * RAW_PER_SHARD + half * HALF_RAW, HALF_RAW)
              for index in HEAVY_SHARDS
              for half, suffix in enumerate(("a", "b"))]
    merged = sorted([*zero, *active], key=lambda item: item[1])
    cursor = 0
    for _, begin, count in merged:
        if begin != cursor:
            raise RuntimeError("Jester/Ghost range coverage residual")
        cursor += count
    if cursor != 38_450_880 or len(zero) != 40 or len(active) != 40:
        raise RuntimeError("Jester/Ghost range inventory residual")
    return zero, active, merged


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def require_sha(path: Path, expected: str, label: str) -> None:
    if not path.is_file() or sha256_path(path) != expected:
        raise RuntimeError(f"{label} SHA-256 mismatch: {path}")


def require_lower_jester_binding(path: Path, table_sha256: str,
                                 model_sha256: str) -> None:
    with path.open("rb") as stream:
        header = stream.read(160)
    words = struct.unpack_from("<6I", header, 8) if len(header) == 160 else ()
    if (path.stat().st_size != 160 + LOWER_JESTER_STATES or
            header[:8] != b"UFIW2\0\0\0" or
            words != (2, 1, 30, 0, LOWER_JESTER_STATES, 1) or
            header[32:96].decode("ascii", errors="replace") != table_sha256 or
            header[96:160].decode("ascii", errors="replace") != model_sha256):
        raise RuntimeError("lower Jester UFIW2 binding mismatch")


def run(command: list[str], root: Path, log: Path) -> None:
    with log.open("xb") as output:
        completed = subprocess.run(command, cwd=root, stdout=output,
                                   stderr=subprocess.STDOUT, check=False)
    if completed.returncode:
        raise RuntimeError(f"command failed ({completed.returncode}): {log}")


def run_ranges(commands: list[tuple[str, list[str]]], work: Path,
               workers: int) -> None:
    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = [executor.submit(
            run, command, work, work / f"work/logs/{name}.log")
                   for name, command in commands]
        for future in futures:
            future.result()


def write_manifest(work: Path, args: argparse.Namespace) -> None:
    results = sorted((work / "work/results").glob("*"))
    logs = sorted((work / "work/logs").glob("*.log"))
    if ({path.suffix for path in results} != {".ufiw", ".ufjg"} or
            not any(path.name == "solve.log" for path in logs)):
        raise RuntimeError("Jester/Ghost result/proof inventory incomplete")
    files = [*results, *logs]
    (work / "work/artifact-manifest.json").write_text(json.dumps({
        "schema": "ultimate-jester-ghost-current-artifacts-v1",
        "filename": "kjesterghostk.uftb",
        "source_sha256": args.source_sha256,
        "model_sha256": args.model_sha256,
        "observation_sha256": args.observation_sha256,
        "lower_jester_overlay_sha256": args.lower_jester_overlay_sha256,
        "lower_ghost_sha256": args.lower_ghost_sha256,
        "files": {str(path.relative_to(work)): {
            "bytes": path.stat().st_size, "sha256": sha256_path(path)}
            for path in files},
    }, indent=2, sort_keys=True) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--source-table", type=Path, required=True)
    parser.add_argument("--source-sha256", required=True)
    parser.add_argument("--lower-jester-table", type=Path, required=True)
    parser.add_argument("--lower-jester-sha256", required=True)
    parser.add_argument("--lower-jester-overlay", type=Path, required=True)
    parser.add_argument("--lower-jester-overlay-sha256", required=True)
    parser.add_argument("--lower-jester-model-sha256", required=True)
    parser.add_argument("--lower-ghost-sidecar", type=Path, required=True)
    parser.add_argument("--lower-ghost-sha256", required=True)
    parser.add_argument("--model-sha256", required=True)
    parser.add_argument("--observation-sha256", required=True)
    parser.add_argument("--parallelism", type=int, default=29)
    parser.add_argument("--max-disk-bytes", type=int, default=1_700 << 30)
    parser.add_argument("--max-resident-bytes", type=int, default=170 << 30)
    parser.add_argument("--min-free-disk-bytes", type=int, default=50 << 30)
    args = parser.parse_args()
    if not 1 <= args.parallelism <= 30:
        raise RuntimeError("Jester/Ghost parallelism residual")
    root = args.source_root.resolve()
    sys.path.insert(0, str(root / "tools/tablebases"))
    import ultimate_information_tablebases as information  # pylint: disable=import-outside-toplevel
    if information.solver_model_fingerprint(
            "kjesterghostk.uftb", root=root) != args.model_sha256:
        raise RuntimeError("Jester/Ghost solver model mismatch")
    for path, digest, label in (
      (args.source_table, args.source_sha256, "source table"),
      (args.lower_jester_table, args.lower_jester_sha256, "lower Jester"),
      (args.lower_jester_overlay, args.lower_jester_overlay_sha256,
       "lower Jester overlay"),
      (args.lower_ghost_sidecar, args.lower_ghost_sha256, "lower Ghost")):
        require_sha(path, digest, label)
    require_lower_jester_binding(
        args.lower_jester_overlay, args.lower_jester_sha256,
        args.lower_jester_model_sha256)
    if args.work.exists():
        raise RuntimeError("Jester/Ghost work directory already exists")
    work = args.work.resolve()
    for directory in ("tablebases", "work/logs", "work/transitions",
                      "work/results", "work/solve", "work/self-test"):
        (work / directory).mkdir(parents=True, exist_ok=True)
    for source, name in (
      (args.source_table, "kjesterghostk.uftb"),
      (args.lower_jester_table, "kjesterk.uftb"),
      (args.lower_jester_overlay, "kjesterk.ufiw"),
      (args.lower_ghost_sidecar, "kghostk.ufgm")):
        shutil.copyfile(source, work / "tablebases" / name)

    executable = work / "ultimate_jester_ghost_information_tablebase"
    sources = [
      "src/ultimate/tablebases/jester_ghost_information_tablebase.cpp",
      "src/ultimate/tablebases/jester_ghost_information_solver.cpp",
      "src/ultimate/tablebases/jester_ghost_information_model.cpp",
      "src/ultimate/tablebases/external_robdd.cpp",
      "src/ultimate/tablebases/ghost_information_probe.cpp",
      "src/ultimate/tablebases/information.cpp", "src/ultimate/position.cpp",
      "src/ultimate/nnue.cpp"]
    run(["clang++", "-std=c++17", "-O3", "-DNDEBUG", "-Wall",
         "-Wextra", "-Wpedantic", "-Werror", "-Isrc/ultimate",
         "-Isrc/ultimate/tablebases", *sources, "-o", str(executable)],
        root, work / "work/logs/build.log")
    run([str(executable), "--self-test", "--scratch",
         "work/self-test/kjesterghostk"], work,
        work / "work/logs/self-test.log")
    binding = ["--source-sha256", args.source_sha256,
               "--model-sha256", args.model_sha256,
               "--observation-sha256", args.observation_sha256]
    zero, active, merged_ranges = ranges()
    def compile_command(spec: tuple[str, int, int]) -> tuple[str, list[str]]:
        name, begin, count = spec
        return name, [str(executable), "--compile-transitions",
          "--transition-prefix", f"work/transitions/{name}",
          "--raw-begin", str(begin), "--raw-count", str(count), *binding]
    run_ranges([compile_command(spec) for spec in zero], work,
               args.parallelism)
    run_ranges([compile_command(spec) for spec in active], work,
               args.parallelism)
    merged = "work/transitions/kjesterghostk"
    merge = [str(executable), "--merge-transitions",
             "--transition-prefix", merged]
    for name, _, _ in merged_ranges:
        merge += ["--shard", f"work/transitions/{name}"]
    merge += binding
    run(merge, work, work / "work/logs/merge.log")
    common = ["--transition-prefix", merged,
      "--input", "tablebases/kjesterghostk.uftb",
      "--lower-jester-table", "tablebases/kjesterk.uftb",
      "--lower-jester-overlay", "tablebases/kjesterk.ufiw",
      "--lower-jester-model-sha256", args.lower_jester_model_sha256,
      "--lower-jester-overlay-sha256", args.lower_jester_overlay_sha256,
      "--lower-ghost-sidecar", "tablebases/kghostk.ufgm",
      "--lower-ghost-sidecar-sha256", args.lower_ghost_sha256,
      "--scratch", "work/solve/kjesterghostk",
      "--output", "work/results/kjesterghostk.ufiw",
      "--output-arbitrary", "work/results/kjesterghostk.ufjg", *binding,
      "--max-disk-bytes", str(args.max_disk_bytes),
      "--max-resident-bytes", str(args.max_resident_bytes),
      "--min-free-disk-bytes", str(args.min_free_disk_bytes),
      "--compact-every", "1"]
    run([str(executable), "--solve", *common], work,
        work / "work/logs/solve.log")
    write_manifest(work, args)


if __name__ == "__main__":
    main()
