#!/usr/bin/env python3
"""Print side-to-move WDL splits for packed Ultimate tablebases."""

from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path
import struct

import ultimate_tablebase_shards as shards


MAGIC = b"UFTB1\0\0\0"
BYTE_RESULTS = tuple(
    tuple(sum(((byte >> (slot * 2)) & 3) == result for slot in range(4))
          for result in range(4))
    for byte in range(256))


def kings_for(index: int, count: int, substates: int) -> tuple[int, int]:
    placements = count // substates
    placement = index // substates
    if placements == 985_920:
        placement //= 78
        black_rank = placement % 79
        placement //= 79
        white = placement % 80
        black = black_rank + (black_rank >= white)
        return white, black
    pair_states = 3_003 if placements == 18_978_960 else 78 * 77
    placement //= pair_states
    black_rank = placement % 79
    placement //= 79
    white_rank = placement % 40
    white = (white_rank // 4) * 8 + white_rank % 4
    black = black_rank + (black_rank >= white)
    return white, black


def adjacent(first: int, second: int) -> bool:
    return max(abs(first % 8 - second % 8), abs(first // 8 - second // 8)) == 1


def count_results(wdl: bytes, begin: int, end: int) -> list[int]:
    """Count packed two-bit WDL values in [begin, end) without a Python state loop."""
    totals = [0, 0, 0, 0]
    while begin < end and begin % 4:
        totals[(wdl[begin // 4] >> ((begin % 4) * 2)) & 3] += 1
        begin += 1
    aligned_end = end - (end % 4)
    if begin < aligned_end:
        for byte, occurrences in Counter(wdl[begin // 4:aligned_end // 4]).items():
            for result in range(4):
                totals[result] += BYTE_RESULTS[byte][result] * occurrences
        begin = aligned_end
    while begin < end:
        totals[(wdl[begin // 4] >> ((begin % 4) * 2)) & 3] += 1
        begin += 1
    return totals


def adjacent_ranges(count: int, substates: int, side: int):
    """Yield contiguous state ranges for every adjacent canonical King pair."""
    placements = count // substates
    if placements == 985_920:
        white_squares = range(80)
        material_placements = 78
    elif placements == 18_978_960:
        white_squares = ((rank // 4) * 8 + rank % 4 for rank in range(40))
        material_placements = 3_003
    else:
        if placements != 37_957_920:
            raise ValueError(f"unsupported placement count {placements}")
        white_squares = ((rank // 4) * 8 + rank % 4 for rank in range(40))
        material_placements = 78 * 77
    side_placements = placements // 2
    for white in white_squares:
        for black in range(80):
            if black == white or not adjacent(white, black):
                continue
            black_rank = black - (black > white)
            placement = (side * side_placements +
                         (white if placements == 985_920 else
                          (white // 8) * 4 + white % 8) * 79 * material_placements +
                         black_rank * material_placements)
            begin = placement * substates
            yield begin, begin + material_placements * substates


def summary(path: Path, data: bytes | None = None) -> tuple[list[list[int]], list[list[int]]]:
    if data is None:
        data = shards.read_logical(path)
    magic, version, piece, count, _edges = struct.unpack_from("<8sIIII", data)
    if magic != MAGIC or version not in (4, 5, 6):
        raise ValueError(f"{path}: summary requires packed v4/v5/v6")
    substates, wdl_bytes, dtw_bytes, exceptions = struct.unpack_from("<IIII", data, 24)
    offset = 40 + (8 if version >= 5 else 0) + (8 if version >= 6 else 0)
    wdl = data[offset:offset + wdl_bytes]
    if len(wdl) != wdl_bytes or dtw_bytes != count:
        raise ValueError(f"{path}: invalid plane sizes")
    totals = [count_results(wdl, side * count // 2, (side + 1) * count // 2)
              for side in range(2)]
    illegal = [[0, 0, 0, 0] for _ in range(2)]
    # Canonical tables always assign the primary material to Ivory. In same-
    # team classes Onyx is bare; in opposing v5/v6 classes Onyx owns the
    # secondary material as well.
    secondary_color = 0
    if version >= 5:
        _secondary, secondary_color = struct.unpack_from("<II", data, 40)
    owns_material = (True, secondary_color == 1)
    # A live Jester intentionally permits its real King to remain threatened.
    # Inventory ordering makes Jester the primary piece in every class that
    # contains one, so its adjacent-King states are not artifacts. For other
    # classes the dense geometric index includes adjacent Kings. Preserve the
    # established useful-result convention: expected material-owner wins and
    # bare-side losses remain in the headline totals, while adjacent-King
    # artifacts that invent a bare-side win or a material-owner loss are shown
    # parenthetically. The latter matters for stateful continuations such as
    # Prince cont=2, which used to appear as 38,536 genuine losses.
    count_adjacent_as_illegal = piece != 1
    if count_adjacent_as_illegal:
        for side in range(2):
            artifact = 2 if owns_material[side] else 1
            for begin, end in adjacent_ranges(count, substates, side):
                illegal[side][artifact] += count_results(wdl, begin, end)[artifact]
    expected_size = offset + wdl_bytes + dtw_bytes + exceptions * 6
    if len(data) != expected_size:
        raise ValueError(f"{path}: trailing or truncated packed data")
    return totals, illegal


def result_cell(total: int, illegal: int) -> str:
    legal = total - illegal
    return f"{legal:,}" + (f" ({illegal:,})" if illegal else "")


def cell(counts: list[int], illegal: list[int]) -> str:
    return " / ".join(result_cell(counts[result], illegal[result])
                      for result in (1, 2, 3))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="+", type=Path)
    args = parser.parse_args()
    for path in args.files:
        totals, illegal = summary(path)
        print(f"{path.name}\t{cell(totals[0], illegal[0])}\t{cell(totals[1], illegal[1])}")


if __name__ == "__main__":
    main()
