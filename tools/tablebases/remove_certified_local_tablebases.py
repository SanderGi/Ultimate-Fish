#!/usr/bin/env python3
"""Remove only local tablebase payloads authenticated by an S3 certificate."""

from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath

from archive_ultimate_local_tablebases import (
    PAYLOAD,
    ROOT,
    SCHEMA,
    payload_paths,
    sha256_path,
)


def certified_paths(root: Path, certificate_path: Path) -> list[Path]:
    certificate = json.loads(certificate_path.read_text(encoding="utf-8"))
    if (certificate.get("schema") != SCHEMA or
            certificate.get("safe_to_delete_local_payloads") is not True):
        raise RuntimeError("certificate does not authorize local payload removal")
    s3 = certificate.get("s3", {})
    if (not s3.get("version_id") or s3.get("head_residual") != 0 or
            s3.get("download_residual") != 0 or
            s3.get("stream_restore_residual") != 0):
        raise RuntimeError("certificate lacks exact versioned-S3 restore proof")
    records = certificate.get("manifest", {}).get("artifacts", [])
    if not isinstance(records, list) or not records:
        raise RuntimeError("certificate has no payload inventory")
    expected: dict[Path, tuple[int, str]] = {}
    for record in records:
        pure = PurePosixPath(str(record.get("path", "")))
        if (pure.is_absolute() or len(pure.parts) != 2 or
                pure.parts[0] != "tablebases" or
                not PAYLOAD.fullmatch(pure.parts[1])):
            raise RuntimeError(f"unsafe certified payload path: {pure}")
        path = root.joinpath(*pure.parts)
        if path in expected:
            raise RuntimeError(f"duplicate certified payload path: {pure}")
        expected[path] = (int(record["bytes"]), str(record["sha256"]))
    actual = set(payload_paths(root))
    if actual != set(expected):
        raise RuntimeError("local payload inventory differs from certificate")
    for path, (extent, digest) in expected.items():
        if (not path.is_file() or path.is_symlink() or
                path.stat().st_size != extent or sha256_path(path) != digest):
            raise RuntimeError(f"local payload changed after certification: {path}")
    return sorted(expected)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--certificate", type=Path, required=True)
    parser.add_argument("--delete", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    paths = certified_paths(root, args.certificate.resolve(strict=True))
    total = sum(path.stat().st_size for path in paths)
    if args.delete:
        for path in paths:
            path.unlink()
    print(json.dumps({"deleted": bool(args.delete), "files": len(paths),
                      "bytes": total, "residual_files": sum(path.exists()
                                                               for path in paths)},
                     sort_keys=True))


if __name__ == "__main__":
    main()
