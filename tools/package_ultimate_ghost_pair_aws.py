#!/usr/bin/env python3
"""Build a deterministic load-balanced KGhostGhost AWS bundle."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile

import ultimate_information_tablebases as information


ROOT = Path(__file__).resolve().parents[1]
RAW_GEOMETRIES = 38_956_480
ORIGINAL_SHARDS = 32
RAW_PER_SHARD = 1_217_390
HALF_SHARD = 608_695
QUARTER_A = 304_347
QUARTER_B = 304_348
PARALLELISM = 30
HEAVY_SHARDS = frozenset((*range(0, 8), *range(16, 24)))
ZERO_FULL_SHARDS = frozenset((*range(8, 16), *range(24, 32)))
ZERO_HALVES = frozenset((
    "01a", "02b", "04a", "07b", "17a", "18b", "20a", "23b"))
DENSE_HALVES = frozenset((
    "00a", "03b", "05a", "06b", "16a", "19b", "21a", "22b"))
SOURCE_SHA256 = (
    "204f4de6d0f9ff6da111d3d0c0a08c3562493cdec2130cc946b4eae7183012ee")
LOWER_SHA256 = (
    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
BISHOP_GHOST_FINGERPRINT = (
    "d59736789155c52d9b697f6e3f05c2fd0c565beba5d5cce49e3648719289da91")
GHOST_PAIR_FINGERPRINT = (
    "2280445ed5c6f024b0cd8d00fca45dd48e5359cd591d4e3d28ee0259f74ff286")

BUILD_INPUTS = (
    "src/ultimate/ghost_pair_information_tablebase.cpp",
    "src/ultimate/ghost_pair_information_solver.cpp",
    "src/ultimate/ghost_pair_information_solver.h",
    "src/ultimate/ghost_pair_information_model.cpp",
    "src/ultimate/ghost_pair_information_model.h",
    "src/ultimate/ghost_information_probe.cpp",
    "src/ultimate/ghost_information_probe.h",
    "src/ultimate/information.cpp",
    "src/ultimate/information.h",
    "src/ultimate/position.cpp",
    "src/ultimate/position.h",
    "src/ultimate/nnue.cpp",
    "src/ultimate/nnue.h",
    "tablebases/kghostghostk.uftb",
    "tablebases/kghostk.ufgm",
    "tools/run_ultimate_ghost_pair_aws.py",
)
CPP_SOURCES = tuple(path for path in BUILD_INPUTS
                    if path.startswith("src/") and path.endswith(".cpp"))


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def file_record(path: str) -> dict[str, object]:
    payload = (ROOT / path).read_bytes()
    return {"path": path, "bytes": len(payload),
            "sha256": sha256_bytes(payload)}


def lower_binding(payload: bytes) -> tuple[str, str, str]:
    if (len(payload) < 320 or payload[:8] != b"UFGM1\0\0\0" or
            int.from_bytes(payload[12:16], "little") != 320):
        raise RuntimeError("kghostk.ufgm has an invalid authenticated header")
    return (payload[96:160].decode(), payload[160:224].decode(),
            payload[224:288].decode())


def solver_arguments(model: str, lower: tuple[str, str, str]) -> list[str]:
    return [
        "--transition-prefix", "work/transitions/kghostghostk",
        "--input", "tablebases/kghostghostk.uftb",
        "--lower-ghost-sidecar", "tablebases/kghostk.ufgm",
        "--scratch", "work/solve/kghostghostk",
        "--output", "work/results/kghostghostk.ufiw",
        "--output-arbitrary", "work/results/kghostghostk.ufgg",
        "--source-sha256", SOURCE_SHA256,
        "--model-sha256", model,
        "--observation-sha256", information.observation_model_fingerprint(),
        "--lower-sidecar-sha256", LOWER_SHA256,
        "--lower-source-sha256", lower[0],
        "--lower-model-sha256", lower[1],
        "--lower-observation-sha256", lower[2],
        "--max-disk-bytes", str(1_700 << 30),
        "--max-resident-bytes", str(170 << 30),
        "--min-free-disk-bytes", str(50 << 30),
    ]


# (prefix basename, raw begin, raw count).  The zero ranges are cheap to
# compile on a fresh machine and safe to reuse when their complete file set is
# already present; the native merge authenticates every reused marker.
RangeSpec = tuple[str, int, int]


def balanced_ranges() -> tuple[list[RangeSpec], list[RangeSpec],
                               list[RangeSpec]]:
    """Return zero/bootstrap, active, and raw-ordered merge ranges."""
    zero: list[RangeSpec] = []
    active: list[RangeSpec] = []
    merged: list[RangeSpec] = []
    for index in range(ORIGINAL_SHARDS):
        original_begin = index * RAW_PER_SHARD
        if index in ZERO_FULL_SHARDS:
            spec = (f"shard-{index:02d}", original_begin, RAW_PER_SHARD)
            zero.append(spec)
            merged.append(spec)
            continue
        if index not in HEAVY_SHARDS:
            raise RuntimeError("unclassified Ghost-pair original shard")
        for part, half_begin in (
                ("a", original_begin),
                ("b", original_begin + HALF_SHARD)):
            label = f"{index:02d}{part}"
            if label in ZERO_HALVES:
                spec = (f"split-{label}", half_begin, HALF_SHARD)
                zero.append(spec)
                merged.append(spec)
            elif label in DENSE_HALVES:
                first = (f"resume-{label}q0", half_begin, QUARTER_A)
                second = (f"resume-{label}q1",
                          half_begin + QUARTER_A, QUARTER_B)
                active.extend((first, second))
                merged.extend((first, second))
            else:
                spec = (f"resume-{label}h", half_begin, HALF_SHARD)
                active.append(spec)
                merged.append(spec)
    return zero, active, merged


def compile_command(spec: RangeSpec, binding: list[str]) -> list[str]:
    name, begin, count = spec
    return [
        "./ultimate_ghost_pair_information_tablebase",
        "--compile-transitions",
        "--transition-prefix", f"work/transitions/{name}",
        "--raw-begin", str(begin), "--raw-count", str(count), *binding,
    ]


def build_manifest() -> dict[str, object]:
    if RAW_PER_SHARD * ORIGINAL_SHARDS != RAW_GEOMETRIES:
        raise RuntimeError("Ghost-pair shard division is not exact")
    if (HALF_SHARD * 2 != RAW_PER_SHARD or
            QUARTER_A + QUARTER_B != HALF_SHARD or
            RAW_PER_SHARD * 16 != RAW_GEOMETRIES // 2):
        raise RuntimeError("Ghost-pair shards do not align at the side half")
    if information.solver_model_fingerprint(
            "kbishopghostk.uftb") != BISHOP_GHOST_FINGERPRINT:
        raise RuntimeError("active Bishop+Ghost fingerprint changed")
    concrete = (ROOT / "tablebases" / "kghostghostk.uftb").read_bytes()
    lower_payload = (ROOT / "tablebases" / "kghostk.ufgm").read_bytes()
    if sha256_bytes(concrete) != SOURCE_SHA256:
        raise RuntimeError("kghostghostk concrete SHA-256 mismatch")
    if sha256_bytes(lower_payload) != LOWER_SHA256:
        raise RuntimeError("kghostk lower UFGM SHA-256 mismatch")
    lower = lower_binding(lower_payload)
    model = information.solver_model_fingerprint("kghostghostk.uftb")
    if model != GHOST_PAIR_FINGERPRINT:
        raise RuntimeError("active Ghost-pair fingerprint changed")
    observation = information.observation_model_fingerprint()
    executable = "./ultimate_ghost_pair_information_tablebase"
    binding = [
        "--source-sha256", SOURCE_SHA256,
        "--model-sha256", model,
        "--observation-sha256", observation,
    ]
    zero_ranges, active_ranges, merge_ranges = balanced_ranges()
    ordered = sorted(merge_ranges, key=lambda item: item[1])
    cursor = 0
    for name, begin, count in ordered:
        if begin != cursor or count <= 0:
            raise RuntimeError(
                f"Ghost-pair balanced ranges are not gap-free at {name}")
        cursor += count
    if (cursor != RAW_GEOMETRIES or len(zero_ranges) != 24 or
            len(active_ranges) != 32 or len(merge_ranges) != 56 or
            len({item[0] for item in merge_ranges}) != len(merge_ranges)):
        raise RuntimeError("Ghost-pair balanced range inventory is invalid")
    zero_commands = [compile_command(spec, binding) for spec in zero_ranges]
    shard_commands = [compile_command(spec, binding)
                      for spec in active_ranges]
    merge = [executable, "--merge-transitions", "--transition-prefix",
             "work/transitions/kghostghostk"]
    for name, _, _ in merge_ranges:
        merge.extend(["--shard", f"work/transitions/{name}"])
    merge.extend(binding)
    solve = solver_arguments(model, lower)
    transition = "work/transitions/kghostghostk"
    artifacts = [
        *(f"{transition}.{suffix}" for suffix in
          ("header", "meta", "strata", "actual", "index", "blocks",
           "verified")),
        "work/results/kghostghostk.ufiw",
        "work/results/kghostghostk.ufgg",
        "work/logs/measure.log",
        "work/logs/solve.log",
    ]
    return {
        "schema": "ultimate-ghost-pair-aws-v2",
        "source_sha256": SOURCE_SHA256,
        "model_sha256": model,
        "observation_sha256": observation,
        "lower_sidecar_sha256": LOWER_SHA256,
        "lower_source_sha256": lower[0],
        "lower_model_sha256": lower[1],
        "lower_observation_sha256": lower[2],
        "raw_geometries": RAW_GEOMETRIES,
        "active_jobs": len(active_ranges),
        "zero_bootstrap_jobs": len(zero_ranges),
        "merge_inputs": len(merge_ranges),
        "raw_per_shard": RAW_PER_SHARD,
        "side_half_boundary": RAW_PER_SHARD * 16,
        "parallelism": PARALLELISM,
        "files": [file_record(path) for path in BUILD_INPUTS],
        "commands": {
            "build": ["c++", "-std=c++17", "-O3", "-DNDEBUG", "-Wall",
                      "-Wextra", "-Wpedantic", "-Isrc/ultimate",
                      *CPP_SOURCES, "-o",
                      "ultimate_ghost_pair_information_tablebase"],
            "zero_shards": zero_commands,
            "shards": shard_commands,
            "merge": merge,
            "measure": [executable, "--measure", "1", *solve],
            "solve": [executable, "--solve", *solve],
        },
        "artifacts": artifacts,
    }


def add_bytes(archive: tarfile.TarFile, name: str, payload: bytes,
              mode: int = 0o644) -> None:
    record = tarfile.TarInfo(name)
    record.size = len(payload)
    record.mode = mode
    record.mtime = record.uid = record.gid = 0
    record.uname = record.gname = ""
    archive.addfile(record, io.BytesIO(payload))


def build_bundle(output: Path) -> dict[str, object]:
    manifest = build_manifest()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        add_bytes(archive, "bundle-manifest.json",
                  (json.dumps(manifest, indent=2, sort_keys=True) +
                   "\n").encode())
        for path in sorted(BUILD_INPUTS):
            mode = 0o755 if path.endswith("run_ultimate_ghost_pair_aws.py") \
                else 0o644
            add_bytes(archive, path, (ROOT / path).read_bytes(), mode)
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = build_bundle(args.output)
    print(json.dumps({
        "bundle": str(args.output),
        "bytes": args.output.stat().st_size,
        "sha256": sha256_bytes(args.output.read_bytes()),
        "model_sha256": manifest["model_sha256"],
        "active_jobs": manifest["active_jobs"],
        "merge_inputs": manifest["merge_inputs"],
        "raw_per_shard": manifest["raw_per_shard"],
    }, sort_keys=True))


if __name__ == "__main__":
    main()
