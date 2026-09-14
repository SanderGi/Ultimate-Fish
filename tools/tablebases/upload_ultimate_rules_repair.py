#!/usr/bin/env python3
"""Upload hash-certified files using temporary LFS actions, without an HF token."""
import argparse
import json
from pathlib import Path

from run_ultimate_rules_repair_concrete import sha256
from upload_ultimate_hf_lfs_action_aws import upload_validated_file


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--actions', type=Path, required=True)
    parser.add_argument('--results', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--workers', type=int, default=16)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    completed = []
    for entry in json.loads(args.actions.read_text())['files']:
        name = entry['filename']
        if Path(name).name != name:
            raise ValueError('unsafe artifact filename')
        path = args.results / name
        if path.stat().st_size != entry['bytes'] or sha256(path) != entry['sha256']:
            raise ValueError('artifact authentication failed: ' + name)
        action = entry['lfs_batch_action']
        if action['oid'] != entry['sha256']:
            raise ValueError('LFS action digest mismatch')
        if action.get('actions'):
            try:
                upload_validated_file(path, entry, args.workers, args.work)
            except Exception as error:
                # Underlying curl exceptions can contain signed URLs.
                raise RuntimeError(f'LFS upload failed for {name}: {type(error).__name__}') from None
        completed.append({key: entry[key] for key in ('filename', 'bytes', 'sha256')})
        temporary = args.receipt.with_suffix('.tmp')
        temporary.write_text(json.dumps({'schema': 'ultimate-fish-lfs-upload-receipt-v1',
                                        'files': completed}, indent=2) + '\n')
        temporary.replace(args.receipt)
        print('LFS_READY', name, entry['sha256'], flush=True)


if __name__ == '__main__':
    main()
