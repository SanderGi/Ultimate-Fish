#!/usr/bin/env python3
"""Build deterministic AWS bundles for exact Bomb/Ghost information."""

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
    "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
LOWER_BOMB_SHA256 = (
    "18e057c83faf940db1ad7404a39623a5db208724de892d604735576d7583ce2f")
LOWER_BOMB_MODEL_SHA256 = (
    "8e066add3d323bde8b2bdff2ebc7f6620a2e8aeb6f74ab2e544ef04d99210174")
FROZEN_FINGERPRINTS = {
    "kbishopghostk.uftb":
        "416f3792dfa0b5f5317a0ee4f8ae91b2a4dd874807d7223c5918dc0b291a2dcf",
    "kbishopkghost.uftb":
        "83045f01b7ac65800e662fe68c19de48a2fda5bc4d1fd48cbffc6c2c6dc56238",
    "kghostghostk.uftb":
        "84807bb96f10a77240b2e0c0584e0739d33d5135241b9e1b567f682b67e085af",
    "kghostdragonk.uftb":
        "87caaabfef6f77742a02ca911ccb79001c69e7e17c0a9384598f715dfb9c6d6b",
    "kghostkdragon.uftb":
        "b08194315a690de73ec8555e6154c3e9369fcca8cd3f726f9a6ad9425f125e28",
}
ROWS = {
    "kbombghostk.uftb": {
        "orientation": "same",
        "source_sha256":
            "25fc8d85b05c1104b50118cc7486bd3cfb8bb853eeedbb748cc81d29b95fa86b",
        "model_sha256":
            "87aeb201a1fe108f8787ea16671579b487e05597676fbdb6b914e73acb1aa68b",
        "implementation_sha256":
            "0db1091648374e733347d237d7f79386e3bd057aae3426a91f4be832a8884c31",
        "normalized_source_sha256":
            "d922dd43d9a22a0ff788741742db11937f547a26ac3b3863b32f591cb1d8f1fc",
    },
    "kbombkghost.uftb": {
        "orientation": "opposing",
        "source_sha256":
            "29305aa38fce6a02b0ca1dca724388c02d6e8a4ec5002edc123248c59850bfd0",
        "model_sha256":
            "f2afd099261931a66e034bc9de68b3ee35bf3cb2778d46bbefaaf1e0cdd6eda8",
        "implementation_sha256":
            "b9cddf50b7c52b3927b734f3c5e893a882b953b702f4cedb36ea4f68a1031c8e",
        "normalized_source_sha256":
            "1e942d9386b6e26ad468242dcabfa489861915b76ba168429e791f761f4f11a9",
    },
}
BUILD_INPUTS_COMMON = (
    "src/ultimate/tablebases/ghost_bomb_information_tablebase.cpp",
    "src/ultimate/tablebases/ghost_bomb_information_solver.cpp",
    "src/ultimate/tablebases/ghost_bomb_information_solver.h",
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
    "tablebases/kbombk.uftb",
    "tools/tablebases/run_ultimate_reciprocal_bishop_ghost_aws.py",
    "tools/tablebases/run_ultimate_ghost_bomb_aws.py",
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
        raise RuntimeError("balanced Bomb/Ghost shard arithmetic residual")
    return ranges


def build_manifest(filename: str) -> dict[str, object]:
    if filename not in ROWS:
        raise RuntimeError(f"unsupported Bomb/Ghost row {filename}")
    row = ROWS[filename]
    for frozen, expected in FROZEN_FINGERPRINTS.items():
        if information.solver_model_fingerprint(frozen) != expected:
            raise RuntimeError(f"frozen fingerprint changed: {frozen}")
    implementation = information.solver_model_fingerprint(filename)
    if implementation != row["implementation_sha256"]:
        raise RuntimeError(
            f"Bomb/Ghost implementation fingerprint changed: {filename}")
    # The transition model is intentionally stable across a solve-only fix.
    # Its old verified shards remain authoritative; the implementation hash
    # and complete file inventory separately authenticate the patched solver.
    model = str(row["model_sha256"])
    build_inputs = (*BUILD_INPUTS_COMMON, f"tablebases/{filename}")
    concrete = (ROOT / "tablebases" / filename).read_bytes()
    lower_payload = (ROOT / "tablebases" / "kghostk.ufgm").read_bytes()
    lower_bomb_payload = (ROOT / "tablebases" / "kbombk.uftb").read_bytes()
    if sha256_bytes(concrete) != row["source_sha256"]:
        raise RuntimeError(f"{filename}: concrete SHA-256 mismatch")
    if sha256_bytes(lower_payload) != LOWER_SHA256:
        raise RuntimeError("kghostk lower UFGM SHA-256 mismatch")
    if (sha256_bytes(lower_bomb_payload) != LOWER_BOMB_SHA256 or
            information.concrete_tablebase_model_fingerprint(
                "kbombk.uftb") != LOWER_BOMB_MODEL_SHA256):
        raise RuntimeError("kbombk lower concrete binding mismatch")
    lower = lower_binding(lower_payload)
    observation = information.observation_model_fingerprint()
    stem = Path(filename).stem
    orientation = str(row["orientation"])
    executable = "./ultimate_ghost_bomb_information_tablebase"
    binding = ["--orientation", orientation,
               "--lower-bomb-table", "tablebases/kbombk.uftb",
               "--lower-bomb-sha256", LOWER_BOMB_SHA256,
               "--lower-bomb-source-sha256", LOWER_BOMB_SHA256,
               "--lower-bomb-model-sha256", LOWER_BOMB_MODEL_SHA256,
               "--source-sha256", str(row["source_sha256"]),
               "--model-sha256", model,
               "--observation-sha256", observation]
    ranges = shard_ranges()
    if ranges[-1][1] + ranges[-1][2] != GEOMETRIES:
        raise RuntimeError("Bomb/Ghost shards do not cover the domain")
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
        "--output-arbitrary", f"work/results/{stem}.ufgb",
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
        f"work/results/{stem}.ufiw", f"work/results/{stem}.ufgb",
        "work/logs/build.log", "work/logs/self-test.log",
        "work/logs/merge.log",
        *(f"work/logs/shard-{index:02d}.log" for index in range(SHARDS)),
        "work/logs/measure.log", "work/logs/solve.log",
    ]
    return {
        "schema": "ultimate-bomb-ghost-aws-v2",
        "filename": filename, "orientation": orientation,
        "source_sha256": row["source_sha256"], "model_sha256": model,
        "implementation_sha256": implementation,
        "normalized_source_sha256": row["normalized_source_sha256"],
        "observation_sha256": observation,
        "lower_sidecar_sha256": LOWER_SHA256,
        "lower_bomb_full_sha256": LOWER_BOMB_SHA256,
        "lower_bomb_source_sha256": LOWER_BOMB_SHA256,
        "lower_bomb_model_sha256": LOWER_BOMB_MODEL_SHA256,
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
