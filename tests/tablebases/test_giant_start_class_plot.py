import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


def load(name: str, relative: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


plot = load("plot_ultimate_tablebases_giant_test",
            "tools/tablebases/plot_ultimate_tablebases.py")
audit = load("audit_ultimate_giant_start_classes_test",
             "tools/tablebases/audit_ultimate_giant_start_classes.py")


def side(wins: int, losses: int, draws: int) -> dict[str, object]:
    return {
        "admitted": {"wins": wins + 1, "losses": losses + 2,
                     "draws": draws + 3},
        "trivial": {"wins": 1, "losses": 2, "draws": 3},
        "display": {"wins": wins, "losses": losses, "draws": draws},
    }


class GiantStartClassPlotTests(unittest.TestCase):
    def test_rows_follow_anchor_parity_component_sizes(self):
        counts = [0, 0, 0, 0]
        for rank in range(9):
            for file in range(7):
                counts[(2 if file & 1 else 0) + (1 if rank & 1 else 0)] += 1
        self.assertEqual([20, 16, 15, 12], counts)
        self.assertEqual(
            ("Giant-20", "Giant-16", "Giant-15", "Giant-12"),
            tuple(plot.PIECE_LABELS[row] for row in plot.GIANT_START_ROWS))

    def test_summary_parser_checks_and_preserves_exact_classes(self):
        document = {
            "schema": 1,
            "semantics": "reachability-admitted-minus-trivial-v3",
            "class_sizes": [20, 16, 15, 12],
            "files": {
                "kgiantkdragon.uftb": {
                    "classes": {
                        str(value): {
                            "first_starts": side(value, 0, 1),
                            "second_starts": side(0, value, 2),
                        } for value in (20, 16, 15, 12)
                    }
                }
            },
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            rows = plot.read_giant_start_classes(path)
        self.assertEqual(plot.WDL(20, 0, 1),
                         rows[("kgiantkdragon.uftb", 20)].first_starts)
        self.assertEqual(plot.WDL(0, 12, 2),
                         rows[("kgiantkdragon.uftb", 12)].second_starts)

    def test_concrete_native_rows_become_admitted_minus_trivial(self):
        lines = []
        for giant_class in (20, 16, 15, 12):
            for side_index in range(2):
                lines += [
                    f"reachability_primary_giant_class_total class {giant_class} "
                    f"side {side_index} unknown 0 win 10 loss 8 draw 6",
                    f"reachability_primary_giant_class_excluded class {giant_class} "
                    f"side {side_index} unknown 0 win 1 loss 2 draw 3",
                    f"reachability_primary_giant_class_trivial class {giant_class} "
                    f"side {side_index} unknown 0 win 2 loss 1 draw 1",
                ]
        rows = audit.parse_counts("\n".join(lines), "primary", False)
        self.assertEqual(
            {"wins": 7, "losses": 5, "draws": 2},
            rows[20]["first_starts"]["display"])

    def test_row_orientation_uses_giant_owner_not_filename_order(self):
        record = {
            "filename": "kdragonkgiant.uftb", "primary": "dragon",
            "secondary": "giant", "phase": "kings+2-stateless",
            "opposing": True,
        }
        catalog = plot.OutcomeCatalog(
            {record["filename"]: plot.ReadmeResult(
                plot.WDL(1, 0, 0), plot.WDL(0, 1, 0), "certified")},
            giant_start_classes={
                (record["filename"], 20): plot.ReadmeResult(
                    plot.WDL(3, 0, 0), plot.WDL(0, 4, 0))})
        catalog.opposing[("giant", "dragon")] = record
        cell = catalog.opposed_row("giant_start_20", "dragon")
        self.assertEqual(plot.WDL(0, 4, 0), cell.first)
        self.assertEqual(plot.WDL(0, 3, 0), cell.second)

    def test_information_role_counts_are_normalized_to_ledger_wdl(self):
        classes = {
            value: {
                "first_starts": side(0, value, 1),
                "second_starts": side(value, 0, 2),
            } for value in (20, 16, 15, 12)
        }
        total = sum((20, 16, 15, 12))
        flipped = audit.normalize_and_validate_aggregate(
            "kghostkgiant.uftb", classes,
            plot.ReadmeResult(plot.WDL(total, 0, 4),
                              plot.WDL(0, total, 8)), True)
        self.assertEqual([0, 1], flipped)
        self.assertEqual(20,
                         classes[20]["first_starts"]["display"]["wins"])
        self.assertEqual(20,
                         classes[20]["second_starts"]["display"]["losses"])

    def test_same_team_two_giants_repeat_exchange_folded_aggregate(self):
        catalog = plot.OutcomeCatalog({})
        expected = plot.Cell("draw", plot.WDL(0, 0, 1), plot.WDL(0, 0, 1))
        catalog.together = lambda row, column: expected
        self.assertIs(expected,
                      catalog.together_row("giant_start_20", "giant"))


if __name__ == "__main__":
    unittest.main()
