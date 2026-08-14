#!/usr/bin/env python3
"""Build deterministic AWS bundles for exact Parasite/Ghost information."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile

import ultimate_information_tablebases as information


ROOT = Path(__file__).resolve().parents[2]
GEOMETRIES = 492_960
SHARDS = 64
LARGE_SHARDS = 32
LARGE_GEOMETRIES = 7_703
SMALL_GEOMETRIES = 7_702
PARALLELISM = 29
LOWER_SHA256 = (
    "472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb")
LOWER_PARASITE_SHA256 = (
    "c53364f87ce4ff23372aa3565d279e9fadde706eb1aeca5695aecc1ea3e83047")
LOWER_PARASITE_MODEL_SHA256 = (
    "c9889fd2d77617a1f0c77ed43a7f8a054c299732194ec89456f68ed865681c25")
LOWER_TRACKED_GHOST_SHA256 = (
    "32dc7889506f4dd4948dbf1bed3f31c6c3ebdd4a8600c4381c7a8c710b9ec671")
LOWER_TRACKED_GHOST_MODEL_SHA256 = (
    "5b71141f6213872133db4877b76e4eb3e67186111624e76c62eced1e722ce10f")
ROWS = {
    "kghostparasitek.uftb": {
        "orientation": "same",
        "source_sha256":
            "d551ba980ab7fb51c63713524e2bf23a31758cad7629835c8d4bb339976b4a58",
        "model_sha256":
            "dbf6e2777ac700693164f461be688ecedc3e58f4c65fbc967025cc3b860d4361",
        "normalized_source_sha256":
            "54c8d0bf2fa36254f1e1c4b9efa507c9128a5dc0030555857726fbef6551dd44",
    },
    "kghostkparasite.uftb": {
        "orientation": "opposing",
        "source_sha256":
            "af888568f496353e4477d364f1652f5b2329b51820ab07a7d4b0c80bd362baf1",
        "model_sha256":
            "66692c3d36301be90f6c5a069f2a86067b4772a5af8a04ec539afe0c60ede19f",
        "normalized_source_sha256":
            "8abd056e7260376999ca03416d2304e66191cf4100b15418a6ebcc7a6a276229",
    },
}
BUILD_INPUTS_COMMON = (
    "src/ultimate/tablebases/ghost_parasite_information_tablebase.cpp",
    "src/ultimate/tablebases/ghost_parasite_information_solver.cpp",
    "src/ultimate/tablebases/ghost_parasite_information_solver.h",
    "src/ultimate/tablebases/ghost_public_extra_information_solver.cpp",
    "src/ultimate/tablebases/ghost_public_extra_information_solver.h",
    "src/ultimate/tablebases/ghost_public_extra_model.cpp",
    "src/ultimate/tablebases/ghost_public_extra_model.h",
    "src/ultimate/tablebases/ghost_extra_information_tablebase.cpp",
    "src/ultimate/tablebases/external_robdd.cpp",
    "src/ultimate/tablebases/external_robdd.h",
    "src/ultimate/tablebases/ghost_information_probe.cpp",
    "src/ultimate/tablebases/ghost_information_probe.h",
    "src/ultimate/tablebases/information.cpp",
    "src/ultimate/tablebases/information.h",
    "src/ultimate/position.cpp",
    "src/ultimate/position.h",
    "src/ultimate/nnue.cpp",
    "src/ultimate/nnue.h",
    "tablebases/kghostk.ufgm",
    "tablebases/kghostk-tracked.uftb",
    "tablebases/kparasitek.uftb",
    "tools/tablebases/run_ultimate_reciprocal_bishop_ghost_aws.py",
    "tools/tablebases/run_ultimate_ghost_parasite_aws.py",
)
TEXTUAL_CPP = {
    "src/ultimate/tablebases/ghost_public_extra_information_solver.cpp",
    "src/ultimate/tablebases/ghost_extra_information_tablebase.cpp",
}


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
    ranges = []
    cursor = 0
    for index in range(SHARDS):
        count = LARGE_GEOMETRIES if index < LARGE_SHARDS else SMALL_GEOMETRIES
        ranges.append((f"shard-{index:02d}", cursor, count))
        cursor += count
    if cursor != GEOMETRIES:
        raise RuntimeError("balanced Parasite/Ghost shard arithmetic residual")
    return ranges


def build_manifest(filename: str) -> dict[str, object]:
    if filename not in ROWS:
        raise RuntimeError(f"unsupported Parasite/Ghost row {filename}")
    row = ROWS[filename]
    model = information.solver_model_fingerprint(filename)
    if model != row["model_sha256"]:
        raise RuntimeError(f"Parasite/Ghost model fingerprint changed: {filename}")
    build_inputs = (*BUILD_INPUTS_COMMON, f"tablebases/{filename}")
    concrete = (ROOT / "tablebases" / filename).read_bytes()
    lower_payload = (ROOT / "tablebases" / "kghostk.ufgm").read_bytes()
    lower_parasite_payload = (ROOT / "tablebases" / "kparasitek.uftb").read_bytes()
    lower_tracked_ghost_payload = (
        ROOT / "tablebases" / "kghostk-tracked.uftb").read_bytes()
    if sha256_bytes(concrete) != row["source_sha256"]:
        raise RuntimeError(f"{filename}: concrete SHA-256 mismatch")
    if sha256_bytes(lower_payload) != LOWER_SHA256:
        raise RuntimeError("kghostk lower UFGM SHA-256 mismatch")
    if sha256_bytes(lower_parasite_payload) != LOWER_PARASITE_SHA256:
        raise RuntimeError("kparasitek lower concrete binding mismatch")
    if (sha256_bytes(lower_tracked_ghost_payload) !=
            LOWER_TRACKED_GHOST_SHA256 or
            information.tracked_ghost_tablebase_model_fingerprint() !=
            LOWER_TRACKED_GHOST_MODEL_SHA256):
        raise RuntimeError("tracked-Ghost lower concrete binding mismatch")
    lower = lower_binding(lower_payload)
    observation = information.observation_model_fingerprint()
    stem = Path(filename).stem
    orientation = str(row["orientation"])
    executable = "./ultimate_ghost_parasite_information_tablebase"
    binding = ["--orientation", orientation,
               "--lower-parasite-table", "tablebases/kparasitek.uftb",
               "--lower-parasite-sha256", LOWER_PARASITE_SHA256,
               "--lower-parasite-source-sha256", LOWER_PARASITE_SHA256,
               "--lower-parasite-model-sha256", LOWER_PARASITE_MODEL_SHA256,
               "--lower-tracked-ghost-table",
               "tablebases/kghostk-tracked.uftb",
               "--lower-tracked-ghost-sha256", LOWER_TRACKED_GHOST_SHA256,
               "--lower-tracked-ghost-source-sha256",
               LOWER_TRACKED_GHOST_SHA256,
               "--lower-tracked-ghost-model-sha256",
               LOWER_TRACKED_GHOST_MODEL_SHA256,
               "--lower-ghost-sidecar", "tablebases/kghostk.ufgm",
               "--lower-sidecar-sha256", LOWER_SHA256,
               "--lower-source-sha256", lower[0],
               "--lower-model-sha256", lower[1],
               "--lower-observation-sha256", lower[2],
               "--source-sha256", str(row["source_sha256"]),
               "--model-sha256", model,
               "--observation-sha256", observation]
    ranges = shard_ranges()
    if ranges[-1][1] + ranges[-1][2] != GEOMETRIES:
        raise RuntimeError("Parasite/Ghost shards do not cover the domain")
    shards = [[executable, "--compile-transitions",
               "--transition-prefix", f"work/transitions/{name}",
               "--geometry-begin", str(begin), "--geometry-count", str(count),
               *binding] for name, begin, count in ranges]
    merge = [executable, "--merge-transitions", "--orientation", orientation,
             "--transition-prefix", f"work/transitions/{stem}"]
    for name, _, _ in ranges:
        merge.extend(["--shard", f"work/transitions/{name}"])
    merge.extend(["--expected-geometries", str(GEOMETRIES), *binding[2:]])
    solve_arguments = [
        "--orientation", orientation,
        "--transition-prefix", f"work/transitions/{stem}",
        "--input", f"tablebases/{filename}",
        "--lower-ghost-sidecar", "tablebases/kghostk.ufgm",
        "--scratch", f"work/solve/{stem}",
        "--output", f"work/results/{stem}.ufiw",
        "--output-arbitrary", f"work/results/{stem}.ufgp",
        *binding[2:],
        "--max-nodes", "500000000", "--unique-slots", str(1 << 30),
        "--compact-every", "1",
    ]
    transition = f"work/transitions/{stem}"
    cpp_sources = tuple(path for path in build_inputs
                        if path.startswith("src/") and path.endswith(".cpp")
                        and path not in TEXTUAL_CPP)
    artifacts = [
        *(f"{transition}.{suffix}" for suffix in
          ("header", "meta", "strata", "index", "blocks", "verified")),
        f"work/results/{stem}.ufiw", f"work/results/{stem}.ufgp",
        "work/logs/build.log", "work/logs/self-test.log",
        "work/logs/merge.log",
        *(f"work/logs/shard-{index:02d}.log" for index in range(SHARDS)),
        "work/logs/measure.log", "work/logs/solve.log",
    ]
    return {
        "schema": "ultimate-parasite-ghost-aws-v1",
        "filename": filename, "orientation": orientation,
        "source_sha256": row["source_sha256"], "model_sha256": model,
        "normalized_source_sha256": row["normalized_source_sha256"],
        "observation_sha256": observation,
        "lower_sidecar_sha256": LOWER_SHA256,
        "lower_parasite_full_sha256": LOWER_PARASITE_SHA256,
        "lower_parasite_source_sha256": LOWER_PARASITE_SHA256,
        "lower_parasite_model_sha256": LOWER_PARASITE_MODEL_SHA256,
        "lower_tracked_ghost_full_sha256": LOWER_TRACKED_GHOST_SHA256,
        "lower_tracked_ghost_source_sha256": LOWER_TRACKED_GHOST_SHA256,
        "lower_tracked_ghost_model_sha256":
            LOWER_TRACKED_GHOST_MODEL_SHA256,
        "lower_source_sha256": lower[0], "lower_model_sha256": lower[1],
        "lower_observation_sha256": lower[2],
        "geometries": GEOMETRIES, "shards": SHARDS,
        "shard_count_distribution": {
            str(LARGE_GEOMETRIES): LARGE_SHARDS,
            str(SMALL_GEOMETRIES): SHARDS - LARGE_SHARDS,
        },
        "parallelism": PARALLELISM,
        "estimated_transition_bytes": 16_000_000_000,
        "estimated_peak_scratch_bytes": 36 << 30,
        "estimated_peak_resident_bytes": 20 << 30,
        "files": [file_record(path) for path in build_inputs],
        "commands": {
            "build": ["clang++", "-std=c++17", "-O3", "-DNDEBUG",
                      "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                      "-Wno-error=range-loop-construct",
                      "-include", "sstream", "-Isrc/ultimate", "-Isrc/ultimate/tablebases",
                      *cpp_sources, "-o", executable[2:]],
            "self_test": [executable, "--orientation", orientation,
                          "--self-test", "--scratch", f"work/self-test/{stem}",
                          "--input", f"tablebases/{filename}",
                          "--source-sha256", str(row["source_sha256"])],
            "shards": shards, "merge": merge,
            "measure": [executable, "--measure", "1", *solve_arguments],
            "solve": [executable, "--solve", *solve_arguments],
        },
        "artifacts": artifacts,
    }


def add_bytes(archive: tarfile.TarFile, name: str, payload: bytes,
              mode: int = 0o644) -> None:
    record = tarfile.TarInfo(name)
    record.size = len(payload); record.mode = mode
    record.mtime = record.uid = record.gid = 0
    record.uname = record.gname = ""
    archive.addfile(record, io.BytesIO(payload))


def build_bundle(filename: str, output: Path) -> dict[str, object]:
    manifest = build_manifest(filename)
    output.parent.mkdir(parents=True, exist_ok=True)
    paths = [str(record["path"]) for record in manifest["files"]]
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        add_bytes(archive, "bundle-manifest.json",
                  (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode())
        for path in sorted(paths):
            mode = 0o755 if path.endswith("_aws.py") else 0o644
            add_bytes(archive, path, (ROOT / path).read_bytes(), mode)
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--filename", choices=tuple(ROWS), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = build_bundle(args.filename, args.output)
    print(json.dumps({"bundle": str(args.output),
      "bytes": args.output.stat().st_size,
      "sha256": sha256_bytes(args.output.read_bytes()),
      "model_sha256": manifest["model_sha256"],
      "shards": manifest["shards"]}, sort_keys=True))


if __name__ == "__main__":
    main()
