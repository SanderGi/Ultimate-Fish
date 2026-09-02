#!/usr/bin/env python3
"""Authenticate and source-rebind one immutable extra-piece Ghost graph."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path


COMPONENTS = (".header", ".meta", ".strata", ".index", ".blocks")
PAYLOAD_BINDINGS = 3 + len(COMPONENTS)


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
    parser.add_argument("--old-source-sha256", required=True)
    parser.add_argument("--new-source-sha256", required=True)
    parser.add_argument("--model-sha256", required=True)
    parser.add_argument("--observation-sha256", required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()

    old_source = binding(args.old_source_sha256, "old source SHA-256")
    new_source = binding(args.new_source_sha256, "new source SHA-256")
    model = binding(args.model_sha256, "model SHA-256")
    observation = binding(args.observation_sha256,
                          "observation SHA-256")
    if old_source == new_source:
        raise ValueError("extra Ghost source rebind is a no-op")

    source_marker_path = Path(str(args.source_prefix) + ".verified")
    destination_marker_path = Path(str(args.destination_prefix) + ".verified")
    header_path = Path(str(args.source_prefix) + ".header")
    header = header_path.read_bytes()
    marker = source_marker_path.read_bytes()
    if not header or marker[:len(header)] != header:
        raise ValueError("extra Ghost transition marker/header mismatch")
    tail_bytes = len(marker) - len(header)
    if tail_bytes < 0 or tail_bytes % 64:
        raise ValueError("extra Ghost transition marker extent mismatch")
    binding_count = tail_bytes // 64
    lower_binding_count = binding_count - PAYLOAD_BINDINGS
    if lower_binding_count not in (3, 6):
        raise ValueError("extra Ghost transition marker binding count")

    source_offset = len(header) + 64 * lower_binding_count
    model_offset = source_offset + 64
    observation_offset = model_offset + 64
    component_offset = observation_offset + 64
    if (marker[source_offset:source_offset + 64] != old_source or
            marker[model_offset:model_offset + 64] != model or
            marker[observation_offset:observation_offset + 64] !=
            observation):
        raise ValueError("extra Ghost transition provenance mismatch")

    components: list[dict[str, object]] = []
    for index, suffix in enumerate(COMPONENTS):
        source_path = Path(str(args.source_prefix) + suffix)
        destination_path = Path(str(args.destination_prefix) + suffix)
        digest = sha256_file(source_path)
        expected = marker[
            component_offset + index * 64:
            component_offset + (index + 1) * 64].decode("ascii")
        if digest != expected:
            raise ValueError(f"extra Ghost transition {suffix} mismatch")
        if destination_path.exists():
            if (os.stat(destination_path).st_ino != os.stat(source_path).st_ino
                    or sha256_file(destination_path) != digest):
                raise ValueError(
                    f"extra Ghost destination {suffix} is not immutable link")
        else:
            os.link(source_path, destination_path)
        components.append({"suffix": suffix,
                           "bytes": source_path.stat().st_size,
                           "sha256": digest})

    rebound = bytearray(marker)
    rebound[source_offset:source_offset + 64] = new_source
    differing = [index for index, pair in enumerate(zip(marker, rebound))
                 if pair[0] != pair[1]]
    allowed = range(source_offset, source_offset + 64)
    if not differing or any(index not in allowed for index in differing):
        raise ValueError("extra Ghost marker rebind escaped source field")
    if destination_marker_path.exists():
        raise ValueError("destination extra Ghost marker already exists")
    temporary = Path(str(destination_marker_path) + ".tmp")
    temporary.write_bytes(rebound)
    temporary.replace(destination_marker_path)

    manifest = {
        "schema": "ultimate-extra-ghost-transition-marker-rebind-v1",
        "status": "payload-authenticated-source-provenance-rebound",
        "source_prefix": str(args.source_prefix.resolve()),
        "destination_prefix": str(args.destination_prefix.resolve()),
        "old_source_sha256": args.old_source_sha256,
        "new_source_sha256": args.new_source_sha256,
        "model_sha256": args.model_sha256,
        "observation_sha256": args.observation_sha256,
        "lower_binding_count": lower_binding_count,
        "old_marker_sha256": hashlib.sha256(marker).hexdigest(),
        "new_marker_sha256": hashlib.sha256(rebound).hexdigest(),
        "changed_bytes": len(differing),
        "components": components,
        "payload_changed": False,
        "residual": 0,
    }
    if args.manifest.exists():
        raise ValueError("extra Ghost rebind manifest already exists")
    args.manifest.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    print(json.dumps({
        "new_marker_sha256": manifest["new_marker_sha256"],
        "payload_changed": False,
        "residual": 0,
    }, sort_keys=True))


if __name__ == "__main__":
    main()
