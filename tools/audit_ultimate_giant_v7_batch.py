#!/usr/bin/env python3
"""Audit regenerated folded-Giant tablebases before repository installation.

The generation certificate authenticates the v7 payload and its zero-residual
Bellman pass.  This second, independent stage recomputes both native causal
admission counts used by the README; it never inherits reachability counts
from an anchor-v1 payload.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
from typing import Mapping


MAGIC = b"UFTB1\0\0\0"
GIANT_ANCHOR_V2 = 0x32474E4149474655
AUDIT_RE = re.compile(
    r"^(reachability|predecessor_safety) side ([01]) unknown (\d+) "
    r"win (\d+) loss (\d+) draw (\d+)$",
    re.MULTILINE)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_payload(path: Path, record: Mapping[str, object]) -> None:
    data = path.read_bytes()
    if len(data) < 64:
        raise ValueError(f"{path}: truncated v7 tablebase")
    (magic, version, _piece, count, legacy_edges, substates, wdl_bytes,
     dtw_bytes, exceptions, _secondary, secondary_color, exact_edges,
     codec_tag) = struct.unpack_from("<8sIIIIIIIIIIQQ", data)
    if (magic != MAGIC or version != 7 or codec_tag != GIANT_ANCHOR_V2 or
            secondary_color != int(bool(record["opposing"])) or not substates or
            wdl_bytes != (count + 3) // 4 or dtw_bytes != count or
            len(data) != 64 + wdl_bytes + dtw_bytes + exceptions * 6 or
            legacy_edges != min(exact_edges, 0xFFFFFFFF) or
            count != int(record["states"]) or exact_edges != int(record["edges"]) or
            sha256(path) != record["sha256"]):
        raise ValueError(f"{path}: v7 payload disagrees with generation certificate")


def parse_audit(output: str, label: str,
                expected_prefix: str) -> list[list[int]]:
    matches = AUDIT_RE.findall(output)
    if (len(matches) != 2 or
            {item[0] for item in matches} != {expected_prefix} or
            {int(item[1]) for item in matches} != {0, 1}):
        raise ValueError(f"{label}: expected exactly one audit row per side")
    result = [[0, 0, 0, 0] for _ in range(2)]
    for _prefix, side, unknown, win, loss, draw in matches:
        values = [int(unknown), int(win), int(loss), int(draw)]
        if values[0] != 0:
            raise ValueError(f"{label}: native audit left unknown records")
        result[int(side)] = values
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--batch-certificate", required=True, type=Path)
    parser.add_argument("--source-manifest", required=True, type=Path)
    parser.add_argument("--artifacts", required=True, type=Path)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--workdir", type=Path, default=Path.cwd())
    parser.add_argument("--logs", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--workers", type=int, default=8)
    args = parser.parse_args()
    if not 1 <= args.workers <= 8:
        parser.error("--workers must be between 1 and 8")

    batch = json.loads(args.batch_certificate.read_text())
    source_sha = sha256(args.source_manifest)
    if (batch.get("format") != "Ultimate-Fish-GiantAnchorV2-batch-v1" or
            batch.get("source_manifest_sha256") != source_sha):
        raise ValueError("batch certificate is not bound to the supplied source manifest")
    records = batch.get("files")
    if (not isinstance(records, list) or len(records) != 26 or
            len({record.get("filename") for record in records}) != 26):
        raise ValueError("batch certificate must contain 26 unique files")

    args.logs.mkdir(parents=True, exist_ok=True)
    binary = args.binary.resolve()
    workdir = args.workdir.resolve()
    environment = dict(os.environ)
    environment["ULTIMATE_TABLEBASE_PATH"] = str(workdir / "tablebases")

    def audit(record: Mapping[str, object]) -> dict[str, object]:
        filename = str(record["filename"])
        if Path(filename).name != filename:
            raise ValueError(f"unsafe artifact filename: {filename}")
        artifact = (args.artifacts / filename).resolve()
        validate_payload(artifact, record)
        base = [str(binary), "--piece", str(record["primary"]),
                "--piece2", str(record["secondary"])]
        if record["opposing"]:
            base.append("--opposing")
        audits: dict[str, object] = {}
        for field, option, output_prefix in (
                ("necessary_reachability", "--audit-reachability", "reachability"),
                ("ordinary_predecessor_safety", "--audit-predecessor-safety",
                 "predecessor_safety")):
            completed = subprocess.run(
                [*base, option, str(artifact)], cwd=workdir,
                env=environment, check=False, text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            log = args.logs / f"{Path(filename).stem}.{field}.log"
            log.write_text(completed.stdout)
            if completed.returncode:
                raise RuntimeError(
                    f"{filename}: {field} exited with {completed.returncode}")
            audits[field] = parse_audit(
                completed.stdout, f"{filename}: {field}", output_prefix)
            audits[f"{field}_log_sha256"] = sha256(log)
        return {
            "filename": filename,
            "tablebase_sha256": str(record["sha256"]),
            "states": int(record["states"]),
            **audits,
        }

    completed_records: list[dict[str, object]] = []
    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        futures = {executor.submit(audit, record): record for record in records}
        for future in as_completed(futures):
            completed_records.append(future.result())
    completed_records.sort(key=lambda record: str(record["filename"]))
    certificate = {
        "format": "Ultimate-Fish-GiantAnchorV2-reachability-v1",
        "source_manifest_sha256": source_sha,
        "generation_certificate_sha256": sha256(args.batch_certificate),
        "workers_max": args.workers,
        "files": completed_records,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_text(json.dumps(certificate, indent=2, sort_keys=True) + "\n")
    temporary.replace(args.output)
    print(f"audited {len(completed_records)} v7 Giant tablebases -> {args.output}")


if __name__ == "__main__":
    main()
