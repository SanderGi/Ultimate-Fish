#!/usr/bin/env python3

"""Locate raw public-relation domain conflicts in a UFGX1 transition graph."""

from __future__ import annotations

import argparse
import mmap
from pathlib import Path
import struct


OFFSETS = struct.Struct("<81I")
EDGE = struct.Struct("<IIIBBBB")
META_BYTES = 392
NO_INDEX = (1 << 32) - 1


def same_class_public_state(meta: mmap.mmap, child: int, actual: int) -> tuple[int, int]:
    base = child * META_BYTES
    terminal_low = struct.unpack_from("<Q", meta, base + 16)[0]
    terminal_high = struct.unpack_from("<H", meta, base + 24)[0]
    terminal = ((terminal_low >> actual) & 1) if actual < 64 else \
        ((terminal_high >> (actual - 64)) & 1)
    stratum = struct.unpack_from("<I", meta, base + 64 + actual * 4)[0]
    return stratum, terminal


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("prefix", type=Path)
    args = parser.parse_args()
    index_path = Path(f"{args.prefix}.index")
    blocks_path = Path(f"{args.prefix}.blocks")
    meta_path = Path(f"{args.prefix}.meta")
    index_bytes = index_path.read_bytes()
    if len(index_bytes) % 8:
        raise RuntimeError("transition index has a partial entry")
    index = struct.unpack(f"<{len(index_bytes) // 8}Q", index_bytes)
    with blocks_path.open("rb") as handle, meta_path.open("rb") as meta_handle, \
            mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ) as blocks, \
            mmap.mmap(meta_handle.fileno(), 0, access=mmap.ACCESS_READ) as meta_map:
        for geometry, (begin, end) in enumerate(zip(index, index[1:])):
            if end - begin < OFFSETS.size or (end - begin - OFFSETS.size) % EDGE.size:
                raise RuntimeError(f"malformed block at geometry {geometry}")
            offsets = OFFSETS.unpack_from(blocks, begin)
            edge_base = begin + OFFSETS.size
            relations: dict[
                int, tuple[int, int, int, int, int, int, int, int]
            ] = {}
            for source in range(80):
                for ordinal in range(offsets[source], offsets[source + 1]):
                    relation, action, child, actual, domain, exact, reserved = \
                        EDGE.unpack_from(blocks, edge_base + ordinal * EDGE.size)
                    stratum, terminal = (NO_INDEX, 0)
                    if domain == 0:
                        stratum, terminal = same_class_public_state(
                            meta_map, child, actual)
                    value = (
                        domain, child, stratum, terminal, source, ordinal,
                        action, actual)
                    previous = relations.get(relation)
                    if previous is None:
                        relations[relation] = value
                    elif previous[0] != domain or (
                            domain == 0 and previous[:4] != value[:4]):
                        print(
                            f"geometry={geometry} relation={relation} "
                            f"previous_domain={previous[0]} previous_child={previous[1]} "
                            f"previous_stratum={previous[2]} previous_terminal={previous[3]} "
                            f"previous_source={previous[4]} previous_ordinal={previous[5]} "
                            f"previous_action={previous[6]} previous_actual={previous[7]} "
                            f"domain={domain} child={child} stratum={stratum} "
                            f"terminal={terminal} source={source} "
                            f"ordinal={ordinal} action={action} actual={actual} "
                            f"exact={exact} reserved={reserved}")
                        return
            if geometry and geometry % 50_000 == 0:
                print(f"scanned={geometry}", flush=True)
    print(f"same_class_relation_conflicts=0 geometries={len(index) - 1}")


if __name__ == "__main__":
    main()
