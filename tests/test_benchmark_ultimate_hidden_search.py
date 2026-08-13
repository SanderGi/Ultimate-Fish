#!/usr/bin/env python3
"""Regression tests for deterministic hidden-information search scaling."""

from __future__ import annotations

import json
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "src" / "ultimate_hidden_search_benchmark"


class HiddenSearchBenchmarkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        subprocess.run(
            ["make", "-C", str(ROOT / "src"),
             "ultimate-hidden-search-benchmark"],
            check=True,
        )

    def run_benchmark(self) -> dict[str, object]:
        completed = subprocess.run(
            [str(BINARY)], check=True, text=True, capture_output=True,
        )
        return json.loads(completed.stdout)

    def test_suite_is_deterministic_and_exercises_reexpansion(self) -> None:
        first = self.run_benchmark()
        second = self.run_benchmark()
        # Wall-clock fields are measurements; every search result and counter
        # must be deterministic.
        for result in (first, second):
            for fixture in result["fixtures"]:
                fixture.pop("engine_ms")
                fixture.pop("wall_ms")
        self.assertEqual(first, second)
        self.assertEqual(
            "ultimate-hidden-search-benchmark-v1", first["schema"])
        self.assertTrue(first["deterministic"])
        self.assertEqual(0, first["seed"])

        fixtures = {item["name"]: item for item in first["fixtures"]}
        self.assertEqual(2, fixtures["royal-two-world-endgame"]["root_beliefs"])
        self.assertGreater(
            fixtures["royal-two-world-mixed-midgame"]["singleton_handoffs"], 0)
        self.assertEqual(
            2_775, fixtures["ghost-pair-2775-world-endgame"]["root_beliefs"])
        self.assertGreater(
            fixtures["ghost-pair-2775-world-endgame"]["peak_beliefs"], 2_775)
        reexpansion = fixtures["ghost-visible-singleton-reexpands"]
        self.assertEqual(1, reexpansion["root_beliefs"])
        self.assertEqual(0, reexpansion["singleton_handoffs"])
        self.assertGreater(reexpansion["peak_beliefs"], 1)


if __name__ == "__main__":
    unittest.main()
