#!/usr/bin/env python3
"""Recompute the eight affected public-information overlays and root audits."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from validate_ultimate_information_outcomes import validate_information_outcomes

from run_ultimate_rules_repair_concrete import sha256
import ultimate_information_tablebases as information


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--kind', choices=('jester', 'ghost'), required=True)
    parser.add_argument('--workers', type=int, default=32)
    parser.add_argument('--only', action='append')
    args = parser.parse_args()
    source, work = args.source.resolve(), args.work.resolve()
    results, inputs, logs = work / 'results', work / 'inputs', work / 'logs'
    solver = source / 'src/ultimate_tablebase'
    environment = dict(os.environ, ULTIMATE_TABLEBASE_PATH=f'{results}:{inputs}')
    for piece in ('checker', 'sniper'):
        for opposing in (False, True):
            name = f'k{args.kind}{"k" if opposing else ""}{piece}{"" if opposing else "k"}.uftb'
            if args.only and name not in args.only:
                continue
            table = results / name
            while not (results / (name + '.repair.json')).exists():
                time.sleep(10)
            source_sha = sha256(table)
            if source_sha != json.loads((results / (name + '.repair.json')).read_text())['sha256']:
                raise ValueError('concrete source hash mismatch')
            model_sha = information.solver_model_fingerprint(name, root=source)
            overlay = table.with_suffix('.ufiw')
            receipt_path = results / (overlay.name + '.repair.json')
            if receipt_path.exists():
                if sha256(overlay) != json.loads(receipt_path.read_text())['sha256']:
                    raise ValueError('completed information result hash mismatch')
                continue
            class_work = work / 'information' / table.stem
            if class_work.exists():
                raise FileExistsError(class_work)
            class_work.parent.mkdir(parents=True, exist_ok=True)
            if args.kind == "jester":
                class_work.mkdir()
            started = time.time()
            print('START', overlay.name, flush=True)
            common = [str(solver), '--piece', args.kind, '--piece2', piece,
                      '--workers', str(args.workers)]
            if opposing:
                common += ['--opposing']
            binding = ['--information-source-sha256', source_sha,
                       '--information-model-sha256', model_sha]
            solve_log = logs / (overlay.name + '.solve.log')
            if args.kind == 'jester':
                with (inputs / 'kjesterk.ufiw').open('rb') as stream:
                    header = stream.read(160)
                if header[:8] != b'UFIW2\0\0\0' or header[32:96].decode() != sha256(inputs / 'kjesterk.uftb'):
                    raise ValueError('lower Jester binding mismatch')
                command = [*common, '--solve-jester-information', str(table),
                           '--information-overlay', str(overlay), *binding,
                           '--information-scratch', str(class_work),
                           '--lower-information-overlay', str(inputs / 'kjesterk.ufiw'),
                           '--lower-information-source-sha256', header[32:96].decode(),
                           '--lower-information-model-sha256', header[96:160].decode()]
            else:
                ghost = inputs / 'kghostk.ufgm'
                with ghost.open('rb') as stream:
                    header = stream.read(320)
                if header[:8] != b'UFGM1\0\0\0':
                    raise ValueError('lower Ghost binding mismatch')
                lower_sha = (hashlib.sha256(b'ultimate-insufficient-lower-v1:checker').hexdigest()
                             if piece == 'checker' else sha256(results / 'ksniperk.uftb'))
                lower_model = (hashlib.sha256(b'ultimate-insufficient-lower-model-v1:checker').hexdigest()
                               if piece == 'checker' else information.concrete_tablebase_model_fingerprint(
                                   'ksniperk.uftb', root=source))
                command = [sys.executable, str(source / 'tools/tablebases/run_ultimate_ghost_ordinary_aws.py'),
                    '--source-root', str(source), '--work', str(class_work), '--filename', name,
                    '--piece', piece, '--orientation', 'opposing' if opposing else 'same',
                    '--source-table', str(table), '--source-sha256', source_sha,
                    '--model-sha256', model_sha, '--lower-sha256', lower_sha,
                    '--lower-model-sha256', lower_model, '--lower-ghost-sidecar', str(ghost),
                    '--lower-ghost-sha256', sha256(ghost),
                    '--lower-ghost-source-sha256', header[96:160].decode(),
                    '--lower-ghost-model-sha256', header[160:224].decode(),
                    '--lower-ghost-observation-sha256', header[224:288].decode(),
                    '--observation-sha256', information.observation_model_fingerprint(),
                    '--parallelism', str(args.workers), '--solve-workers', str(min(32, args.workers)),
                    '--solve-max-nodes', '1500000000', '--solve-unique-slots', '2147483648']
                if piece == 'sniper':
                    command += ['--lower-table', str(results / 'ksniperk.uftb')]
            with solve_log.open('x') as stream:
                subprocess.run(['/usr/bin/time', '-v', *command], env=environment,
                    stdout=stream, stderr=subprocess.STDOUT, check=True)
            if args.kind == 'ghost':
                for path in (class_work / 'work/results').iterdir():
                    if path.suffix in ('.ufiw', '.ufgd'):
                        shutil.copyfile(path, results / path.name)
                if not overlay.exists():
                    raise RuntimeError('Ghost runner did not produce its canonical overlay')
            audit_log = logs / (overlay.name + '.audit.log')
            audit = [*common, '--audit-information-trivial', str(table),
                     '--information-overlay', str(overlay), *binding]
            if args.kind == 'ghost':
                audit += ['--information-transpose-substates']
            with audit_log.open('x') as stream:
                subprocess.run(['/usr/bin/time', '-v', *audit], env=environment,
                    stdout=stream, stderr=subprocess.STDOUT, check=True)
            outcome_log = (class_work / 'work/logs/solve.log'
                           if args.kind == 'ghost' else solve_log)
            outcome_check = validate_information_outcomes(
                outcome_log.read_text(), audit_log.read_text(), name)
            receipt = dict(schema='ultimate-fish-information-repair-v1', filename=overlay.name,
                           sha256=sha256(overlay), bytes=overlay.stat().st_size,
                           source_sha256=source_sha, model_sha256=model_sha,
                           elapsed_seconds=time.time()-started, workers=args.workers,
                           audit_binary_sha256=sha256(solver),
                           solve_log_sha256=sha256(solve_log), audit_log_sha256=sha256(audit_log),
                           solver_audit_wdl=outcome_check)
            receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
            print(f'COMPLETE {overlay.name} seconds={receipt["elapsed_seconds"]:.1f}', flush=True)


if __name__ == '__main__':
    main()
