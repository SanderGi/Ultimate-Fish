#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "run_ultimate_jester_ghost_current_aws",
    ROOT / "tools/tablebases/run_ultimate_jester_ghost_current_aws.py")
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class CurrentJesterGhostRunnerTests(unittest.TestCase):
    def test_fresh_ranges_cover_every_raw_frame_once(self) -> None:
        zero, active, merged = runner.ranges()
        self.assertEqual((len(zero), len(active), len(merged)), (40, 40, 80))
        self.assertEqual(merged[0][1], 0)
        self.assertEqual(merged[-1][1] + merged[-1][2], 38_450_880)
        self.assertEqual(len({name for name, _, _ in merged}), 80)


if __name__ == "__main__":
    unittest.main()
