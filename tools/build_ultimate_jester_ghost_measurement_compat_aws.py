#!/usr/bin/env python3
"""Build a source-authenticated loader-compatible Jester/Ghost AWS binary."""

from __future__ import annotations

import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from typing import Any


SCHEMA = "ultimate-jester-ghost-measurement-binary-compatibility-v1"
STATUS = "codec-loader-only-transition-semantics-unchanged"
OLD_BUNDLE_COMMIT = "b51f514087bacff46e9433cc684e07772d6110e8"
OLD_SOLVER_SHA256 = "06ba77bbdf0ad26481568d43332e55054ce4b592e13d1ca9eba4b0e4e27993ac"
NEW_SOLVER_SHA256 = "1fb64c1e2818c665f4b39885a87fbff625480cdd840813b01e7ad4cb6515dd36"
OLD_HEADER_SHA256 = "ea129d411069814320e569983e5af4a0929188090d1369ddb590e5531e07306e"
NEW_HEADER_SHA256 = "d615d818862ca46014a98b749ab4b41372b869c67ba15233a0b94fae13d3df42"
OLD_CLI_SHA256 = "eec4fa81506eaea0d89e883740f7b3dcfb81faebd74991fcb48c23e3a2a46578"
NEW_CLI_SHA256 = "ae63aadc4d8c13f7037d3a193a604f03a2fb38c2ed049037aae2832e76e83b33"
PATCH_SHA256 = "f265bf3d0cead42fbb47eb5672d12d8bd23891f9eea1ff2d5416a46536c44de0"
OLD_BINARY_SHA256 = "978fdf69362577c824d9e456a28a51d133cca4b6823fcab9e8e7555e2c11a147"
SOURCE_SHA256 = "ad82489372318a7561a43a7c2d0cbc1fdc3e5a840c67470c5eb25704fdfecf7e"
SEMANTIC_MODEL_SHA256 = "732852b8ddc43b8a1c66bb0f9fb5fd4c94aa9ad6faa5daeda31053f63c98f6db"
FULL_SOURCE_MODEL_SHA256 = "553a2b3f7b223d877c1d94ec9a87dd9c627393bd8a5ff4bf10e3fd2ace03086b"
OBSERVATION_SHA256 = "af09ebab834599de83d546f8729b8329dbe5ba8ff1cc7f24be3ac63086273adf"
SHA = re.compile(r"[0-9a-f]{64}\Z")
COMMIT = re.compile(r"[0-9a-f]{40}\Z")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text())
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected JSON object")
    return value


def loader_patch_sha(files: list[tuple[str, bytes, bytes]]) -> str:
    patch = b"".join("".join(difflib.unified_diff(
        old.decode().splitlines(True), new.decode().splitlines(True),
        fromfile=f"old/{name}", tofile=f"new/{name}")).encode()
        for name, old, new in files)
    return hashlib.sha256(patch).hexdigest()


def write_exclusive_json(path: Path, value: object) -> None:
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())


def run_logged(command: list[str], cwd: Path, log: Path) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("xb") as output:
        result = subprocess.run(command, cwd=cwd, stdout=output,
                                stderr=subprocess.STDOUT, check=False)
        output.flush()
        os.fsync(output.fileno())
    if result.returncode:
        raise RuntimeError(f"compatibility command failed; inspect {log}")


