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


plot = load("plot_ultimate_tablebases_devil_minion_test",
            "tools/tablebases/plot_ultimate_tablebases.py")
audit = load("audit_ultimate_devil_minion_starts_test",
             "tools/tablebases/audit_ultimate_devil_minion_starts.py")


def side(wins: int, losses: int, draws: int) -> dict[str, object]:
    return {
        "admitted": {"wins": wins, "losses": losses, "draws": draws},
        "trivial": {"wins": 0, "losses": 0, "draws": 0},
        "display": {"wins": wins, "losses": losses, "draws": draws},
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
            "schema": 1,
            "semantics": "reachable-devil-alive-root-current-minions-v1",
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

    def test_summary_aggregates_only_alive_roots_into_rows(self):
        rows = []
        for count in range(6):
            first = {"win": count + 1, "loss": 0, "draw": 0}
            second = {"win": 0, "loss": count + 2, "draw": 0}
            rows.append({
                "minions": count,
                "alive_outcomes_by_side_to_move": [first, second],
            })
        alive = sum(count + 1 + count + 2 for count in range(6))
        certificate = {
            "aggregate": {
                "states": alive + 7, "wins": 21, "losses": 27,
                "draws": 7,
            },
            "partitions": [{
                "label": "A1", "square": 0, "states": alive + 7,
                "sidecar_sha256": "a" * 64,
            }],
        }
        outcomes = {
            "win": 21, "loss": 27, "draw": 7}
        progress = {"0": {
            "states": alive + 7, "outcomes": outcomes,
            "by_minion_count": rows, "elapsed_seconds": 1.0,
            "payloads": [],
        }}
        summary = audit.build_summary(
            certificate, "b" * 64, progress, "dataset", "c" * 40,
            "d" * 64, "e" * 64)
        self.assertEqual(alive, summary["alive_root_states"])
        self.assertEqual(7, summary["dead_devil_continuation_states"])
        self.assertEqual(
            {"wins": 4, "losses": 0, "draws": 0},
            summary["files"]["kdevilk.uftb"]["minion_counts"]["3"]
            ["first_starts"]["display"],
        )


if __name__ == "__main__":
    unittest.main()
