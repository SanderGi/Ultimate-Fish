#!/usr/bin/env python3
"""Build deterministic AWS bundles for exact Giant/Ghost information."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile

import ultimate_information_tablebases as information


ROOT = Path(__file__).resolve().parents[2]
GEOMETRIES = 359_100
SHARDS = 64
LARGE_SHARDS = 60
LARGE_GEOMETRIES = 5_611
SMALL_GEOMETRIES = 5_610
PARALLELISM = 29
LOWER_SHA256 = (
    "472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb")
LOWER_GIANT_SHA256 = (
    "eb52f2c08cf88e1e3682d0c72dfde191d9009e79779ad7eee23ca82fdcade591")
LOWER_GIANT_MODEL_SHA256 = (
    "89df7874ea2bb5aab359acc001db24746a84e47b4c9783965e883467c724666b")
# The second fingerprint differs only in the byte-equivalence-certified,
# read-only v6 verifier scheduler.  The lower artifact remains bound to its
# original generator model below.
LOWER_GIANT_COMPATIBLE_GENERATOR_MODELS = frozenset((
    LOWER_GIANT_MODEL_SHA256,
    "6da5ef2aa28a423b7e4271992ba952719baeddf54a0b9136c52c531fd1d68147",
    # Opposed Copycat/Angel-only source expansion; Giant is unaffected.
    "f7de219cb28f8cee34a292ffe0b2ebba76f74814f6f75b2ff4ea4a6c81280e8e",
    # Scheduling-only completed-frontier parallel replay; byte-equivalent.
    "2aeb7f7c23103e16d0e123e075b37c673e7b14d75463a8bfb0735c52ac02c3fa",
))
ROWS = {
    "kghostgiantk.uftb": {
        "orientation": "same",
        "source_sha256":
            "cd18051587a58abc21c84828ca05ff901967b8e42a6c4f694ba3066fab657b7d",
        "model_sha256":
            "1ba07ab7e6db45810baed56f159a09446665cdbfeaf44c28aa3b24efaaeb54a0",
        "normalized_source_sha256":
            "e67b2a61d44ce03885fba9817bf8628e2ae6d33bd097ffe7d472a80f324bf8e8",
    },
    "kghostkgiant.uftb": {
        "orientation": "opposing",
        "source_sha256":
            "aecce77c512fb3427cbb00b147650b4f95777e41b325e024d2720f7fcf2869cd",
        "model_sha256":
            "92c27f314648eea3e3e0536011b4fa4604a2558855485f9ac099be76b330587f",
        "normalized_source_sha256":
            "153045d7db5a0b1a7b64d52755411e00ad2425711ebe2d90a1b1d23614de487e",
    },
}
BUILD_INPUTS_COMMON = (
    "src/ultimate/tablebases/ghost_giant_information_tablebase.cpp",
    "src/ultimate/tablebases/ghost_giant_information_solver.cpp",
    "src/ultimate/tablebases/ghost_giant_information_solver.h",
    "src/ultimate/tablebases/ghost_giant_information_model.cpp",
    "src/ultimate/tablebases/ghost_giant_information_model.h",
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
    "tablebases/kgiantk.uftb",
    "tools/tablebases/run_ultimate_reciprocal_bishop_ghost_aws.py",
    "tools/tablebases/run_ultimate_ghost_giant_aws.py",
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
        raise RuntimeError("balanced Giant/Ghost shard arithmetic residual")
    return ranges


def build_manifest(filename: str) -> dict[str, object]:
    if filename not in ROWS:
        raise RuntimeError(f"unsupported Giant/Ghost row {filename}")
    row = ROWS[filename]
    model = information.solver_model_fingerprint(filename)
    if model != row["model_sha256"]:
        raise RuntimeError(f"Giant/Ghost model fingerprint changed: {filename}")
    build_inputs = (*BUILD_INPUTS_COMMON, f"tablebases/{filename}")
    concrete = (ROOT / "tablebases" / filename).read_bytes()
    lower_payload = (ROOT / "tablebases" / "kghostk.ufgm").read_bytes()
    lower_giant = (ROOT / "tablebases" / "kgiantk.uftb").read_bytes()
    if sha256_bytes(concrete) != row["source_sha256"]:
        raise RuntimeError(f"{filename}: concrete SHA-256 mismatch")
    if sha256_bytes(lower_payload) != LOWER_SHA256:
        raise RuntimeError("kghostk lower UFGM SHA-256 mismatch")
    if (sha256_bytes(lower_giant) != LOWER_GIANT_SHA256 or
            information.concrete_tablebase_model_fingerprint(
                "kgiantk.uftb") not in
            LOWER_GIANT_COMPATIBLE_GENERATOR_MODELS):
        raise RuntimeError("kgiantk lower concrete binding mismatch")
    lower = lower_binding(lower_payload)
    observation = information.observation_model_fingerprint()
    stem = Path(filename).stem
    orientation = str(row["orientation"])
    executable = "./ultimate_ghost_giant_information_tablebase"
    binding = ["--orientation", orientation,
               "--lower-giant-table", "tablebases/kgiantk.uftb",
               "--lower-giant-sha256", LOWER_GIANT_SHA256,
               "--lower-giant-source-sha256", LOWER_GIANT_SHA256,
               "--lower-giant-model-sha256", LOWER_GIANT_MODEL_SHA256,
               "--source-sha256", str(row["source_sha256"]),
               "--model-sha256", model,
               "--observation-sha256", observation]
    ranges = shard_ranges()
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
        "--output-arbitrary", f"work/results/{stem}.ufgi",
        *binding[2:], "--lower-sidecar-sha256", LOWER_SHA256,
        "--lower-source-sha256", lower[0],
        "--lower-model-sha256", lower[1],
        "--lower-observation-sha256", lower[2],
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
        f"work/results/{stem}.ufiw", f"work/results/{stem}.ufgi",
        "work/logs/build.log", "work/logs/self-test.log",
        "work/logs/merge.log",
        *(f"work/logs/shard-{index:02d}.log" for index in range(SHARDS)),
        "work/logs/solve.log",
    ]
    return {
        "schema": "ultimate-giant-ghost-aws-v1",
        "filename": filename, "orientation": orientation,
        "source_sha256": row["source_sha256"], "model_sha256": model,
        "normalized_source_sha256": row["normalized_source_sha256"],
        "observation_sha256": observation,
        "lower_sidecar_sha256": LOWER_SHA256,
        "lower_giant_full_sha256": LOWER_GIANT_SHA256,
        "lower_giant_source_sha256": LOWER_GIANT_SHA256,
        "lower_giant_model_sha256": LOWER_GIANT_MODEL_SHA256,
        "lower_source_sha256": lower[0], "lower_model_sha256": lower[1],
        "lower_observation_sha256": lower[2],
        "geometries": GEOMETRIES, "shards": SHARDS,
        "shard_count_distribution": {
            str(LARGE_GEOMETRIES): LARGE_SHARDS,
            str(SMALL_GEOMETRIES): SHARDS - LARGE_SHARDS,
        },
        "parallelism": PARALLELISM,
        "estimated_transition_bytes": 13_000_000_000,
        "estimated_peak_scratch_bytes": 32 << 30,
        "estimated_peak_resident_bytes": 18 << 30,
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
                      "sha256": sha256_bytes(args.output.read_bytes()),
                      "manifest": manifest}, sort_keys=True))


if __name__ == "__main__":
    main()
