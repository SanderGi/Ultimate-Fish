#!/usr/bin/env python3
"""Build the committed exact crossed Jester/Ghost AWS source bundle."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
import ultimate_information_tablebases as information  # noqa: E402


SCHEMA = "ultimate-crossed-jester-ghost-aws-v2"
SEMANTICS = "crossed-jester-ghost-perfect-recall-dense-and-arbitrary-v2"
SOURCE_SHA256 = "d9b75b6aa4d4cad713206e18f12efb1c4781adea0cc5f99a4a77fd58eca92cc3"
LOWER_JESTER_TABLE_SHA256 = "3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa"
LOWER_JESTER_OVERLAY_SHA256 = "ab806963760bcd52163d6acfbb7d5a0c6f2b2dcc8a616a61c16ae060a2d412ee"
LOWER_JESTER_MODEL_SHA256 = "0ed6d361e313623234c21f9a1c800947014ce47320b4a71fca4fb20a255587c2"
LOWER_GHOST_SIDECAR_SHA256 = "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b"
LOWER_GHOST_SOURCE_SHA256 = "3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31"
LOWER_GHOST_MODEL_SHA256 = "4a2d9d7b503b29204cf9af08985345771b9046c07bd2116e592fab40ee12e430"
LOWER_GHOST_OBSERVATION_SHA256 = "af09ebab834599de83d546f8729b8329dbe5ba8ff1cc7f24be3ac63086273adf"

MODEL_SOURCES = (
    "src/ultimate/crossed_jester_ghost_information_model.h",
    "src/ultimate/crossed_jester_ghost_information_model.cpp",
    "src/ultimate/crossed_jester_ghost_information_solver.h",
    "src/ultimate/crossed_jester_ghost_information_solver.cpp",
    "src/ultimate/crossed_jester_ghost_information_fixed_point.h",
    "src/ultimate/crossed_jester_ghost_information_fixed_point.cpp",
    "src/ultimate/crossed_jester_ghost_information_lower_oracle.h",
    "src/ultimate/crossed_jester_ghost_information_lower_oracle.cpp",
    "src/ultimate/crossed_jester_ghost_information_sidecar.h",
    "src/ultimate/crossed_jester_ghost_information_sidecar.cpp",
    "src/ultimate/crossed_jester_ghost_information_tablebase.cpp",
    "src/ultimate/information_solver.h",
    "src/ultimate/information_solver.cpp",
    "src/ultimate/information.h",
    "src/ultimate/information.cpp",
    "src/ultimate/position.h",
    "src/ultimate/position.cpp",
    "src/ultimate/nnue.h",
    "src/ultimate/nnue.cpp",
)
CLI_SOURCES = tuple(path for path in MODEL_SOURCES if path.endswith(".cpp"))
MODEL_TEST_SOURCES = (
    "tests/ultimate_crossed_jester_ghost_information_model.cpp",
    "src/ultimate/crossed_jester_ghost_information_model.cpp",
    "src/ultimate/position.cpp", "src/ultimate/information.cpp",
    "src/ultimate/nnue.cpp",
)
SOLVER_TEST_SOURCES = (
    "tests/ultimate_crossed_jester_ghost_information_solver.cpp",
    *tuple(path for path in CLI_SOURCES
           if not path.endswith("information_tablebase.cpp")),
)
BUNDLE_FILES = (
    *MODEL_SOURCES,
    "tests/ultimate_crossed_jester_ghost_information_model.cpp",
    "tests/ultimate_crossed_jester_ghost_information_solver.cpp",
    "tablebases/kjesterkghost.uftb",
    "tablebases/kjesterk.uftb",
    "tablebases/kghostk.ufgm",
    "tools/package_ultimate_crossed_jester_ghost_aws.py",
    "tools/run_ultimate_crossed_jester_ghost_aws.py",
    "tools/ultimate_information_tablebases.py",
)


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def model_fingerprint(repo: Path = ROOT) -> str:
    digest = hashlib.sha256()
    contract = json.dumps(
        {"domain": "solver-model:crossed-jester-ghost",
         "semantics": SEMANTICS}, sort_keys=True,
        separators=(",", ":")).encode()
    digest.update(len(contract).to_bytes(8, "little"))
    digest.update(contract)
    for relative in MODEL_SOURCES:
        name = relative.encode()
        payload = (repo / relative).read_bytes()
        digest.update(len(name).to_bytes(4, "little"))
        digest.update(name)
        digest.update(len(payload).to_bytes(8, "little"))
        digest.update(payload)
    return digest.hexdigest()


def committed_source(repo: Path, relative: str, commit: str) -> bytes:
    return subprocess.check_output(
        ["git", "show", f"{commit}:{relative}"], cwd=repo)


def require_committed_files(repo: Path, files: tuple[str, ...]) -> str:
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip()
    for relative in files:
        if committed_source(repo, relative, commit) != (repo / relative).read_bytes():
            raise RuntimeError(
                f"crossed AWS bundle input is not committed: {relative}")
    return commit


def command(sources: tuple[str, ...], output: str, optimization: str) -> list[str]:
    return [
        "clang++", "-std=c++17", optimization, "-DNDEBUG", "-Wall",
        "-Wextra", "-Wpedantic", "-Werror", "-Wno-error=pedantic",
        "-Wno-error=range-loop-construct", "-Isrc/ultimate", *sources,
        "-o", output,
    ]


def build_manifest(repo: Path = ROOT, *, require_committed: bool = True) -> dict[str, object]:
    expected = {
        "tablebases/kjesterkghost.uftb": SOURCE_SHA256,
        "tablebases/kjesterk.uftb": LOWER_JESTER_TABLE_SHA256,
        "tablebases/kghostk.ufgm": LOWER_GHOST_SIDECAR_SHA256,
    }
    for relative, digest in expected.items():
        if sha256_path(repo / relative) != digest:
            raise RuntimeError(f"crossed AWS dependency drift: {relative}")
    if information.observation_model_fingerprint() != LOWER_GHOST_OBSERVATION_SHA256:
        raise RuntimeError("crossed observation model drift")
    commit = require_committed_files(repo, BUNDLE_FILES) if require_committed else None
    files = []
    for relative in BUNDLE_FILES:
        payload = (repo / relative).read_bytes()
        files.append({"path": relative, "bytes": len(payload),
                      "sha256": sha256_bytes(payload)})
    return {
        "schema": SCHEMA,
        "semantics": SEMANTICS,
        "source_commit": commit,
        "source_sha256": SOURCE_SHA256,
        "model_sha256": model_fingerprint(repo),
        "observation_sha256": information.observation_model_fingerprint(),
        "lower_jester_table_sha256": LOWER_JESTER_TABLE_SHA256,
        "lower_jester_overlay_sha256": LOWER_JESTER_OVERLAY_SHA256,
        "lower_jester_model_sha256": LOWER_JESTER_MODEL_SHA256,
        "lower_ghost_sidecar_sha256": LOWER_GHOST_SIDECAR_SHA256,
        "lower_ghost_source_sha256": LOWER_GHOST_SOURCE_SHA256,
        "lower_ghost_model_sha256": LOWER_GHOST_MODEL_SHA256,
        "lower_ghost_observation_sha256": LOWER_GHOST_OBSERVATION_SHA256,
        "raw_public_frames": 38_450_880,
        "default_mode": "measurement-only",
        "files": files,
        "commands": {
            "build": command(CLI_SOURCES,
                             "ultimate_crossed_jester_ghost_information_tablebase", "-O3"),
            "model_test": command(MODEL_TEST_SOURCES,
                                  "ultimate_crossed_jester_ghost_information_model_test", "-O2"),
            "solver_test": command(SOLVER_TEST_SOURCES,
                                   "ultimate_crossed_jester_ghost_information_solver_test", "-O2"),
        },
    }


def canonical_info(name: str, payload: bytes, mode: int) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = len(payload)
    info.mode = mode
    info.mtime = info.uid = info.gid = 0
    info.uname = info.gname = ""
    return info


def build_bundle(output: Path, repo: Path = ROOT, *,
                 require_committed: bool = True) -> dict[str, object]:
    manifest = build_manifest(repo, require_committed=require_committed)
    members = {
        relative: ((repo / relative).read_bytes(),
                   0o755 if relative.startswith("tools/run_") else 0o644)
        for relative in BUNDLE_FILES
    }
    members["bundle-manifest.json"] = (
        (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode(), 0o644)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        for name in sorted(members):
            payload, mode = members[name]
            archive.addfile(canonical_info(name, payload, mode), io.BytesIO(payload))
    return manifest


def verify_bundle(path: Path) -> str:
    with tarfile.open(path, "r") as archive:
        members = archive.getmembers()
        names = [member.name for member in members]
        if names != sorted(names) or len(names) != len(set(names)):
            raise RuntimeError("crossed bundle index is not sorted/unique")
        manifest_stream = archive.extractfile("bundle-manifest.json")
        if manifest_stream is None:
            raise RuntimeError("crossed bundle manifest is missing")
        manifest = json.loads(manifest_stream.read())
        if manifest.get("schema") != SCHEMA or not manifest.get("source_commit"):
            raise RuntimeError("crossed bundle is not committed/source-bound")
        records = {record["path"]: record for record in manifest["files"]}
        if set(records) != set(names) - {"bundle-manifest.json"}:
            raise RuntimeError("crossed bundle membership residual")
        for name, record in records.items():
            stream = archive.extractfile(name)
            if stream is None:
                raise RuntimeError("crossed bundle member is missing")
            payload = stream.read()
            if len(payload) != record["bytes"] or sha256_bytes(payload) != record["sha256"]:
                raise RuntimeError("crossed bundle member hash residual")
    return sha256_path(path)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repo", type=Path, default=ROOT)
    args = parser.parse_args()
    manifest = build_bundle(args.output, args.repo.resolve())
    print(json.dumps({"bundle": str(args.output),
                      "bytes": args.output.stat().st_size,
                      "sha256": verify_bundle(args.output),
                      "source_commit": manifest["source_commit"],
                      "model_sha256": manifest["model_sha256"]},
                     sort_keys=True))


if __name__ == "__main__":
    main()
