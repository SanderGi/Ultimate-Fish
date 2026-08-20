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
    "f8d5900989428261a17714a80010270e2d3a4011eb740c21ed8f32fead808a11")
# v6 changes only the read-only Bellman verifier's worker scheduling.  The
# exact source diff and a byte-identical v5/v6 table are authenticated by
# angel_graph_v6_dynamic_verifier_equivalence.json.  Keep the lower artifact's
# original model as provenance while accepting only these proven-compatible
# live generator models.
LOWER_PARASITE_COMPATIBLE_GENERATOR_MODELS = frozenset((
    LOWER_PARASITE_MODEL_SHA256,
    "c7bb5d8338aa13cc9e4879c4b87797479d981e94e461343d74ba602c66e2fc34",
    # The additional source branch is confined to opposed Copycat/Angel;
    # singleton Parasite generation is unchanged.
    "d012548c8c33ab66f4fcc6d7eb3ef1155949ef8ae90aef308fad501a985efcaa",
    # Completed-frontier parallel reverse scanning is scheduling-only and is
    # byte-proved by angel_parallel_resume_v7_equivalence.json.
    "672ebe9e5beaa8ee46b141985210455970838fdb397f0c636b6d1333fff1b68b",
    # Copycat/Angel v10 adds a disjoint linked-Copycat codec. Singleton
    # Parasite generation and the authenticated lower payload are unchanged.
    "ad30918c0cc50b89dfb805b923af77e46174ae623b7c21c6ed9a6baacc35c72c",
    # The follow-up v10 change only admits the already generated v10 tags in
    # the read-only reachability auditor. It cannot affect Parasite generation.
    "92abde0c59071711b4e3867e0301c24812945ca38c19c4e7cd63982a50620944",
    # Primary-Jester/Angel belief materialization and its exact observation
    # fallback are disjoint from singleton Parasite generation.
    "bd64d950e7d7e0d41b77e6114243194aefac76b9a3544dd3d46883c3de946ac4",
    # Adding the directly required <sstream> include is a Linux portability
    # fix and cannot change singleton Parasite generation semantics.
    "67d08f838b49c24e92c6b7eb36abbb3b765838abffe700a4f504a03c5a05dd4b",
    # The Angel/Jester next-decision observation fix is confined to the
    # primary-Jester information solver and cannot affect singleton Parasite
    # concrete generation.
    "1c1cbb7965eab53074b86caf1d5ca9067bce793be3c047ef5a952bd2cb7ec7a7",
))
LOWER_TRACKED_GHOST_SHA256 = (
    "32dc7889506f4dd4948dbf1bed3f31c6c3ebdd4a8600c4381c7a8c710b9ec671")
LOWER_TRACKED_GHOST_MODEL_SHA256 = (
    "017912d4dd089ae14aa16b0435aa16e6000bdc2f19d2934b0b60f120a5d6eecd")
LOWER_TRACKED_GHOST_COMPATIBLE_GENERATOR_MODELS = frozenset((
    LOWER_TRACKED_GHOST_MODEL_SHA256,
    "fb480f69ad325253405fc9a704dcd9d8536e4f9053376fdcff700fb4ff9f837f",
    # Runtime color-shape validation is specific to Copycat/Angel v9.
    "2608cfcf0a18091de44ce4d1c1265ca1e0e5e17c9385d55b5c75cc3292d4c094",
    "6a80bb9c0dd5a34d16986675d02cf346a1705cfce52f3709bb32dafce1ae3f9c",
    # The v10 probe change distinguishes intact mirrored Copycats from the
    # new arbitrary linked-pair lower table; tracked singleton Ghosts never
    # exercise either Copycat probe branch.
    "ede2e2ce2001904cf71bc405333610328d0c31a1726621f7f9446291f0ff791c",
    # The follow-up v10 change is confined to packed-header validation in the
    # read-only reachability auditor; tracked-Ghost generation is unchanged.
    "b3f399ac9c8dd34ed0d0078531d2888a512279422903495602270c38c36b5aac",
    # Primary-Jester/Angel belief materialization and its exact observation
    # fallback cannot affect the tracked singleton-Ghost codec.
    "673ccfb0138c687c3417ae30f12b2a9e5eac86fbf6e85a6cb2451f747f2297e4",
    # Adding the directly required <sstream> include is a Linux portability
    # fix and cannot change the tracked singleton-Ghost codec.
    "81f926554a3e54c1d2591e4120d86108fc13b715957a3ea029b6e0480a665b22",
    # The Angel/Jester next-decision observation fix is disjoint from tracked
    # singleton-Ghost generation and its packed codec.
    "da0fcd0d5adef24ce90c4803459ee52e58c340700b1295497409f697e62d10c4",
))
ROWS = {
    "kghostparasitek.uftb": {
        "orientation": "same",
        "source_sha256":
            "d551ba980ab7fb51c63713524e2bf23a31758cad7629835c8d4bb339976b4a58",
        "model_sha256":
            "ecccace6048729f3e7472b060d0e0d633e424ed1305bba24d78767c0d9d0a26c",
        "normalized_source_sha256":
            "54c8d0bf2fa36254f1e1c4b9efa507c9128a5dc0030555857726fbef6551dd44",
    },
    "kghostkparasite.uftb": {
        "orientation": "opposing",
        "source_sha256":
            "af888568f496353e4477d364f1652f5b2329b51820ab07a7d4b0c80bd362baf1",
        "model_sha256":
            "0b8ad6e96de88c9633f3e9233eb32d095eab4471f61122fe506c2382577d5530",
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
    if (sha256_bytes(lower_parasite_payload) != LOWER_PARASITE_SHA256 or
            information.concrete_tablebase_model_fingerprint(
                "kparasitek.uftb") not in
            LOWER_PARASITE_COMPATIBLE_GENERATOR_MODELS):
        raise RuntimeError("kparasitek lower concrete binding mismatch")
    if (sha256_bytes(lower_tracked_ghost_payload) !=
            LOWER_TRACKED_GHOST_SHA256 or
            information.tracked_ghost_tablebase_model_fingerprint() not in
            LOWER_TRACKED_GHOST_COMPATIBLE_GENERATOR_MODELS):
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
