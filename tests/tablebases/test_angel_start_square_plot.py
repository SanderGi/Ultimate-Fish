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


plot = load("plot_ultimate_tablebases_angel_test",
            "tools/tablebases/plot_ultimate_tablebases.py")
audit = load("audit_ultimate_angel_start_squares_test",
             "tools/tablebases/audit_ultimate_angel_start_squares.py")


def counts(wins: int, losses: int, draws: int) -> list[int]:
    return [0, wins, losses, draws]


def square_side(wins: int, losses: int, draws: int,
                concrete: bool = True) -> dict[str, object]:
    admitted = {"wins": wins + 1, "losses": losses + 2, "draws": draws + 3}
    trivial = {"wins": 1, "losses": 2, "draws": 3}
    result: dict[str, object] = {
        "admitted": admitted,
        "trivial": trivial,
        "display": {"wins": wins, "losses": losses, "draws": draws},
    }
    if concrete:
        excluded = {"wins": 4, "losses": 5, "draws": 6}
        result["excluded"] = excluded
        result["total"] = {
            "wins": admitted["wins"] + excluded["wins"],
            "losses": admitted["losses"] + excluded["losses"],
            "draws": admitted["draws"] + excluded["draws"],
        }
    return result


class AngelStartSquarePlotTests(unittest.TestCase):
    def test_rows_cover_twelve_legal_squares_up_to_file_reflection(self):
        self.assertEqual("Angel", plot.PIECE_LABELS["angel"])
        self.assertEqual(
            (
                "Angel (a1/h1)", "Angel (b1/g1)",
                "Angel (c1/f1)", "Angel (d1/e1)",
                "Angel (a2/h2)", "Angel (b2/g2)",
                "Angel (c2/f2)", "Angel (d2/e2)",
                "Angel (a3/h3)", "Angel (b3/g3)",
                "Angel (c3/f3)", "Angel (d3/e3)",
            ),
            tuple(plot.PIECE_LABELS[row] for row in plot.ANGEL_START_ROWS),
        )

    def test_concrete_parser_requires_all_forty_square_classes(self):
        lines = []
        for index, square in enumerate(audit.ALL_SQUARES, 1):
            for side in range(2):
                total = counts(index + side + 6, index + side + 7,
                               index + side + 8)
                excluded = counts(1, 2, 3)
                trivial = counts(2, 1, 1)
                for kind, values in (("total", total),
                                     ("excluded", excluded),
                                     ("trivial", trivial)):
                    lines.append(
                        f"reachability_secondary_angel_square_{kind} "
                        f"square {square} side {side} unknown {values[0]} "
                        f"win {values[1]} loss {values[2]} draw {values[3]}"
                    )
        rows = audit.parse_counts(
            "\n".join(lines), {"primary": "rook", "secondary": "angel"},
            False,
        )
        self.assertEqual(set(audit.ALL_SQUARES), set(rows))
        self.assertEqual(
            {"wins": 4, "losses": 5, "draws": 5},
            rows["a1"]["first_starts"]["display"],
        )

    def test_information_parser_uses_the_angel_material_slot(self):
        lines = []
        for index, square in enumerate(audit.ALL_SQUARES, 1):
            for side in range(2):
                admitted = counts(index + 1, index + 2, index + 3)
                trivial = counts(1, 1, 1)
                for kind, values in (("admitted", admitted),
                                     ("trivial", trivial)):
                    lines.append(
                        f"information_reachability_primary_angel_square_{kind} "
                        f"square {square} side {side} unknown {values[0]} "
                        f"win {values[1]} loss {values[2]} draw {values[3]}"
                    )
        rows = audit.parse_counts(
            "\n".join(lines), {"primary": "angel", "secondary": "ghost"},
            True,
        )
        self.assertEqual(
            {"wins": 12, "losses": 13, "draws": 14},
            rows["d3"]["second_starts"]["display"],
        )

    def test_all_forty_buckets_must_reproduce_the_aggregate(self):
        squares = {
            square: {
                "first_starts": square_side(index, 0, 1, False),
                "second_starts": square_side(0, index + 1, 2, False),
            }
            for index, square in enumerate(audit.ALL_SQUARES, 1)
        }
        aggregate = plot.ReadmeResult(
            plot.WDL(sum(range(1, 41)), 0, 40),
            plot.WDL(0, sum(range(2, 42)), 80),
        )
        self.assertEqual(
            [], audit.normalize_and_validate_aggregate(
                "krookangelk.uftb", squares, aggregate, False
            )
        )

    def test_summary_reader_validates_all_buckets_but_returns_twelve(self):
        squares = {
            square: {
                "first_starts": square_side(index, 0, 1),
                "second_starts": square_side(0, index, 2),
            }
            for index, square in enumerate(audit.ALL_SQUARES, 1)
        }
        document = {
            "schema": 1,
            "semantics": "reachability-admitted-minus-trivial-v3",
            "all_square_buckets": list(audit.ALL_SQUARES),
            "plotted_start_squares": list(audit.PLOTTED_SQUARES),
            "file_symmetry": {"a": "a/h", "b": "b/g",
                              "c": "c/f", "d": "d/e"},
            "files": {"krookangelk.uftb": {"squares": squares}},
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            result = plot.read_angel_start_squares(path)
        self.assertEqual(
            {("krookangelk.uftb", square)
             for square in audit.PLOTTED_SQUARES},
            set(result),
        )
        self.assertEqual(
            plot.WDL(8, 0, 1), result[("krookangelk.uftb", "d2")].first_starts
        )

    def test_opposed_row_uses_the_angel_owner_orientation(self):
        record = {
            "filename": "krookkangel.uftb", "primary": "rook",
            "secondary": "angel", "phase": "kings+2-angel-graph-v1",
            "opposing": True,
        }
        catalog = plot.OutcomeCatalog(
            {record["filename"]: plot.ReadmeResult(
                plot.WDL(1, 0, 0), plot.WDL(0, 1, 0), "certified")},
            angel_start_squares={
                (record["filename"], "b2"): plot.ReadmeResult(
                    plot.WDL(3, 0, 0), plot.WDL(0, 4, 0))
            },
        )
        catalog.opposing[("rook", "angel")] = record
        cell = catalog.opposed_row("angel_square_b2", "rook")
        self.assertEqual(plot.WDL(0, 4, 0), cell.first)
        self.assertEqual(plot.WDL(0, 3, 0), cell.second)

    def test_same_team_row_keeps_material_side_when_angel_is_secondary(self):
        record = {
            "filename": "krookangelk.uftb", "primary": "rook",
            "secondary": "angel", "phase": "kings+2-angel-graph-v1",
            "opposing": False,
        }
        catalog = plot.OutcomeCatalog(
            {record["filename"]: plot.ReadmeResult(
                plot.WDL(1, 0, 0), plot.WDL(0, 1, 0), "certified")},
            angel_start_squares={
                (record["filename"], "b2"): plot.ReadmeResult(
                    plot.WDL(3, 0, 0), plot.WDL(0, 4, 0))
            },
        )
        catalog.same_team[("rook", "angel")] = record
        cell = catalog.together_row("angel_square_b2", "rook")
        self.assertEqual(plot.WDL(3, 0, 0), cell.first)
        self.assertEqual(plot.WDL(4, 0, 0), cell.second)

    def test_lone_and_two_angel_rows_remain_closed_form_draws(self):
        catalog = plot.OutcomeCatalog({})
        self.assertEqual("draw", catalog.single_row("angel_square_a1").kind)
        self.assertEqual("draw",
                         catalog.together_row("angel_square_a1", "angel").kind)
        self.assertEqual("draw",
                         catalog.opposed_row("angel_square_a1", "angel").kind)

    @unittest.skipUnless(
        (ROOT / "tablebases/angel-start-square-summary.json").exists(),
        "Angel square audit has not been generated",
    )
    def test_published_summary_covers_all_certified_angel_records(self):
        summary_path = ROOT / "tablebases/angel-start-square-summary.json"
        document = json.loads(summary_path.read_text(encoding="utf-8"))
        ledger = plot.read_summary(ROOT / "tablebases/README.md")
        expected = {
            filename for filename in audit.records()
            if ledger.get(filename) and ledger[filename].status == "certified"
        }
        self.assertEqual(expected, set(document["files"]))
        excluded = {
            filename for filename, record in document["files"].items()
            if record.get("excluded")
        }
        self.assertEqual(audit.KNOWN_UNAVAILABLE_OVERLAYS, excluded)
        self.assertEqual(
            28,
            sum(record.get("result_kind") == "concrete"
                for record in document["files"].values()),
        )
        self.assertEqual(
            2,
            sum(record.get("result_kind") == "information-v2"
                for record in document["files"].values()),
        )
        self.assertEqual(
            {"kghostkangel.uftb": [0, 1]},
            {
                filename: record["role_normalized_sides"]
                for filename, record in document["files"].items()
                if record.get("role_normalized_sides")
            },
        )
        plot.read_angel_start_squares(summary_path)
        for filename, record in document["files"].items():
            if record.get("excluded"):
                self.assertIn(filename, audit.KNOWN_UNAVAILABLE_OVERLAYS)
                continue
            self.assertEqual(
                [],
                audit.normalize_and_validate_aggregate(
                    filename, record["squares"], ledger[filename], False
                ),
            )


if __name__ == "__main__":
    unittest.main()
