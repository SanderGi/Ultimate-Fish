#!/usr/bin/env python3
"""Deterministic tests for the Android Local conformance runner."""

from __future__ import annotations

from collections import Counter
import copy
import importlib.util
import json
import random
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]
NATIVE_AUDIT = ROOT / "tests" / "ultimate_native_interaction_audit.json"
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
    @classmethod
    def setUpClass(cls) -> None:
        subprocess.run(
            ["make", "-C", str(ROOT / "src"), "ultimatefish"],
            check=True,
            stdout=subprocess.DEVNULL,
        )

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

    def test_every_declared_contract_has_one_exact_native_proof_step(self) -> None:
        document = conform.load_manifest(conform.DEFAULT_MANIFEST)
        for fixture in document["fixtures"]:
            proofs = [
                contract
                for step in fixture["steps"]
                for contract in step.get("proves", [])
            ]
            self.assertCountEqual(fixture["covers"], proofs, fixture["id"])

    def test_missing_native_proof_invalidates_manifest(self) -> None:
        document = copy.deepcopy(
            conform.load_manifest(conform.DEFAULT_MANIFEST)
        )
        proof_step = next(
            step
            for fixture in document["fixtures"]
            for step in fixture["steps"]
            if step.get("proves")
        )
        proof_step.pop("proves")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "manifest.json"
            path.write_text(json.dumps(document))
            with self.assertRaisesRegex(ValueError, "without an exact native proof"):
                conform.load_manifest(path)

    def test_every_native_fixture_is_a_self_consistent_engine_oracle(self) -> None:
        document = conform.load_manifest(conform.DEFAULT_MANIFEST)
        engine = str(ROOT / "src" / "ultimatefish")
        for fixture in document["fixtures"]:
            with self.subTest(fixture=fixture["id"]):
                conform.validate_fixture_engine_trace(fixture, engine)

    def test_death_oracle_rejects_a_native_death_absent_from_engine(self) -> None:
        before = (
            "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1"
            ";king,w,a1;king,b,a10;ghost,b,b2"
        )
        after = before.replace("w;hm=", "b;hm=", 1)
        with self.assertRaisesRegex(
            AssertionError, "exceeds engine-predicted deaths"
        ):
            conform.validate_engine_death_oracle(before, after, ["ghost"])

    def test_native_validation_certificate_matches_fixture_semantics(self) -> None:
        document = conform.load_manifest(conform.DEFAULT_MANIFEST)
        certificate = json.loads(conform.DEFAULT_VALIDATION.read_text())
        contracts = {
            contract
            for fixture in document["fixtures"]
            for contract in fixture["covers"]
        }
        self.assertEqual(1, certificate["schema"])
        self.assertTrue(certificate["passed"])
        self.assertEqual(document["app_version"], certificate["app_version"])
        self.assertEqual(len(document["fixtures"]), certificate["fixture_count"])
        self.assertEqual(len(contracts), certificate["contract_count"])
        self.assertEqual(
            conform.native_fixture_digest(document),
            certificate["native_fixture_sha256"],
        )

    def test_private_royal_legal_dot_pair_is_in_native_gate(self) -> None:
        document = conform.load_manifest(conform.DEFAULT_MANIFEST)
        contracts = {
            contract
            for fixture in document["fixtures"]
            for contract in fixture["covers"]
        }
        self.assertIn(
            "legal-dots.jester-target-filtered-by-protected-real-king",
            contracts,
        )
        self.assertIn(
            "legal-dots.real-king-target-is-legal-capture",
            contracts,
        )

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

    def test_native_interaction_audit_covers_every_piece_and_simulator(self) -> None:
        document = conform.load_manifest(conform.DEFAULT_MANIFEST)
        audit = json.loads(NATIVE_AUDIT.read_text())
        source = (ROOT / "tests" / "ultimate_rules.cpp").read_text()
        contracts = {
            contract
            for fixture in document["fixtures"]
            for contract in fixture["covers"]
        }
        audited_pieces = {
            piece
            for family in audit["families"]
            for piece in family["pieces"]
        }
        audited_classes = {
            class_name
            for family in audit["families"]
            for class_name in family["native_classes"]
        }
        piece_occurrences = Counter(
            piece for family in audit["families"] for piece in family["pieces"]
        )
        class_occurrences = Counter(
            class_name
            for family in audit["families"]
            for class_name in family["native_classes"]
        )
        self.assertEqual(1, audit["schema"])
        self.assertEqual(document["app_version"], audit["app_version"])
        self.assertEqual(set(conform.PIECE_COST), audited_pieces)
        self.assertEqual(set(audit["native_classes"]), audited_classes)
        self.assertTrue(all(count == 1 for count in piece_occurrences.values()))
        self.assertTrue(all(count == 1 for count in class_occurrences.values()))
        self.assertEqual(
            len(audit["families"]),
            len({family["id"] for family in audit["families"]}),
        )
        self.assertEqual(28, len(audit["native_classes"]))
        self.assertEqual(118, audit["native_override_count"])
        self.assertRegex(audit["native_override_sha256"], r"^[0-9a-f]{64}$")
        self.assertEqual(
            51,
            sum(len(family["native_anchors"]) for family in audit["families"]),
        )
        for family in audit["families"]:
            with self.subTest(family=family["id"]):
                self.assertTrue(family["native_anchors"])
                self.assertTrue(family["reference_tests"])
                self.assertTrue(family["local_contracts"])
                self.assertTrue(family["finding"])
                self.assertTrue(set(family["local_contracts"]) <= contracts)
                for test_name in family["reference_tests"]:
                    self.assertIn(f"void {test_name}()", source)
                for anchor in family["native_anchors"]:
                    self.assertRegex(
                        anchor,
                        r"^Simulated[A-Za-z0-9_]+\.[A-Za-z0-9_]+@0x[0-9a-f]+$",
                    )

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