def build(args: argparse.Namespace) -> dict[str, Any]:
    if (not args.builder_source.is_file() or
            sha256_file(args.builder_source) != args.builder_sha256):
        raise ValueError("compatibility builder SHA-256 mismatch")
    if (not COMMIT.fullmatch(args.source_commit) or args.work.exists()):
        raise ValueError("invalid source commit or nonfresh build work")
    manifest = load_json(args.old_bundle_manifest)
    if (manifest.get("schema") != "ultimate-jester-ghost-aws-v2" or
            manifest.get("source_sha256") != SOURCE_SHA256 or
            manifest.get("model_sha256") != SEMANTIC_MODEL_SHA256 or
            manifest.get("observation_sha256") != OBSERVATION_SHA256):
        raise ValueError("old bundle semantic binding mismatch")
    records = manifest.get("files")
    if not isinstance(records, list) or not records:
        raise ValueError("old bundle file inventory missing")
    replacements = [
        ("src/ultimate/jester_ghost_information_solver.cpp",
         args.replacement_solver, OLD_SOLVER_SHA256, NEW_SOLVER_SHA256),
        ("src/ultimate/jester_ghost_information_solver.h",
         args.replacement_header, OLD_HEADER_SHA256, NEW_HEADER_SHA256),
        ("src/ultimate/jester_ghost_information_tablebase.cpp",
         args.replacement_cli, OLD_CLI_SHA256, NEW_CLI_SHA256),
    ]
    patches: list[tuple[str, bytes, bytes]] = []
    for relative, replacement, old_sha, new_sha in replacements:
        old = args.old_source_root / relative
        if (sha256_file(old) != old_sha or
                sha256_file(replacement) != new_sha):
            raise ValueError(f"loader-only source mismatch: {relative}")
        patches.append((relative, old.read_bytes(), replacement.read_bytes()))
    if loader_patch_sha(patches) != PATCH_SHA256:
        raise ValueError("loader-only source patch mismatch")
    if (not args.old_binary.is_file() or
            sha256_file(args.old_binary) != OLD_BINARY_SHA256):
        raise ValueError("old certified binary SHA-256 mismatch")

    args.work.mkdir(parents=True)
    source = args.work / "source"
    source.mkdir()
    authenticated = 0
    for record in records:
        relative = Path(str(record.get("path", "")))
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("unsafe old bundle file path")
        old = args.old_source_root / relative
        if (not old.is_file() or old.stat().st_size != record.get("bytes") or
                sha256_file(old) != record.get("sha256")):
            raise ValueError(f"old bundle file mismatch: {relative}")
        target = source / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(old, target)
        authenticated += 1
    for relative, replacement, _, new_sha in replacements:
        staged = source / relative
        shutil.copyfile(replacement, staged)
        if sha256_file(staged) != new_sha:
            raise ValueError(f"staged replacement mismatch: {relative}")

    command = list(manifest["commands"]["build"])
    binary = source / Path(command[-1]).name
    command[-1] = str(binary)
    run_logged(command, source, args.work / "logs/build.log")
    binary.chmod(0o555)
    selftest = args.work / "selftest"
    selftest.mkdir()
    run_logged([str(binary), "--self-test", "--scratch",
                str(selftest / "jg")], source,
               args.work / "logs/selftest.log")
    binary_sha = sha256_file(binary)
    certificate = {
        "schema": SCHEMA, "status": STATUS,
        "source_commit": args.source_commit,
        "old_bundle_commit": OLD_BUNDLE_COMMIT,
        "builder_sha256": args.builder_sha256,
        "old_bundle_manifest_sha256": sha256_file(args.old_bundle_manifest),
        "authenticated_bundle_files": authenticated,
        "old_solver_sha256": OLD_SOLVER_SHA256,
        "new_solver_sha256": NEW_SOLVER_SHA256,
        "old_header_sha256": OLD_HEADER_SHA256,
        "new_header_sha256": NEW_HEADER_SHA256,
        "old_cli_sha256": OLD_CLI_SHA256,
        "new_cli_sha256": NEW_CLI_SHA256,
        "loader_patch_sha256": PATCH_SHA256,
        "old_binary_sha256": OLD_BINARY_SHA256,
        "new_binary": {"path": str(binary.relative_to(args.work)),
                       "bytes": binary.stat().st_size,
                       "sha256": binary_sha},
        "source_sha256": SOURCE_SHA256,
        "semantic_transition_model_sha256": SEMANTIC_MODEL_SHA256,
        "full_source_model_sha256": FULL_SOURCE_MODEL_SHA256,
        "observation_sha256": OBSERVATION_SHA256,
        "build_log_sha256": sha256_file(args.work / "logs/build.log"),
        "selftest_log_sha256": sha256_file(args.work / "logs/selftest.log"),
        "residuals": {"bundle": 0, "loader_patch": 0, "build": 0,
                      "selftest": 0, "transition_semantics": 0},
    }
    write_exclusive_json(args.work / "compatibility-certificate.json",
                         certificate)
    return {"status": STATUS, "binary_sha256": binary_sha,
            "binary_bytes": binary.stat().st_size,
            "certificate_sha256": sha256_file(
                args.work / "compatibility-certificate.json")}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--builder-source", type=Path, required=True)
    parser.add_argument("--builder-sha256", required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--old-source-root", type=Path, required=True)
    parser.add_argument("--old-bundle-manifest", type=Path, required=True)
    parser.add_argument("--old-binary", type=Path, required=True)
    parser.add_argument("--replacement-solver", type=Path, required=True)
    parser.add_argument("--replacement-header", type=Path, required=True)
    parser.add_argument("--replacement-cli", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(build(args), sort_keys=True))


if __name__ == "__main__":
    main()
