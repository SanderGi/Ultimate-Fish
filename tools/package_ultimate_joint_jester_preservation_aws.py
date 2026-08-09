#!/usr/bin/env python3
"""Package the isolated exact joint-Jester preservation recomputation.

The lower K+Jester information overlay and the completed ordinary joint-Jester
overlay are deliberately external, immutable inputs.  The AWS runner binds
their full SHA-256 values at run time.  This keeps this deterministic source
bundle independent of a live proof while making a stale dependency fail
closed before measurement or capture.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import sys
import tarfile

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))
import ultimate_information_tablebases as information


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "ultimate-joint-jester-preservation-aws-v1"
MODEL_DOMAIN = "joint-jester-arbitrary-history-capture-v1"
SEMANTICS = "fresh-maximal-public-view-v2:joint-jester-arbitrary-history-d2-v1"
SOURCE_SHA256 = "243f0eaaf838774684082dadfeb69496b200e0061514239a83ccb771b879deee"
LOWER_TABLE_SHA256 = "3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa"
ORIGINAL_MODEL_SHA256 = "3885049bcc33fbd8e9fcd09a8b4f087af5014f5d2150a2644d2ee94b2d75febf"
LOWER_MODEL_SHA256 = "0ed6d361e313623234c21f9a1c800947014ce47320b4a71fca4fb20a255587c2"

MODEL_SOURCES = (
    "src/ultimate/joint_jester_information_preserver.cpp",
    "src/ultimate/joint_jester_information_tablebase.cpp",
    "src/ultimate/information_solver.cpp",
    "src/ultimate/information_solver.h",
    "src/ultimate/information.cpp",
    "src/ultimate/information.h",
    "src/ultimate/position.cpp",
    "src/ultimate/position.h",
    "src/ultimate/nnue.cpp",
    "src/ultimate/nnue.h",
)
BUILD_SOURCES = (
    "src/ultimate/joint_jester_information_preserver.cpp",
    "src/ultimate/information_solver.cpp",
    "src/ultimate/information.cpp",
    "src/ultimate/position.cpp",
    "src/ultimate/nnue.cpp",
)
BUNDLE_FILES = (
    *MODEL_SOURCES,
    "tablebases/kjesterkjester.uftb",
    "tablebases/kjesterk.uftb",
    "tools/package_ultimate_joint_jester_preservation_aws.py",
    "tools/run_ultimate_joint_jester_preservation_aws.py",
    "tools/ultimate_information_tablebases.py",
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def capture_model_fingerprint(repo: Path = ROOT) -> str:
    digest = hashlib.sha256()
    contract = json.dumps(
        {"domain": MODEL_DOMAIN, "schema": SCHEMA, "semantics": SEMANTICS},
        sort_keys=True, separators=(",", ":"),
    ).encode("utf-8")
    digest.update(len(contract).to_bytes(8, "little"))
    digest.update(contract)
    for relative in MODEL_SOURCES:
        name = relative.encode("utf-8")
        payload = (repo / relative).read_bytes()
        digest.update(len(name).to_bytes(4, "little"))
        digest.update(name)
        digest.update(len(payload).to_bytes(8, "little"))
        digest.update(payload)
    return digest.hexdigest()


def canonical_info(name: str, data: bytes, mode: int = 0o644) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = len(data)
    info.mode = mode
    info.mtime = info.uid = info.gid = 0
    info.uname = info.gname = ""
    return info


def build_manifest(repo: Path = ROOT) -> dict[str, object]:
    if sha256_path(repo / "tablebases/kjesterkjester.uftb") != SOURCE_SHA256:
        raise ValueError("joint-Jester concrete source SHA-256 drift")
    if sha256_path(repo / "tablebases/kjesterk.uftb") != LOWER_TABLE_SHA256:
        raise ValueError("lower K+Jester concrete SHA-256 drift")
    if information.solver_model_fingerprint("kjesterkjester.uftb") != ORIGINAL_MODEL_SHA256:
        raise ValueError("ordinary joint-Jester model fingerprint drift")
    if information.solver_model_fingerprint("kjesterk.uftb") != LOWER_MODEL_SHA256:
        raise ValueError("lower K+Jester model fingerprint drift")
    files = []
    for relative in BUNDLE_FILES:
        path = repo / relative
        payload = path.read_bytes()
        files.append({"path": relative, "bytes": len(payload),
                      "sha256": sha256_bytes(payload)})
    return {
        "schema": SCHEMA,
        "semantics": SEMANTICS,
        "source_sha256": SOURCE_SHA256,
        "original_model_sha256": ORIGINAL_MODEL_SHA256,
        "capture_model_sha256": capture_model_fingerprint(repo),
        "observation_sha256": information.observation_model_fingerprint(),
        "lower_table_sha256": LOWER_TABLE_SHA256,
        "lower_model_sha256": LOWER_MODEL_SHA256,
        "files": files,
        "build": [
            "clang++", "-std=c++17", "-O3", "-DNDEBUG", "-Wall",
            "-Wextra", "-Wpedantic", "-Werror",
            "-Wno-error=range-loop-construct", "-Isrc/ultimate",
            *BUILD_SOURCES, "-o", "ultimate_joint_jester_preserver",
        ],
        "default_mode": "measure-only",
        "full_requires": [
            "--full", "--lower-overlay", "--expected-overlay",
            "--scratch-limit", "--resident-limit",
        ],
    }


def build_bundle(output: Path, repo: Path = ROOT) -> dict[str, object]:
    manifest = build_manifest(repo)
    members: dict[str, tuple[bytes, int]] = {}
    for relative in BUNDLE_FILES:
        mode = 0o755 if relative.startswith("tools/run_") else 0o644
        members[relative] = ((repo / relative).read_bytes(), mode)
    members["bundle-manifest.json"] = (
        (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode(), 0o644)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as raw:
        with tarfile.open(fileobj=raw, mode="w", format=tarfile.PAX_FORMAT) as archive:
            for name in sorted(members):
                payload, mode = members[name]
                archive.addfile(canonical_info(name, payload, mode), io.BytesIO(payload))
    return manifest


def verify_bundle(path: Path) -> str:
    with tarfile.open(path, "r") as archive:
        members = archive.getmembers()
        names = [member.name for member in members]
        if names != sorted(names) or len(names) != len(set(names)):
            raise ValueError("joint preservation bundle index is not sorted/unique")
        for member in members:
            if (not member.isfile() or member.mtime or member.uid or member.gid or
                    member.mode not in (0o644, 0o755)):
                raise ValueError("joint preservation bundle metadata is not canonical")
        manifest_file = archive.extractfile("bundle-manifest.json")
        if manifest_file is None:
            raise ValueError("joint preservation bundle lacks its manifest")
        manifest = json.loads(manifest_file.read())
        if manifest.get("schema") != SCHEMA:
            raise ValueError("joint preservation bundle schema mismatch")
        by_name = {entry["path"]: entry for entry in manifest["files"]}
        if set(by_name) != set(names) - {"bundle-manifest.json"}:
            raise ValueError("joint preservation bundle membership residual")
        for name, record in by_name.items():
            stream = archive.extractfile(name)
            if stream is None:
                raise ValueError("joint preservation bundle member missing")
            payload = stream.read()
            if len(payload) != record["bytes"] or sha256_bytes(payload) != record["sha256"]:
                raise ValueError("joint preservation bundle file residual")
    return sha256_path(path)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--verify", type=Path)
    parser.add_argument("--repo", type=Path, default=ROOT)
    args = parser.parse_args()
    if bool(args.output) == bool(args.verify):
        parser.error("choose exactly one of --output or --verify")
    if args.verify:
        print(json.dumps({"bundle": str(args.verify),
                          "sha256": verify_bundle(args.verify)}, sort_keys=True))
        return
    manifest = build_bundle(args.output, args.repo.resolve())
    digest = verify_bundle(args.output)
    print(json.dumps({"bundle": str(args.output),
                      "bytes": args.output.stat().st_size,
                      "sha256": digest,
                      "capture_model_sha256": manifest["capture_model_sha256"]},
                     sort_keys=True))


if __name__ == "__main__":
    main()
