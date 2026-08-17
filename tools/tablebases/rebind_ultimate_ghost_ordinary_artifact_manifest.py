#!/usr/bin/env python3
"""Rebind an ordinary Ghost artifact manifest from authenticated file headers.

Older AWS runners passed observation and lower-Ghost bindings to the exact
solver but omitted them from ``artifact-manifest.json``.  This utility verifies
every allowlisted artifact, authenticates the UFGD1 and UFGM1 headers, preserves
the original manifest as evidence, and atomically writes the missing bindings.
It never changes a result, log, transition graph, or solve scratch file.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import struct


MANIFEST_SCHEMA = "ultimate-ordinary-ghost-artifacts-v1"
REBIND_SCHEMA = "ultimate-ordinary-ghost-manifest-rebind-v1"


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def read_exact(path: Path, offset: int, extent: int) -> bytes:
    with path.open("rb") as stream:
        stream.seek(offset)
        result = stream.read(extent)
    if len(result) != extent:
        raise RuntimeError(f"short authenticated header in {path}")
    return result


def digest_text(path: Path, offset: int) -> str:
    raw = read_exact(path, offset, 64)
    try:
        value = raw.decode("ascii")
    except UnicodeDecodeError as error:
        raise RuntimeError(f"non-ASCII SHA-256 in {path}") from error
    if len(value) != 64 or any(character not in "0123456789abcdef"
                               for character in value):
        raise RuntimeError(f"invalid SHA-256 in {path}")
    return value


def arbitrary_bindings(path: Path) -> dict[str, str]:
    header = read_exact(path, 0, 1248)
    if header[:8] != b"UFGD1\0\0\0":
        raise RuntimeError("ordinary Ghost arbitrary sidecar is not UFGD1")
    version, header_bytes = struct.unpack_from("<II", header, 8)
    if version != 1 or header_bytes != 1248:
        raise RuntimeError("ordinary Ghost arbitrary header contract residual")
    return {
        "source_sha256": digest_text(path, 160),
        "normalized_source_sha256": digest_text(path, 224),
        "model_sha256": digest_text(path, 288),
        "observation_sha256": digest_text(path, 352),
        "lower_ghost_sidecar_sha256": digest_text(path, 416),
        "lower_sha256": digest_text(path, 480),
        "lower_source_sha256": digest_text(path, 544),
        "lower_model_sha256": digest_text(path, 608),
    }


def lower_ghost_bindings(path: Path) -> dict[str, str]:
    header = read_exact(path, 0, 320)
    if header[:8] != b"UFGM1\0\0\0":
        raise RuntimeError("lower Ghost sidecar is not UFGM1")
    version, header_bytes = struct.unpack_from("<II", header, 8)
    if version != 1 or header_bytes != 320:
        raise RuntimeError("lower Ghost header contract residual")
    return {
        "lower_ghost_sidecar_sha256": sha256_path(path),
        "lower_ghost_source_sha256": digest_text(path, 96),
        "lower_ghost_model_sha256": digest_text(path, 160),
        "lower_ghost_observation_sha256": digest_text(path, 224),
    }


def verify_inventory(root: Path, manifest: dict[str, object]) -> None:
    files = manifest.get("files")
    if not isinstance(files, dict) or not files:
        raise RuntimeError("artifact manifest has no file inventory")
    resolved_root = root.resolve(strict=True)
    for name, record in files.items():
        if not isinstance(name, str) or not isinstance(record, dict):
            raise RuntimeError("malformed artifact inventory record")
        relative = PurePosixPath(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise RuntimeError(f"unsafe artifact path: {name}")
        path = root.joinpath(*relative.parts)
        if path.is_symlink() or not path.is_file():
            raise RuntimeError(f"artifact is missing: {name}")
        try:
            path.resolve(strict=True).relative_to(resolved_root)
        except ValueError as error:
            raise RuntimeError(f"artifact escapes root: {name}") from error
        if (path.stat().st_size != int(record.get("bytes", -1)) or
                sha256_path(path) != record.get("sha256")):
            raise RuntimeError(f"artifact extent/full-SHA residual: {name}")


def require_equal(label: str, left: object, right: object) -> None:
    if left != right:
        raise RuntimeError(f"{label} binding mismatch: {left!r} != {right!r}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--arbitrary", type=Path, required=True)
    parser.add_argument("--lower-ghost-sidecar", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    if manifest.get("schema") != MANIFEST_SCHEMA:
        raise RuntimeError("ordinary Ghost artifact manifest schema mismatch")
    verify_inventory(args.root, manifest)
    arbitrary = arbitrary_bindings(args.arbitrary)
    lower = lower_ghost_bindings(args.lower_ghost_sidecar)

    for key in ("source_sha256", "model_sha256", "lower_sha256",
                "lower_model_sha256"):
        require_equal(key, manifest.get(key), arbitrary[key])
    require_equal("lower source", arbitrary["lower_sha256"],
                  arbitrary["lower_source_sha256"])
    require_equal("lower Ghost sidecar", arbitrary["lower_ghost_sidecar_sha256"],
                  lower["lower_ghost_sidecar_sha256"])
    for key, value in {**arbitrary, **lower}.items():
        if key in manifest:
            require_equal(key, manifest[key], value)

    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    if args.evidence.exists():
        raise RuntimeError(f"evidence destination already exists: {args.evidence}")
    shutil.copy2(args.manifest, args.evidence)

    manifest.update(arbitrary)
    manifest.update(lower)
    manifest["manifest_rebind"] = {
        "schema": REBIND_SCHEMA,
        "original_manifest_sha256": sha256_path(args.evidence),
        "evidence": str(args.evidence),
        "artifact_inventory_residual": 0,
        "arbitrary_header_residual": 0,
        "lower_ghost_header_residual": 0,
    }
    temporary = args.manifest.with_name(args.manifest.name + ".rebind.tmp")
    temporary.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                         encoding="utf-8")
    os.replace(temporary, args.manifest)
    print(json.dumps({"schema": REBIND_SCHEMA,
                      "manifest_sha256": sha256_path(args.manifest),
                      "original_manifest_sha256": sha256_path(args.evidence),
                      "bindings": {**arbitrary, **lower}}, sort_keys=True))


if __name__ == "__main__":
    main()
