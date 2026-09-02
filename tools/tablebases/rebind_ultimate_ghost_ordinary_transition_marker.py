#!/usr/bin/env python3
"""Authenticate and model-rebind one immutable ordinary Ghost edge graph."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


HEADER_BYTES = 56
LOWER_BINDINGS = 3
COMPONENTS = (".header", ".meta", ".strata", ".index", ".blocks")
MARKER_BYTES = HEADER_BYTES + 64 * (LOWER_BINDINGS + 3 + len(COMPONENTS))
SOURCE_OFFSET = HEADER_BYTES + 64 * LOWER_BINDINGS
MODEL_OFFSET = SOURCE_OFFSET + 64
OBSERVATION_OFFSET = MODEL_OFFSET + 64
COMPONENT_OFFSET = OBSERVATION_OFFSET + 64


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def binding(value: str, label: str) -> bytes:
    if (len(value) != 64 or
            any(character not in "0123456789abcdef" for character in value)):
        raise ValueError(f"invalid {label}")
    return value.encode("ascii")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_prefix", type=Path)
    parser.add_argument("destination_prefix", type=Path)
    parser.add_argument(
        "--source-sha256", required=True,
        help="SHA-256 bound by the authenticated source marker",
    )
    parser.add_argument(
        "--new-source-sha256",
        help=("replacement concrete-source SHA-256 when the immutable edge "
              "payload is reused against a corrected WDL oracle"),
    )
    parser.add_argument("--old-model-sha256", required=True)
    parser.add_argument("--new-model-sha256", required=True)
    parser.add_argument("--observation-sha256", required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument(
        "--legacy-dependency-only-output", action="store_true",
        help=("emit the authenticated 248-byte dependency-only marker used "
              "by the pinned parallel solver; the manifest retains the "
              "discarded payload bindings and component authentication"),
    )
    args = parser.parse_args()

    source = binding(args.source_sha256, "source SHA-256")
    new_source = binding(
        args.new_source_sha256 or args.source_sha256,
        "new source SHA-256",
    )
    old_model = binding(args.old_model_sha256, "old model SHA-256")
    new_model = binding(args.new_model_sha256, "new model SHA-256")
    observation = binding(args.observation_sha256, "observation SHA-256")
    source_marker_path = args.source_prefix.with_suffix(".verified")
    destination_marker_path = args.destination_prefix.with_suffix(".verified")
    marker = source_marker_path.read_bytes()
    if len(marker) != MARKER_BYTES:
        raise ValueError("ordinary Ghost transition marker extent mismatch")
    header = args.source_prefix.with_suffix(".header").read_bytes()
    if len(header) != HEADER_BYTES or marker[:HEADER_BYTES] != header:
        raise ValueError("ordinary Ghost transition marker/header mismatch")
    if (marker[SOURCE_OFFSET:SOURCE_OFFSET + 64] != source or
            marker[MODEL_OFFSET:MODEL_OFFSET + 64] != old_model or
            marker[OBSERVATION_OFFSET:OBSERVATION_OFFSET + 64] != observation):
        raise ValueError("ordinary Ghost transition provenance mismatch")

    components: list[dict[str, object]] = []
    for index, suffix in enumerate(COMPONENTS):
        source_path = args.source_prefix.with_suffix(suffix)
        destination_path = args.destination_prefix.with_suffix(suffix)
        source_digest = sha256_file(source_path)
        destination_digest = sha256_file(destination_path)
        expected = marker[
            COMPONENT_OFFSET + index * 64:
            COMPONENT_OFFSET + (index + 1) * 64].decode("ascii")
        if source_digest != expected or destination_digest != source_digest:
            raise ValueError(f"ordinary Ghost transition {suffix} mismatch")
        components.append({"suffix": suffix, "bytes": source_path.stat().st_size,
                           "sha256": source_digest})

    rebound = bytearray(marker)
    rebound[SOURCE_OFFSET:SOURCE_OFFSET + 64] = new_source
    rebound[MODEL_OFFSET:MODEL_OFFSET + 64] = new_model
    differing = [index for index, pair in enumerate(zip(marker, rebound))
                 if pair[0] != pair[1]]
    allowed = (
        range(SOURCE_OFFSET, SOURCE_OFFSET + 64),
        range(MODEL_OFFSET, MODEL_OFFSET + 64),
    )
    if (not differing or
            any(not any(index in field for field in allowed)
                for index in differing)):
        raise ValueError("ordinary Ghost marker rebind escaped provenance fields")
    if destination_marker_path.exists():
        raise ValueError("destination ordinary Ghost marker already exists")
    temporary = destination_marker_path.with_suffix(".verified.tmp")
    output_marker = (rebound[:HEADER_BYTES + 64 * LOWER_BINDINGS]
                     if args.legacy_dependency_only_output else rebound)
    temporary.write_bytes(output_marker)
    temporary.replace(destination_marker_path)
    if (destination_marker_path.read_bytes() != output_marker or
            sha256_file(args.destination_prefix.with_suffix(".header")) !=
            components[0]["sha256"]):
        raise ValueError("ordinary Ghost rebound marker reread residual")

    manifest = {
        "schema": "ultimate-ordinary-ghost-transition-marker-rebind-v1",
        "status": "payload-authenticated-provenance-rebound",
        "source_prefix": str(args.source_prefix.resolve()),
        "destination_prefix": str(args.destination_prefix.resolve()),
        "source_sha256": args.new_source_sha256 or args.source_sha256,
        "old_source_sha256": args.source_sha256,
        "new_source_sha256": args.new_source_sha256 or args.source_sha256,
        "old_model_sha256": args.old_model_sha256,
        "new_model_sha256": args.new_model_sha256,
        "observation_sha256": args.observation_sha256,
        "old_marker_sha256": hashlib.sha256(marker).hexdigest(),
        "new_marker_sha256": hashlib.sha256(output_marker).hexdigest(),
        "full_rebound_marker_sha256": hashlib.sha256(rebound).hexdigest(),
        "legacy_dependency_only_output":
            args.legacy_dependency_only_output,
        "authenticated_payload_bindings_retained_in_manifest": {
            "source_sha256": (args.new_source_sha256 or
                              args.source_sha256),
            "model_sha256": args.new_model_sha256,
            "observation_sha256": args.observation_sha256,
        },
        "changed_bytes": len(differing),
        "components": components,
        "payload_changed": False,
        "residual": 0,
    }
    if args.manifest.exists():
        raise ValueError("ordinary Ghost rebind manifest already exists")
    args.manifest.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    print(json.dumps({
        "new_marker_sha256": manifest["new_marker_sha256"],
        "payload_changed": False,
        "residual": 0,
    }, sort_keys=True))


if __name__ == "__main__":
    main()
