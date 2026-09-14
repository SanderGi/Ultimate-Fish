#!/usr/bin/env python3
"""Export verified, already-sorted repair keys without another external sort."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct

import numpy as np


def export(work: Path, output: Path, verification_log: Path) -> dict:
    imported = json.loads((work / 'key-import.json').read_text())
    square, count = imported['square'], imported['states']
    marker = f'DEVIL_SPAWNED_SQUARE_OK square {square} states {count} '
    log = verification_log.read_bytes()
    if marker.encode() not in log:
        raise ValueError('missing exhaustive corrected-solver verification')
    prefix = work / f'devil-{square}'
    if prefix.with_suffix('.nodes').stat().st_size != count * 8:
        raise ValueError('node plane extent mismatch')
    if output.exists():
        raise FileExistsError(output)
    temporary = output.with_suffix(output.suffix + '.tmp')
    header = struct.pack('<8sIIQII', b'UFDSV1\0\0', 2, square, count, 10, 0)
    digest = hashlib.sha256(header)
    previous = -1
    with prefix.with_suffix('.keys').open('rb') as keys, \
         prefix.with_suffix('.nodes').open('rb') as nodes, temporary.open('xb') as target:
        target.write(header)
        remaining = count
        while remaining:
            size = min(remaining, 1000000)
            key_block = np.frombuffer(keys.read(size * 7), dtype=np.uint8).reshape(size, 7)
            node_block = np.frombuffer(nodes.read(size * 8), dtype=np.uint8).reshape(size, 8)
            logical = np.zeros(size, dtype='<u8')
            logical.view(np.uint8).reshape(size, 8)[:, :7] = key_block
            if (int(logical[0]) <= previous or np.any(logical[1:] <= logical[:-1]) or
                    np.any(logical >> 49) or np.any(node_block[:, 0] < 1) or
                    np.any(node_block[:, 0] > 3)):
                raise ValueError('unsorted keys or unsolved WDL')
            previous = int(logical[-1])
            records = np.empty((size, 10), dtype=np.uint8)
            records[:, :7] = key_block
            records[:, 7] = node_block[:, 0]
            records[:, 8:10] = node_block[:, 2:4]
            block = records.tobytes()
            digest.update(block)
            target.write(block)
            remaining -= size
        target.flush()
        os.fsync(target.fileno())
    temporary.replace(output)
    return dict(schema='ultimate-fish-devil-repair-export-v1', square=square,
                states=count, bytes=output.stat().st_size, sha256=digest.hexdigest(),
                rules_revision=2, historical_keys_sha256=imported['source_sha256'],
                verification_log_sha256=hashlib.sha256(log).hexdigest())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--verification-log', type=Path, required=True)
    args = parser.parse_args()
    receipt = export(args.work, args.output, args.verification_log)
    args.output.with_name(args.output.name + '.repair.json').write_text(
        json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt), flush=True)
