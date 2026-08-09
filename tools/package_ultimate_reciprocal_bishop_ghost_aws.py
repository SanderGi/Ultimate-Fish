#!/usr/bin/env python3
"""Build a deterministic AWS bundle for exact K+Bishop-v-K+Ghost."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile

import ultimate_information_tablebases as information


ROOT = Path(__file__).resolve().parents[1]
GEOMETRIES = 492_960
SHARDS = 32
GEOMETRIES_PER_SHARD = 15_405
PARALLELISM = 29
SOURCE_SHA256 = (
    "0649b7859ba72c8534929a902f18b3ca1a74cd7d7b3713ac109633e83a85ac15")
LOWER_SHA256 = (
    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
BISHOP_GHOST_FINGERPRINT = (
    "d59736789155c52d9b697f6e3f05c2fd0c565beba5d5cce49e3648719289da91")
GHOST_PAIR_FINGERPRINT = (
    "2280445ed5c6f024b0cd8d00fca45dd48e5359cd591d4e3d28ee0259f74ff286")
RECIPROCAL_FINGERPRINT = (
    "a88ffbb75d130563b7cd14801a9a36f6e35f99b336dc79e40477b0a268823c85")

BUILD_INPUTS = (
    "src/ultimate/ghost_public_extra_information_tablebase.cpp",
    "src/ultimate/ghost_public_extra_information_solver.cpp",
    "src/ultimate/ghost_public_extra_information_solver.h",
    "src/ultimate/ghost_public_extra_model.cpp",
    "src/ultimate/ghost_public_extra_model.h",
    # Included textually by the isolated reciprocal implementation. It is an
    # authenticated source input, not a separately compiled translation unit.
    "src/ultimate/ghost_extra_information_tablebase.cpp",
    "src/ultimate/external_robdd.cpp",
    "src/ultimate/external_robdd.h",
    "src/ultimate/ghost_information_probe.cpp",
    "src/ultimate/ghost_information_probe.h",
    "src/ultimate/information.cpp",
    "src/ultimate/information.h",
    "src/ultimate/position.cpp",
    "src/ultimate/position.h",
    "src/ultimate/nnue.cpp",
    "src/ultimate/nnue.h",
    "tablebases/kbishopkghost.uftb",
    "tablebases/kghostk.ufgm",
    "tools/run_ultimate_reciprocal_bishop_ghost_aws.py",
)
CPP_SOURCES = tuple(
    path for path in BUILD_INPUTS
    if path.startswith("src/") and path.endswith(".cpp") and
    not path.endswith("ghost_extra_information_tablebase.cpp"))


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


def shard_ranges() -> list[tuple[str, int, int]]:
    return [
        (f"shard-{index:02d}", index * GEOMETRIES_PER_SHARD,
         GEOMETRIES_PER_SHARD)
        for index in range(SHARDS)
    ]


def build_manifest() -> dict[str, object]:
    if SHARDS * GEOMETRIES_PER_SHARD != GEOMETRIES:
        raise RuntimeError("reciprocal geometry division is not exact")
    if information.solver_model_fingerprint(
            "kbishopghostk.uftb") != BISHOP_GHOST_FINGERPRINT:
        raise RuntimeError("frozen Bishop+Ghost d597 fingerprint changed")
    if information.solver_model_fingerprint(
            "kghostghostk.uftb") != GHOST_PAIR_FINGERPRINT:
        raise RuntimeError("frozen Ghost-pair 2280 fingerprint changed")
    model = information.solver_model_fingerprint("kbishopkghost.uftb")
    if model != RECIPROCAL_FINGERPRINT:
        raise RuntimeError("reciprocal Bishop/Ghost fingerprint changed")
    concrete = (ROOT / "tablebases" / "kbishopkghost.uftb").read_bytes()
    lower_payload = (ROOT / "tablebases" / "kghostk.ufgm").read_bytes()
    if sha256_bytes(concrete) != SOURCE_SHA256:
        raise RuntimeError("kbishopkghost concrete SHA-256 mismatch")
    if sha256_bytes(lower_payload) != LOWER_SHA256:
        raise RuntimeError("kghostk lower UFGM SHA-256 mismatch")
    lower = lower_binding(lower_payload)
    observation = information.observation_model_fingerprint()
    binding = [
        "--source-sha256", SOURCE_SHA256,
        "--model-sha256", model,
        "--observation-sha256", observation,
    ]
    ranges = shard_ranges()
    cursor = 0
    for name, begin, count in ranges:
        if begin != cursor or count <= 0:
            raise RuntimeError(f"reciprocal shards are not gap-free at {name}")
        cursor += count
    if cursor != GEOMETRIES:
        raise RuntimeError("reciprocal shards do not cover the domain")
    executable = "./ultimate_reciprocal_bishop_ghost_information_tablebase"
    shards = [[
        executable, "--compile-transitions", "--transition-prefix",
        f"work/transitions/{name}", "--geometry-begin", str(begin),
        "--geometry-count", str(count), *binding,
    ] for name, begin, count in ranges]
    merge = [executable, "--merge-transitions", "--transition-prefix",
             "work/transitions/kbishopkghost"]
    for name, _, _ in ranges:
        merge.extend(["--shard", f"work/transitions/{name}"])
    merge.extend(["--expected-geometries", str(GEOMETRIES), *binding])
    solve_arguments = [
        "--transition-prefix", "work/transitions/kbishopkghost",
        "--input", "tablebases/kbishopkghost.uftb",
        "--lower-ghost-sidecar", "tablebases/kghostk.ufgm",
        "--scratch", "work/solve/kbishopkghost",
        "--output", "work/results/kbishopkghost.ufiw",
        "--output-arbitrary", "work/results/kbishopkghost.ufgx",
        *binding,
        "--lower-sidecar-sha256", LOWER_SHA256,
        "--lower-source-sha256", lower[0],
        "--lower-model-sha256", lower[1],
        "--lower-observation-sha256", lower[2],
        "--max-nodes", "500000000",
        "--unique-slots", str(1 << 30),
        "--compact-every", "1",
    ]
    transition = "work/transitions/kbishopkghost"
    artifacts = [
        *(f"{transition}.{suffix}" for suffix in
          ("header", "meta", "strata", "index", "blocks", "verified")),
        "work/results/kbishopkghost.ufiw",
        "work/results/kbishopkghost.ufgx",
        "work/logs/build.log",
        "work/logs/self-test.log",
        "work/logs/merge.log",
        *(f"work/logs/shard-{index:02d}.log" for index in range(SHARDS)),
        "work/logs/measure.log",
        "work/logs/solve.log",
    ]
    return {
        "schema": "ultimate-reciprocal-bishop-ghost-aws-v1",
        "source_sha256": SOURCE_SHA256,
        "model_sha256": model,
        "observation_sha256": observation,
        "lower_sidecar_sha256": LOWER_SHA256,
        "lower_source_sha256": lower[0],
        "lower_model_sha256": lower[1],
        "lower_observation_sha256": lower[2],
        "geometries": GEOMETRIES,
        "shards": SHARDS,
        "geometries_per_shard": GEOMETRIES_PER_SHARD,
        "parallelism": PARALLELISM,
        "estimated_transition_bytes": 14_500_000_000,
        "estimated_peak_scratch_bytes": 32_500_077_312,
        "estimated_peak_resident_bytes": 19_327_352_832,
        "files": [file_record(path) for path in BUILD_INPUTS],
        "commands": {
            "build": ["clang++", "-std=c++17", "-O3", "-DNDEBUG",
                      "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                      "-Wno-error=range-loop-construct",
                      "-Isrc/ultimate", *CPP_SOURCES, "-o", executable[2:]],
            "self_test": [executable, "--self-test", "--scratch",
                          "work/self-test/kbishopkghost"],
            "shards": shards,
            "merge": merge,
            "measure": [executable, "--measure", "1", *solve_arguments],
            "solve": [executable, "--solve", *solve_arguments],
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
            mode = 0o755 if path.endswith(
                "run_ultimate_reciprocal_bishop_ghost_aws.py") else 0o644
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
        "shards": manifest["shards"],
        "geometries_per_shard": manifest["geometries_per_shard"],
    }, sort_keys=True))


if __name__ == "__main__":
    main()
