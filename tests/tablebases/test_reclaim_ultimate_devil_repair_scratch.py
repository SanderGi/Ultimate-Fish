import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/tablebases'))
from reclaim_ultimate_devil_repair_scratch import reverse_files


class ReverseScratchScopeTest(unittest.TestCase):
    def test_only_reverse_arrays_and_spools_are_selected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            spool = root / 'devil-2.reverse-spool'
            spool.mkdir()
            for name in ('devil-2.keys', 'devil-2.nodes', 'devil-2.roots',
                         'devil-2.closure', 'devil-2.degrees'):
                (root / name).write_bytes(b'preserve unless reverse array')
            for name in ('shard-0-bucket-3.edges', 'shard-0-bucket-3.edges.sorted',
                         'shard-0.complete', 'keep.bin'):
                (spool / name).write_bytes(b'preserve unless reverse edge')
            self.assertEqual({
                'devil-2.degrees',
                'devil-2.reverse-spool/shard-0-bucket-3.edges',
                'devil-2.reverse-spool/shard-0-bucket-3.edges.sorted',
            }, {str(path.relative_to(root)) for path in reverse_files(root, 2)})

    def test_reverse_file_symlink_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'devil-2.keys').write_bytes(b'solved input')
            (root / 'devil-2.degrees').symlink_to(root / 'devil-2.keys')
            with self.assertRaisesRegex(ValueError, 'symlink'):
                reverse_files(root, 2)

    def test_reverse_spool_symlink_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'devil-2.reverse-spool').symlink_to(root, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, 'symlink'):
                reverse_files(root, 2)


if __name__ == '__main__':
    unittest.main()
