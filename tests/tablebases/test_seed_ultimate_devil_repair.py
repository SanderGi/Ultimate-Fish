import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    'seed_devil', ROOT / 'tools/tablebases/seed_ultimate_devil_repair.py')
seed_devil = importlib.util.module_from_spec(spec)
spec.loader.exec_module(seed_devil)
spec_export = importlib.util.spec_from_file_location(
    'export_devil', ROOT / 'tools/tablebases/export_ultimate_devil_repair.py')
export_devil = importlib.util.module_from_spec(spec_export)
spec_export.loader.exec_module(export_devil)


class SeedRepairTests(unittest.TestCase):
    def fixture(self, directory, keys=(1, 7, 123)):
        path = Path(directory) / 'old.ufds'
        payload = struct.pack('<8sIIQII', b'UFDSV1\0\0', 1, 0, len(keys), 10, 0)
        # Deliberately nonsensical historical outcomes must never be reused.
        payload += b''.join(k.to_bytes(7, 'little') + b'\xff\xff\xff' for k in keys)
        path.write_bytes(payload)
        return path, hashlib.sha256(payload).hexdigest()

    def test_import_discards_outcomes_and_commits_exact_key_prefix(self):
        with tempfile.TemporaryDirectory() as directory:
            path, digest = self.fixture(directory)
            work = Path(directory) / 'work'
            receipt = seed_devil.seed(path, work, digest)
            data = (work / 'devil-0.keys').read_bytes()
            self.assertEqual(data[:21], b''.join(k.to_bytes(7, 'little') for k in (1, 7, 123)))
            self.assertEqual(len(data), receipt['state_limit'] * 7)
            self.assertFalse(receipt['reused_outcomes'])
            checkpoint = struct.unpack('<8sIIQQQI', (work / 'devil-0.closure').read_bytes())
            self.assertEqual(checkpoint[4:], (3, 0, 0))
            self.assertFalse((work / 'devil-0.nodes').exists())
            with self.assertRaises(FileExistsError):
                seed_devil.seed(path, work, digest)

    def test_wrong_digest_creates_no_checkpoint(self):
        with tempfile.TemporaryDirectory() as directory:
            path, _ = self.fixture(directory)
            work = Path(directory) / 'work'
            with self.assertRaisesRegex(ValueError, 'SHA-256'):
                seed_devil.seed(path, work, '0' * 64)
            self.assertFalse(work.exists())

    def test_unsorted_keys_never_commit(self):
        with tempfile.TemporaryDirectory() as directory:
            path, digest = self.fixture(directory, (7, 1))
            work = Path(directory) / 'work'
            with self.assertRaisesRegex(ValueError, 'unsorted'):
                seed_devil.seed(path, work, digest)
            self.assertFalse((work / 'devil-0.closure').exists())

    def test_sorted_export_uses_new_outcomes_and_rules_revision(self):
        with tempfile.TemporaryDirectory() as directory:
            path, digest = self.fixture(directory)
            work = Path(directory) / 'work'
            seed_devil.seed(path, work, digest)
            # Node layout: WDL, padding, DTW, remaining, longest winning child.
            (work / 'devil-0.nodes').write_bytes(b''.join(
                struct.pack('<BBHHH', wdl, 0, dtw, 0, 0)
                for wdl, dtw in ((1, 3), (2, 0), (3, 0))))
            log = Path(directory) / 'verify.log'
            log.write_text('DEVIL_SPAWNED_SQUARE_OK square 0 states 3 edges 4 roots 1\n')
            output = Path(directory) / 'fixed.ufds'
            receipt = export_devil.export(work, output, log)
            data = output.read_bytes()
            self.assertEqual(struct.unpack_from('<I', data, 8)[0], 2)
            self.assertEqual(data[32:], b''.join(
                key.to_bytes(7, 'little') + struct.pack('<BH', wdl, dtw)
                for key, wdl, dtw in ((1, 1, 3), (7, 2, 0), (123, 3, 0))))
            self.assertEqual(receipt['sha256'], hashlib.sha256(data).hexdigest())
            with self.assertRaises(FileExistsError):
                export_devil.export(work, output, log)


if __name__ == '__main__':
    unittest.main()
