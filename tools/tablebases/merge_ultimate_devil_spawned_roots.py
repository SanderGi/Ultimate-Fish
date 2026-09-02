#!/usr/bin/env python3
"""Authenticate twelve Devil graph root fragments and write UFTB v11.

The fragment graphs are disjoint by the canonical, stationary Devil square.
Only their minion-free indexed roots enter the compact probe file; complete
spawned-Minion keys, reverse edges, and Bellman replays remain certifying proof
artifacts beside each fragment.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct


ROOT_MAGIC = b"UFDVROOT1\0\0\0"
UFTB_MAGIC = b"UFTB1\0\0\0"
ROOT_HEADER = struct.Struct("<12sIIQQQQ")
ROOT_RECORD = struct.Struct("<IBH")
UFTB_HEADER = struct.Struct("<8sIIIIIIIIIIQQ")
VERSION = 11
DEVIL_PIECE = 16
PIECE_COUNT = 30
WHITE = 0
SUBSTATES = 4
PLACEMENT_STATES = 2 * 80 * 79 * 78
STATE_COUNT = PLACEMENT_STATES * SUBSTATES
ADMITTED_STATE_COUNT = 1_183_104
EXCLUDED_STATE_COUNT = STATE_COUNT - ADMITTED_STATE_COUNT
SPAWNED_DEVIL_ROOT_V1_TAG = 0x315256444E505355
EXPECTED_SQUARES = frozenset(
    rank * 8 + file for rank in range(3) for file in range(4)
)


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def decode_attacker(index: int) -> int:
    placement = index // SUBSTATES
    attacker_rank = placement % 78
    placement //= 78
    black_rank = placement % 79
    placement //= 79
    white_king = placement % 80
    black_king = black_rank + (black_rank >= white_king)
    low, high = sorted((white_king, black_king))
    attacker = attacker_rank
    if attacker >= low:
        attacker += 1
    if attacker >= high:
        attacker += 1
    return attacker


def canonical_square(square: int) -> int:
    if square % 8 >= 4:
        return square // 8 * 8 + 7 - square % 8
    return square


def read_fragment(path: Path) -> tuple[dict[str, int | str], list[tuple[int, int, int]]]:
    payload = path.read_bytes()
    if len(payload) < ROOT_HEADER.size:
        raise RuntimeError(f"truncated Devil root fragment: {path}")
    (magic, version, square, limit, states, edges,
     roots) = ROOT_HEADER.unpack_from(payload)
    if magic != ROOT_MAGIC or version != 1 or square not in EXPECTED_SQUARES:
        raise RuntimeError(f"invalid Devil root fragment header: {path}")
    expected = ROOT_HEADER.size + roots * ROOT_RECORD.size
    if len(payload) != expected or not states or states > limit:
        raise RuntimeError(f"Devil root fragment extent residual: {path}")
    records: list[tuple[int, int, int]] = []
    offset = ROOT_HEADER.size
    previous = -1
    for _ in range(roots):
        index, wdl, dtw = ROOT_RECORD.unpack_from(payload, offset)
        offset += ROOT_RECORD.size
        if (index <= previous or index >= STATE_COUNT or wdl not in (1, 2, 3) or
                canonical_square(decode_attacker(index)) != square or
                decode_attacker(index) // 8 >= 3):
            raise RuntimeError(f"Devil root fragment record residual: {path}")
        previous = index
        records.append((index, wdl, dtw))
    return ({"path": str(path), "sha256": sha256_path(path),
             "square": square, "limit": limit, "states": states,
             "edges": edges, "roots": roots}, records)


def merge(paths: list[Path], output: Path, receipt: Path) -> dict[str, object]:
    if len(paths) != len(EXPECTED_SQUARES):
        raise RuntimeError("exactly twelve Devil root fragments are required")
    wdl = bytearray([0xFF]) * ((STATE_COUNT + 3) // 4)  # draw sentinels
    dtw = bytearray(STATE_COUNT)
    exceptions: list[tuple[int, int]] = []
    occupied: set[int] = set()
    summaries: list[dict[str, int | str]] = []
    squares: set[int] = set()
    totals = [0, 0, 0, 0]
    exact_edges = 0
    for path in sorted(paths):
        summary, records = read_fragment(path)
        square = int(summary["square"])
        if square in squares:
            raise RuntimeError(f"duplicate Devil fixed square {square}")
        squares.add(square)
        summaries.append(summary)
        exact_edges += int(summary["edges"])
        for index, value, distance in records:
            if index in occupied:
                raise RuntimeError(f"overlapping Devil root index {index}")
            occupied.add(index)
            shift = (index % 4) * 2
            wdl[index // 4] = (wdl[index // 4] & ~(3 << shift)) | (value << shift)
            dtw[index] = min(distance, 255)
            if distance >= 255:
                exceptions.append((index, distance))
            totals[value] += 1
    if squares != EXPECTED_SQUARES:
        raise RuntimeError("Devil fixed-square coverage residual")
    if len(occupied) != ADMITTED_STATE_COUNT:
        raise RuntimeError(
            f"Devil root coverage residual: {len(occupied)}/"
            f"{ADMITTED_STATE_COUNT}")
    header = UFTB_HEADER.pack(
        UFTB_MAGIC, VERSION, DEVIL_PIECE, STATE_COUNT,
        min(exact_edges, 0xFFFFFFFF), SUBSTATES, len(wdl), len(dtw),
        len(exceptions), PIECE_COUNT, WHITE, exact_edges,
        SPAWNED_DEVIL_ROOT_V1_TAG,
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as stream:
        stream.write(header)
        stream.write(wdl)
        stream.write(dtw)
        for index, distance in exceptions:
            stream.write(struct.pack("<IH", index, distance))
    result: dict[str, object] = {
        "schema": "ultimate-devil-spawned-root-merge-v1",
        "semantics": "first-three-ranks-no-preexisting-minions-max-five-proved",
        "output": {"path": str(output), "bytes": output.stat().st_size,
                   "sha256": sha256_path(output), "version": VERSION,
                   "codec_tag": SPAWNED_DEVIL_ROOT_V1_TAG,
                   "states": STATE_COUNT, "root_records": len(occupied),
                   "admitted_states": ADMITTED_STATE_COUNT,
                   "excluded_states": EXCLUDED_STATE_COUNT,
                   "edges": exact_edges, "win": totals[1],
                   "loss": totals[2], "draw": totals[3],
                   "packed_win": totals[1], "packed_loss": totals[2],
                   "packed_draw": totals[3] + EXCLUDED_STATE_COUNT},
        "fragments": sorted(summaries, key=lambda item: int(item["square"])),
        "fixed_square_residual": 0,
        "root_coverage_residual": 0,
        "duplicate_root_residual": 0,
    }
    receipt.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--receipt", required=True, type=Path)
    parser.add_argument("fragments", nargs="+", type=Path)
    args = parser.parse_args()
    print(json.dumps(merge(args.fragments, args.output, args.receipt),
                     indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
