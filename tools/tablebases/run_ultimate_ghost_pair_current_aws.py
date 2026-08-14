#!/usr/bin/env python3
"""Run a fresh current-source K+Ghost+Ghost versus K information table."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys


TRANSITION_SUFFIXES = (
    ".header", ".meta", ".strata", ".actual", ".index", ".blocks",
    ".verified")


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def require_sha(path: Path, expected: str, label: str) -> None:
    if not path.is_file() or sha256_path(path) != expected:
        raise RuntimeError(f"{label} SHA-256 mismatch: {path}")


def run(command: list[str], root: Path, log: Path) -> None:
    with log.open("xb") as output:
        completed = subprocess.run(command, cwd=root, stdout=output,
                                   stderr=subprocess.STDOUT, check=False)
    if completed.returncode:
        raise RuntimeError(f"command failed ({completed.returncode}): {log}")


def write_manifest(work: Path, args: argparse.Namespace) -> None:
    results = sorted((work / "work/results").glob("*"))
    logs = sorted((work / "work/logs").glob("*.log"))
    if ({path.suffix for path in results} != {".ufiw", ".ufgg"} or
            not any(path.name == "solve.log" for path in logs)):
        raise RuntimeError("Ghost-pair result/proof inventory incomplete")
    files = [*results, *logs]
    manifest = {
        "schema": "ultimate-ghost-pair-current-artifacts-v1",
        "filename": "kghostghostk.uftb",
        "source_sha256": args.source_sha256,
        "model_sha256": args.model_sha256,
        "observation_sha256": args.observation_sha256,
        "lower_ghost_sha256": args.lower_ghost_sha256,
        "files": {str(path.relative_to(work)): {
            "bytes": path.stat().st_size, "sha256": sha256_path(path)}
            for path in files},
    }
    (work / "work/artifact-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--source-table", type=Path, required=True)
    parser.add_argument("--source-sha256", required=True)
    parser.add_argument("--lower-ghost-sidecar", type=Path, required=True)
    parser.add_argument("--lower-ghost-sha256", required=True)
    parser.add_argument("--model-sha256", required=True)
    parser.add_argument("--observation-sha256", required=True)
    parser.add_argument("--lower-ghost-source-sha256", required=True)
    parser.add_argument("--lower-ghost-model-sha256", required=True)
    parser.add_argument("--lower-ghost-observation-sha256", required=True)
    parser.add_argument("--parallelism", type=int, default=30)
    parser.add_argument("--max-disk-bytes", type=int, default=1_700 << 30)
    parser.add_argument("--max-resident-bytes", type=int, default=170 << 30)
    parser.add_argument("--min-free-disk-bytes", type=int, default=50 << 30)
    args = parser.parse_args()
    if not 1 <= args.parallelism <= 30:
        raise RuntimeError("Ghost-pair parallelism residual")

    root = args.source_root.resolve()
    sys.path.insert(0, str(root / "tools/tablebases"))
    import package_ultimate_ghost_pair_aws as schedule  # pylint: disable=import-outside-toplevel
    import run_ultimate_ghost_pair_aws as shared  # pylint: disable=import-outside-toplevel
    import ultimate_information_tablebases as information  # pylint: disable=import-outside-toplevel
    if information.solver_model_fingerprint(
            "kghostghostk.uftb", root=root) != args.model_sha256:
        raise RuntimeError("Ghost-pair solver model mismatch")
    require_sha(args.source_table, args.source_sha256, "source table")
    require_sha(args.lower_ghost_sidecar, args.lower_ghost_sha256,
                "lower Ghost sidecar")
    if args.work.exists():
        raise RuntimeError("Ghost-pair work directory already exists")
    work = args.work.resolve()
    for directory in ("tablebases", "work/logs", "work/transitions",
                      "work/results", "work/solve", "work/self-test"):
        (work / directory).mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.source_table,
                    work / "tablebases/kghostghostk.uftb")
    shutil.copyfile(args.lower_ghost_sidecar,
                    work / "tablebases/kghostk.ufgm")
    require_sha(work / "tablebases/kghostghostk.uftb",
                args.source_sha256, "copied source")
    require_sha(work / "tablebases/kghostk.ufgm",
                args.lower_ghost_sha256, "copied lower Ghost")

    executable = work / "ultimate_ghost_pair_information_tablebase"
    sources = [
        "src/ultimate/tablebases/ghost_pair_information_tablebase.cpp",
        "src/ultimate/tablebases/ghost_pair_information_solver.cpp",
        "src/ultimate/tablebases/ghost_pair_information_model.cpp",
        "src/ultimate/tablebases/ghost_information_probe.cpp",
        "src/ultimate/tablebases/information.cpp", "src/ultimate/position.cpp",
        "src/ultimate/nnue.cpp",
    ]
    run(["clang++", "-std=c++17", "-O3", "-DNDEBUG", "-Wall",
         "-Wextra", "-Wpedantic", "-Werror", "-Isrc/ultimate",
         "-Isrc/ultimate/tablebases", *sources, "-o", str(executable)],
        root, work / "work/logs/build.log")
    run([str(executable), "--self-test", "--scratch",
         "work/self-test/kghostghostk"], work,
        work / "work/logs/self-test.log")
    binding = ["--source-sha256", args.source_sha256,
               "--model-sha256", args.model_sha256,
               "--observation-sha256", args.observation_sha256]
    zero, active, merge_ranges = schedule.balanced_ranges()
    def command(spec: tuple[str, int, int]) -> list[str]:
        name, begin, count = spec
        return [str(executable), "--compile-transitions",
                "--transition-prefix", f"work/transitions/{name}",
                "--raw-begin", str(begin), "--raw-count", str(count),
                *binding]
    shared.run_ranges([command(spec) for spec in zero], work,
                      args.parallelism)
    shared.run_ranges([command(spec) for spec in active], work,
                      args.parallelism)
    merged = "work/transitions/kghostghostk"
    merge = [str(executable), "--merge-transitions",
             "--transition-prefix", merged]
    for name, _, _ in merge_ranges:
        merge += ["--shard", f"work/transitions/{name}"]
    merge += binding
    run(merge, work, work / "work/logs/merge.log")
    solve = [str(executable), "--solve", "--transition-prefix", merged,
      "--input", "tablebases/kghostghostk.uftb",
      "--lower-ghost-sidecar", "tablebases/kghostk.ufgm",
      "--scratch", "work/solve/kghostghostk",
      "--output", "work/results/kghostghostk.ufiw",
      "--output-arbitrary", "work/results/kghostghostk.ufgg", *binding,
      "--lower-sidecar-sha256", args.lower_ghost_sha256,
      "--lower-source-sha256", args.lower_ghost_source_sha256,
      "--lower-model-sha256", args.lower_ghost_model_sha256,
      "--lower-observation-sha256", args.lower_ghost_observation_sha256,
      "--max-disk-bytes", str(args.max_disk_bytes),
      "--max-resident-bytes", str(args.max_resident_bytes),
      "--min-free-disk-bytes", str(args.min_free_disk_bytes),
      "--compact-every", "1"]
    run(solve, work, work / "work/logs/solve.log")
    write_manifest(work, args)


if __name__ == "__main__":
    main()
