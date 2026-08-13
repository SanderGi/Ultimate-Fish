#!/usr/bin/env python3
"""Regression tests for the deterministic public-belief match benchmark."""

from __future__ import annotations

import json
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / "src" / "ultimate_belief_benchmark"


class BeliefBenchmarkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        subprocess.run(
            ["make", "-C", str(ROOT / "src"), "ultimate-belief-benchmark"],
            check=True,
        )

    def run_benchmark(self) -> dict[str, object]:
        completed = subprocess.run(
            [str(BINARY)], check=True, text=True, capture_output=True,
        )
        return json.loads(completed.stdout)

    def test_match_is_color_balanced_deterministic_and_robust(self) -> None:
        first = self.run_benchmark()
        second = self.run_benchmark()
        self.assertEqual(first, second)
        self.assertEqual("ultimate-public-belief-match-v1", first["schema"])
        self.assertTrue(first["color_balanced"])
        self.assertEqual(8, len(first["games"]))

        combinations = {
            (game["mode"], game["color"], game["actual"])
            for game in first["games"]
        }
        self.assertEqual(
            {
                (mode, color, actual)
                for mode in ("exact", "single-determinization")
                for color in ("white", "black")
                for actual in ("safe", "unsafe")
            },
            combinations,
        )
        self.assertTrue(first["exact_robust_root"])
        self.assertTrue(first["determinization_exposed"])


if __name__ == "__main__":
    unittest.main()
