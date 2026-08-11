#!/usr/bin/env python3
"""Prepare lossless v5 lower-Jester inputs for the frozen Jester/Ghost solver."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
from typing import Any


SCHEMA = "ultimate-jester-ghost-lower-jester-compatibility-v1"
STATUS = "v4-to-v5-header-only-overlay-rebound"
MAGIC = b"UFTB1\0\0\0"
OVERLAY_MAGIC = b"UFIW2\0\0\0"
JESTER = 1
PIECE_COUNT = 30
WHITE = 0
COUNT = 985_920
OLD_TABLE_SHA256 = "3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa"
OLD_OVERLAY_SHA256 = "ab806963760bcd52163d6acfbb7d5a0c6f2b2dcc8a616a61c16ae060a2d412ee"
LOWER_MODEL_SHA256 = "0ed6d361e313623234c21f9a1c800947014ce47320b4a71fca4fb20a255587c2"


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_exclusive(path: Path, payload: bytes) -> None:
    with path.open("xb") as stream:
        stream.write(payload)
        stream.flush()
        os.fsync(stream.fileno())


def u32(payload: bytes, offset: int) -> int:
    return struct.unpack_from("<I", payload, offset)[0]


def prepare(args: argparse.Namespace) -> dict[str, Any]:
    if (not args.tool_source.is_file() or
            sha256_file(args.tool_source) != args.tool_sha256):
        raise ValueError("compatibility tool SHA-256 mismatch")
    if args.output_dir.exists():
        raise ValueError("fresh compatibility output directory already exists")
    old_table = args.lower_table.read_bytes()
    old_overlay = args.lower_overlay.read_bytes()
    if sha256_bytes(old_table) != OLD_TABLE_SHA256:
        raise ValueError("lower Jester v4 table SHA-256 mismatch")
    if sha256_bytes(old_overlay) != OLD_OVERLAY_SHA256:
        raise ValueError("lower Jester overlay SHA-256 mismatch")
    if (len(old_table) < 40 or old_table[:8] != MAGIC or
            u32(old_table, 8) != 4 or u32(old_table, 12) != JESTER or
            u32(old_table, 16) != COUNT or u32(old_table, 24) != 1 or
            u32(old_table, 28) != (COUNT + 3) // 4 or
            u32(old_table, 32) != COUNT or
            len(old_table) != 40 + u32(old_table, 28) +
            u32(old_table, 32) + 6 * u32(old_table, 36)):
        raise ValueError("lower Jester v4 table layout mismatch")
    if (len(old_overlay) != 160 + COUNT or
            old_overlay[:8] != OVERLAY_MAGIC or
            u32(old_overlay, 8) != 2 or u32(old_overlay, 12) != JESTER or
            u32(old_overlay, 16) != PIECE_COUNT or
            u32(old_overlay, 20) != WHITE or
            u32(old_overlay, 24) != COUNT or u32(old_overlay, 28) != 1 or
            old_overlay[32:96].decode("ascii") != OLD_TABLE_SHA256 or
            old_overlay[96:160].decode("ascii") != LOWER_MODEL_SHA256):
        raise ValueError("lower Jester overlay binding/layout mismatch")
    if any((flags & ~7) or (not (flags & 4) and (flags & 3))
           for flags in old_overlay[160:]):
        raise ValueError("lower Jester overlay flag residual")

    new_table = bytearray(old_table[:40])
    struct.pack_into("<I", new_table, 8, 5)
    new_table.extend(struct.pack("<II", PIECE_COUNT, WHITE))
    new_table.extend(old_table[40:])
    new_table_sha = sha256_bytes(new_table)
    new_overlay = bytearray(old_overlay)
    new_overlay[32:96] = new_table_sha.encode("ascii")
    new_overlay_sha = sha256_bytes(new_overlay)

    args.output_dir.mkdir(parents=True)
    table_output = args.output_dir / "kjesterk-v5-compat.uftb"
    overlay_output = args.output_dir / "kjesterk-v5-compat.ufiw"
    certificate_output = args.output_dir / "compatibility-certificate.json"
    write_exclusive(table_output, new_table)
    write_exclusive(overlay_output, new_overlay)
    certificate = {
        "schema": SCHEMA, "status": STATUS,
        "tool_sha256": args.tool_sha256,
        "lower_model_sha256": LOWER_MODEL_SHA256,
        "old_table": {"bytes": len(old_table),
                      "sha256": OLD_TABLE_SHA256, "version": 4},
        "new_table": {"path": table_output.name, "bytes": len(new_table),
                      "sha256": new_table_sha, "version": 5},
        "old_overlay": {"bytes": len(old_overlay),
                        "sha256": OLD_OVERLAY_SHA256},
        "new_overlay": {"path": overlay_output.name,
                        "bytes": len(new_overlay),
                        "sha256": new_overlay_sha},
        "payload_sha256": sha256_bytes(old_table[40:]),
        "overlay_flags_sha256": sha256_bytes(old_overlay[160:]),
        "residuals": {"table_layout": 0, "table_payload": 0,
                      "overlay_layout": 0, "overlay_flags": 0,
                      "header_diff": 0, "binding": 0},
    }
    write_exclusive(certificate_output,
                    (json.dumps(certificate, indent=2, sort_keys=True) +
                     "\n").encode())
    return {"status": STATUS,
            "certificate_sha256": sha256_file(certificate_output),
            "table_sha256": new_table_sha,
            "overlay_sha256": new_overlay_sha,
            "table_bytes": len(new_table),
            "overlay_bytes": len(new_overlay)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool-source", type=Path, required=True)
    parser.add_argument("--tool-sha256", required=True)
    parser.add_argument("--lower-table", type=Path, required=True)
    parser.add_argument("--lower-overlay", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args), sort_keys=True))


if __name__ == "__main__":
    main()
