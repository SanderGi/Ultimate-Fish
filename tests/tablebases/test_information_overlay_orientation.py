"""Serializer -> native dense reader/audit -> publication gate -> plot regressions."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/tablebases'))
import finalize_ultimate_rules_repair_plots as repair
import finalize_ultimate_aws_concrete_class as finalizer
import plot_ultimate_tablebases as plot
import publish_ultimate_rules_repair_metadata as publisher
from validate_ultimate_information_outcomes import validate_information_outcomes


def index(side, wk, bk, first, second, substates, substate):
    # The real dense codec folds horizontal reflection into the White king's
    # left-half anchor. Source-side indices are always normalized primary White.
    if wk % 8 >= 4:
        wk, bk, first, second = [s // 8 * 8 + 7 - s % 8 for s in (wk, bk, first, second)]
    rank = lambda square, excluded: square - sum(s < square for s in excluded)
    result = (side * 40 + wk // 8 * 4 + wk % 8) * 79 + rank(bk, [wk])
    result = result * 78 + rank(first, [wk, bk])
    result = result * 77 + rank(second, [wk, bk, first])
    return result * substates + substate


def solver_summary(counts, half):
    return ''.join(f'information_summary side {side} win {w} loss {l} draw {d} '
                   f'unreachable_win 0 unreachable_loss 0 unreachable_draw {half-w-l-d} '
                   f'sets {w+l+d} concrete {half} bellman_residual 0 rank_residual 0 '
                   'belief_cap none exhaustive 1\n'
                   for side, (w, l, d) in enumerate(counts))


class InformationOverlayOrientationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.work = Path(cls.temp.name)
        cls.writer = cls.work / 'writer'
        subprocess.run(['c++', '-std=c++17', '-O2', '-I'+str(ROOT/'src/ultimate'),
                        '-I'+str(ROOT/'src/ultimate/tablebases'),
                        str(ROOT/'tests/tablebases/ultimate_information_overlay_fixture.cpp'),
                        '-o', str(cls.writer)], check=True)
        subprocess.run(['make', '-C', str(ROOT/'src'), 'ultimate-tablebase'],
                       check=True, stdout=subprocess.DEVNULL)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_serialized_roles_through_native_audit_and_plot(self):
        for primary, secondary, primary_name, secondary_name, substates in (
                (11, 21, 'ghost', 'checker', 8),
                (11, 19, 'ghost', 'sniper', 8),
                (5, 11, 'rook', 'ghost', 2)):
            for opposed in (False, True):
                with self.subTest(primary=primary_name, secondary=secondary_name, opposed=opposed):
                    count = 37_957_920 * substates
                    table, overlay = self.work/'source.uftb', self.work/'result.ufiw'
                    version, tag = (12, 0x3132393036524655) if primary == 11 else (6, 0)
                    header = struct.pack('<8s10IQ', b'UFTB1\0\0\0', version, primary,
                        count, 0, substates, count//4, count, 0, secondary, int(opposed), 0)
                    if version == 12: header += struct.pack('<Q', tag)
                    with table.open('wb') as f:
                        f.write(header)
                        remaining = count//4
                        while remaining:
                            chunk = min(remaining, 1<<20);f.write(b'\xff'*chunk);remaining -= chunk
                        f.truncate(len(header)+count//4+count)
                    source_sha = repair.sha256(table)
                    subprocess.run([str(self.writer), str(overlay), str(primary), str(secondary),
                                    str(int(opposed)), str(count), str(substates), source_sha, 'b'*64], check=True)
                    expected = ((1, 2, 3), (3, 1, 2))
                    owner = 0 if primary == 11 else int(opposed)
                    with overlay.open('r+b') as f:
                        f.truncate(160+count)
                        for side, outcomes in enumerate(expected):
                            j = 0
                            for result, multiplicity in enumerate(outcomes, 1):
                                for _ in range(multiplicity):
                                    ghost = (16, 18, 19, 20, 22, 23)[j]
                                    first, second = (ghost, 53) if primary == 11 else (53, ghost)
                                    visible = j % 2
                                    substate = visible if primary == 11 else visible
                                    encoded = index(side, 1, 79, first, second, substates, substate)
                                    winner = side if result == 1 else 1-side
                                    flag = 4 if result == 3 else 4 | (1 if winner == owner else 2)
                                    f.seek(160+encoded);f.write(bytes([flag]));j += 1
                    command = [str(ROOT/'src/ultimate_tablebase'), '--piece', primary_name,
                        '--piece2', secondary_name, '--workers', '4', '--audit-information-trivial', str(table),
                        '--information-overlay', str(overlay), '--information-source-sha256', source_sha,
                        '--information-model-sha256', 'b'*64]
                    if primary == 11: command += ['--information-transpose-substates']
                    if opposed: command += ['--opposing']
                    audit = subprocess.check_output(command, text=True)
                    summary = solver_summary(expected, count//2)
                    validate_information_outcomes(summary, audit, 'fixture')
                    name = ('k' + primary_name + ('k' if opposed else '') +
                            secondary_name + ('' if opposed else 'k') + '.uftb')
                    total, excluded, trivial = finalizer.information_reporting_counts(name, audit)
                    # These deliberately quiet fixtures must survive the real
                    # native trivial filter, keeping all three outcome labels.
                    self.assertEqual([[0]*4, [0]*4], trivial)
                    cells = repair.cells(name, (total, excluded, trivial))
                    readme = self.work/'README.md'
                    readme.write_text('<!-- GENERATED_TABLE_START -->\n' +
                        f'| `{name}` | {count} | 0 | {cells[0]} | {cells[1]} | `digest` |\n' +
                        '<!-- GENERATED_TABLE_END -->\n')
                    parsed = plot.read_summary(readme)
                    for physical_primary in (0, 1):
                        for physical_mover in (0, 1):
                            normalized_side = physical_mover ^ physical_primary
                            actual = (parsed[name].first_starts, parsed[name].second_starts)[normalized_side]
                            self.assertEqual(plot.WDL(*expected[normalized_side]), actual)
                    catalog = plot.OutcomeCatalog(parsed)
                    cell = (catalog.opposed(primary_name, secondary_name) if opposed else
                            catalog.together(primary_name, secondary_name))
                    self.assertEqual(plot.WDL(1, 2, 3), cell.first)
                    self.assertEqual(plot.WDL(1, 3, 2), cell.second)
                    if opposed:
                        # Reversing the row's owner also exchanges the starting
                        # turn: exercise the actual color-swapped plot path.
                        mirrored = catalog.opposed(secondary_name, primary_name)
                        self.assertEqual(plot.WDL(3, 1, 2), mirrored.first)
                        self.assertEqual(plot.WDL(2, 1, 3), mirrored.second)
                    self.assertIn('secondary_color '+str(int(opposed)), audit)
                    self.assertIn('owner_color '+str(owner), audit)

    def test_swapped_audit_rejected_even_with_valid_hashes_and_conservation(self):
        # Build complete, hash-bound fake receipts. Only the audit orientation
        # is wrong; its totals, hashes, source/model binding, and proof match.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); results = root/'results';logs = root/'logs'
            results.mkdir();logs.mkdir()
            name = 'kghostkchecker.uftb';overlay = name.replace('.uftb','.ufiw')
            count = 303663360;half=count//2
            material = dict(primary='ghost', secondary='checker', states=count, opposing=True)
            solve = f'output fixture edges 0 win 0 loss 0 draw {count}\nverifyok states {count}\n'
            native_summary = solver_summary(((1,2,3),(3,1,2)),half)
            audit=''
            for side,(w,l,d) in enumerate(((2,1,3),(1,3,2))):
                audit += f'information_reachability_admitted side {side} unknown 0 win {w} loss {l} draw {d}\n'
                audit += f'information_reachability_excluded side {side} unknown 0 win 0 loss 0 draw {half-6}\n'
                audit += f'information_reachability_trivial side {side} unknown 0 win 0 loss 0 draw 0\n'
            audit += 'information_reachability_binding source_sha256 '+'a'*64+' model_sha256 '+'b'*64+' conservation_residual 0\n'
            def save(path,text):path.write_text(text);return repair.sha256(path)
            concrete = dict(filename=name, states=count, sha256='a'*64,
                solve_log_sha256=save(logs/(name+'.solve.log'),solve),
                audit_log_sha256=save(logs/(name+'.audit.log'),'concrete fixture'))
            info=dict(filename=overlay, source_sha256='a'*64, model_sha256='b'*64,
                solve_log_sha256=save(logs/(overlay+'.solve.log'),'wrapper'),
                audit_log_sha256=save(logs/(overlay+'.audit.log'),audit),
                proof_manifest_sha256=save(results/'kghostkchecker.ghost-artifact-manifest.json','{}'),
                proof_logs={'kghostkchecker.ghost-solve.log':{'sha256':save(logs/'kghostkchecker.ghost-solve.log',native_summary)}},
                arbitrary_sidecar=dict(filename='kghostkchecker.ufgd',sha256='c'*64))
            for filename,data in [(name,concrete),(overlay,info),('kghostkchecker.ufgd',{'sha256':'c'*64})]:
                (results/(filename+'.repair.json')).write_text(json.dumps(data))
            # Concrete root parsing is unrelated to this failure; the actual
            # information parser and hash authentication remain in the path.
            with mock.patch.object(repair.finalizer, 'reporting_counts', return_value=None):
                with self.assertRaisesRegex(ValueError,'solver/audit WDL orientation residual'):
                    repair.checked_receipts(results,logs,{name:material})
                materials = root/'materials.json'
                materials.write_text(json.dumps({name: material}))
                argv = ['finalize', '--results', str(results), '--logs', str(logs),
                        '--materials', str(materials), '--revision', 'd'*40,
                        '--output', str(root/'not-created')]
                with mock.patch.object(sys, 'argv', argv):
                    with self.assertRaisesRegex(ValueError,'solver/audit WDL orientation residual'):
                        repair.main()
                remote=mock.Mock()
                with self.assertRaisesRegex(ValueError,'solver/audit WDL orientation residual'):
                    publisher.publish(root/'not-created',results,logs,{name:material},'d'*40,api=remote)
                remote.assert_not_called()
                self.assertEqual([],remote.mock_calls)
            self.assertFalse((root/'not-created').exists())
