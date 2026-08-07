import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
SPEC = importlib.util.spec_from_file_location(
    "tb_plan", ROOT / "tools" / "plan_ultimate_tablebases.py")
assert SPEC and SPEC.loader
tb = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = tb
SPEC.loader.exec_module(tb)
SUMMARY_SPEC = importlib.util.spec_from_file_location(
    "tb_summary", ROOT / "tools" / "summarize_ultimate_tablebases.py")
assert SUMMARY_SPEC and SUMMARY_SPEC.loader
summary = importlib.util.module_from_spec(SUMMARY_SPEC)
sys.modules[SUMMARY_SPEC.name] = summary
SUMMARY_SPEC.loader.exec_module(summary)
SHARD_SPEC = importlib.util.spec_from_file_location(
    "tb_shards", ROOT / "tools" / "ultimate_tablebase_shards.py")
assert SHARD_SPEC and SHARD_SPEC.loader
shards = importlib.util.module_from_spec(SHARD_SPEC)
sys.modules[SHARD_SPEC.name] = shards
SHARD_SPEC.loader.exec_module(shards)


class TablebasePlanTests(unittest.TestCase):
    def test_horizontal_symmetry_counts(self):
        self.assertEqual(tb.placement_states(1), 492_960)
        self.assertEqual(tb.placement_states(2), 37_957_920)
        self.assertEqual(tb.placement_states(2, identical_pair=True), 18_978_960)

    def test_all_decisive_single_material_is_planned(self):
        planned = {record["class"] for record in tb.inventory()
                   if record["phase"] == "kings+1"}
        expected = {f"K{piece.name}vK" for piece in tb.PIECES if piece.decisive}
        self.assertEqual(planned, expected)
        self.assertNotIn("KbishopvK", planned)
        self.assertNotIn("KknightvK", planned)

    def test_stateless_pair_material_filter(self):
        planned = {record["class"] for record in tb.inventory()
                   if record["phase"] == "kings+2-stateless"}
        self.assertIn("KknightturtlevK", planned)
        self.assertIn("KbishopmagevK", planned)
        self.assertIn("KbishopbishopvK", planned)
        self.assertNotIn("KmagefishermanvK", planned)
        self.assertNotIn("KknightvKturtle", planned)

    def test_every_planned_file_is_sharded_below_github_limit(self):
        for record in tb.inventory():
            per_shard = (record["packed_bytes"] + record["shards"] - 1) // record["shards"]
            self.assertLess(per_shard, tb.GITHUB_FILE_LIMIT)

    def test_stateful_inventory_is_exactly_budget_capped(self):
        records = tb.inventory()
        self.assertLessEqual(sum(record["packed_bytes"] for record in records),
                             tb.DEFAULT_BUDGET)
        admitted = [record for record in records
                    if record["phase"] == "kings+2-stateful"]
        represented = {str(record[side]) for record in admitted
                       for side in ("primary", "secondary")}
        self.assertTrue({"pawn", "berserker", "ghost", "penguin", "sniper",
                         "prince", "checker"}.issubset(represented))
        self.assertTrue(represented.isdisjoint(
            {"devil", "sludge", "copycat", "angel"}))

    def test_stateful_candidate_closures_are_explicit(self):
        candidates = tb.stateful_candidates()
        self.assertTrue(candidates)
        for record in candidates:
            pieces = {str(record["primary"]), str(record["secondary"])}
            self.assertFalse(pieces & {"devil", "sludge", "copycat", "angel"})

    def test_krk_side_split_and_illegal_annotation(self):
        totals, illegal = summary.summary(ROOT / "tablebases" / "krk.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]), "492,960 / 0 / 0")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "0 (41,808) / 414,300 / 36,852")

    def test_jester_adjacent_king_wins_are_legal(self):
        totals, illegal = summary.summary(ROOT / "tablebases" / "kjesterk.uftb")
        self.assertEqual(illegal[1], 0)
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "41,808 / 414,344 / 36,808")

    def test_regular_git_shards_round_trip_and_verify(self):
        payload = bytes(range(251)) * 17
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.uftb"
            path.write_bytes(payload)
            outputs = shards.split(path, limit=333)
            self.assertGreater(len(outputs), 2)
            self.assertLess(path.stat().st_size, len(payload))
            self.assertTrue(all(part.stat().st_size <= 333 for part in outputs[1:]))
            self.assertEqual(shards.read_logical(path), payload)
            import hashlib
            self.assertEqual(shards.logical_sha256(path),
                             hashlib.sha256(payload).hexdigest())

    def test_stateful_side_summary_removes_substate_dimension(self):
        full = tb.placement_states(2)
        identical = tb.placement_states(2, identical_pair=True)
        for placement in (0, 1, 123_456, full - 1):
            expected = summary.kings_for(placement, full, 1)
            for substate in range(4):
                self.assertEqual(
                    summary.kings_for(placement * 4 + substate, full * 4, 4),
                    expected)
        for placement in (0, 1, 123_456, identical - 1):
            expected = summary.kings_for(placement, identical, 1)
            for substate in range(2):
                self.assertEqual(
                    summary.kings_for(placement * 2 + substate, identical * 2, 2),
                    expected)


if __name__ == "__main__":
    unittest.main()
