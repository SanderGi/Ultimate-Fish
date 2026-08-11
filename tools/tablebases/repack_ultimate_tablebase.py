#!/usr/bin/env python3
"""Losslessly repack Ultimate tablebase v2/v3 records into split-plane v4."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import struct


HEADER = struct.Struct("<8sIIIII")
MAGIC = b"UFTB1\0\0\0"


def repack(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise ValueError(f"{path}: truncated header")
    magic, version, piece, count, edges, first = HEADER.unpack_from(data)
    if magic != MAGIC or version not in (2, 3):
        raise ValueError(f"{path}: expected a v2/v3 Ultimate tablebase")
    if version == 2:
        substates = 1
        offset = 24
    else:
        substates = first
        offset = HEADER.size
    records_bytes = count * 2
    if len(data) != offset + records_bytes:
        raise ValueError(f"{path}: record count does not match file size")

    wdl = bytearray((count + 3) // 4)
    dtw = bytearray(count)
    exceptions: list[tuple[int, int]] = []
    for index, (packed,) in enumerate(struct.iter_unpack("<H", data[offset:])):
        result = packed >> 14
        distance = packed & 0x3FFF
        if result not in (1, 2, 3):
            raise ValueError(f"{path}: invalid WDL at record {index}")
        wdl[index // 4] |= result << ((index % 4) * 2)
        dtw[index] = min(distance, 255)
        if distance >= 255:
            exceptions.append((index, distance))

    output = bytearray(struct.pack("<8sIIII", MAGIC, 4, piece, count, edges))
    output.extend(struct.pack("<IIII", substates, len(wdl), len(dtw), len(exceptions)))
    output.extend(wdl)
    output.extend(dtw)
    for index, distance in exceptions:
        output.extend(struct.pack("<IH", index, distance))
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(output)
    os.replace(temporary, path)
    return len(data), len(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="+", type=Path)
    args = parser.parse_args()
    for path in args.files:
        before, after = repack(path)
        print(f"{path}: {before} -> {after} bytes ({after / before:.1%})")


if __name__ == "__main__":
    main()
