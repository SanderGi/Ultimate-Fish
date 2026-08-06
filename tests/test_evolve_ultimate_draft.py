#!/usr/bin/env python3
"""Deterministic tests for Ranked draft-policy coevolution."""

from __future__ import annotations

import importlib.util
import random
import sys
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "tools"))
SPEC = importlib.util.spec_from_file_location(
    "evolve_ultimate_draft", ROOT / "tools/evolve_ultimate_draft.py",
)
assert SPEC and SPEC.loader
draft = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = draft
SPEC.loader.exec_module(draft)


class DraftEvolutionTests(unittest.TestCase):
    def test_base_policy_completes_all_windows_and_legal_deployments(self) -> None:
        outcome = draft.simulate_draft(draft.BASE_POLICY, draft.BASE_POLICY)
        self.assertEqual(len(outcome.bans), 6)
        self.assertEqual(len(set(outcome.bans)), 6)
        self.assertEqual(len(outcome.white_groups), 3)
        self.assertEqual(len(outcome.black_groups), 3)
        for army in (outcome.white, outcome.black):
            points = draft.team_points(piece for piece, _square in army)
            self.assertGreaterEqual(points, 80)
            self.assertLessEqual(points, 100)
            self.assertLessEqual(
                sum(draft.footprint_size(piece) for piece, _square in army), 24
            )
            occupied = set()
            for piece, square in army:
                cells = draft.footprint(piece, square)
                self.assertTrue(cells)
                self.assertFalse(cells & occupied)
                occupied.update(cells)
        for groups in (outcome.white_groups, outcome.black_groups):
            self.assertNotIn("jester", groups[1])
            self.assertNotIn("jester", groups[2])

    def test_live_deployment_policy_closes_corner_king_rays(self) -> None:
        outcome = draft.simulate_draft(draft.BASE_POLICY, draft.BASE_POLICY)
        for army in (outcome.white, outcome.black):
            occupied = set()
            for piece, square in army:
                if piece not in ("ghost", "bomb"):
                    occupied.update(draft.footprint(piece, square))
            self.assertTrue(draft.KING_SHIELD <= occupied)

    def test_mutation_and_crossover_preserve_policy_shape(self) -> None:
        rng = random.Random(5732)
        mutations = [draft.mutate_policy(draft.BASE_POLICY, rng) for _ in range(20)]
        self.assertGreater(len({draft.policy_key(item) for item in mutations}), 10)
        for policy in mutations:
            self.assertEqual(len(policy.pick), len(draft.PIECES))
            draft.simulate_draft(policy, draft.BASE_POLICY)
        child = draft.crossover_policy(mutations[0], mutations[1], rng)
        draft.simulate_draft(child, draft.BASE_POLICY)

    def test_public_enemy_roster_changes_later_ban_score(self) -> None:
        policy = draft.DraftPolicy(
            draft.BASE_POLICY.pick, draft.BASE_POLICY.repeat,
            tuple(0 for _piece in draft.PIECES),
            opponent_deny=200, self_preserve=0, final_penguin=0,
        )
        self.assertGreater(
            policy.ban_score("queen", ["king"], ["king", "queen"]),
            policy.ban_score("rook", ["king"], ["king", "queen"]),
        )

    def test_giant_biased_policy_preserves_each_material_minimum(self) -> None:
        giant = draft.INDEX["giant"]
        pick = list(draft.BASE_POLICY.pick)
        pick[giant] = 10_000
        policy = draft.DraftPolicy(
            tuple(pick), draft.BASE_POLICY.repeat, draft.BASE_POLICY.ban,
        )
        outcome = draft.simulate_draft(policy, draft.BASE_POLICY)
        cumulative = 0
        minimum_additions = (15, 15, 0)
        maxima = (40, 80, 100)
        for group, added_minimum, maximum in zip(
                outcome.white_groups, minimum_additions, maxima):
            before = cumulative
            cumulative += sum(draft.PIECE_COST[piece] for piece in group)
            self.assertGreaterEqual(cumulative, before + added_minimum)
            self.assertLessEqual(cumulative, maximum)

    def test_worker_reuses_deterministic_duplicate_positions(self) -> None:
        class FakeEngine:
            def __init__(self, _path):
                self.new_games = 0

            def new_game(self):
                self.new_games += 1

            def close(self):
                pass

        jobs = [
            (0, 1, draft.BASE_POLICY, draft.BASE_POLICY),
            (0, 2, draft.BASE_POLICY, draft.BASE_POLICY),
        ]
        with patch.object(draft, "Engine", FakeEngine), \
             patch.object(draft, "play_game", return_value=0.5) as played:
            completed = draft._play_chunk(("engine", jobs, 6, 100, 20))
        self.assertEqual(len(completed), 2)
        self.assertEqual(played.call_count, 2)

    def test_macro_generator_contains_the_promoted_greedy_line(self) -> None:
        state = draft.PublicDraftState().apply(("prince",)).apply(("mage",))
        actions = draft.ranked_macro_actions(state, width=8)
        self.assertEqual(
            actions[0], ("queen", "queen", "giant", "copycat")
        )
        self.assertIn(("queen", "queen", "copycat"), actions)

    def test_adversarial_search_can_override_policy_order_at_engine_leaf(self) -> None:
        state = draft.PublicDraftState(
            11,
            (
                ("queen", "queen", "copycat", "giant"),
                ("queen", "queen", "copycat", "giant"),
                ("queen", "checker", "giant"),
            ),
            (
                ("queen", "queen", "copycat", "giant"),
                ("queen", "queen", "penguin", "giant"),
            ),
            ("prince", "mage", "bomb", "jester", "ghost", "sniper"),
        )

        def evaluator(outcome):
            return 1.0 if "turtle" in outcome.black_groups[-1] else 0.0

        result = draft.search_public_draft(
            state, "b", evaluator, action_width=30,
        )
        self.assertEqual(result.score, 1.0)
        self.assertIn("turtle", result.action)
        self.assertNotEqual(
            result.action, draft.ranked_macro_actions(state, width=1)[0]
        )


if __name__ == "__main__":
    unittest.main()
