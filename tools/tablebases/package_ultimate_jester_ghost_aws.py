#!/usr/bin/env python3
"""Build the deterministic exact K+Jester+Ghost-v-K AWS proof bundle."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile

import ultimate_information_tablebases as information

ROOT = Path(__file__).resolve().parents[2]
RAW_GEOMETRIES = 38_450_880
BASE_SHARDS = 60
RAW_PER_SHARD = 640_848  # exactly 8,216 complete 78-visibility cycles
HALF_RAW = 320_424       # exactly 4,108 complete visibility cycles
PARALLELISM = 29         # reserve three cores for AWS/system proof work
ZERO_SHARDS = (
    2, 5, 8, 11, *range(14, 30), 32, 35, 38, 41, *range(44, 60),
)
HEAVY_SHARDS = tuple(index for index in range(BASE_SHARDS)
                     if index not in ZERO_SHARDS)
SOURCE_SHA256 = "ad82489372318a7561a43a7c2d0cbc1fdc3e5a840c67470c5eb25704fdfecf7e"
LOWER_JESTER_SOURCE_SHA256 = "3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa"
LOWER_JESTER_MODEL_SHA256 = "0ed6d361e313623234c21f9a1c800947014ce47320b4a71fca4fb20a255587c2"
LOWER_GHOST_SHA256 = "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b"
BISHOP_GHOST_FINGERPRINT = "416f3792dfa0b5f5317a0ee4f8ae91b2a4dd874807d7223c5918dc0b291a2dcf"
GHOST_PAIR_FINGERPRINT = "84807bb96f10a77240b2e0c0584e0739d33d5135241b9e1b567f682b67e085af"

BUILD_INPUTS = (
    "src/ultimate/tablebases/jester_ghost_information_tablebase.cpp",
    "src/ultimate/tablebases/jester_ghost_information_solver.cpp",
    "src/ultimate/tablebases/jester_ghost_information_solver.h",
    "src/ultimate/tablebases/jester_ghost_information_model.cpp",
    "src/ultimate/tablebases/jester_ghost_information_model.h",
    "src/ultimate/tablebases/external_robdd.cpp", "src/ultimate/tablebases/external_robdd.h",
    "src/ultimate/tablebases/ghost_information_probe.cpp",
    "src/ultimate/tablebases/ghost_information_probe.h",
    "src/ultimate/tablebases/information.cpp", "src/ultimate/tablebases/information.h",
    "src/ultimate/position.cpp", "src/ultimate/position.h",
    "src/ultimate/nnue.cpp", "src/ultimate/nnue.h",
    "tablebases/kjesterghostk.uftb", "tablebases/kjesterk.uftb",
    "tablebases/kghostk.ufgm", "tools/tablebases/run_ultimate_jester_ghost_aws.py",
)
CPP_SOURCES = tuple(path for path in BUILD_INPUTS
                    if path.startswith("src/") and path.endswith(".cpp"))


def sha(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def record(path: str, payload: bytes | None = None) -> dict[str, object]:
    data = (ROOT / path).read_bytes() if payload is None else payload
    return {"path": path, "bytes": len(data), "sha256": sha(data)}


def build_manifest(lower_overlay: Path) -> dict[str, object]:
    if (BASE_SHARDS * RAW_PER_SHARD != RAW_GEOMETRIES or
            RAW_PER_SHARD % 78 or 2 * HALF_RAW != RAW_PER_SHARD or
            HALF_RAW % 78 or len(ZERO_SHARDS) != 40 or
            len(HEAVY_SHARDS) != 20):
        raise RuntimeError("Jester/Ghost visibility-cycle shard drift")
    if information.solver_model_fingerprint("kbishopghostk.uftb") != BISHOP_GHOST_FINGERPRINT:
        raise RuntimeError("active Bishop+Ghost fingerprint changed")
    if information.solver_model_fingerprint("kghostghostk.uftb") != GHOST_PAIR_FINGERPRINT:
        raise RuntimeError("active GhostPair fingerprint changed")
    concrete = (ROOT / "tablebases/kjesterghostk.uftb").read_bytes()
    lower_jester = (ROOT / "tablebases/kjesterk.uftb").read_bytes()
    lower_ghost = (ROOT / "tablebases/kghostk.ufgm").read_bytes()
    overlay = lower_overlay.read_bytes()
    if sha(concrete) != SOURCE_SHA256 or sha(lower_jester) != LOWER_JESTER_SOURCE_SHA256:
        raise RuntimeError("concrete dependency SHA mismatch")
    if sha(lower_ghost) != LOWER_GHOST_SHA256:
        raise RuntimeError("lower Ghost UFGM SHA mismatch")
    if (len(overlay) < 160 or overlay[:8] != b"UFIW2\0\0\0" or
            overlay[32:96].decode() != LOWER_JESTER_SOURCE_SHA256 or
            overlay[96:160].decode() != LOWER_JESTER_MODEL_SHA256):
        raise RuntimeError("stale lower Jester UFIW2")
    lower_overlay_sha = sha(overlay)
    model = information.solver_model_fingerprint("kjesterghostk.uftb")
    observation = information.observation_model_fingerprint()
    binding = ["--source-sha256", SOURCE_SHA256, "--model-sha256", model,
               "--observation-sha256", observation]
    common = [
        "--transition-prefix", "work/transitions/kjesterghostk",
        "--input", "tablebases/kjesterghostk.uftb",
        "--lower-jester-table", "tablebases/kjesterk.uftb",
        "--lower-jester-overlay", "tablebases/kjesterk.ufiw",
        "--lower-jester-model-sha256", LOWER_JESTER_MODEL_SHA256,
        "--lower-jester-overlay-sha256", lower_overlay_sha,
        "--lower-ghost-sidecar", "tablebases/kghostk.ufgm",
        "--lower-ghost-sidecar-sha256", LOWER_GHOST_SHA256,
        "--scratch", "work/solve/kjesterghostk",
        "--output", "work/results/kjesterghostk.ufiw",
        "--output-arbitrary", "work/results/kjesterghostk.ufjg",
        *binding, "--max-disk-bytes", str(1_700 << 30),
        "--max-resident-bytes", str(170 << 30),
        "--min-free-disk-bytes", str(50 << 30),
    ]
    bootstrap_ranges = [
        (f"shard-{index:02d}", index * RAW_PER_SHARD, RAW_PER_SHARD)
        for index in ZERO_SHARDS]
    active_ranges = [
        (f"shard-{index:02d}{suffix}",
         index * RAW_PER_SHARD + half * HALF_RAW, HALF_RAW)
        for index in HEAVY_SHARDS
        for half, suffix in enumerate(("a", "b"))]
    merge_ranges = sorted(bootstrap_ranges + active_ranges,
                          key=lambda item: item[1])
    bootstrap_commands = [["./ultimate_jester_ghost_information_tablebase",
        "--verify-transitions", "--transition-prefix",
        f"work/transitions/{name}", "--raw-begin", str(begin),
        "--raw-count", str(count), "--allow-partial-merge", *binding]
        for name, begin, count in bootstrap_ranges]
    shard_commands = [["./ultimate_jester_ghost_information_tablebase",
        "--compile-transitions", "--transition-prefix",
        f"work/transitions/{name}", "--raw-begin", str(begin),
        "--raw-count", str(count), *binding]
        for name, begin, count in active_ranges]
    merge = ["./ultimate_jester_ghost_information_tablebase",
             "--merge-transitions", "--transition-prefix",
             "work/transitions/kjesterghostk"]
    for name, _, _ in merge_ranges:
        merge += ["--shard", f"work/transitions/{name}"]
    merge += binding
    files = [record(path) for path in BUILD_INPUTS]
    files.append(record("tablebases/kjesterk.ufiw", overlay))
    transition = "work/transitions/kjesterghostk"
    return {
        "schema": "ultimate-jester-ghost-aws-v2",
        "source_sha256": SOURCE_SHA256, "model_sha256": model,
        "observation_sha256": observation,
        "lower_jester_overlay_sha256": lower_overlay_sha,
        "lower_ghost_sidecar_sha256": LOWER_GHOST_SHA256,
        "raw_geometries": RAW_GEOMETRIES, "raw_per_shard": RAW_PER_SHARD,
        "half_raw": HALF_RAW, "base_shards": BASE_SHARDS,
        "bootstrap_inputs": len(bootstrap_ranges),
        "merge_inputs": len(merge_ranges),
        "active_jobs": len(active_ranges), "parallelism": PARALLELISM,
        "zero_shards": list(ZERO_SHARDS),
        "heavy_shards": list(HEAVY_SHARDS),
        "bootstrap_ranges": [list(item) for item in bootstrap_ranges],
        "active_ranges": [list(item) for item in active_ranges],
        "merge_ranges": [list(item) for item in merge_ranges],
        "visibility_cycle": 78, "files": files,
        "commands": {
            "build": ["c++", "-std=c++17", "-O3", "-DNDEBUG", "-Wall",
                      "-Wextra", "-Wpedantic", "-Isrc/ultimate", "-Isrc/ultimate/tablebases", *CPP_SOURCES,
                      "-o", "ultimate_jester_ghost_information_tablebase"],
            "bootstrap": bootstrap_commands,
            "shards": shard_commands, "merge": merge,
            "measure": ["./ultimate_jester_ghost_information_tablebase",
                        "--measure", "1", *common],
            "solve": ["./ultimate_jester_ghost_information_tablebase",
                      "--solve", *common],
        },
        "artifacts": [
            *(f"{transition}.{suffix}" for suffix in
              ("header", "meta", "strata", "index", "blocks", "verified")),
            "work/results/kjesterghostk.ufiw",
            "work/results/kjesterghostk.ufjg",
            "work/logs/measure.log", "work/logs/solve.log"],
    }


def add(archive: tarfile.TarFile, name: str, payload: bytes, mode: int = 0o644) -> None:
    info = tarfile.TarInfo(name); info.size = len(payload); info.mode = mode
    info.mtime = info.uid = info.gid = 0; info.uname = info.gname = ""
    archive.addfile(info, io.BytesIO(payload))


def build_bundle(output: Path, lower_overlay: Path) -> dict[str, object]:
    manifest = build_manifest(lower_overlay)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        add(archive, "bundle-manifest.json",
            (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode())
        for path in sorted(BUILD_INPUTS):
            add(archive, path, (ROOT / path).read_bytes(),
                0o755 if path.endswith("run_ultimate_jester_ghost_aws.py") else 0o644)
        add(archive, "tablebases/kjesterk.ufiw", lower_overlay.read_bytes())
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--lower-jester-overlay", type=Path, required=True)
    args = parser.parse_args()
    manifest = build_bundle(args.output, args.lower_jester_overlay)
    print(json.dumps({"bundle": str(args.output), "bytes": args.output.stat().st_size,
                      "sha256": sha(args.output.read_bytes()),
                      "model_sha256": manifest["model_sha256"],
                      "active_jobs": manifest["active_jobs"]}, sort_keys=True))


if __name__ == "__main__":
    main()
