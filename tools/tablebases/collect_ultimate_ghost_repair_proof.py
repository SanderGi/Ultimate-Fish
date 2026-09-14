#!/usr/bin/env python3
"""Authenticate and collect a completed Ghost repair's full proof inventory."""
import argparse
import json
from pathlib import Path
import re
import shutil

from run_ultimate_rules_repair_concrete import sha256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--filename', required=True)
    args = parser.parse_args()
    name = args.filename
    if name not in ('kghostcheckerk.uftb', 'kghostkchecker.uftb',
                    'kghostsniperk.uftb', 'kghostksniper.uftb'):
        raise ValueError('unsupported repaired Ghost material')
    stem = Path(name).stem
    work = args.work.resolve()
    original = work / 'information' / stem
    results, logs = work / 'results', work / 'logs'
    receipt_path = results / (stem + '.ufiw.repair.json')
    receipt = json.loads(receipt_path.read_text())
    manifest_path = original / 'work/artifact-manifest.json'
    manifest = json.loads(manifest_path.read_text())
    if (manifest['filename'] != name or manifest['source_sha256'] != receipt['source_sha256'] or
            manifest['model_sha256'] != receipt['model_sha256']):
        raise ValueError('Ghost proof source/model mismatch')
    copied_logs = {}
    for relative, binding in manifest['files'].items():
        path = original / relative
        if not path.resolve().is_relative_to(original):
            raise ValueError('unsafe proof inventory path')
        if path.stat().st_size != binding['bytes'] or sha256(path) != binding['sha256']:
            raise ValueError('Ghost proof file hash mismatch: ' + relative)
        if path.suffix == '.log':
            destination = logs / (stem + '.ghost-' + path.name)
            shutil.copyfile(path, destination)
            copied_logs[destination.name] = binding
    sidecar = results / (stem + '.ufgd')
    sidecar_binding = manifest['files']['work/results/' + sidecar.name]
    if sha256(sidecar) != sidecar_binding['sha256']:
        raise ValueError('copied arbitrary sidecar mismatch')
    if sha256(results / (stem + '.ufiw')) != receipt['sha256']:
        raise ValueError('copied overlay mismatch')
    log = (original / 'work/logs/solve.log').read_text()
    match = re.search(r'dragon_ghost_certificate dual_force_residual 0 structural_residual 0 '
                     r'singleton_residual 0 source_remap_residual 0 normalized_source_sha256 ([0-9a-f]{64}) '
                     r'transition_payload_sha256 ([0-9a-f]{64}) arbitrary_sha256 ([0-9a-f]{64})', log)
    if not match or match[3] != sidecar_binding['sha256']:
        raise ValueError('missing exact Ghost certificate or arbitrary hash binding')
    preserved = results / (stem + '.ghost-artifact-manifest.json')
    shutil.copyfile(manifest_path, preserved)
    receipt.update(proof_manifest_sha256=sha256(preserved), proof_logs=copied_logs,
        solver_binary_sha256=sha256(original / 'ultimate_ghost_ordinary_information_tablebase'),
        normalized_source_sha256=match[1], transition_payload_sha256=match[2],
        arbitrary_sidecar=dict(filename=sidecar.name, **sidecar_binding))
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    sidecar_receipt = dict(schema='ultimate-fish-ghost-sidecar-repair-v1',
        filename=sidecar.name, **sidecar_binding,
        source_sha256=receipt['source_sha256'], model_sha256=receipt['model_sha256'],
        proof_manifest_sha256=receipt['proof_manifest_sha256'],
        solve_log_sha256=sha256(original / 'work/logs/solve.log'))
    (results / (sidecar.name + '.repair.json')).write_text(json.dumps(sidecar_receipt, indent=2) + '\n')
    print('GHOST_REPAIR_PROOF_COLLECTED', name, len(copied_logs), sidecar_binding['sha256'])


if __name__ == '__main__':
    main()
