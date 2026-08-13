#!/usr/bin/env python3
"""Start an ordinary-piece/Ghost solve from a certified concrete output."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import sys


CERTIFICATE_SCHEMA = "ultimate-concrete-k2-s3-certificate-v2"
CERTIFICATE_STATUS = "head-download-full-sha-archive-restore-verified"


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def certified_source(certificate_path: Path, source_table: Path,
                     filename: str, model_sha256: str) -> str:
    certificate = json.loads(certificate_path.read_text())
    if certificate.get("schema") != CERTIFICATE_SCHEMA:
        raise RuntimeError("concrete certificate schema mismatch")
    if certificate.get("status") != CERTIFICATE_STATUS:
        raise RuntimeError("concrete certificate preservation status mismatch")
    if certificate.get("generator_model_sha256") != model_sha256:
        raise RuntimeError("concrete certificate model mismatch")
    matches = [row for row in certificate.get("completed", [])
               if row.get("filename") == filename]
    if len(matches) != 1:
        raise RuntimeError("concrete certificate filename residual")
    row = matches[0]
    if row.get("status") != "generated-preserved":
        raise RuntimeError("concrete result is not preserved")
    expected = row.get("output", {}).get("sha256")
    if not isinstance(expected, str) or len(expected) != 64:
        raise RuntimeError("concrete output SHA-256 is missing")
    if not source_table.is_file() or sha256_path(source_table) != expected:
        raise RuntimeError("concrete output SHA-256 mismatch")
    return expected


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--certificate", type=Path, required=True)
    parser.add_argument("--source-table", type=Path, required=True)
    parser.add_argument("--filename", required=True)
    parser.add_argument("--concrete-model-sha256", required=True)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("runner_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    digest = certified_source(
        args.certificate, args.source_table, args.filename,
        args.concrete_model_sha256)
    forwarded = args.runner_args
    if forwarded[:1] == ["--"]:
        forwarded = forwarded[1:]
    if "--source-table" in forwarded or "--source-sha256" in forwarded:
        raise RuntimeError("source arguments must be certificate-derived")
    os.execv(sys.executable, [sys.executable, str(args.runner),
             "--source-table", str(args.source_table),
             "--source-sha256", digest, *forwarded])


if __name__ == "__main__":
    main()
