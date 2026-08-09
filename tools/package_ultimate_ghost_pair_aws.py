#!/usr/bin/env python3
"""Build a deterministic authenticated 32-shard KGhostGhost AWS bundle."""

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
SHARDS = 32
RAW_PER_SHARD = 1_217_390
SOURCE_SHA256 = (
    "204f4de6d0f9ff6da111d3d0c0a08c3562493cdec2130cc946b4eae7183012ee")
LOWER_SHA256 = (
    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
BISHOP_GHOST_FINGERPRINT = (
    "d59736789155c52d9b697f6e3f05c2fd0c565beba5d5cce49e3648719289da91")

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


def build_manifest() -> dict[str, object]:
    if RAW_PER_SHARD * SHARDS != RAW_GEOMETRIES:
        raise RuntimeError("Ghost-pair shard division is not exact")
    if RAW_PER_SHARD % 2 or RAW_PER_SHARD * 16 != RAW_GEOMETRIES // 2:
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
    observation = information.observation_model_fingerprint()
    executable = "./ultimate_ghost_pair_information_tablebase"
    binding = [
        "--source-sha256", SOURCE_SHA256,
        "--model-sha256", model,
        "--observation-sha256", observation,
    ]
    shard_commands = []
    for index in range(SHARDS):
        shard_commands.append([
            executable, "--compile-transitions",
            "--transition-prefix", f"work/transitions/shard-{index:02d}",
            "--raw-begin", str(index * RAW_PER_SHARD),
            "--raw-count", str(RAW_PER_SHARD), *binding,
        ])
    merge = [executable, "--merge-transitions", "--transition-prefix",
             "work/transitions/kghostghostk"]
    for index in range(SHARDS):
        merge.extend(["--shard", f"work/transitions/shard-{index:02d}"])
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
        "schema": "ultimate-ghost-pair-aws-v1",
        "source_sha256": SOURCE_SHA256,
        "model_sha256": model,
        "observation_sha256": observation,
        "lower_sidecar_sha256": LOWER_SHA256,
        "lower_source_sha256": lower[0],
        "lower_model_sha256": lower[1],
        "lower_observation_sha256": lower[2],
        "raw_geometries": RAW_GEOMETRIES,
        "shards": SHARDS,
        "raw_per_shard": RAW_PER_SHARD,
        "side_half_boundary": RAW_PER_SHARD * 16,
        "parallelism": 32,
        "files": [file_record(path) for path in BUILD_INPUTS],
        "commands": {
            "build": ["c++", "-std=c++17", "-O3", "-DNDEBUG", "-Wall",
                      "-Wextra", "-Wpedantic", "-Isrc/ultimate",
                      *CPP_SOURCES, "-o",
                      "ultimate_ghost_pair_information_tablebase"],
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
        "shards": manifest["shards"],
        "raw_per_shard": manifest["raw_per_shard"],
    }, sort_keys=True))


if __name__ == "__main__":
    main()
