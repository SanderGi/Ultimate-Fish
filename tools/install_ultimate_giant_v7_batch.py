#!/usr/bin/env python3
"""Authenticate and install a complete 26-file GiantAnchorV2 batch."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys
from typing import Mapping


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import summarize_ultimate_tablebases as summarize  # noqa: E402


MAGIC = b"UFTB1\0\0\0"
GIANT_ANCHOR_V2 = 0x32474E4149474655


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
    header = struct.unpack_from("<8sIIIIIIIIIIQQ", data)
    (magic, version, _piece, count, legacy_edges, substates, wdl_bytes,
     dtw_bytes, exceptions, _secondary, secondary_color, exact_edges,
     tag) = header
    if (magic != MAGIC or version != 7 or tag != GIANT_ANCHOR_V2 or
            secondary_color != int(bool(record["opposing"])) or not substates or
            wdl_bytes != (count + 3) // 4 or dtw_bytes != count or
            len(data) != 64 + wdl_bytes + dtw_bytes + exceptions * 6 or
            legacy_edges != min(exact_edges, 0xFFFFFFFF) or
            count != int(record["states"]) or exact_edges != int(record["edges"]) or
            sha256(path) != record["sha256"]):
        raise ValueError(f"{path}: v7 payload authentication failed")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--batch-certificate", required=True, type=Path)
    parser.add_argument("--audit-certificate", required=True, type=Path)
    parser.add_argument("--source-manifest", required=True, type=Path)
    parser.add_argument("--artifacts", required=True, type=Path)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--apply", action="store_true",
                        help="replace payloads and catalogs after validation")
    args = parser.parse_args()
    root = args.root.resolve()
    batch = json.loads(args.batch_certificate.read_text())
    audit = json.loads(args.audit_certificate.read_text())
    source_sha = sha256(args.source_manifest)
    if (batch.get("format") != "Ultimate-Fish-GiantAnchorV2-batch-v1" or
            batch.get("source_manifest_sha256") != source_sha):
        raise ValueError("generation certificate/source manifest mismatch")
    if (audit.get("format") != "Ultimate-Fish-GiantAnchorV2-reachability-v1" or
            audit.get("source_manifest_sha256") != source_sha or
            audit.get("generation_certificate_sha256") !=
            sha256(args.batch_certificate)):
        raise ValueError("reachability certificate is not bound to generation")

    generated = {str(record["filename"]): record for record in batch["files"]}
    audited = {str(record["filename"]): record for record in audit["files"]}
    codec_path = root / "tablebases" / "giant_codec_regeneration.json"
    reachability_path = root / "tablebases" / "reachability.json"
    codec = json.loads(codec_path.read_text())
    reachability = json.loads(reachability_path.read_text())
    expected = {
        filename for filename, record in codec["files"].items()
        if record["status"] == "stale-anchor-v1-requires-regeneration"
    }
    if len(expected) != 26 or set(generated) != expected or set(audited) != expected:
        raise ValueError("certificates do not exactly cover the 26 stale Giant files")

    artifacts: dict[str, Path] = {}
    for filename in sorted(expected):
        generation = generated[filename]
        admission = audited[filename]
        artifact = args.artifacts / filename
        validate_payload(artifact, generation)
        if (admission["tablebase_sha256"] != generation["sha256"] or
                int(admission["states"]) != int(generation["states"])):
            raise ValueError(f"{filename}: reachability audit payload mismatch")
        for field in ("necessary_reachability", "ordinary_predecessor_safety"):
            counts = admission[field]
            if (not isinstance(counts, list) or len(counts) != 2 or
                    any(not isinstance(side, list) or len(side) != 4 or
                        int(side[0]) != 0 for side in counts)):
                raise ValueError(f"{filename}: malformed {field} certificate")
        artifacts[filename] = artifact
        record = codec["files"][filename]
        record["status"] = "verified-anchor-v2"
        record["current_sha256"] = generation["sha256"]
        record["generation_certificate"] = {
            key: int(generation[key]) for key in
            ("states", "edges", "win", "loss", "draw", "bellman_residual")
        }
        record["packaging_certificate"] = {
            "format_version": 7,
            "giant_anchor_tag": generation["giant_anchor_tag"],
            "logical_payload_sha256": generation["logical_payload_sha256"],
        }
        reachability["files"][filename] = {
            "necessary_reachability": admission["necessary_reachability"],
            "ordinary_predecessor_safety": admission["ordinary_predecessor_safety"],
            "sha256": generation["sha256"],
        }

    if not args.apply:
        print("validated 26 artifacts and both authenticated catalogs (dry run)")
        return

    tablebases = root / "tablebases"
    staged: dict[str, Path] = {}
    for filename, artifact in artifacts.items():
        temporary = tablebases / f".{filename}.giant-v7.tmp"
        shutil.copyfile(artifact, temporary)
        if sha256(temporary) != generated[filename]["sha256"]:
            raise ValueError(f"{filename}: staged copy failed authentication")
        staged[filename] = temporary
    codec_temporary = codec_path.with_suffix(".json.tmp")
    reachability_temporary = reachability_path.with_suffix(".json.tmp")
    codec_temporary.write_text(json.dumps(codec, indent=2, sort_keys=True) + "\n")
    reachability_temporary.write_text(
        json.dumps(reachability, indent=2, sort_keys=True) + "\n")
    for filename in sorted(staged):
        os.replace(staged[filename], tablebases / filename)
    os.replace(codec_temporary, codec_path)
    os.replace(reachability_temporary, reachability_path)

    summarize.giant_codec_catalog.cache_clear()
    summarize.reachability_catalog.cache_clear()
    for filename in sorted(expected):
        path = tablebases / filename
        summarize.require_current_giant_codec(path, generated[filename]["sha256"])
        summarize.summary(path)
    print("installed and revalidated 26 GiantAnchorV2 payloads and catalogs")


if __name__ == "__main__":
    main()
