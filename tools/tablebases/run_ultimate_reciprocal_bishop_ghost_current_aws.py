#!/usr/bin/env python3
"""Build fresh current-rule K+Bishop-v-K+Ghost information on EC2."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys


GEOMETRIES = 492_960
SHARDS = 32
GEOMETRIES_PER_SHARD = 15_405


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
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("xb") as output:
        completed = subprocess.run(command, cwd=root, stdout=output,
                                   stderr=subprocess.STDOUT, check=False)
    if completed.returncode:
        raise RuntimeError(f"command failed ({completed.returncode}): {log}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--source-table", type=Path, required=True)
    parser.add_argument("--source-sha256", required=True)
    parser.add_argument("--lower-ghost-sidecar", type=Path, required=True)
    parser.add_argument("--lower-ghost-sha256", required=True)
    parser.add_argument("--lower-ghost-source-sha256", required=True)
    parser.add_argument("--lower-ghost-model-sha256", required=True)
    parser.add_argument("--lower-ghost-observation-sha256", required=True)
    parser.add_argument("--model-sha256", required=True)
    parser.add_argument("--observation-sha256", required=True)
    parser.add_argument("--parallelism", type=int, default=24)
    args = parser.parse_args()
    if not 1 <= args.parallelism <= SHARDS:
        raise RuntimeError("reciprocal Bishop/Ghost parallelism residual")
    if SHARDS * GEOMETRIES_PER_SHARD != GEOMETRIES:
        raise RuntimeError("reciprocal Bishop/Ghost shard coverage residual")

    root = args.source_root.resolve()
    sys.path.insert(0, str(root / "tools/tablebases"))
    import ultimate_information_tablebases as information  # pylint: disable=import-outside-toplevel
    if information.solver_model_fingerprint(
            "kbishopkghost.uftb", root=root) != args.model_sha256:
        raise RuntimeError("reciprocal Bishop/Ghost solver model mismatch")
    if information.observation_model_fingerprint() != args.observation_sha256:
        raise RuntimeError("reciprocal Bishop/Ghost observation model mismatch")
    require_sha(args.source_table, args.source_sha256, "source table")
    require_sha(args.lower_ghost_sidecar, args.lower_ghost_sha256,
                "lower Ghost sidecar")
    if args.work.exists():
        raise RuntimeError("reciprocal Bishop/Ghost work directory exists")

    work = args.work.resolve()
    for directory in ("tablebases", "work/logs", "work/transitions",
                      "work/results", "work/solve", "work/self-test"):
        (work / directory).mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.source_table,
                    work / "tablebases/kbishopkghost.uftb")
    shutil.copyfile(args.lower_ghost_sidecar,
                    work / "tablebases/kghostk.ufgm")
    require_sha(work / "tablebases/kbishopkghost.uftb",
                args.source_sha256, "copied source")
    require_sha(work / "tablebases/kghostk.ufgm",
                args.lower_ghost_sha256, "copied lower Ghost")

    executable = work / "ultimate_reciprocal_bishop_ghost_information_tablebase"
    sources = [
        "src/ultimate/tablebases/ghost_public_extra_information_tablebase.cpp",
        "src/ultimate/tablebases/ghost_reciprocal_bishop_information_solver.cpp",
        "src/ultimate/tablebases/ghost_public_extra_model.cpp",
        "src/ultimate/tablebases/external_robdd.cpp",
        "src/ultimate/tablebases/ghost_information_probe.cpp",
        "src/ultimate/tablebases/information.cpp",
        "src/ultimate/position.cpp", "src/ultimate/nnue.cpp",
    ]
    run(["clang++", "-std=c++17", "-O3", "-DNDEBUG", "-Wall",
         "-Wextra", "-Wpedantic", "-Werror",
         "-Wno-error=range-loop-construct", "-include", "sstream",
         "-Isrc/ultimate", "-Isrc/ultimate/tablebases", *sources,
         "-o", str(executable)], root, work / "work/logs/build.log")
    run([str(executable), "--self-test", "--scratch",
         "work/self-test/kbishopkghost"], work,
        work / "work/logs/self-test.log")

    binding = ["--source-sha256", args.source_sha256,
               "--model-sha256", args.model_sha256,
               "--observation-sha256", args.observation_sha256]
    commands = []
    for index in range(SHARDS):
        commands.append((index, [
            str(executable), "--compile-transitions",
            "--transition-prefix", f"work/transitions/shard-{index:02d}",
            "--geometry-begin", str(index * GEOMETRIES_PER_SHARD),
            "--geometry-count", str(GEOMETRIES_PER_SHARD), *binding,
        ]))
    with ThreadPoolExecutor(max_workers=args.parallelism) as executor:
        futures = [executor.submit(
            run, command, work,
            work / f"work/logs/shard-{index:02d}.log")
                   for index, command in commands]
        for future in futures:
            future.result()

    merged = "work/transitions/kbishopkghost"
    merge = [str(executable), "--merge-transitions",
             "--transition-prefix", merged]
    for index in range(SHARDS):
        merge += ["--shard", f"work/transitions/shard-{index:02d}"]
    merge += ["--expected-geometries", str(GEOMETRIES), *binding]
    run(merge, work, work / "work/logs/merge.log")

    run([str(executable), "--solve",
         "--transition-prefix", merged,
         "--input", "tablebases/kbishopkghost.uftb",
         "--lower-ghost-sidecar", "tablebases/kghostk.ufgm",
         "--scratch", "work/solve/kbishopkghost",
         "--output", "work/results/kbishopkghost.ufiw",
         "--output-arbitrary", "work/results/kbishopkghost.ufgx",
         *binding,
         "--lower-sidecar-sha256", args.lower_ghost_sha256,
         "--lower-source-sha256", args.lower_ghost_source_sha256,
         "--lower-model-sha256", args.lower_ghost_model_sha256,
         "--lower-observation-sha256",
         args.lower_ghost_observation_sha256,
         "--max-nodes", "500000000",
         "--unique-slots", str(1 << 30),
         "--compact-every", "1"], work,
        work / "work/logs/solve.log")

    artifacts = sorted((work / "work/results").glob("*")) + \
        sorted((work / "work/logs").glob("*.log"))
    expected = {"kbishopkghost.ufiw", "kbishopkghost.ufgx"}
    if {path.name for path in (work / "work/results").glob("*")} != expected:
        raise RuntimeError("reciprocal Bishop/Ghost result inventory residual")
    (work / "work/artifact-manifest.json").write_text(json.dumps({
        "schema": "ultimate-reciprocal-bishop-ghost-current-artifacts-v1",
        "filename": "kbishopkghost.uftb",
        "source_sha256": args.source_sha256,
        "model_sha256": args.model_sha256,
        "observation_sha256": args.observation_sha256,
        "lower_ghost_sha256": args.lower_ghost_sha256,
        "files": {str(path.relative_to(work)): {
            "bytes": path.stat().st_size,
            "sha256": sha256_path(path),
        } for path in artifacts},
    }, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
