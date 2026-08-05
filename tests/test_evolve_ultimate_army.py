#!/usr/bin/env python3
"""Deterministic tests for the setup coevolution model."""

from __future__ import annotations

import importlib.util
import random
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]
SPEC = importlib.util.spec_from_file_location(
    "evolve_ultimate_army", ROOT / "tools/evolve_ultimate_army.py",
)
assert SPEC and SPEC.loader
evolve = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = evolve
SPEC.loader.exec_module(evolve)


class ArmyEvolutionTests(unittest.TestCase):
    def test_reference_armies_are_exact_cost_and_non_overlapping(self) -> None:
        for army in evolve.OPPONENTS:
            evolve.validate_army(army)
            self.assertEqual(evolve.army_cost(army), 100)

    def test_mutation_and_crossover_always_create_legal_armies(self) -> None:
        rng = random.Random(5731)
        mutations = {
            evolve.army_key(evolve.mutate_army(evolve.LIVE_GIANT_ARMY, rng))
            for _attempt in range(100)
        }
        self.assertGreater(len(mutations), 20)
        for key in mutations:
            evolve.validate_army(key)
        self.assertTrue(any(dict(army)["king"] != "a1" for army in mutations))

        for _attempt in range(50):
            child = evolve.crossover_armies(
                evolve.LIVE_GIANT_ARMY, evolve.SPECIAL_ARMY, rng,
            )
            evolve.validate_army(child)

    def test_league_pairs_population_once_and_filters_archive_duplicates(self) -> None:
        population = evolve.OPPONENTS[:4]
        schedule = evolve.build_league_schedule(
            population, (population[0], evolve.OPPONENTS[4]),
        )
        internal = [(first, second) for first, second, _a, _b in schedule if second >= 0]
        external = [(first, second) for first, second, _a, _b in schedule if second < 0]
        self.assertEqual(len(internal), 6)
        self.assertEqual(len(set(internal)), 6)
        self.assertEqual(len(external), 4)

    def test_color_mirroring_preserves_giant_footprints(self) -> None:
        self.assertEqual(evolve.mirror_square("giant", "c1"), "c9")
        self.assertEqual(evolve.mirror_square("giant", "c2"), "c8")
        self.assertEqual(evolve.mirror_square("queen", "c1"), "c10")

    def test_live_sniper_adversary_preserves_off_corner_king(self) -> None:
        evolve.validate_army(evolve.SNIPER_ARMY)
        self.assertEqual(dict(evolve.SNIPER_ARMY)["king"], "e2")
        black = evolve.position(
            evolve.CURRENT_ARMY, evolve.SNIPER_ARMY, "w"
        )
        self.assertIn("king,b,e9", black)
        self.assertIn("sniper,b,a8", black)

    def test_result_summary_keeps_capped_tiebreaks_as_draws(self) -> None:
        self.assertEqual(
            evolve.result_summary((1.0, 0.5, 0.62, 0.0)),
            "1W-2D-1L",
        )


if __name__ == "__main__":
    unittest.main()
