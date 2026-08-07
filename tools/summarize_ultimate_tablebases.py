#!/usr/bin/env python3
"""Print side-to-move WDL splits for packed Ultimate tablebases."""

from __future__ import annotations

import argparse
from pathlib import Path
import struct

import ultimate_tablebase_shards as shards


MAGIC = b"UFTB1\0\0\0"


def kings_for(index: int, count: int, substates: int) -> tuple[int, int]:
    if count // substates == 985_920:
        placement = index // substates
        placement //= 78
        black_rank = placement % 79
        placement //= 79
        white = placement % 80
        black = black_rank + (black_rank >= white)
        return white, black
    pair_states = 3_003 if count == 18_978_960 else 78 * 77
    placement = index // pair_states
    black_rank = placement % 79
    placement //= 79
    white_rank = placement % 40
    white = (white_rank // 4) * 8 + white_rank % 4
    black = black_rank + (black_rank >= white)
    return white, black


def adjacent(first: int, second: int) -> bool:
    return max(abs(first % 8 - second % 8), abs(first // 8 - second // 8)) == 1


def summary(path: Path, data: bytes | None = None) -> tuple[list[list[int]], list[int]]:
    if data is None:
        data = shards.read_logical(path)
    magic, version, piece, count, _edges = struct.unpack_from("<8sIIII", data)
    if magic != MAGIC or version not in (4, 5):
        raise ValueError(f"{path}: summary requires packed v4/v5")
    substates, wdl_bytes, dtw_bytes, exceptions = struct.unpack_from("<IIII", data, 24)
    offset = 40 + (8 if version == 5 else 0)
    wdl = data[offset:offset + wdl_bytes]
    if len(wdl) != wdl_bytes or dtw_bytes != count:
        raise ValueError(f"{path}: invalid plane sizes")
    totals = [[0, 0, 0, 0] for _ in range(2)]
    bare_adjacent_wins = [0, 0]
    # Canonical v4/v5 tables always assign the first material owner to Ivory.
    # In same-team classes Onyx is bare; in opposing v5 classes neither side is.
    secondary_color = 0
    if version == 5:
        _secondary, secondary_color = struct.unpack_from("<II", data, 40)
    # A live Jester intentionally permits its real King to remain threatened,
    # so adjacent-King wins by the bare side are reachable in that class.
    bare_side = 1 if (version == 4 or secondary_color == 0) and piece != 1 else -1
    for index in range(count):
        side = 0 if index < count // 2 else 1
        result = (wdl[index // 4] >> ((index % 4) * 2)) & 3
        totals[side][result] += 1
        if side == bare_side and result == 1:
            white, black = kings_for(index, count, substates)
            if adjacent(white, black):
                bare_adjacent_wins[side] += 1
    expected_size = offset + wdl_bytes + dtw_bytes + exceptions * 6
    if len(data) != expected_size:
        raise ValueError(f"{path}: trailing or truncated packed data")
    return totals, bare_adjacent_wins


def cell(counts: list[int], illegal_wins: int) -> str:
    win = counts[1] - illegal_wins
    win_text = f"{win:,}" + (f" ({illegal_wins:,})" if illegal_wins else "")
    return f"{win_text} / {counts[2]:,} / {counts[3]:,}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="+", type=Path)
    args = parser.parse_args()
    for path in args.files:
        totals, illegal = summary(path)
        print(f"{path.name}\t{cell(totals[0], illegal[0])}\t{cell(totals[1], illegal[1])}")


if __name__ == "__main__":
    main()
