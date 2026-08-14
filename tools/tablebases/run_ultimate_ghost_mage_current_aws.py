#!/usr/bin/env python3
"""Run one fresh exact Mage/Ghost information class on EC2."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys


GEOMETRIES = 492_960
SHARDS = 64
NORMALIZED = re.compile(
    r"mage_ghost_normalized_source_sha256(?:\s+\S+)*\s+([0-9a-f]{64})")


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


def ranges() -> list[tuple[int, int]]:
    base, extra = divmod(GEOMETRIES, SHARDS)
    cursor = 0
    result = []
    for index in range(SHARDS):
        count = base + (index < extra)
        result.append((cursor, count))
        cursor += count
    if cursor != GEOMETRIES:
        raise RuntimeError("Mage/Ghost shard coverage residual")
    return result


def write_manifest(work: Path, args: argparse.Namespace) -> None:
    results = sorted((work / "work/results").glob("*"))
    logs = sorted((work / "work/logs").glob("*.log"))
    if ({path.suffix for path in results} != {".ufiw", ".ufmg"} or
            not any(path.name == "solve.log" for path in logs)):
        raise RuntimeError("Mage/Ghost result/proof inventory is incomplete")
    self_test = (work / "work/logs/self-test.log").read_text()
    matches = NORMALIZED.findall(self_test)
    if not matches:
        raise RuntimeError("Mage/Ghost normalized-source certificate missing")
    artifacts = [*results, *logs]
    manifest = {
        "schema": "ultimate-mage-ghost-current-artifacts-v1",
        "filename": args.filename,
        "orientation": args.orientation,
        "source_sha256": args.source_sha256,
        "model_sha256": args.model_sha256,
        "observation_sha256": args.observation_sha256,
        "normalized_source_sha256": matches[-1],
        "lower_ghost_sha256": args.lower_ghost_sha256,
        "files": {str(path.relative_to(work)): {
            "bytes": path.stat().st_size, "sha256": sha256_path(path)}
            for path in artifacts},
    }
    (work / "work/artifact-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--filename", choices=("kghostmagek.uftb",
                                                 "kghostkmage.uftb"),
                        required=True)
    parser.add_argument("--orientation", choices=("same", "opposing"),
                        required=True)
    parser.add_argument("--source-table", type=Path, required=True)
    parser.add_argument("--source-sha256", required=True)
    parser.add_argument("--lower-ghost-sidecar", type=Path, required=True)
    parser.add_argument("--lower-ghost-sha256", required=True)
    parser.add_argument("--model-sha256", required=True)
    parser.add_argument("--observation-sha256", required=True)
    parser.add_argument("--lower-ghost-source-sha256", required=True)
    parser.add_argument("--lower-ghost-model-sha256", required=True)
    parser.add_argument("--lower-ghost-observation-sha256", required=True)
    parser.add_argument("--parallelism", type=int, default=29)
    args = parser.parse_args()
    expected = ("kghostmagek.uftb" if args.orientation == "same"
                else "kghostkmage.uftb")
    if args.filename != expected or not 1 <= args.parallelism <= SHARDS:
        raise RuntimeError("Mage/Ghost material or parallelism residual")

    root = args.source_root.resolve()
    sys.path.insert(0, str(root / "tools/tablebases"))
    import ultimate_information_tablebases as information  # pylint: disable=import-outside-toplevel
    import run_ultimate_reciprocal_bishop_ghost_aws as shared  # pylint: disable=import-outside-toplevel
    if information.solver_model_fingerprint(
            args.filename, root=root) != args.model_sha256:
        raise RuntimeError("Mage/Ghost solver model mismatch")
    require_sha(args.source_table, args.source_sha256, "source table")
    require_sha(args.lower_ghost_sidecar, args.lower_ghost_sha256,
                "lower Ghost sidecar")
    if args.work.exists():
        raise RuntimeError("Mage/Ghost work directory already exists")
    work = args.work.resolve()
    (work / "tablebases").mkdir(parents=True)
    shutil.copyfile(args.source_table, work / "tablebases" / args.filename)
    shutil.copyfile(args.lower_ghost_sidecar,
                    work / "tablebases/kghostk.ufgm")
    require_sha(work / "tablebases" / args.filename, args.source_sha256,
                "copied source")
    require_sha(work / "tablebases/kghostk.ufgm", args.lower_ghost_sha256,
                "copied lower Ghost")
    for directory in ("work/logs", "work/transitions", "work/results",
                      "work/solve", "work/self-test"):
        (work / directory).mkdir(parents=True, exist_ok=True)

    executable = work / "ultimate_ghost_mage_information_tablebase"
    sources = [
        "src/ultimate/tablebases/ghost_mage_information_tablebase.cpp",
        "src/ultimate/tablebases/ghost_mage_information_solver.cpp",
        "src/ultimate/tablebases/ghost_public_extra_model.cpp",
        "src/ultimate/tablebases/external_robdd.cpp",
        "src/ultimate/tablebases/ghost_information_probe.cpp",
        "src/ultimate/tablebases/information.cpp", "src/ultimate/position.cpp",
        "src/ultimate/nnue.cpp",
    ]
    run(["clang++", "-std=c++17", "-O3", "-DNDEBUG", "-Wall",
         "-Wextra", "-Wpedantic", "-Werror",
         "-Wno-error=range-loop-construct", "-include", "sstream",
         "-Isrc/ultimate", "-Isrc/ultimate/tablebases", *sources,
         "-o", str(executable)], root, work / "work/logs/build.log")
    binding = ["--orientation", args.orientation,
               "--source-sha256", args.source_sha256,
               "--model-sha256", args.model_sha256,
               "--observation-sha256", args.observation_sha256]
    run([str(executable), "--self-test", "--orientation", args.orientation,
         "--scratch", f"work/self-test/{Path(args.filename).stem}",
         "--input", f"tablebases/{args.filename}",
         "--source-sha256", args.source_sha256], work,
        work / "work/logs/self-test.log")
    commands = []
    for index, (begin, count) in enumerate(ranges()):
        commands.append([str(executable), "--compile-transitions",
          "--transition-prefix", f"work/transitions/shard-{index:02d}",
          "--geometry-begin", str(begin), "--geometry-count", str(count),
          *binding])
    shared.run_ranges(commands, work, args.parallelism)
    stem = Path(args.filename).stem
    merged = f"work/transitions/{stem}"
    merge = [str(executable), "--merge-transitions", "--orientation",
             args.orientation, "--transition-prefix", merged]
    for index in range(SHARDS):
        merge += ["--shard", f"work/transitions/shard-{index:02d}"]
    merge += ["--expected-geometries", str(GEOMETRIES), *binding[2:]]
    run(merge, work, work / "work/logs/merge.log")
    solve = [str(executable), "--solve", "--orientation", args.orientation,
      "--transition-prefix", merged, "--input", f"tablebases/{args.filename}",
      "--lower-ghost-sidecar", "tablebases/kghostk.ufgm",
      "--scratch", f"work/solve/{stem}",
      "--output", f"work/results/{stem}.ufiw",
      "--output-arbitrary", f"work/results/{stem}.ufmg", *binding[2:],
      "--lower-sidecar-sha256", args.lower_ghost_sha256,
      "--lower-source-sha256", args.lower_ghost_source_sha256,
      "--lower-model-sha256", args.lower_ghost_model_sha256,
      "--lower-observation-sha256", args.lower_ghost_observation_sha256,
      "--max-nodes", "500000000", "--unique-slots", str(1 << 30),
      "--compact-every", "1"]
    run(solve, work, work / "work/logs/solve.log")
    write_manifest(work, args)


if __name__ == "__main__":
    main()
