#!/usr/bin/env python3
"""Regenerate repaired README cells and plot slices from hash-bound audit logs.

No tablebase downloads or historical AWS certificates are needed. Every requested
class must have a completed solve receipt and its exact exhaustive audit log.
Unchanged classes keep their original proof bindings.
"""
import argparse
import json
from pathlib import Path
import re

import audit_ultimate_checker_start_states as checker
import audit_ultimate_sniper_start_ranks as sniper
import audit_ultimate_angel_start_squares as angel
import audit_ultimate_giant_start_classes as giant
import finalize_ultimate_aws_concrete_class as finalizer
import plot_ultimate_tablebases as plot
import update_ultimate_tablebase_ledger as ledger
from run_ultimate_rules_repair_concrete import sha256

ROOT = Path(__file__).resolve().parents[2]
KINDS = ('total', 'excluded', 'admitted', 'trivial', 'display')
OUTCOMES = ('wins', 'losses', 'draws')


def document(path):
    return json.loads(path.read_text())


def write(path, data):
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n')


def authenticated_log(logs, receipt, kind):
    path = logs / (receipt['filename'] + '.' + kind + '.log')
    if sha256(path) != receipt[kind + '_log_sha256']:
        raise ValueError(f'{kind} log hash mismatch for {receipt["filename"]}')
    return path.read_text()


def checked_receipts(results, logs, materials):
    concrete, information, counts, generations = {}, {}, {}, {}
    for name, material in materials.items():
        receipt = document(results / (name + '.repair.json'))
        if receipt['filename'] != name or receipt['states'] != material['states']:
            raise ValueError('concrete receipt material mismatch')
        solve = authenticated_log(logs, receipt, 'solve')
        if f'verifyok states {material["states"]}' not in solve:
            raise ValueError('missing exhaustive concrete verification')
        match = re.search(r'^output .* edges (\d+) win (\d+) loss (\d+) draw (\d+)$', solve, re.M)
        if not match:
            raise ValueError('missing concrete output census: ' + name)
        generation = dict(zip(('edges', 'win', 'loss', 'draw'), map(int, match.groups())))
        if sum(generation[k] for k in ('win', 'loss', 'draw')) != material['states']:
            raise ValueError('concrete solve conservation residual')
        generations[name] = generation
        concrete[name] = receipt
        audit = authenticated_log(logs, receipt, 'audit')
        counts[name] = finalizer.reporting_counts(name, audit)
        if any(piece in (material['primary'], material.get('secondary'))
               for piece in ('jester', 'ghost')):
            overlay_name = name.replace('.uftb', '.ufiw')
            info = document(results / (overlay_name + '.repair.json'))
            if info['filename'] != overlay_name or info['source_sha256'] != receipt['sha256']:
                raise ValueError('information source binding residual')
            authenticated_log(logs, info, 'solve')
            audit = authenticated_log(logs, info, 'audit')
            binding = (f'information_reachability_binding source_sha256 {receipt["sha256"]} '
                       f'model_sha256 {info["model_sha256"]} ')
            if binding not in audit or 'conservation_residual 0' not in audit:
                raise ValueError('information audit binding residual')
            if 'ghost' in name:
                manifest = results / (Path(name).stem + '.ghost-artifact-manifest.json')
                if sha256(manifest) != info['proof_manifest_sha256']:
                    raise ValueError('Ghost proof inventory mismatch')
                for log_name, binding in info['proof_logs'].items():
                    if sha256(logs / log_name) != binding['sha256']:
                        raise ValueError('Ghost exact proof log mismatch')
                arbitrary = info['arbitrary_sidecar']
                sidecar_receipt = document(results / (arbitrary['filename'] + '.repair.json'))
                if sidecar_receipt['sha256'] != arbitrary['sha256']:
                    raise ValueError('Ghost arbitrary sidecar binding mismatch')
            information[name] = info
            counts[name] = finalizer.information_reporting_counts(name, audit)
    return concrete, information, counts, generations


def cells(filename, raw):
    totals, excluded, trivial = raw
    return tuple(finalizer.render(totals[side], excluded[side], trivial[side])
                 for side in finalizer.ledger_side_order(filename))


def rewrite_readme(text, concrete, information, counts, generations, revision):
    current = {}
    for name in concrete:
        first, second = cells(name, counts[name])
        receipt = information.get(name, concrete[name])
        proof = (f'Corrected native rules 2026-09-12; HF revision `{revision}`; '
                 f'result sha256:{receipt["sha256"]}; '
                 f'solve log sha256:{receipt["solve_log_sha256"]}; '
                 f'root audit sha256:{receipt["audit_log_sha256"]}; '
                 'full verification and root conservation passed. '
                 'See `rules-repair-certificate-20260912.json`.')
        current[name] = first, second, proof
    lines, section = [], None
    seen = set()
    for line in text.splitlines():
        if ledger.RESULT_START in line:
            section = 'results'
        elif ledger.START in line:
            section = 'ledger'
        elif ledger.RESULT_END in line or ledger.END in line:
            section = None
        if line.startswith('| `') and section:
            fields = [f.strip() for f in line.strip().strip('|').split('|')]
            name = fields[0 if section == 'results' else 3].strip('`')
            if name in current:
                first, second, proof = current[name]
                if section == 'results':
                    fields[2] = f'{generations[name]["edges"]:,}'
                    fields[3:5] = [first, second]
                    fields[-1] = '`' + information.get(name, concrete[name])['sha256'] + '`'
                else:
                    fields[7:11] = [first, second, ledger.reachability(first, second), proof]
                    seen.add(name)
                line = '| ' + ' | '.join(fields) + ' |'
        lines.append(line)
    if seen != set(concrete):
        raise ValueError('README repair coverage residual: ' + str(set(concrete) - seen))
    return '\n'.join(lines) + '\n'


