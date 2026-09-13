#!/usr/bin/env python3
"""Record native full-circuit responses to legal actions for the waiting display.
Run from the repository root after tools/fly/build-native.sh. No fabricated spikes.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'ui/public/fly/data'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def record():
    graph = ROOT / 'networks/fly/connectome.bin'
    manifest = json.loads((OUT / 'manifest.json').read_text())
    assert digest(graph) == manifest['binarySha256']
    child = subprocess.Popen([str(ROOT / 'src/ultimate_fly_brain'), str(graph)],
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    assert child.stdout.readline().strip() == 'ready 2129'
    def command(line):
        child.stdin.write(line + '\n'); child.stdin.flush()
        return json.loads(child.stdout.readline())
    clips, payload = [], bytearray()
    started = time.monotonic()
    try:
        for preset in range(3):
            moves = []
            for ply in range(49):
                request = f'{preset} 0 {len(moves)}\n' + ' '.join(map(str, moves)) + '\n'
                state = json.loads(subprocess.check_output([str(ROOT / 'src/ultimate_fly_rules')], input=request, text=True))
                if state['terminal']: break
                move = state['moves'][(ply * 31 + 17) % len(state['moves'])]
                if ply % 8 == 0:
                    command('1 ' + ' '.join(map(str, move['features'])))
                    result = command('0')
                    assert len(result['frames']) == 8
                    for frame in result['frames']:
                        assert len(frame) == (manifest['neurons'] + 7) // 8
                        assert all(0 <= x <= 255 for x in frame)
                    clips.append({'preset': preset, 'ply': ply, 'key': state['key'],
                                  'move': move['notation'], 'features': move['features'],
                                  'byteOffset': len(payload), 'active': result['active']})
                    for frame in result['frames']: payload.extend(frame)
                moves.append(move['index'])
    finally:
        child.stdin.close(); child.wait(timeout=10)
    assert len(clips) >= 6
    (OUT / 'recorded-activity.bin').write_bytes(payload)
    metadata = {'version': 1, 'source': 'Native brain_encode full-circuit runs on legal Ultimate actions',
                'neurons': manifest['neurons'], 'connections': manifest['edges'],
                'circuitSha256': manifest['binarySha256'], 'runtimeSha256': digest(ROOT / 'tools/fly/brain.cpp'),
                'binarySha256': hashlib.sha256(payload).hexdigest(), 'stride': 8,
                'samples': (manifest['neurons'] + 7) // 8, 'framesPerClip': 8,
                'encoding': 'uint8 floor(abs(activity) * 255), frame-major',
                'displayFrameMs': 140, 'clips': clips}
    (OUT / 'recorded-activity.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(f'Recorded {len(clips)} full-circuit runs, {len(payload):,} bytes in {time.monotonic()-started:.2f}s')

if __name__ == '__main__': record()
