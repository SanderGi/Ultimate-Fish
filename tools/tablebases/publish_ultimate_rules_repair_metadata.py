#!/usr/bin/env python3
"""Publish corrected repair metadata only after independent solver/audit checks.

This does not upload or replace tablebase payloads. It preserves their immutable
revision and checks every managed payload before and after the metadata commit.
"""
import argparse
import json
from pathlib import Path

from finalize_ultimate_rules_repair_plots import checked_receipts, document

DATASET = 'SanderGi/Ultimate-Fish-Tablebases'


def validate_publication(metadata, results, logs, materials):
    # Reopen and authenticate the logs; a self-reported residual in a JSON
    # certificate is not sufficient authorization to publish its counts.
    concrete, information, _, generations = checked_receipts(results, logs, materials)
    proof = document(metadata / 'rules-repair-certificate-20260912.json')
    if proof.get('information_outcome_gate') != 'solver-audit-wdl-equality-v1':
        raise ValueError('publication lacks the mandatory information outcome gate')
    for field, checked in (('concrete', concrete), ('information', information),
                           ('generations', generations)):
        if proof[field] != checked:
            raise ValueError('publication proof differs from authenticated ' + field)
    if len(concrete) != 80 or len(information) != 8:
        raise ValueError('incomplete native-rules repair publication')
    return proof


def payload_inventory(info):
    return {item.rfilename: (item.size, item.lfs.sha256 if item.lfs else None)
            for item in info.siblings if item.rfilename.startswith('tablebases/')}


def publish(metadata, results, logs, materials, expected_revision, *, api=None, dry_run=False):
    proof = validate_publication(metadata, results, logs, materials)
    if proof['dataset_revision'] != expected_revision:
        raise ValueError('metadata must bind the existing immutable payload revision')
    # Import/create the remote client only after the mandatory outcome gate.
    from huggingface_hub import HfApi, CommitOperationAdd, hf_hub_download
    api = api or HfApi()
    before = api.repo_info(DATASET, repo_type='dataset', files_metadata=True)
    if before.sha != expected_revision:
        raise ValueError('dataset changed concurrently; review its new revision')
    inventory = payload_inventory(before)
    for receipt in list(proof['concrete'].values()) + list(proof['information'].values()):
        for item in (receipt, *([receipt['arbitrary_sidecar']] if 'arbitrary_sidecar' in receipt else [])):
            if inventory.get('tablebases/' + item['filename']) != (item['bytes'], item['sha256']):
                raise ValueError('published payload differs from the audited proof')
    if dry_run:
        return dict(revision=before.sha, outcome_gate='passed', payload_files=len(inventory))
    readme = Path(hf_hub_download(DATASET, 'README.md', repo_type='dataset',
                                  revision=expected_revision)).read_text()
    notice = '''## Information plot orientation correction — 2026-09-14

The opposed Ghost/Checker and Ghost/Sniper root audits previously interpreted
the UFIW2 secondary-piece color as the Ghost owner's color, reversing W/L.
The audit reader and plot metadata are corrected. Every repaired information
overlay now passes exact solver-versus-audit W/L/D equality for each side
before trivial-position filtering. The certificate records those checks.
All tablebase and information payload bytes remain unchanged; no solve was
repeated. UFIW2 byte 20 denotes secondary color, with the primary normalized
White; Ghost force bits denote owner/observer, and Jester bits White/Black.

'''
    if '## File roles' not in readme or notice.splitlines()[0] in readme:
        raise ValueError('unexpected dataset README state')
    readme = readme.replace('## File roles', notice + '## File roles', 1)
    commit = api.create_commit(DATASET, repo_type='dataset', parent_commit=expected_revision,
        commit_message='Correct Ghost audit orientation and require solver/audit WDL equality',
        operations=[CommitOperationAdd(path_in_repo='README.md', path_or_fileobj=readme.encode()),
                    CommitOperationAdd(path_in_repo='certificates/ultimate-rules-repair-20260912.json',
                                       path_or_fileobj=metadata / 'rules-repair-certificate-20260912.json')])
    # Persist the commit before checking the remote response, allowing safe
    # recovery if a subsequent network check is interrupted.
    result = dict(revision=commit.oid, payload_revision=expected_revision,
                  outcome_gate='passed', payload_files=len(inventory))
    (metadata / 'orientation-publication.json').write_text(json.dumps(result, indent=2) + '\n')
    after = api.repo_info(DATASET, repo_type='dataset', revision=commit.oid, files_metadata=True)
    if payload_inventory(after) != inventory:
        raise ValueError('metadata publication unexpectedly changed tablebase payloads')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('metadata', 'results', 'logs', 'materials'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-revision', required=True)
    parser.add_argument('--publish', action='store_true')
    args = parser.parse_args()
    print(json.dumps(publish(args.metadata, args.results, args.logs, document(args.materials),
                             args.expected_revision, dry_run=not args.publish), indent=2))


if __name__ == '__main__':
    main()
