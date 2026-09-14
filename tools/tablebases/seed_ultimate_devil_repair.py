#!/usr/bin/env python3
"""Recover only authenticated Devil keys; discard every historical outcome.

The current solver rebuilds all edges/seeds, rejects missing successors, and
exhaustively verifies WDL/DTW. A zero-frontier checkpoint only avoids repeating
key discovery; it is never a certificate of a repaired solution.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct

import numpy as np


def seed(source: Path, work: Path, expected_sha256: str) -> dict:
    digest = hashlib.sha256()
    with source.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            digest.update(block)
    if digest.hexdigest() != expected_sha256:
        raise ValueError('source SHA-256 mismatch')
    with source.open('rb') as stream:
        magic, version, square, count, width, reserved = struct.unpack(
            '<8sIIQII', stream.read(32))
        if (magic != b'UFDSV1\0\0' or version != 1 or width != 10 or
                reserved or square not in (0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19) or
                not count or source.stat().st_size != 32 + count * width):
            raise ValueError('invalid historical sidecar')
        work.mkdir(parents=True, exist_ok=False)
        prefix = work / f'devil-{square}'
        previous = -1
        # A small margin satisfies the solver's strict hash-capacity check.
        limit = count + max(10000, count // 100)
        with prefix.with_suffix('.keys').open('xb') as output:
            remaining = count
            while remaining:
                size = min(remaining, 1000000)
                data = stream.read(size * width)
                logical = np.ndarray((size,), dtype='<u8', buffer=data,
                                     strides=(10,)) & ((1 << 56) - 1)
                if (int(logical[0]) <= previous or np.any(logical[1:] <= logical[:-1]) or
                        np.any(logical >> 49)):
                    raise ValueError('unsorted, duplicate, or invalid key')
                previous = int(logical[-1])
                output.write(np.frombuffer(data, dtype=np.uint8).reshape(size, 10)[:, :7].tobytes())
                remaining -= size
            output.truncate(limit * 7)
            output.flush()
            os.fsync(output.fileno())
        prefix.with_suffix('.frontier').write_bytes(b'')
        # Install the checkpoint only after the complete key prefix is durable.
        with prefix.with_suffix('.closure').open('xb') as output:
            output.write(struct.pack('<8sIIQQQI', b'UFDVCP1\0', 4, square,
                                     limit, count, 0, 0))
            output.flush()
            os.fsync(output.fileno())
        receipt = dict(square=square, states=count, state_limit=limit,
                       source_sha256=expected_sha256, reused_outcomes=False)
        (work / 'key-import.json').write_text(json.dumps(receipt, indent=2) + '\n')
        return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('work', type=Path)
    parser.add_argument('--sha256', required=True)
    args = parser.parse_args()
    print(json.dumps(seed(args.source, args.work, args.sha256)), flush=True)
