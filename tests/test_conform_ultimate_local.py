#!/usr/bin/env python3
"""Deterministic tests for the Android Local conformance runner."""

from __future__ import annotations

from collections import Counter
import importlib.util
import random
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT))
SPEC = importlib.util.spec_from_file_location(
    "conform_ultimate_local", ROOT / "tools/conform_ultimate_local.py",
)
assert SPEC and SPEC.loader
conform = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = conform
SPEC.loader.exec_module(conform)


class FakeEngine:
    def __init__(self, positions: dict[str, str]):
        self.positions = positions

    def apply(self, _upn: str, move: str) -> str:
        return self.positions[move]


class LocalConformanceTests(unittest.TestCase):
    def test_manifest_contains_native_terminal_assertion(self) -> None:
        document = conform.load_manifest(conform.DEFAULT_MANIFEST)
        terminal_steps = [
            step["terminal_move"]
            for fixture in document["fixtures"]
            for step in fixture["steps"]
            if "terminal_move" in step
        ]
        self.assertIn(
            {"move": "b1-b10", "label": "knockout"}, terminal_steps
        )

    def test_manifest_selfplay_profiles_cover_every_deployable_piece(self) -> None:
        document = conform.load_manifest(conform.DEFAULT_MANIFEST)
        present = {
            piece
            for profile in document["selfplay_profiles"]
            for side in ("player1", "player2")
            for piece, _square in profile[side]
        }
        generated = {
            "goop", "minion", "checkerKing", "copycatClone", "halo",
        }
        self.assertEqual(set(conform.PIECE_COST) - generated, present)

    def test_manifest_profiles_cover_every_deployable_opponent_pair(self) -> None:
        document = conform.load_manifest(conform.DEFAULT_MANIFEST)
        generated = {
            "goop", "minion", "checkerKing", "copycatClone", "halo",
        }
        deployable = set(conform.PIECE_COST) - generated
        covered = set()
        for profile in document["selfplay_profiles"]:
            first = {piece for piece, _square in profile["player1"]}
            second = {piece for piece, _square in profile["player2"]}
            covered.update(
                frozenset((left, right))
                for left in first
                for right in second
            )
        expected = {
            frozenset((left, right))
            for left in deployable
            for right in deployable
        }
        self.assertEqual(expected, covered & expected)

    def test_features_include_hidden_target_and_ownership_delta(self) -> None:
        before = (
            "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";parasite,w,a1"
            ";ghost,b,b2,0,0,0,0,0,0,-1,1,-1,0"
        )
        after = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";ghost,w,b2,0,0,0,0,0,1,-1,1,-1,0"
        )
        features = conform.move_coverage_features(before, "a1-b2", after)
        self.assertIn("action:parasite:-:enemy-ghost-hidden", features)
        self.assertIn("delta:parasite:w:-1", features)
        self.assertIn("delta:ghost:b:-1", features)
        self.assertIn("delta:ghost:w:+1", features)

    def test_automatic_minion_transition_detects_delayed_turn_start_work(self) -> None:
        before = (
            "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";king,w,a1;king,b,h10;minion,b,a5"
        )
        after = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";king,w,a1;king,b,h10;minion,b,a4"
        )
        quiet = after.replace("minion,b,a4", "minion,b,a5")
        self.assertTrue(conform.automatic_minion_transition(before, after))
        self.assertFalse(conform.automatic_minion_transition(before, quiet))

        captured = after.replace(";minion,b,a4", "")
        self.assertFalse(
            conform.automatic_minion_transition(
                before, captured, "b4-a5"
            )
        )

        second_before = before + ";minion,b,c5"
        second_after = captured + ";minion,b,c4"
        self.assertTrue(
            conform.automatic_minion_transition(
                second_before, second_after, "b4-a5"
            )
        )

        devil_before = (
            "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";king,w,a1;devil,w,g3;king,b,h10"
        )
        devil_after = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";king,w,a1;devil,w,g3;minion,w,f5;king,b,h10"
        )
        self.assertFalse(
            conform.automatic_minion_transition(devil_before, devil_after)
        )

    def test_chooser_prefers_novel_effect_over_quiet_move(self) -> None:
        before = (
            "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";rook,w,a1;pawn,b,a2;king,b,h10"
        )
        quiet = (
            "b;hm=1;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";rook,w,b1;pawn,b,a2;king,b,h10"
        )
        capture = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";rook,w,a2;king,b,h10"
        )
        chosen = conform.choose_coverage_move(
            FakeEngine({"a1-b1": quiet, "a1-a2": capture}),
            before,
            ["a1-b1", "a1-a2", "pass"],
            set(),
            Counter(),
            {conform.upn_repetition_key(before)},
            random.Random(1),
        )
        self.assertIsNotNone(chosen)
        self.assertEqual(chosen.move, "a1-a2")

    def test_chooser_avoids_ending_game_when_alternative_exists(self) -> None:
        before = (
            "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";rook,w,a1;king,b,a2"
        )
        quiet = (
            "b;hm=1;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";rook,w,b1;king,b,a2"
        )
        terminal = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;rook,w,a2"
        )
        chosen = conform.choose_coverage_move(
            FakeEngine({"a1-b1": quiet, "a1-a2": terminal}),
            before,
            ["a1-b1", "a1-a2"],
            set(),
            Counter(),
            set(),
            random.Random(2),
        )
        self.assertEqual(chosen.move, "a1-b1")

    def test_chooser_uses_campaign_coverage_to_seek_a_different_effect(self) -> None:
        before = (
            "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";rook,w,a1;pawn,b,a2;bishop,b,b2;king,b,h10"
        )
        pawn_capture = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";rook,w,a2;bishop,b,b2;king,b,h10"
        )
        bishop_capture = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";rook,w,b2;pawn,b,a2;king,b,h10"
        )
        engine = FakeEngine({"a1-a2": pawn_capture, "a1-b2": bishop_capture})
        first_features = conform.move_coverage_features(
            before, "a1-a2", pawn_capture
        )
        chosen = conform.choose_coverage_move(
            engine,
            before,
            ["a1-a2", "a1-b2"],
            set(first_features),
            Counter({"rook": 1}),
            set(),
            random.Random(3),
        )
        self.assertEqual(chosen.move, "a1-b2")


if __name__ == "__main__":
    unittest.main()
