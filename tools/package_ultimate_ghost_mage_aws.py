#!/usr/bin/env python3
"""Build deterministic AWS bundles for exact Mage/Ghost information."""

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
SHARDS = 64
LARGE_SHARDS = 32
LARGE_GEOMETRIES = 7_703
SMALL_GEOMETRIES = 7_702
PARALLELISM = 29
LOWER_SHA256 = (
    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
FROZEN_FINGERPRINTS = {
    "kbishopghostk.uftb":
        "d59736789155c52d9b697f6e3f05c2fd0c565beba5d5cce49e3648719289da91",
    "kbishopkghost.uftb":
        "a9889978e2c217035a90c31d51c294b5c368ed06bc0b592ea82174c36cec58cc",
    "kghostghostk.uftb":
        "2280445ed5c6f024b0cd8d00fca45dd48e5359cd591d4e3d28ee0259f74ff286",
    "kghostdragonk.uftb":
        "dfd0ce7f2a1127a7a15d62b8c8442b76d6e46c716a4d78fb3de6208ce6afdf5b",
    "kghostkdragon.uftb":
        "9432915f6a90cbad1cd5c2d16df0944de2fdc0c0d206dcc90dc0dda799953103",
    "kbombghostk.uftb":
        "87aeb201a1fe108f8787ea16671579b487e05597676fbdb6b914e73acb1aa68b",
    "kbombkghost.uftb":
        "f2afd099261931a66e034bc9de68b3ee35bf3cb2778d46bbefaaf1e0cdd6eda8",
    "kghostfishermank.uftb":
        "da5355677990551625781639d18804a9381f7dd8153f6a1a96804497f47acec7",
    "kghostkfisherman.uftb":
        "5a1d80a69d7df0fcf763d8bb5231c92c1399b5fb0b55ce5533147b21e3e5a9c4",
}
ROWS = {
    "kghostmagek.uftb": {
        "orientation": "same",
        "source_sha256":
            "061a250c66c18e769baec7410be2257db98e669cf6b3d6b39cd5153b27792596",
        "model_sha256":
            "21e04690c947749e1bcc044513f22fc66a6882688e066cd5a2b500c74f52d205",
        "normalized_source_sha256":
            "17d07b24bb3fd08a42ee3464257894b04554f3d82d25c171e334e01e831b0a58",
    },
    "kghostkmage.uftb": {
        "orientation": "opposing",
        "source_sha256":
            "d23e1c2253e1ff26822069f5852989c73c1ab308b3720ce4a5473fa29c881519",
        "model_sha256":
            "7071380483a55c200a6e33cb47819e234ef735070533b137483e16969ff2053c",
        "normalized_source_sha256":
            "05cd0d3ed092e1d30ed9572f0561444b92dc1acc10f3781c3cb1336ffe0da9b2",
    },
}
BUILD_INPUTS_COMMON = (
    "src/ultimate/ghost_mage_information_tablebase.cpp",
    "src/ultimate/ghost_mage_information_solver.cpp",
    "src/ultimate/ghost_mage_information_solver.h",
    "src/ultimate/ghost_public_extra_information_solver.cpp",
    "src/ultimate/ghost_public_extra_information_solver.h",
    "src/ultimate/ghost_public_extra_model.cpp",
    "src/ultimate/ghost_public_extra_model.h",
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
    "tablebases/kghostk.ufgm",
    "tools/run_ultimate_reciprocal_bishop_ghost_aws.py",
    "tools/run_ultimate_ghost_mage_aws.py",
)
TEXTUAL_CPP = {
    "src/ultimate/ghost_public_extra_information_solver.cpp",
    "src/ultimate/ghost_extra_information_tablebase.cpp",
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
        raise RuntimeError("balanced Mage/Ghost shard arithmetic residual")
    return ranges


def build_manifest(filename: str) -> dict[str, object]:
    if filename not in ROWS:
        raise RuntimeError(f"unsupported Mage/Ghost row {filename}")
    row = ROWS[filename]
    for frozen, expected in FROZEN_FINGERPRINTS.items():
        if information.solver_model_fingerprint(frozen) != expected:
            raise RuntimeError(f"frozen fingerprint changed: {frozen}")
    model = information.solver_model_fingerprint(filename)
    if model != row["model_sha256"]:
        raise RuntimeError(f"Mage/Ghost model fingerprint changed: {filename}")
    build_inputs = (*BUILD_INPUTS_COMMON, f"tablebases/{filename}")
    concrete = (ROOT / "tablebases" / filename).read_bytes()
    lower_payload = (ROOT / "tablebases" / "kghostk.ufgm").read_bytes()
    if sha256_bytes(concrete) != row["source_sha256"]:
        raise RuntimeError(f"{filename}: concrete SHA-256 mismatch")
    if sha256_bytes(lower_payload) != LOWER_SHA256:
        raise RuntimeError("kghostk lower UFGM SHA-256 mismatch")
    lower = lower_binding(lower_payload)
    observation = information.observation_model_fingerprint()
    stem = Path(filename).stem
    orientation = str(row["orientation"])
    executable = "./ultimate_ghost_mage_information_tablebase"
    binding = ["--orientation", orientation,
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
        "--output-arbitrary", f"work/results/{stem}.ufmg",
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
        f"work/results/{stem}.ufiw", f"work/results/{stem}.ufmg",
        "work/logs/build.log", "work/logs/self-test.log",
        "work/logs/merge.log",
        *(f"work/logs/shard-{index:02d}.log" for index in range(SHARDS)),
        "work/logs/measure.log", "work/logs/solve.log",
    ]
    return {
        "schema": "ultimate-mage-ghost-aws-v1",
        "filename": filename, "orientation": orientation,
        "source_sha256": row["source_sha256"], "model_sha256": model,
        "normalized_source_sha256": row["normalized_source_sha256"],
        "observation_sha256": observation,
        "lower_sidecar_sha256": LOWER_SHA256,
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
                      "-include", "sstream", "-Isrc/ultimate",
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
    print(json.dumps(manifest, indent=2, sort_keys=True))
    print(f"bundle_sha256 {sha256_bytes(args.output.read_bytes())}")


if __name__ == "__main__":
    main()
