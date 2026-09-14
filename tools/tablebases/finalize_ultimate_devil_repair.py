#!/usr/bin/env python3
"""Bind twelve corrected Devil partitions and regenerate their plotted census."""
import argparse
import json
from pathlib import Path
import re

import audit_ultimate_devil_minion_starts as audit
import finalize_ultimate_aws_concrete_class as concrete
import publish_ultimate_devil_stateful_hf as publisher
import update_ultimate_tablebase_ledger as ledger
from run_ultimate_rules_repair_concrete import sha256


def write(path, data):
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--proofs', type=Path, required=True)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--verifier-log', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    baseline = publisher.validate_certificate(args.baseline)
    partitions, progress = [], {}
    for old in baseline['partitions']:
        name = publisher.remote_name(old)
        receipt_path = args.proofs / (name + '.repair.json')
        receipt = json.loads(receipt_path.read_text())
        census_path = args.proofs / (name + '.census.json')
        log_path = args.proofs / (name + '.solve.log')
        if (receipt['rules_revision'] != 2 or receipt['restore_residual'] != 0 or
                receipt['historical_keys_sha256'] != old['sidecar_sha256'] or
                receipt['square'] != old['square'] or receipt['states'] != old['states'] or
                receipt['bytes'] != 32 + 10 * old['states'] or
                sha256(census_path) != receipt['census_sha256'] or
                sha256(log_path) != receipt['verification_log_sha256']):
            raise ValueError('Devil proof authentication failed: ' + name)
        census = json.loads(census_path.read_text())
        marker = (f'DEVIL_SPAWNED_SQUARE_OK square {old["square"]} '
                  f'states {old["states"]} edges ')
        match = re.search(re.escape(marker) + r'(\d+) roots (\d+)', log_path.read_text())
        if not match or any(census[k] for k in (
                'conservation_residual', 'root_filter_conservation_residual', 'sorted_key_residual')):
            raise ValueError('Devil exhaustive verification residual: ' + name)
        if census['states'] != old['states'] or sum(census['outcomes'].values()) != old['states']:
            raise ValueError('Devil state-count residual')
        part = dict(label=old['label'], square=old['square'], states=old['states'],
                    outcomes=census['outcomes'], max_dtw=max(census['max_dtw'].values()),
                    conservation_residual=0, sidecar_sha256=receipt['sha256'],
                    sidecar_format_version=2, census_sha256=receipt['census_sha256'],
                    verification_log_sha256=receipt['verification_log_sha256'],
                    solver_binary_sha256=receipt['binary_sha256'],
                    auditor_sha256=receipt['auditor_sha256'],
                    edges=int(match[1]), roots=int(match[2]),
                    historical_key_source_sha256=old['sidecar_sha256'],
                    repair_receipt_sha256=sha256(receipt_path), restore_residual=0)
        partitions.append(part)
        census['label'] = old['label']
        census['elapsed_seconds'] = receipt.get('elapsed_seconds', 0)
        if receipt['bytes'] > 50_000_000_000:
            manifest = json.loads((args.proofs / name.replace('.ufds', '.ufdsm')).read_text())
            if manifest['sha256'] != receipt['sha256'] or manifest['bytes'] != receipt['bytes']:
                raise ValueError('Devil transport manifest residual')
            census['payloads'] = manifest['parts']
        else:
            census['payloads'] = [dict(filename=name, bytes=receipt['bytes'], sha256=receipt['sha256'])]
        progress[str(old['square'])] = census
    certificate = {key: baseline[key] for key in ('schema', 'material', 'fixed_squares',
        'fixed_square_coverage_residual', 'entry_slice_filename', 'entry_slice_excluded_as_class_proof')}
    certificate.update(rules_revision=2,
        rules='native flying Checker Kings and ChangeTurn terminal threats, 2026-09-12',
        historical_keys_certificate_sha256=sha256(args.baseline),
        historical_dataset_revision='3532f701f81f8ba190d932891818a16af13243b8',
        reused_historical_outcomes=False, partitions=partitions,
        aggregate=dict(states=sum(p['states'] for p in partitions),
            wins=sum(p['outcomes']['win'] for p in partitions),
            losses=sum(p['outcomes']['loss'] for p in partitions),
            draws=sum(p['outcomes']['draw'] for p in partitions),
            max_dtw=max(p['max_dtw'] for p in partitions), conservation_residual=0))
    args.output.mkdir(parents=True, exist_ok=True)
    cert_path = args.output / 'ultimate-devil-stateful-class-certificate.json'
    write(cert_path, certificate)
    publisher.validate_certificate(cert_path)
    audit.validate_certificate(cert_path)
    audit.validate_a1_gate(progress['0'], 2)
    verifier_log = args.verifier_log.read_text()
    match = re.search(r'DEVIL_SLICE_VERIFIED[^\n]*', verifier_log)
    if not match:
        raise ValueError('missing native classifier verification')
    verifier_sources = (audit.VERIFIER_SOURCE, audit.FILTER_SOURCE, audit.POSITION_SOURCE,
                        audit.POSITION_HEADER, audit.NNUE_SOURCE, audit.NNUE_HEADER)
    summary = audit.build_summary(certificate, sha256(cert_path), progress,
        'SanderGi/Ultimate-Fish-Tablebases', args.revision,
        audit.source_bundle_sha256((audit.SOURCE, audit.FILTER_SOURCE)),
        partitions[0]['auditor_sha256'], audit.source_bundle_sha256(verifier_sources), match[0])
    summary_path = args.output / 'devil-minion-start-summary.json'
    write(summary_path, summary)
    cells = []
    for side_index, side in enumerate(('first_starts', 'second_starts')):
        totals = {bucket: [0] + [sum(row[side][bucket][outcome] for row in
            summary['files']['kdevilk.uftb']['minion_counts'].values())
            for outcome in ('wins', 'losses', 'draws')] for bucket in ('total', 'excluded', 'trivial')}
        # Dead-Devil continuations belong to the solved class, but cannot be
        # displayed as roots containing a live Devil. Keep them in the ledger's
        # excluded counts so both side totals still conserve the full domain.
        for index, outcome in enumerate(('win', 'loss', 'draw'), 1):
            full = sum(census['by_side_to_move'][side_index][outcome]
                       for census in progress.values())
            dead = full - totals['total'][index]
            if dead < 0:
                raise ValueError('Devil alive/full side census residual')
            totals['total'][index] = full
            totals['excluded'][index] += dead
        cells.append(concrete.render(totals['total'], totals['excluded'], totals['trivial']))
    readme = args.output / 'README.md'
    lines, changed = [], False
    for line in readme.read_text().splitlines():
        if line.startswith('| `single:devil` |'):
            fields = [part.strip() for part in line.strip().strip('|').split('|')]
            fields[7:11] = [*cells, ledger.reachability(*cells),
                f'Corrected native turn-start rules; twelve UFDS v2 partitions; '
                'entry-root projection excluded as class proof; '
                'all twelve primary planes recomputed; '
                'dead-Devil continuations excluded from displayed roots; '
                f'class certificate sha256:{sha256(cert_path)}; '
                f'plot census sha256:{sha256(summary_path)}; HF revision `{args.revision}`; '
                'all Bellman, census, and storage restore checks passed.']
            line = '| ' + ' | '.join(fields) + ' |'
            changed = True
        lines.append(line)
    if not changed:
        raise ValueError('README lacks lone-Devil ledger row')
    readme.write_text('\n'.join(lines) + '\n')
    print('DEVIL_REPAIR_CERTIFIED', certificate['aggregate'])


if __name__ == '__main__':
    main()
