import importlib.util
import copy
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


plot = load("plot_ultimate_tablebases_devil_minion_test",
            "tools/tablebases/plot_ultimate_tablebases.py")
audit = load("audit_ultimate_devil_minion_starts_test",
             "tools/tablebases/audit_ultimate_devil_minion_starts.py")


def side(wins: int, losses: int, draws: int) -> dict[str, object]:
    counts = {"wins": wins, "losses": losses, "draws": draws}
    zero = {"wins": 0, "losses": 0, "draws": 0}
    return {
        "total": dict(counts), "excluded": dict(zero),
        "admitted": dict(counts), "trivial": dict(zero),
        "display": dict(counts),
    }


def root_filter(outcomes: dict[str, int]) -> dict[str, dict[str, int]]:
    zero = {name: 0 for name in ("win", "loss", "draw")}
    return {
        "total": dict(outcomes), "excluded": dict(zero),
        "admitted": dict(outcomes), "trivial": dict(zero),
        "display": dict(outcomes),
    }


class DevilMinionStartPlotTests(unittest.TestCase):
    def test_rows_label_zero_through_five_current_minions(self):
        self.assertEqual(6, len(plot.DEVIL_MINION_ROWS))
        self.assertEqual(
            ("Devil + 0 Minions", "Devil + 1 Minion", "Devil + 2 Minions",
             "Devil + 3 Minions", "Devil + 4 Minions", "Devil + 5 Minions"),
            tuple(plot.PIECE_LABELS[row] for row in plot.DEVIL_MINION_ROWS),
        )

    def test_summary_parser_and_lone_cell_leave_companions_blank(self):
        document = {
            "schema": 2,
            "semantics": "stateful-reachability-admitted-minus-trivial-v1",
            "minion_counts": list(range(6)),
            "files": {
                "kdevilk.uftb": {
                    "minion_counts": {
                        str(count): {
                            "first_starts": side(count + 1, 0, 0),
                            "second_starts": side(0, count + 2, 0),
                        } for count in range(6)
                    }
                }
            },
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            starts = plot.read_devil_minion_starts(path)
        catalog = plot.OutcomeCatalog(
            {"kdevilk.uftb": plot.ReadmeResult(
                plot.WDL(1, 0, 0), plot.WDL(0, 1, 0), "certified")},
            devil_minion_starts=starts,
        )
        cell = catalog.single_row("devil_minions_3")
        self.assertEqual("win", cell.kind)
        self.assertEqual(plot.WDL(4, 0, 0), cell.first)
        self.assertEqual(plot.WDL(5, 0, 0), cell.second)
        self.assertEqual("unknown",
                         catalog.together_row("devil_minions_3", "bishop").kind)
        self.assertEqual("unknown",
                         catalog.opposed_row("devil_minions_3", "bishop").kind)

    def test_overall_devil_cell_equals_all_filtered_minion_cohorts(self):
        starts = plot.read_devil_minion_starts(
            ROOT / "tablebases" / "devil-minion-start-summary.json")
        summary = plot.read_summary(ROOT / "tablebases" / "README.md")
        aggregate = summary["kdevilk.uftb"]

        for key in ("first_starts", "second_starts"):
            rows = [getattr(starts[("kdevilk.uftb", count)], key)
                    for count in range(6)]
            expected = plot.WDL(*(
                sum(getattr(row, field) for row in rows)
                for field in ("wins", "losses", "draws")
            ))
            self.assertEqual(expected, getattr(aggregate, key))

        cell = plot.OutcomeCatalog(summary).single("devil")
        self.assertEqual(32_411_061_509,
                         cell.first.total + cell.second.total)

    def test_summary_aggregates_only_alive_roots_into_rows(self):
        rows = []
        for count in range(6):
            if count == 0:
                first = dict(audit.ZERO_MINION_ORACLE[0]["total"])
                second = dict(audit.ZERO_MINION_ORACLE[1]["total"])
                filters = copy.deepcopy(audit.ZERO_MINION_ORACLE)
            else:
                first = {"win": count + 1, "loss": 0, "draw": 0}
                second = {"win": 0, "loss": count + 2, "draw": 0}
                filters = [root_filter(first), root_filter(second)]
            rows.append({
                "minions": count,
                "alive_outcomes_by_side_to_move": [first, second],
                "root_filter_by_side_to_move": filters,
            })
        alive_outcomes = {
            name: sum(row["alive_outcomes_by_side_to_move"][side][name]
                      for row in rows for side in range(2))
            for name in ("win", "loss", "draw")
        }
        alive = sum(alive_outcomes.values())
        certificate = {
            "aggregate": {
                "states": alive + 7, "wins": alive_outcomes["win"],
                "losses": alive_outcomes["loss"],
                "draws": alive_outcomes["draw"] + 7,
            },
            "partitions": [{
                "label": "A1", "square": 0, "states": alive + 7,
                "sidecar_sha256": "a" * 64,
            }],
        }
        outcomes = {
            "win": alive_outcomes["win"], "loss": alive_outcomes["loss"],
            "draw": alive_outcomes["draw"] + 7}
        progress = {"0": {
            "states": alive + 7, "outcomes": outcomes,
            "by_minion_count": rows, "elapsed_seconds": 1.0,
            "payloads": [],
        }}
        summary = audit.build_summary(
            certificate, "b" * 64, progress, "dataset", "c" * 40,
            "d" * 64, "e" * 64, "f" * 64,
            "DEVIL_SLICE_VERIFIED states=1 admitted=1")
        self.assertEqual(alive, summary["alive_root_states"])
        self.assertEqual(7, summary["dead_devil_continuation_states"])
        self.assertEqual(
            {"wins": 4, "losses": 0, "draws": 0},
            summary["files"]["kdevilk.uftb"]["minion_counts"]["3"]
            ["first_starts"]["display"],
        )
        self.assertEqual(2 * 25_120,
                         summary["root_filter_states"]["excluded"])

    def test_a1_gate_rejects_semantic_drift(self):
        census = {
            "label": "A1", "square": 0,
            "by_minion_count": [
                {"minions": count,
                 "root_filter_by_side_to_move": [
                     *copy.deepcopy(audit.A1_ZERO_MINION_ORACLE)]}
                for count in range(6)
            ],
        }
        audit.validate_a1_gate(census)
        census["by_minion_count"][0]["root_filter_by_side_to_move"][0][
            "display"]["draw"] -= 1
        with self.assertRaisesRegex(RuntimeError, "A1 pilot"):
            audit.validate_a1_gate(census)


if __name__ == "__main__":
    unittest.main()
