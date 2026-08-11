#!/usr/bin/env python3
"""Split and transparently read regular-Git Ultimate tablebase shards."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
from pathlib import Path
import struct


MAGIC = b"UFTBS1\0\0"
VERSION = 1
HEADER = struct.Struct("<8sIIQ")
ENTRY = struct.Struct("<HQ32s")
DEFAULT_LIMIT = 95_000_000


@dataclass(frozen=True)
class Part:
    name: str
    size: int
    digest: bytes


def manifest(path: Path) -> tuple[int, list[Part]] | None:
    with path.open("rb") as stream:
        prefix = stream.read(HEADER.size)
        if len(prefix) < HEADER.size or prefix[:8] != MAGIC:
            return None
        data = prefix + stream.read()
    magic, version, count, total = HEADER.unpack_from(data)
    if magic != MAGIC or version != VERSION or not count:
        raise ValueError(f"{path}: invalid shard manifest header")
    offset = HEADER.size
    parts: list[Part] = []
    for _ in range(count):
        if offset + ENTRY.size > len(data):
            raise ValueError(f"{path}: truncated shard manifest entry")
        name_length, size, digest = ENTRY.unpack_from(data, offset)
        offset += ENTRY.size
        if offset + name_length > len(data):
            raise ValueError(f"{path}: truncated shard name")
        name = data[offset:offset + name_length].decode("utf-8")
        offset += name_length
        if Path(name).name != name:
            raise ValueError(f"{path}: unsafe shard name")
        parts.append(Part(name, size, digest))
    if offset != len(data) or sum(part.size for part in parts) != total:
        raise ValueError(f"{path}: inconsistent shard manifest")
    return total, parts


def iter_logical(path: Path):
    description = manifest(path)
    if description is None:
        yield path.read_bytes()
        return
    _total, parts = description
    for part in parts:
        part_path = path.parent / part.name
        data = part_path.read_bytes()
        if len(data) != part.size or hashlib.sha256(data).digest() != part.digest:
            raise ValueError(f"{part_path}: shard size or SHA-256 mismatch")
        yield data


def read_logical(path: Path) -> bytes:
    return b"".join(iter_logical(path))


def logical_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    for chunk in iter_logical(path):
        digest.update(chunk)
    return digest.hexdigest()


def split(path: Path, limit: int = DEFAULT_LIMIT) -> list[Path]:
    """Replace an oversized logical table with a small checked manifest."""
    size = path.stat().st_size
    if size <= limit:
        return [path]
    source = path.with_name(path.name + ".logical.tmp")
    path.replace(source)
    parts: list[Part] = []
    part_paths: list[Path] = []
    try:
        with source.open("rb") as stream:
            number = 0
            while True:
                data = stream.read(limit)
                if not data:
                    break
                name = f"{path.name}.part{number:03d}"
                part_path = path.parent / name
                part_path.write_bytes(data)
                parts.append(Part(name, len(data), hashlib.sha256(data).digest()))
                part_paths.append(part_path)
                number += 1
        payload = bytearray(HEADER.pack(MAGIC, VERSION, len(parts), size))
        for part in parts:
            encoded = part.name.encode("utf-8")
            payload.extend(ENTRY.pack(len(encoded), part.size, part.digest))
            payload.extend(encoded)
        temporary = path.with_name(path.name + ".manifest.tmp")
        temporary.write_bytes(payload)
        temporary.replace(path)
    except Exception:
        if not path.exists() and source.exists():
            source.replace(path)
        raise
    finally:
        if source.exists():
            source.unlink()
    return [path, *part_paths]
