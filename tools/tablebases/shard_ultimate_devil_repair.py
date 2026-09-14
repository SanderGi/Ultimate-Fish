#!/usr/bin/env python3
"""Prepare transport shards from a verified repair, retaining the logical UFDS."""

import argparse
import hashlib
import json
import os
from pathlib import Path

from run_ultimate_rules_repair_concrete import sha256

SHARD_BYTES = 32_000_000_000


def prepare(source, receipt_path):
    receipt = json.loads(receipt_path.read_text())
    if (receipt['rules_revision'] != 2 or receipt['restore_residual'] != 0 or
            source.stat().st_size != receipt['bytes']):
        raise ValueError('source is not a completed, restored repair')
    manifest_path = source.with_suffix('.ufdsm')
    if manifest_path.exists():
        manifest = json.loads(manifest_path.read_text())
        if (manifest['filename'] != source.name or manifest['bytes'] != receipt['bytes'] or
                manifest['sha256'] != receipt['sha256'] or
                sum(p['bytes'] for p in manifest['parts']) != receipt['bytes']):
            raise ValueError('existing manifest does not match the repair')
        for part in manifest['parts']:
            path = source.parent / part['filename']
            if path.stat().st_size != part['bytes'] or sha256(path) != part['sha256']:
                raise ValueError('existing transport shard changed')
        return manifest_path
    parts, whole = [], hashlib.sha256()
    with source.open('rb') as stream:
        remaining = receipt['bytes']
        while remaining:
            size = min(remaining, SHARD_BYTES)
            target = source.with_name(f'{source.stem}-part{len(parts):03d}.ufdsp')
            temporary = target.with_suffix('.ufdsp.partial')
            digest = hashlib.sha256()
            written = 0
            with temporary.open('wb') as output:
                while written < size:
                    block = stream.read(min(8 << 20, size - written))
                    if not block:
                        raise ValueError('truncated logical UFDS')
                    output.write(block)
                    digest.update(block)
                    whole.update(block)
                    written += len(block)
                output.flush()
                os.fsync(output.fileno())
            temporary.replace(target)
            parts.append(dict(filename=target.name, bytes=size, sha256=digest.hexdigest()))
            remaining -= size
        if stream.read(1) or whole.hexdigest() != receipt['sha256']:
            raise ValueError('logical UFDS hash changed; no manifest published')
    manifest = dict(schema='ultimate-fish-ufds-shard-manifest-v1', filename=source.name,
                    bytes=receipt['bytes'], sha256=receipt['sha256'],
                    square=receipt['square'], parts=parts)
    temporary = manifest_path.with_suffix('.ufdsm.partial')
    with temporary.open('w') as output:
        output.write(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
        output.flush()
        os.fsync(output.fileno())
    temporary.replace(manifest_path)
    return manifest_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    manifest = prepare(args.source, args.source.with_name(args.source.name + '.repair.json'))
    print('REPAIR_SHARDS_VERIFIED', manifest.name, sha256(manifest), flush=True)


if __name__ == '__main__':
    main()