def radius_counts(text, slot):
    seen = {}
    for match in checker.CONCRETE_LINE.finditer(text):
        if match[1] != slot:
            continue
        kind = {None: 'excluded', '_total': 'total', '_trivial': 'trivial'}[match[2]]
        substate, side = int(match[3]), int(match[4])
        key = substate, side
        row = seen.setdefault(key, {})
        if kind in row:
            raise ValueError('duplicate Berserker radius row')
        values = list(map(int, match.groups()[4:]))
        if values[0]:
            raise ValueError('unknown radius outcome')
        row[kind] = dict(zip(OUTCOMES, values[1:]))
    output = {}
    for substate in range(10):
        result = {'power_substate': substate}
        for side, side_name in enumerate(('first_starts', 'second_starts')):
            row = seen[(substate, side)]
            if set(row) != {'total', 'excluded', 'trivial'}:
                raise ValueError('incomplete radius audit')
            row['admitted'] = {k: row['total'][k] - row['excluded'][k] for k in OUTCOMES}
            row['display'] = {k: row['admitted'][k] - row['trivial'][k] for k in OUTCOMES}
            if any(v < 0 for bucket in row.values() for v in bucket.values()):
                raise ValueError('radius conservation residual')
            result[side_name] = row
        output[str(substate + 1)] = result
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--results', type=Path, required=True)
    parser.add_argument('--logs', type=Path, required=True)
    parser.add_argument('--materials', type=Path, required=True)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'tablebases')
    args = parser.parse_args()
    if not re.fullmatch('[0-9a-f]{40}', args.revision):
        raise ValueError('a resolved dataset revision is required')
    materials = document(args.materials)
    concrete, information, counts, generations = checked_receipts(args.results, args.logs, materials)
    original = ROOT / 'tablebases'
    text = rewrite_readme((original / 'README.md').read_text(), concrete, information,
                          counts, generations, args.revision)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'README.md').write_text(text)
    aggregates = plot.read_summary(args.output / 'README.md')
    for stem, module, slice_key, piece in (
            ('checker-start-state-summary', checker, 'start_states', 'checker'),
            ('sniper-start-rank-summary', sniper, 'ranks', 'sniper'),
            ('angel-start-square-summary', angel, 'squares', 'angel'),
            ('giant-start-class-summary', giant, 'classes', 'giant'),
            ('berserker-radius-summary', None, 'radii', 'berserker')):
        summary = document(original / (stem + '.json'))
        for name in set(summary['files']) & set(concrete):
            entry = summary['files'][name]
            if entry.get('excluded'):
                continue
            material = materials[name]
            info = name in information
            receipt = information.get(name, concrete[name])
            audit = authenticated_log(args.logs, receipt, 'audit')
            slot = 'primary' if material['primary'] == piece else 'secondary'
            if module is giant:
                slices = giant.parse_counts(audit, slot, info)
            elif module:
                slices = module.parse_counts(audit, material, info)
            else:
                slices = radius_counts(audit, slot)
            if module:
                flipped = module.normalize_and_validate_aggregate(name, slices, aggregates[name], False)
            else:
                flipped = []
                for side_name in ('first_starts', 'second_starts'):
                    actual = plot.WDL(*(sum(row[side_name]['display'][k] for row in slices.values())
                                        for k in OUTCOMES))
                    if actual != getattr(aggregates[name], side_name):
                        raise ValueError('radius aggregate residual')
            entry.update(tablebase_sha256=concrete[name]['sha256'],
                         information_overlay_sha256=information[name]['sha256'] if info else None,
                         audit_binary_sha256=receipt.get('audit_binary_sha256', receipt.get('binary_sha256')),
                         role_normalized_sides=flipped,
                         result_kind='information-v2' if info else 'concrete')
            if not entry['audit_binary_sha256']:
                raise ValueError('missing audit binary binding: ' + name)
            entry[slice_key] = {str(k): v for k, v in slices.items()}
            entry['dataset_revision'] = args.revision
            entry['audit_log_sha256'] = receipt['audit_log_sha256']
        summary['dataset_revision'] = args.revision
        summary['repair_certificate'] = 'rules-repair-certificate-20260912.json'
        audit_binaries = sorted({entry['audit_binary_sha256']
                                 for entry in summary['files'].values()
                                 if not entry.get('excluded')})
        if len(audit_binaries) == 1:
            summary['audit_binary_sha256'] = audit_binaries[0]
            summary.pop('audit_binary_sha256s', None)
        else:
            summary.pop('audit_binary_sha256', None)
            summary['audit_binary_sha256s'] = audit_binaries
        write(args.output / (stem + '.json'), summary)
    reachability = document(original / 'reachability.json')
    for name, receipt in concrete.items():
        audit = authenticated_log(args.logs, receipt, 'audit')
        totals, excluded, trivial = finalizer.reporting_counts(name, audit)
        reachability['files'][name] = dict(sha256=receipt['sha256'], necessary_reachability=excluded,
                                         totals=totals, trivial=trivial,
                                         audit_log_sha256=receipt['audit_log_sha256'])
    write(args.output / 'reachability.json', reachability)
    write(args.output / 'rules-repair-certificate-20260912.json', dict(
        schema='ultimate-fish-rules-repair-certificate-v1', dataset_revision=args.revision,
        concrete=concrete, information=information, generations=generations))
    print(f'PLOTS_REPAIRED concrete={len(concrete)} information={len(information)}')


if __name__ == '__main__':
    main()
