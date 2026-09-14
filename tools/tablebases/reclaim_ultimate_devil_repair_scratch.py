#!/usr/bin/env python3
"""Reclaim only reproducible reverse graphs after a repaired export is verified.

Retain the authenticated input, keys, solved node plane, root fragments, packed
result, full audit logs, and receipts. This never deletes cloud resources.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess

from run_ultimate_rules_repair_concrete import sha256


def reverse_files(scratch, square):
    """Enumerate native reverse arrays/spools, preserving solved data and markers."""
    targets = [scratch / f'devil-{square}.{extension}' for extension in
               ('degrees', 'offsets', 'predecessors', 'predecessor-sides')]
    spool = scratch / f'devil-{square}.reverse-spool'
    if spool.is_symlink():
        raise ValueError('reverse spool must not be a symlink')
    if spool.exists():
        targets.extend(path for path in sorted(spool.iterdir()) if re.fullmatch(
            r'shard-\d+-bucket-\d+\.edges(?:\.sorted)?', path.name))
    existing = []
    for path in targets:
        if path.is_symlink():
            raise ValueError('reverse scratch must not be a symlink')
        if path.exists():
            if not path.is_file():
                raise ValueError('reverse scratch must be a regular file')
            existing.append(path)
    return existing


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--bucket', required=True)
    args = parser.parse_args()
    work = args.work.resolve()
    if shutil.disk_usage(work).free >= 1024 << 30:
        return
    for path in sorted((work / 'results').glob('*.ufds.repair.json')):
        receipt = json.loads(path.read_text())
        square = receipt['square']
        name = path.name.removesuffix('.repair.json')
        if (square not in (0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19) or
                name != f'kdevilk-{chr(97 + square % 8)}{square // 8 + 1}.ufds' or
                receipt['rules_revision'] != 2):
            raise ValueError('unsupported repaired partition')
        label = name.removeprefix('kdevilk-').removesuffix('.ufds')
        scratch = work / 'work' / ('devil-' + label)
        marker = scratch / 'reverse-graph-reclaimed.json'
        targets = reverse_files(scratch, square)
        if not targets:
            continue
        prior = json.loads(marker.read_text()) if marker.exists() else None
        if prior and prior['repaired_sha256'] != receipt['sha256']:
            raise ValueError('prior reclamation belongs to another repair')
        remote = subprocess.check_output(['aws', 's3', 'cp',
            f's3://{args.bucket}/repaired/devil/{path.name}', '-',
            '--region', 'us-west-2', '--only-show-errors'])
        if (json.loads(remote) != receipt or receipt['restore_residual'] != 0 or
                sha256(work / 'results' / name) != receipt['sha256']):
            raise ValueError('repaired export is not restore-authenticated')
        census = work / 'results' / (name + '.census.json')
        if sha256(census) != receipt['census_sha256']:
            raise ValueError('census authentication failed')
        removed = []
        for target in targets:
            removed.append(dict(filename=str(target.relative_to(scratch)),
                                bytes=target.stat().st_size))
            target.unlink()
        temporary = marker.with_suffix('.tmp')
        temporary.write_text(json.dumps(dict(schema='ultimate-fish-devil-scratch-reclamation-v2',
            repaired_sha256=receipt['sha256'],
            removed=(prior['removed'] if prior else []) + removed), indent=2) + '\n')
        temporary.replace(marker)
        subprocess.run(['aws', 's3', 'cp', str(marker),
            f's3://{args.bucket}/repaired/devil/{name}.scratch-reclaimed.json',
            '--region', 'us-west-2', '--only-show-errors'], check=True)
        print('REVERSE_GRAPH_RECLAIMED', name, sum(row['bytes'] for row in removed), flush=True)
        if shutil.disk_usage(work).free >= 1300 << 30:
            break


if __name__ == '__main__':
    main()
