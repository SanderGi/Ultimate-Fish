#!/usr/bin/env python3
"""Copy and rebind an authenticated UFJGT2 transition tree to a new model.

This is intentionally narrower than a general transition migration.  It accepts
only version-2 Jester/Ghost headers and markers, proves every payload hash and
extent before copying, changes only the two model fields plus the marker's
derived header hash, and writes a deterministic audit manifest.  Exhaustive
native regeneration with the destination solver remains mandatory afterwards.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
from typing import Iterable


HEADER = struct.Struct("<8sIIIIII16Q64s64s64s64s")
MARKER = struct.Struct("<8sIIII64s64s64s64s64s")
HEADER_MAGIC = b"UFJGT2\0\0"
MARKER_MAGIC = b"UFJGV2\0\0"
VERSION = 2
RAW_DOMAIN = 38_450_880
CANONICAL_GEOMETRIES = 9_612_720
CONCRETE_WORLDS = 37_957_920
TRANSITION_EDGES = 479_456_062
# UFJGT2 stores the construction-time GeometryDisk, including 160 correlated
# product variables' dense owner ordinals and actual-stratum map.  This is not
# the smaller 56-byte permanent UFJG catalog record.  Bind the exact production
# extent so a restore migration fails closed on ABI/layout drift.
GEOMETRY_BYTES = 952
STRATUM_BYTES = 24
MODEL_OFFSET = 224
MARKER_MODEL_OFFSET = 88
MARKER_HEADER_SHA_OFFSET = 280
SUFFIXES = (".header", ".meta", ".strata", ".index", ".blocks",
            ".verified")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def payload_sha(prefix: Path) -> str:
    digest = hashlib.sha256()
    for suffix in (".meta", ".strata", ".index", ".blocks"):
        with prefix.with_suffix(suffix).open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    return digest.hexdigest()


def ascii_digest(value: bytes, context: str) -> str:
    try:
        text = value.decode("ascii")
    except UnicodeDecodeError as error:
        raise ValueError(f"{context}: digest is not ASCII") from error
    require(len(text) == 64 and all(c in "0123456789abcdef" for c in text),
            f"{context}: invalid lowercase SHA-256")
    return text


def binding(value: str, context: str) -> bytes:
    require(len(value) == 64 and
            all(c in "0123456789abcdef" for c in value),
            f"invalid {context}")
    return value.encode("ascii")


def authenticate_prefix(prefix: Path, source: bytes, model: bytes,
                        observation: bytes) -> dict[str, object]:
    paths = {suffix: prefix.with_suffix(suffix) for suffix in SUFFIXES}
    require(all(path.is_file() for path in paths.values()),
            f"{prefix}: incomplete verified prefix")
    header_bytes = paths[".header"].read_bytes()
    marker_bytes = paths[".verified"].read_bytes()
    require(len(header_bytes) == HEADER.size,
            f"{prefix}: header extent is not {HEADER.size}")
    require(len(marker_bytes) == MARKER.size,
            f"{prefix}: marker extent is not {MARKER.size}")
    header = HEADER.unpack(header_bytes)
    marker = MARKER.unpack(marker_bytes)
    (magic, version, header_size, raw_begin, raw_count, raw_domain,
     complete, *tail) = header
    counters = tail[:16]
    header_source, header_model, header_observation, header_payload = tail[16:]
    require(magic == HEADER_MAGIC and version == VERSION and
            header_size == HEADER.size and raw_domain == RAW_DOMAIN,
            f"{prefix}: incompatible UFJGT2 header")
    require(raw_begin <= RAW_DOMAIN and raw_count <= RAW_DOMAIN - raw_begin,
            f"{prefix}: invalid raw range")
    require(complete in (0, 1) and
            (not complete or (raw_begin == 0 and raw_count == RAW_DOMAIN)),
            f"{prefix}: invalid complete flag")
    require((header_source, header_model, header_observation) ==
            (source, model, observation),
            f"{prefix}: header provenance binding mismatch")
    payload = payload_sha(prefix)
    require(ascii_digest(header_payload, str(prefix)) == payload,
            f"{prefix}: payload SHA-256 mismatch")
    geometries = counters[0]
    strata = counters[14]
    block_bytes = counters[15]
    require(counters[9] == 4 * (counters[1] + counters[0]),
            f"{prefix}: codec certificate mismatch")
    require(counters[10] == 4 * counters[1],
            f"{prefix}: action certificate mismatch")
    require(counters[11] >= 4 * (counters[14] + counters[0]),
            f"{prefix}: decision certificate mismatch")
    require(counters[12] == 4 * counters[8],
            f"{prefix}: transition certificate mismatch")
    require(counters[13] == 4 * counters[0],
            f"{prefix}: symmetry certificate mismatch")
    require(paths[".meta"].stat().st_size == geometries * GEOMETRY_BYTES,
            f"{prefix}: metadata extent mismatch")
    require(paths[".strata"].stat().st_size == strata * STRATUM_BYTES,
            f"{prefix}: strata extent mismatch")
    require(paths[".index"].stat().st_size == (geometries + 1) * 8,
            f"{prefix}: index extent mismatch")
    require(paths[".blocks"].stat().st_size == block_bytes,
            f"{prefix}: block extent mismatch")
    index_bytes = paths[".index"].read_bytes()
    indices = struct.unpack(f"<{geometries + 1}Q", index_bytes)
    require(indices[0] == 0 and indices[-1] == block_bytes and
            all(left <= right for left, right in zip(indices, indices[1:])),
            f"{prefix}: index coverage mismatch")
    (marker_magic, marker_version, marker_size, marker_begin, marker_count,
     marker_source, marker_model, marker_observation, marker_payload,
     marker_header_sha) = marker
    require(marker_magic == MARKER_MAGIC and marker_version == VERSION and
            marker_size == MARKER.size and marker_begin == raw_begin and
            marker_count == raw_count,
            f"{prefix}: incompatible UFJGV2 marker")
    require((marker_source, marker_model, marker_observation, marker_payload) ==
            (source, model, observation, header_payload),
            f"{prefix}: marker provenance binding mismatch")
    require(ascii_digest(marker_header_sha, str(prefix)) ==
            sha256_bytes(header_bytes),
            f"{prefix}: marker header SHA-256 mismatch")
    return {
        "prefix": prefix.name,
        "raw_begin": raw_begin,
        "raw_count": raw_count,
        "complete": bool(complete),
        "geometries": geometries,
        "worlds": counters[1],
        "edges": counters[8],
        "payload_sha256": payload,
        "header_sha256": sha256_bytes(header_bytes),
        "marker_sha256": sha256_bytes(marker_bytes),
    }


def rebind_prefix(prefix: Path, old_model: bytes,
                  new_model: bytes) -> tuple[str, str, str, str]:
    header_path = prefix.with_suffix(".header")
    marker_path = prefix.with_suffix(".verified")
    old_header = header_path.read_bytes()
    old_marker = marker_path.read_bytes()
    require(old_header[MODEL_OFFSET:MODEL_OFFSET + 64] == old_model,
            f"{prefix}: copied header model changed before rebind")
    require(old_marker[MARKER_MODEL_OFFSET:MARKER_MODEL_OFFSET + 64] == old_model,
            f"{prefix}: copied marker model changed before rebind")
    new_header = bytearray(old_header)
    new_header[MODEL_OFFSET:MODEL_OFFSET + 64] = new_model
    new_header_sha = sha256_bytes(new_header)
    new_marker = bytearray(old_marker)
    new_marker[MARKER_MODEL_OFFSET:MARKER_MODEL_OFFSET + 64] = new_model
    new_marker[MARKER_HEADER_SHA_OFFSET:MARKER_HEADER_SHA_OFFSET + 64] = (
        new_header_sha.encode("ascii"))
    header_path.write_bytes(new_header)
    marker_path.write_bytes(new_marker)
    differing_header = [index for index, pair in
                        enumerate(zip(old_header, new_header))
                        if pair[0] != pair[1]]
    differing_marker = [index for index, pair in
                        enumerate(zip(old_marker, new_marker))
                        if pair[0] != pair[1]]
    require(all(MODEL_OFFSET <= index < MODEL_OFFSET + 64
                for index in differing_header),
            f"{prefix}: header diff escaped model field")
    require(all((MARKER_MODEL_OFFSET <= index < MARKER_MODEL_OFFSET + 64) or
                (MARKER_HEADER_SHA_OFFSET <= index <
                 MARKER_HEADER_SHA_OFFSET + 64)
                for index in differing_marker),
            f"{prefix}: marker diff escaped model/header-SHA fields")
    return (sha256_bytes(old_header), sha256_bytes(new_header),
            sha256_bytes(old_marker), sha256_bytes(new_marker))


def migrate_tree(source_tree: Path, destination_tree: Path, old_model: str,
                 new_model: str, source_sha: str,
                 observation_sha: str, *,
                 require_complete: bool = True) -> dict[str, object]:
    old = binding(old_model, "old model SHA-256")
    new = binding(new_model, "new model SHA-256")
    source = binding(source_sha, "source SHA-256")
    observation = binding(observation_sha, "observation SHA-256")
    require(source_tree.is_dir(), f"{source_tree}: source tree is not a directory")
    require(not destination_tree.exists(),
            f"{destination_tree}: destination already exists")
    verified_paths = sorted(source_tree.glob("*.verified"))
    require(bool(verified_paths), f"{source_tree}: no verified prefixes")
    source_records = [authenticate_prefix(path.with_suffix(""), source, old,
                                          observation)
                      for path in verified_paths]
    complete = [record for record in source_records if record["complete"]]
    if require_complete:
        require(len(complete) == 1,
                f"{source_tree}: expected exactly one complete merged prefix")
        merged = complete[0]
        require(merged["raw_begin"] == 0 and
                merged["raw_count"] == RAW_DOMAIN and
                merged["geometries"] == CANONICAL_GEOMETRIES and
                merged["worlds"] == CONCRETE_WORLDS and
                merged["edges"] == TRANSITION_EDGES,
                f"{source_tree}: complete merged certificate mismatch")
    verified_prefixes = {path.with_suffix("").name for path in verified_paths}
    unverified_headers = sorted(
        path.name for path in source_tree.glob("*.header")
        if path.with_suffix("").name not in verified_prefixes)
    shutil.copytree(source_tree, destination_tree, copy_function=shutil.copy2)
    records: list[dict[str, object]] = []
    for source_record in source_records:
        prefix = destination_tree / str(source_record["prefix"])
        old_header, new_header, old_marker, new_marker = rebind_prefix(
            prefix, old, new)
        destination_record = authenticate_prefix(
            prefix, source, new, observation)
        require(source_record["payload_sha256"] ==
                destination_record["payload_sha256"],
                f"{prefix}: payload changed during rebind")
        records.append({
            **destination_record,
            "old_header_sha256": old_header,
            "new_header_sha256": new_header,
            "old_marker_sha256": old_marker,
            "new_marker_sha256": new_marker,
        })
    manifest = {
        "schema": "ultimate-jester-ghost-transition-rebind-v1",
        "status": "copied-authenticated-model-rebound-payload-unchanged",
        "source_tree": str(source_tree.resolve()),
        "destination_tree": str(destination_tree.resolve()),
        "source_sha256": source_sha,
        "old_model_sha256": old_model,
        "new_model_sha256": new_model,
        "observation_sha256": observation_sha,
        "verified_prefixes": len(records),
        "unverified_headers_copied_unchanged": unverified_headers,
        "records": records,
    }
    manifest_path = destination_tree / "rebind-manifest.json"
    manifest_path.write_text(json.dumps(
        manifest, indent=2, sort_keys=True) + "\n")
    manifest["manifest_sha256"] = sha256_file(manifest_path)
    return manifest


def main(argv: Iterable[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_tree", type=Path)
    parser.add_argument("destination_tree", type=Path)
    parser.add_argument("--old-model-sha256", required=True)
    parser.add_argument("--new-model-sha256", required=True)
    parser.add_argument("--source-sha256", required=True)
    parser.add_argument("--observation-sha256", required=True)
    args = parser.parse_args(argv)
    result = migrate_tree(
        args.source_tree, args.destination_tree,
        args.old_model_sha256, args.new_model_sha256,
        args.source_sha256, args.observation_sha256)
    print(json.dumps({
        "status": result["status"],
        "verified_prefixes": result["verified_prefixes"],
        "manifest_sha256": result["manifest_sha256"],
    }, sort_keys=True))


if __name__ == "__main__":
    main()
