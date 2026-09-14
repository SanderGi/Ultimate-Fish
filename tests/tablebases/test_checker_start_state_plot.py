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


plot = load("plot_ultimate_tablebases_checker_test",
            "tools/tablebases/plot_ultimate_tablebases.py")
audit = load("audit_ultimate_checker_start_states_test",
             "tools/tablebases/audit_ultimate_checker_start_states.py")


def counts(wins: int, losses: int, draws: int) -> list[int]:
    return [0, wins, losses, draws]


def side(wins: int, losses: int, draws: int) -> dict[str, object]:
    total = {"wins": wins + 4, "losses": losses + 5, "draws": draws + 6}
    excluded = {"wins": 1, "losses": 2, "draws": 3}
    admitted = {"wins": wins + 3, "losses": losses + 3, "draws": draws + 3}
    trivial = {"wins": 3, "losses": 3, "draws": 3}
    display = {"wins": wins, "losses": losses, "draws": draws}
    return {"total": total, "excluded": excluded, "admitted": admitted,
            "trivial": trivial, "display": display}


class CheckerStartStatePlotTests(unittest.TestCase):
    def test_rows_are_added_without_renaming_aggregate_checker(self):
        self.assertEqual("Checker", plot.PIECE_LABELS["checker"])
        self.assertEqual(
            ("Checker (normal)", "Checker King"),
            tuple(plot.PIECE_LABELS[row] for row in plot.CHECKER_START_ROWS),
        )

    def test_concrete_parser_groups_ordinary_and_forced_substates(self):
        lines = []
        for substate in range(4):
            for side_index in range(2):
                base = 10 * substate + side_index
                total = counts(base + 9, base + 8, base + 7)
                excluded = counts(1, 2, 3)
                trivial = counts(2, 1, 1)
                for suffix, values in (("_total", total), ("", excluded),
                                       ("_trivial", trivial)):
                    lines.append(
                        f"reachability_secondary_substate{suffix} "
                        f"substate {substate} side {side_index} "
                        f"unknown {values[0]} win {values[1]} "
                        f"loss {values[2]} draw {values[3]}"
                    )
        record = {"primary": "rook", "secondary": "checker"}
        rows = audit.parse_counts("\n".join(lines), record, False)
        # Normal combines substates 0 and 1. For side 0, admitted wins are
        # (9 - 1) + (19 - 1), then two trivial wins are removed per substate.
        self.assertEqual(
            {"wins": 22, "losses": 20, "draws": 16},
            rows["normal"]["first_starts"]["display"],
        )
        self.assertEqual(
            {"wins": 62, "losses": 60, "draws": 56},
            rows["king"]["first_starts"]["display"],
        )

    def test_information_parser_decodes_checker_inside_combined_substate(self):
        # Ghost is primary (factor 2), Checker is secondary (factor 4), so the
        # Checker's substate is the low radix digit of each combined substate.
        lines = []
        for combined in range(8):
            for side_index in range(2):
                checker_substate = combined % 4
                admitted = counts(checker_substate + 2, 0, 1)
                excluded = counts(1, 0, 0)
                trivial = counts(1, 0, 0)
                for kind, values in (("admitted", admitted),
                                     ("excluded", excluded),
                                     ("trivial", trivial)):
                    lines.append(
                        f"information_reachability_substate_{kind} "
                        f"substate {combined} side {side_index} "
                        f"unknown {values[0]} win {values[1]} "
                        f"loss {values[2]} draw {values[3]}"
                    )
        record = {"primary": "ghost", "secondary": "checker"}
        rows = audit.parse_counts("\n".join(lines), record, True)
        self.assertEqual(
            {"wins": 6, "losses": 0, "draws": 4},
            rows["normal"]["first_starts"]["display"],
        )
        self.assertEqual(
            {"wins": 14, "losses": 0, "draws": 4},
            rows["king"]["first_starts"]["display"],
        )

    def test_opposed_ghost_transpose_is_bound_to_exact_overlay(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            overlay = root / "kghostkchecker.ufiw"
            source = (
                "231d2f45d8d1db2a6e47aa413485444d93599fc68ae0d069afabd5e60369ee10")
            model = (
                "bb4e8b3b44571b7ed5caad19dc17d26db9afe976393175f399f5e546ea490316")
            overlay.write_bytes(b"UFIW2\0\0\0" + b"\0" * 24 +
                                source.encode("ascii") + model.encode("ascii"))
            record = {"primary": "ghost", "secondary": "checker",
                      "opposing": True}
            command = audit.command(root / "binary", record,
                                    root / "source.uftb", 2, overlay)
        self.assertIn("--information-transpose-substates", command)

    def test_same_team_ghost_transpose_is_bound_to_exact_overlay(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            overlay = root / "kghostcheckerk.ufiw"
            source = (
                "fdd9329ed29fb7823b27e4bd46263b62f6ac66e638b510e232b3ee386ca6be1e")
            model = (
                "f983e18aae182467f5a0279996c35087d4984b11c80fe6404f16d9214c26d01d")
            overlay.write_bytes(b"UFIW2\0\0\0" + b"\0" * 24 +
                                source.encode("ascii") + model.encode("ascii"))
            record = {"primary": "ghost", "secondary": "checker",
                      "opposing": False}
            command = audit.command(root / "binary", record,
                                    root / "source.uftb", 2, overlay)
        self.assertIn("--information-transpose-substates", command)

    def test_transpose_rejects_the_wrong_material_orientation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            overlay = root / "kghostcheckerk.ufiw"
            source = (
                "fdd9329ed29fb7823b27e4bd46263b62f6ac66e638b510e232b3ee386ca6be1e")
            model = (
                "f983e18aae182467f5a0279996c35087d4984b11c80fe6404f16d9214c26d01d")
            overlay.write_bytes(b"UFIW2\0\0\0" + b"\0" * 24 +
                                source.encode("ascii") + model.encode("ascii"))
            record = {"primary": "ghost", "secondary": "checker",
                      "opposing": True}
            with self.assertRaisesRegex(RuntimeError, "material residual"):
                audit.command(root / "binary", record,
                              root / "source.uftb", 2, overlay)

    def test_summary_parser_checks_conservation_and_preserves_states(self):
        document = {
            "schema": 1,
            "semantics": "reachability-admitted-minus-trivial-v3",
            "start_states": ["normal", "king"],
            "substate_groups": {"normal": [0, 1], "king": [2, 3]},
            "files": {
                "krookkchecker.uftb": {
                    "start_states": {
                        "normal": {
                            "first_starts": side(7, 0, 1),
                            "second_starts": side(0, 8, 2),
                        },
                        "king": {
                            "first_starts": side(9, 0, 3),
                            "second_starts": side(0, 10, 4),
                        },
                    }
                }
            },
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            rows = plot.read_checker_start_states(path)
        self.assertEqual(plot.WDL(7, 0, 1),
                         rows[("krookkchecker.uftb", "normal")].first_starts)
        self.assertEqual(plot.WDL(0, 10, 4),
                         rows[("krookkchecker.uftb", "king")].second_starts)

    def test_opposed_row_uses_checker_owner_not_filename_order(self):
        record = {
            "filename": "krookkchecker.uftb", "primary": "rook",
            "secondary": "checker", "phase": "kings+2-stateful",
            "opposing": True,
        }
        catalog = plot.OutcomeCatalog(
            {record["filename"]: plot.ReadmeResult(
                plot.WDL(1, 0, 0), plot.WDL(0, 1, 0), "certified")},
            checker_start_states={
                (record["filename"], "king"): plot.ReadmeResult(
                    plot.WDL(3, 0, 0), plot.WDL(0, 4, 0))})
        catalog.opposing[("rook", "checker")] = record
        cell = catalog.opposed_row("checker_king", "rook")
        self.assertEqual(plot.WDL(0, 4, 0), cell.first)
        self.assertEqual(plot.WDL(0, 3, 0), cell.second)

    def test_single_and_insufficient_opposed_rows_are_closed_form_draws(self):
        catalog = plot.OutcomeCatalog({})
        self.assertEqual("draw", catalog.single_row("checker_normal").kind)
        self.assertEqual("draw", catalog.single_row("checker_king").kind)
        self.assertEqual("draw",
                         catalog.opposed_row("checker_king", "bishop").kind)

    def test_same_team_two_checkers_repeat_exchange_folded_aggregate(self):
        catalog = plot.OutcomeCatalog({})
        expected = plot.Cell("draw", plot.WDL(0, 0, 1), plot.WDL(0, 0, 1))
        catalog.together = lambda row, column: expected
        self.assertIs(expected,
                      catalog.together_row("checker_normal", "checker"))
        self.assertIs(expected,
                      catalog.together_row("checker_king", "checker"))

    def test_aggregate_validation_can_normalize_information_roles(self):
        states = {
            "normal": {
                "first_starts": side(0, 5, 1),
                "second_starts": side(6, 0, 2),
            },
            "king": {
                "first_starts": side(0, 7, 3),
                "second_starts": side(8, 0, 4),
            },
        }
        flipped = audit.normalize_and_validate_aggregate(
            "kghostkchecker.uftb", states,
            plot.ReadmeResult(plot.WDL(12, 0, 4), plot.WDL(0, 14, 6)), True)
        self.assertEqual([0, 1], flipped)
        self.assertEqual(5,
                         states["normal"]["first_starts"]["display"]["wins"])
        self.assertEqual(6,
                         states["normal"]["second_starts"]["display"]["losses"])

    def test_certified_summary_covers_every_distinguished_checker(self):
        path = ROOT / "tablebases" / "checker-start-state-summary.json"
        document = json.loads(path.read_text(encoding="utf-8"))
        summaries = plot.read_summary(ROOT / "tablebases" / "README.md")
        certified = {
            filename for filename in audit.records()
            if summaries.get(filename) and summaries[filename].status == "certified"
        }
        self.assertEqual(certified, set(document["files"]))
        self.assertEqual(
            "exchange-folded same-team Checkers have no distinguished row Checker",
            document["files"]["kcheckercheckerk.uftb"]["reason"],
        )
        slices = plot.read_checker_start_states(path)
        self.assertEqual(2 * (len(certified) - 1), len(slices))
        for filename in certified - {"kcheckercheckerk.uftb"}:
            aggregate = summaries[filename]
            for key, expected in (("first_starts", aggregate.first_starts),
                                  ("second_starts", aggregate.second_starts)):
                actual = plot.WDL(*(
                    sum(getattr(getattr(slices[(filename, state)], key), field)
                        for state in ("normal", "king"))
                    for field in ("wins", "losses", "draws")
                ))
                self.assertEqual(expected, actual, f"{filename} {key}")

    def test_same_team_ghost_checker_king_cell_is_populated(self):
        path = ROOT / "tablebases" / "checker-start-state-summary.json"
        slices = plot.read_checker_start_states(path)
        king = slices[("kghostcheckerk.uftb", "king")]
        self.assertEqual(plot.WDL(29_224_216, 0, 0), king.first_starts)
        self.assertEqual(plot.WDL(0, 29_344_098, 0), king.second_starts)

        summary = plot.read_summary(ROOT / "tablebases" / "README.md")
        catalog = plot.OutcomeCatalog(summary, checker_start_states=slices)
        cell = catalog.together_row("checker_king", "ghost")
        self.assertEqual("win", cell.kind)
        self.assertEqual(plot.WDL(29_224_216, 0, 0), cell.first)
        self.assertEqual(plot.WDL(29_344_098, 0, 0), cell.second)

        aggregate = catalog.together("checker", "ghost")
        normal = catalog.together_row("checker_normal", "ghost")
        self.assertEqual(aggregate.first.wins,
                         normal.first.wins + cell.first.wins)
        self.assertEqual(aggregate.second.wins,
                         normal.second.wins + cell.second.wins)


if __name__ == "__main__":
    unittest.main()
