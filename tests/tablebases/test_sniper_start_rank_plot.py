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


plot = load("plot_ultimate_tablebases_sniper_test",
            "tools/tablebases/plot_ultimate_tablebases.py")
audit = load("audit_ultimate_sniper_start_ranks_test",
             "tools/tablebases/audit_ultimate_sniper_start_ranks.py")


def counts(wins: int, losses: int, draws: int) -> list[int]:
    return [0, wins, losses, draws]


def rank_side(wins: int, losses: int, draws: int,
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


class SniperStartRankPlotTests(unittest.TestCase):
    def test_rows_use_the_three_color_relative_deployment_ranks(self):
        self.assertEqual("Sniper", plot.PIECE_LABELS["sniper"])
        self.assertEqual(
            ("Sniper (rank 1)", "Sniper (rank 2)", "Sniper (rank 3)"),
            tuple(plot.PIECE_LABELS[row] for row in plot.SNIPER_START_ROWS),
        )

    def test_concrete_parser_requires_and_preserves_all_ten_ranks(self):
        lines = []
        for rank in audit.ALL_RANKS:
            for side in range(2):
                total = counts(rank + side + 6, rank + side + 7,
                               rank + side + 8)
                excluded = counts(1, 2, 3)
                trivial = counts(2, 1, 1)
                for kind, values in (("total", total),
                                     ("excluded", excluded),
                                     ("trivial", trivial)):
                    lines.append(
                        f"reachability_secondary_sniper_rank_{kind} "
                        f"rank {rank} side {side} unknown {values[0]} "
                        f"win {values[1]} loss {values[2]} draw {values[3]}"
                    )
        rows = audit.parse_counts(
            "\n".join(lines), {"primary": "rook", "secondary": "sniper"},
            False,
        )
        self.assertEqual(set(map(str, audit.ALL_RANKS)), set(rows))
        self.assertEqual(
            {"wins": 4, "losses": 5, "draws": 5},
            rows["1"]["first_starts"]["display"],
        )

    def test_information_parser_uses_the_sniper_material_slot(self):
        lines = []
        for rank in audit.ALL_RANKS:
            for side in range(2):
                admitted = counts(rank + 1, rank + 2, rank + 3)
                trivial = counts(1, 1, 1)
                for kind, values in (("admitted", admitted),
                                     ("trivial", trivial)):
                    lines.append(
                        f"information_reachability_primary_sniper_rank_{kind} "
                        f"rank {rank} side {side} unknown {values[0]} "
                        f"win {values[1]} loss {values[2]} draw {values[3]}"
                    )
        rows = audit.parse_counts(
            "\n".join(lines), {"primary": "sniper", "secondary": "ghost"},
            True,
        )
        self.assertEqual(
            {"wins": 3, "losses": 4, "draws": 5},
            rows["3"]["second_starts"]["display"],
        )

    def test_all_ten_buckets_must_reproduce_the_aggregate(self):
        ranks = {
            str(rank): {
                "first_starts": rank_side(rank, 0, 1, False),
                "second_starts": rank_side(0, rank + 1, 2, False),
            }
            for rank in audit.ALL_RANKS
        }
        aggregate = plot.ReadmeResult(
            plot.WDL(sum(audit.ALL_RANKS), 0, 10),
            plot.WDL(0, sum(rank + 1 for rank in audit.ALL_RANKS), 20),
        )
        self.assertEqual(
            [], audit.normalize_and_validate_aggregate(
                "ksniperk.uftb", ranks, aggregate, False
            )
        )

    def test_summary_reader_validates_all_buckets_but_returns_three(self):
        ranks = {
            str(rank): {
                "first_starts": rank_side(rank, 0, 1),
                "second_starts": rank_side(0, rank, 2),
            }
            for rank in audit.ALL_RANKS
        }
        document = {
            "schema": 1,
            "semantics": "reachability-admitted-minus-trivial-v3",
            "all_rank_buckets": list(audit.ALL_RANKS),
            "plotted_start_ranks": list(audit.PLOTTED_RANKS),
            "files": {"krooksniperk.uftb": {"ranks": ranks}},
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            result = plot.read_sniper_start_ranks(path)
        self.assertEqual(
            {("krooksniperk.uftb", rank) for rank in audit.PLOTTED_RANKS},
            set(result),
        )
        self.assertEqual(
            plot.WDL(2, 0, 1), result[("krooksniperk.uftb", 2)].first_starts
        )

    def test_opposed_row_uses_the_sniper_owner_orientation(self):
        record = {
            "filename": "krookksniper.uftb", "primary": "rook",
            "secondary": "sniper", "phase": "kings+2-stateful",
            "opposing": True,
        }
        catalog = plot.OutcomeCatalog(
            {record["filename"]: plot.ReadmeResult(
                plot.WDL(1, 0, 0), plot.WDL(0, 1, 0), "certified")},
            sniper_start_ranks={
                (record["filename"], 2): plot.ReadmeResult(
                    plot.WDL(3, 0, 0), plot.WDL(0, 4, 0))
            },
        )
        catalog.opposing[("rook", "sniper")] = record
        cell = catalog.opposed_row("sniper_rank_2", "rook")
        self.assertEqual(plot.WDL(0, 4, 0), cell.first)
        self.assertEqual(plot.WDL(0, 3, 0), cell.second)

    def test_same_team_row_keeps_material_side_when_sniper_is_secondary(self):
        record = {
            "filename": "krooksniperk.uftb", "primary": "rook",
            "secondary": "sniper", "phase": "kings+2-stateful",
            "opposing": False,
        }
        catalog = plot.OutcomeCatalog(
            {record["filename"]: plot.ReadmeResult(
                plot.WDL(1, 0, 0), plot.WDL(0, 1, 0), "certified")},
            sniper_start_ranks={
                (record["filename"], 2): plot.ReadmeResult(
                    plot.WDL(3, 0, 0), plot.WDL(0, 4, 0))
            },
        )
        catalog.same_team[("rook", "sniper")] = record
        cell = catalog.together_row("sniper_rank_2", "rook")
        self.assertEqual(plot.WDL(3, 0, 0), cell.first)
        self.assertEqual(plot.WDL(4, 0, 0), cell.second)

    def test_same_team_two_snipers_repeat_exchange_folded_aggregate(self):
        catalog = plot.OutcomeCatalog({})
        expected = plot.Cell("draw", plot.WDL(0, 0, 1), plot.WDL(0, 0, 1))
        catalog.together = lambda row, column: expected
        self.assertIs(
            expected, catalog.together_row("sniper_rank_1", "sniper")
        )

    @unittest.skipUnless(
        (ROOT / "tablebases/sniper-start-rank-summary.json").exists(),
        "Sniper rank audit has not been generated",
    )
    def test_published_summary_covers_all_certified_sniper_records(self):
        summary_path = ROOT / "tablebases/sniper-start-rank-summary.json"
        document = json.loads(summary_path.read_text(encoding="utf-8"))
        ledger = plot.read_summary(ROOT / "tablebases/README.md")
        expected = {
            filename for filename in audit.records()
            if ledger.get(filename) and ledger[filename].status == "certified"
        }
        self.assertEqual(expected, set(document["files"]))
        plot.read_sniper_start_ranks(summary_path)
        for filename, record in document["files"].items():
            if record.get("excluded"):
                self.assertTrue(audit.exchange_folded(audit.records()[filename]))
                continue
            self.assertEqual(
                [],
                audit.normalize_and_validate_aggregate(
                    filename, record["ranks"], ledger[filename], False
                ),
            )


if __name__ == "__main__":
    unittest.main()
