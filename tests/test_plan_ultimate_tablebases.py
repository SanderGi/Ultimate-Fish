import importlib.util
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
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


if __name__ == "__main__":
    unittest.main()
