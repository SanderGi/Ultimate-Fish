#!/usr/bin/env python3
"""Rebuild Devil outcomes from authenticated keys, verify, export, and audit."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from seed_ultimate_devil_repair import seed
from export_ultimate_devil_repair import export
from run_ultimate_rules_repair_concrete import sha256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--certificate', type=Path, required=True)
    parser.add_argument('--workers', type=int, default=96)
    parser.add_argument('--square', action='append', type=int)
    parser.add_argument('--bucket', required=True)
    args = parser.parse_args()
    source, work = args.source.resolve(), args.work.resolve()
    results, logs = work / 'results', work / 'logs'
    results.mkdir(exist_ok=True)
    logs.mkdir(exist_ok=True)
    solver = source / 'src/ultimate_tablebase'
    auditor = work / 'audit-devil'
    subprocess.run(['c++', '-std=c++17', '-O3', '-DNDEBUG', '-pthread',
                    str(source / 'tools/tablebases/audit_ultimate_devil_stateful_sidecar.cpp'),
                    '-o', str(auditor)], check=True)
    certificate = json.loads(args.certificate.read_text())
    for partition in sorted(certificate['partitions'], key=lambda row: row['states']):
        square, label, count = partition['square'], partition['label'].lower(), partition['states']
        if args.square and square not in args.square:
            continue
        name = f'kdevilk-{label}.ufds'
        destination = results / name
        receipt_path = results / (name + '.repair.json')
        if receipt_path.exists():
            if sha256(destination) != json.loads(receipt_path.read_text())['sha256']:
                raise ValueError('completed Devil result hash mismatch')
            continue
        if shutil.disk_usage(work).free < 600 << 30:
            raise RuntimeError('less than 600 GiB free before Devil partition')
        historical = work / 'inputs' / name
        if not historical.exists():
            manifest = json.loads(historical.with_suffix('.ufdsm').read_text())
            if manifest['sha256'] != partition['sidecar_sha256']:
                raise ValueError('historical shard manifest/certificate mismatch')
            with historical.open('xb') as target:
                for part in manifest['parts']:
                    with (historical.parent / part['filename']).open('rb') as stream:
                        shutil.copyfileobj(stream, target, 8 << 20)
        class_work = work / 'work' / f'devil-{label}'
        started = time.time()
        print(f'START {name} states={count}', flush=True)
        imported = seed(historical, class_work, partition['sidecar_sha256'])
        if imported['states'] != count:
            raise ValueError('imported count differs from historical certificate')
        solve_log = logs / (name + '.solve.log')
        with solve_log.open('x') as stream:
            subprocess.run(['/usr/bin/time', '-v', str(solver), '--piece', 'devil',
                '--workers', str(args.workers), '--solve-devil-spawned-square', str(square),
                '--devil-spawned-limit', str(imported['state_limit']),
                '--devil-spawned-hash-capacity', str(imported['state_limit']),
                '--devil-spawned-work', str(class_work)], stdout=stream,
                stderr=subprocess.STDOUT, check=True)
        receipt = export(class_work, destination, solve_log)
        census_path = results / (name + '.census.json')
        with census_path.open('x') as stream:
            subprocess.run([str(auditor), '--input', str(destination), '--square', str(square),
                            '--workers', str(args.workers)], stdout=stream, check=True)
        census = json.loads(census_path.read_text())
        if any(census[key] != 0 for key in ('conservation_residual', 'sorted_key_residual',
                                          'root_filter_conservation_residual')):
            raise ValueError('Devil independent census residual')
        key = f'repaired/devil/{name}'
        subprocess.run(['aws', 's3', 'cp', str(destination), f's3://{args.bucket}/{key}',
                        '--region', 'us-west-2', '--only-show-errors'], check=True)
        restored = subprocess.Popen(['aws', 's3', 'cp', f's3://{args.bucket}/{key}', '-',
                        '--region', 'us-west-2', '--only-show-errors'], stdout=subprocess.PIPE)
        digest = hashlib.sha256()
        for block in iter(lambda: restored.stdout.read(8 << 20), b''):
            digest.update(block)
        if restored.wait() != 0 or digest.hexdigest() != receipt['sha256']:
            raise ValueError('S3 restore hash residual')
        receipt.update(elapsed_seconds=time.time()-started, workers=args.workers,
                       binary_sha256=sha256(solver), auditor_sha256=sha256(auditor),
                       census_sha256=sha256(census_path), s3_key=key, restore_residual=0)
        receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
        for path in (receipt_path, census_path, solve_log):
            subprocess.run(['aws', 's3', 'cp', str(path),
                f's3://{args.bucket}/repaired/devil/{path.name}', '--region', 'us-west-2',
                '--only-show-errors'], check=True)
        print(f'COMPLETE {name} seconds={receipt["elapsed_seconds"]:.1f}', flush=True)


if __name__ == '__main__':
    main()
