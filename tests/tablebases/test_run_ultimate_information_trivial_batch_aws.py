#!/usr/bin/env python3

from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tablebases"))

import run_ultimate_information_trivial_batch_aws as runner


class InformationTrivialBatchTest(unittest.TestCase):
    def test_corrected_opposed_penguin_overlay_transposes_substates(self) -> None:
        self.assertIn(
            (
                "kghostkpenguin",
                "06dc64a6ae06df68df65c9819de7a8abec06c124b59ea5cd5010a60769b84b64",
                "c073b92a1243ad3027b85bf539528f115b64a1b9fc9a72cc3c9dd8d2ed2cf0b9",
            ),
            runner.TRANSPOSED_SUBSTATE_OVERLAYS,
        )

    def test_canonical_opposed_checker_overlay_transposes_substates(self) -> None:
        self.assertIn(
            (
                "kghostkchecker",
                "231d2f45d8d1db2a6e47aa413485444d93599fc68ae0d069afabd5e60369ee10",
                "bb4e8b3b44571b7ed5caad19dc17d26db9afe976393175f399f5e546ea490316",
            ),
            runner.TRANSPOSED_SUBSTATE_OVERLAYS,
        )
        self.assertEqual(
            runner.TRANSPOSED_SUBSTATE_MATERIALS["kghostkchecker"],
            ("checker", 8),
        )


if __name__ == "__main__":
    unittest.main()
