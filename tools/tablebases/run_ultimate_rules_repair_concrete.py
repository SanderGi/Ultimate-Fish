#!/usr/bin/env python3
"""Sequential corrected-rule solves, exhaustive verification and root audits.

Inputs are prepared separately from an authenticated dataset revision. Completed
classes have immutable hash receipts; incomplete scratch is never resumed or
truncated by a second invocation. This runner does not publish results.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--materials', type=Path, required=True)
    parser.add_argument('--workers', type=int, default=64)
    parser.add_argument('--only', action='append')
    args = parser.parse_args()
    binary = args.source.resolve() / 'src/ultimate_tablebase'
    work = args.work.resolve()
    results, logs, scratch = (work / name for name in ('results', 'logs', 'concrete'))
    for path in (results, logs, scratch):
        path.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, ULTIMATE_TABLEBASE_PATH=f'{results}:{work / "inputs"}')
    materials = json.loads(args.materials.read_text())
    selected = sorted(materials, key=lambda name: (
        'pawn' in name, int(materials[name].get('states', 0)), name))
    for name in selected:
        if args.only and name not in args.only:
            continue
        record = materials[name]
        if any(piece in (record.get('primary'), record.get('secondary'))
               for piece in ('devil', 'sludge')):
            raise ValueError('material is outside the repaired concrete closure')
        receipt_path = results / (name + '.repair.json')
        if receipt_path.exists():
            receipt = json.loads(receipt_path.read_text())
            if sha256(results / name) != receipt['sha256']:
                raise ValueError('completed result hash changed')
            continue
        class_work = scratch / name.removesuffix('.uftb')
        class_work.mkdir(exist_ok=False)
        command = [str(binary), '--piece', record['primary'],
                   '--workers', str(args.workers)]
        if record.get('secondary'):
            command += ['--piece2', record['secondary']]
        if record.get('opposing'):
            command += ['--opposing']
        output = results / name
        if output.exists():
            raise ValueError('unreceipted output exists; verify it before continuing')
        started = time.time()
        print(f'START {name}', flush=True)
        solve_log = logs / (name + '.solve.log')
        with solve_log.open('x') as stream:
            subprocess.run(['/usr/bin/time', '-v', *command,
                '--output', str(output), '--checkpoint', str(class_work / 'state'),
                '--checkpoint-every', '0', '--disk-backed'],
                env=environment, stdout=stream, stderr=subprocess.STDOUT, check=True)
        if f'verifyok states {record["states"]}' not in solve_log.read_text():
            raise RuntimeError('missing exhaustive verification result')
        audit_log = logs / (name + '.audit.log')
        audit_option = ('--audit-turn-boundary-reachability'
                        if 'prince' in (record.get('primary'), record.get('secondary'))
                        else '--audit-reachability')
        with audit_log.open('x') as stream:
            subprocess.run(['/usr/bin/time', '-v', *command,
                audit_option, str(output)], env=environment,
                stdout=stream, stderr=subprocess.STDOUT, check=True)
        receipt = dict(schema='ultimate-fish-rules-repair-v1', filename=name,
                       rules_revision='checker-rays-and-native-turn-start-20260912',
                       sha256=sha256(output), bytes=output.stat().st_size,
                       binary_sha256=sha256(binary), states=record['states'],
                       workers=args.workers, started_unix=started,
                       elapsed_seconds=time.time()-started,
                       solve_log_sha256=sha256(solve_log),
                       audit_log_sha256=sha256(audit_log))
        temporary = receipt_path.with_suffix('.tmp')
        temporary.write_text(json.dumps(receipt, indent=2) + '\n')
        temporary.replace(receipt_path)
        print(f'COMPLETE {name} seconds={receipt["elapsed_seconds"]:.1f}', flush=True)


if __name__ == '__main__':
    main()
