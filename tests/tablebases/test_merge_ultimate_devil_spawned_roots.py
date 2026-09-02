import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "tools/tablebases/merge_ultimate_devil_spawned_roots.py"
SPEC = importlib.util.spec_from_file_location("devil_merge", MODULE_PATH)
assert SPEC and SPEC.loader
MERGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MERGE)
PREPARE_PATH = (ROOT / "tools/tablebases/"
                "prepare_ultimate_devil_spawned_merge_preservation.py")
PREPARE_SPEC = importlib.util.spec_from_file_location(
    "devil_merge_prepare", PREPARE_PATH)
assert PREPARE_SPEC and PREPARE_SPEC.loader
PREPARE = importlib.util.module_from_spec(PREPARE_SPEC)
PREPARE_SPEC.loader.exec_module(PREPARE)


class DevilSpawnedRootMergeTests(unittest.TestCase):
    def test_twelve_disjoint_fragments_write_v11(self):
        by_square = {square: [] for square in MERGE.EXPECTED_SQUARES}
        for index in range(MERGE.STATE_COUNT):
            square = MERGE.canonical_square(MERGE.decode_attacker(index))
            if square in MERGE.EXPECTED_SQUARES:
                by_square[square].append(index)
        self.assertEqual(MERGE.ADMITTED_STATE_COUNT,
                         sum(map(len, by_square.values())))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fragments = []
            for square, indices in sorted(by_square.items()):
                path = root / f"devil-{square}.roots"
                payload = bytearray(MERGE.ROOT_HEADER.pack(
                    MERGE.ROOT_MAGIC, 1, square, 100, 10, 20,
                    len(indices)))
                for index in indices:
                    payload += MERGE.ROOT_RECORD.pack(index, 3, 0)
                path.write_bytes(payload)
                fragments.append(path)
            output = root / "kdevilk.uftb"
            receipt = root / "merge.json"
            result = MERGE.merge(fragments, output, receipt)
            header = MERGE.UFTB_HEADER.unpack_from(output.read_bytes())
            self.assertEqual(MERGE.UFTB_MAGIC, header[0])
            self.assertEqual(11, header[1])
            self.assertEqual(MERGE.DEVIL_PIECE, header[2])
            self.assertEqual(MERGE.SPAWNED_DEVIL_ROOT_V1_TAG, header[-1])
            self.assertEqual(MERGE.ADMITTED_STATE_COUNT,
                             result["output"]["root_records"])
            self.assertEqual(MERGE.ADMITTED_STATE_COUNT,
                             result["output"]["draw"])
            self.assertEqual(MERGE.STATE_COUNT,
                             result["output"]["packed_draw"])
            self.assertEqual(0, result["root_coverage_residual"])
            self.assertEqual(0, result["duplicate_root_residual"])
            prepared = PREPARE.prepare(output, receipt, root, root / "work")
            self.assertEqual(
                "merged-result-verified-archived-restored",
                prepared["status"])
            self.assertEqual(11, prepared["output"]["version"])

    def test_missing_roots_fail_closed(self):
        by_square = {}
        for index in range(MERGE.STATE_COUNT):
            square = MERGE.canonical_square(MERGE.decode_attacker(index))
            by_square.setdefault(square, index)
            if len(by_square) == 12:
                break
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fragments = []
            for square, index in sorted(by_square.items()):
                path = root / f"devil-{square}.roots"
                path.write_bytes(
                    MERGE.ROOT_HEADER.pack(
                        MERGE.ROOT_MAGIC, 1, square, 100, 10, 20, 1
                    ) + MERGE.ROOT_RECORD.pack(index, 3, 0))
                fragments.append(path)
            with self.assertRaisesRegex(RuntimeError, "root coverage"):
                MERGE.merge(fragments, root / "out.uftb", root / "out.json")

    def test_duplicate_square_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "duplicate.roots"
            index = next(index for index in range(MERGE.STATE_COUNT)
                         if MERGE.canonical_square(
                             MERGE.decode_attacker(index)) == 0)
            path.write_bytes(
                MERGE.ROOT_HEADER.pack(
                    MERGE.ROOT_MAGIC, 1, 0, 100, 10, 20, 1
                ) + MERGE.ROOT_RECORD.pack(index, 3, 0)
            )
            with self.assertRaisesRegex(RuntimeError, "twelve"):
                MERGE.merge([path], root / "out.uftb", root / "out.json")


if __name__ == "__main__":
    unittest.main()
