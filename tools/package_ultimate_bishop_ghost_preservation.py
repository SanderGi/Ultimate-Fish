#!/usr/bin/env python3
"""Build a deterministic archive for a certified Bishop/Ghost UFGX2 result.

The archive contains the compact arbitrary-belief sidecar, its proof manifest,
and the exact standalone loader/exporter sources needed to reconstruct and
strictly verify it from the separately versioned transition/source/lower
dependencies named by that manifest.  It deliberately does not inspect or
copy live solver scratch.
"""

from __future__ import annotations

import argparse
import hashlib
import io
from pathlib import Path
import tarfile


MANIFEST_SCHEMA = "ultimatefish-bishop-ghost-preservation-v1"
ARCHIVE_SCHEMA = "ultimatefish-bishop-ghost-preservation-archive-v1"
DEFAULT_SOURCES = (
    "src/ultimate/bishop_ghost_information_preserver.cpp",
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
    "tools/package_ultimate_bishop_ghost_preservation.py",
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_flat_manifest(path: Path) -> dict[str, str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != MANIFEST_SCHEMA:
        raise ValueError("preservation manifest schema mismatch")
    result: dict[str, str] = {}
    for line in lines[1:]:
        if "=" not in line:
            raise ValueError("malformed preservation manifest field")
        key, value = line.split("=", 1)
        if not key or key in result:
            raise ValueError("empty or duplicate preservation manifest field")
        result[key] = value
    return result


def canonical_info(name: str, data: bytes) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = len(data)
    info.mode = 0o644
    info.mtime = 0
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    return info


def build_archive(
    *, repo: Path, sidecar: Path, proof_manifest: Path, output: Path
) -> str:
    sidecar_data = sidecar.read_bytes()
    proof_data = proof_manifest.read_bytes()
    proof = read_flat_manifest(proof_manifest)
    if proof.get("sidecar_sha256") != sha256(sidecar_data):
        raise ValueError("sidecar SHA-256 differs from proof manifest")
    if int(proof.get("sidecar_bytes", "-1")) != len(sidecar_data):
        raise ValueError("sidecar extent differs from proof manifest")

    members: dict[str, bytes] = {
        "tablebases/kbishopghostk.ufgx": sidecar_data,
        "proof/kbishopghostk.preservation.manifest": proof_data,
    }
    for relative in DEFAULT_SOURCES:
        path = repo / relative
        if not path.is_file():
            raise ValueError(f"required loader source is missing: {relative}")
        members[f"loader/{relative}"] = path.read_bytes()

    restore = (
        "Ultimate Fish same-side Bishop/Ghost arbitrary-belief proof\n\n"
        "The proof manifest authenticates the exact concrete source, lower "
        "KGhost sidecar, observation/model hashes, final zero-Bellman log, "
        "overlay, and versioned transition archive. Restore those named "
        "dependencies separately, build the preserved standalone source, "
        "then run --verify. Add --full-bellman and a fresh --verify-scratch "
        "to exhaustively regenerate the transition and Bellman certificates.\n"
    ).encode("utf-8")
    members["RESTORE.txt"] = restore

    inventory_lines = [ARCHIVE_SCHEMA]
    for name in sorted(members):
        data = members[name]
        inventory_lines.append(f"{sha256(data)} {len(data)} {name}")
    inventory = ("\n".join(inventory_lines) + "\n").encode("utf-8")
    members["ARCHIVE.MANIFEST"] = inventory

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as raw:
        with tarfile.open(fileobj=raw, mode="w", format=tarfile.GNU_FORMAT) as tar:
            for name in sorted(members):
                data = members[name]
                tar.addfile(canonical_info(name, data), io.BytesIO(data))
    digest = sha256(output.read_bytes())
    output.with_name(output.name + ".sha256").write_text(
        f"{digest}  {output.name}\n", encoding="ascii"
    )
    return digest


def verify_archive(path: Path) -> str:
    with tarfile.open(path, "r") as archive:
        members = archive.getmembers()
        names = [member.name for member in members]
        if names != sorted(names) or len(names) != len(set(names)):
            raise ValueError("archive members are not sorted and unique")
        for member in members:
            if (
                not member.isfile()
                or member.mtime != 0
                or member.uid != 0
                or member.gid != 0
                or member.mode != 0o644
            ):
                raise ValueError("archive member metadata is not canonical")
        inventory_file = archive.extractfile("ARCHIVE.MANIFEST")
        if inventory_file is None:
            raise ValueError("archive lacks its inventory")
        lines = inventory_file.read().decode("utf-8").splitlines()
        if not lines or lines[0] != ARCHIVE_SCHEMA:
            raise ValueError("archive inventory schema mismatch")
        inventory: dict[str, tuple[str, int]] = {}
        for line in lines[1:]:
            digest, extent_text, name = line.split(" ", 2)
            if name in inventory or len(digest) != 64:
                raise ValueError("malformed or duplicate archive inventory entry")
            inventory[name] = (digest, int(extent_text))
        expected = set(names) - {"ARCHIVE.MANIFEST"}
        if set(inventory) != expected:
            raise ValueError("archive inventory membership residual")
        for name, (digest, extent) in inventory.items():
            source = archive.extractfile(name)
            if source is None:
                raise ValueError("archive inventory refers to a missing member")
            data = source.read()
            if len(data) != extent or sha256(data) != digest:
                raise ValueError("archive member hash/extent residual")
    return sha256(path.read_bytes())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--sidecar", type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--verify-archive", type=Path)
    args = parser.parse_args()
    if args.verify_archive:
        if args.sidecar or args.manifest or args.output:
            parser.error("--verify-archive cannot be combined with packaging inputs")
        digest = verify_archive(args.verify_archive.resolve())
        print(f"bishop_ghost_preservation_archive_restore sha256 {digest} residual 0")
        return 0
    if not args.sidecar or not args.manifest or not args.output:
        parser.error("packaging requires --sidecar, --manifest, and --output")
    digest = build_archive(
        repo=args.repo.resolve(),
        sidecar=args.sidecar.resolve(),
        proof_manifest=args.manifest.resolve(),
        output=args.output.resolve(),
    )
    print(f"bishop_ghost_preservation_archive sha256 {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
