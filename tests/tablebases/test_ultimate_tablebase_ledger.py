import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


ledger = load("ultimate_tablebase_ledger", TOOLS / "update_ultimate_tablebase_ledger.py")
plot = load("ultimate_tablebase_plot", TOOLS / "plot_ultimate_tablebases.py")
archive = load("ultimate_local_tablebase_archive",
               TOOLS / "archive_ultimate_local_tablebases.py")
cleanup = load("ultimate_local_tablebase_cleanup",
               TOOLS / "remove_certified_local_tablebases.py")


class UltimateTablebaseLedgerTests(unittest.TestCase):
    def test_lone_devil_stateful_recovery_is_distinct_from_entry_root_projection(self):
        """Never promote the entry-root projection as the stateful result."""
        readme = ledger.README.read_text()
        rows = {row.key: row for row in ledger.entries(readme)}
        lone = rows["single:devil"]
        companion = rows["same:bishop+devil"]

        self.assertEqual("certified", lone.status)
        self.assertEqual("ultimate-devil-stateful-class-certificate.json",
                         lone.filename)
        self.assertIn("entry-root projection", lone.storage)
        self.assertIn("all twelve primary planes", lone.storage)
        self.assertIn("95649cf36a9f6287379e9d29ee80b67f7af9c8ca6dff0298e73977f458bd3e0f",
                      lone.storage)
        self.assertNotIn("companion", lone.storage.lower())
        self.assertNotIn("bishop", lone.storage.lower())

        self.assertEqual("planned", companion.status)
        self.assertEqual("kbishopdevilk.uftb", companion.filename)
        self.assertIn("All three current supervisor records are explicitly paused",
                      companion.storage)
        self.assertIn("never evidence for single:devil", companion.storage)

        # The authority note must explicitly retract the former 12/12 root
        # projection as certification of the stateful class.
        self.assertIn("single:devil` is **CERTIFIED**", readme)
        self.assertIn("causal entry-root projection", readme)
        self.assertNotIn("single:devil` C1 job remains", readme)
        self.assertNotIn("the live Devil split", readme)

    def test_opposed_berserker_ghost_records_authenticated_v6_header(self):
        """Do not reintroduce the Bishop/Berserker source conflation."""
        rows = {row.key: row for row in ledger.entries(ledger.README.read_text())}
        storage = rows["opposed:berserker+ghost"].storage
        self.assertIn("primary piece id 7 Berserker", storage)
        self.assertIn("secondary piece id 11 Ghost", storage)
        self.assertIn("WDL plane begins at byte 56", storage)
        self.assertNotIn("primary piece id 6", storage)
        self.assertNotIn("encodes primary piece id 6", storage)

    def test_inventory_has_every_unique_plot_cell_exactly_once(self):
        rows = ledger.entries(ledger.README.read_text())
        self.assertEqual(624, len(rows))
        self.assertEqual(624, len({row.key for row in rows}))
        self.assertEqual(24, sum(row.domain == "single" for row in rows))
        self.assertEqual(300, sum(row.domain == "same" for row in rows))
        self.assertEqual(300, sum(row.domain == "opposed" for row in rows))
        self.assertEqual(53, sum(row.status == "deferred" for row in rows))
        self.assertEqual(571, sum(row.status != "deferred" for row in rows))
        self.assertEqual(
            571,
            sum(row.status == status for row in rows
                for status in ("certified", "preserving", "computing",
                               "planned", "draw", "blocked")),
        )
        # These six stateful opposing cells are exact insufficient draws but
        # were formerly missing from the 450-class compute-scope count.
        stateful_draws = {
            "opposed:knight+checker", "opposed:bishop+checker",
            "opposed:turtle+checker", "opposed:mage+checker",
            "opposed:checker+checker", "opposed:checker+fisherman",
        }
        self.assertTrue(stateful_draws <= {
            row.key for row in rows if row.status == "draw"})

    def test_deferred_rows_exactly_match_campaign_exclusions(self):
        """No closed K+K+1/K+K+2 class may hide behind DEFERRED.

        Encode the campaign boundary independently of the ledger
        implementation: decisive Devil and Sludge pairings are not closed
        material. The Devil is sufficient because it can spawn Minions; the
        Angel
        graph-v1 domain admits one Angel with a visible closed companion.
        Angel/Angel is an exact insufficient-material draw regardless of its
        rescue stack; Ghost/Angel source graphs are admitted while their
        hidden-attachment results remain information-required, and spawning
        material remains excluded. Jester/Angel uses the primary-Jester information solver.
        Same-team Copycat/Angel uses an exact four-mode graph
        and arbitrary-linked-pair lower table. Penguin, Mage, and Fisherman can
        still split the simplified symmetric Copycat compound.
        """
        dynamic = {"sludge"}
        angel_excluded = {"sludge", "angel"}
        copycat_separators = {"penguin", "mage", "fisherman"}
        expected_dynamic = set()
        expected_copycat = set()
        pieces = [piece.name for piece in ledger.plan.PIECES]
        for first_index, first in enumerate(pieces):
            if first in dynamic and first != "devil":
                expected_dynamic.add(f"single:{first}")
            for second in pieces[first_index:]:
                names = {first, second}
                target = None
                if "devil" in names:
                    target = None
                elif names & dynamic:
                    target = expected_dynamic
                elif "angel" in names:
                    companion = second if first == "angel" else first
                    if (companion != "angel" and
                            companion in angel_excluded):
                        target = expected_dynamic
                elif "copycat" in names and names & copycat_separators:
                    target = expected_copycat
                if target is not None:
                    target.add(ledger.material_key("same", first, second))
                    target.add(ledger.material_key("opposed", first, second))

        actual = {
            row.key for row in ledger.entries(ledger.README.read_text())
            if row.status == "deferred"
        }
        self.assertEqual(47, len(expected_dynamic))
        self.assertEqual(6, len(expected_copycat))
        self.assertEqual(53, len(actual))
        self.assertEqual(expected_dynamic | expected_copycat, actual)

        devil_rows = {
            row.key: row for row in ledger.entries(ledger.README.read_text())
            if row.key.startswith(("same:", "opposed:")) and
            "devil" in row.key
        }
        self.assertEqual(48, len(devil_rows))
        self.assertTrue(all(row.status in {
            "planned", "computing", "preserving", "certified"
        } for row in devil_rows.values()))

    def test_visible_one_angel_rows_are_in_scope_or_exact_draws(self):
        rows = {row.key: row for row in ledger.entries(ledger.README.read_text())}
        self.assertEqual("draw", rows["single:angel"].status)
        self.assertEqual("closed-form draw", rows["single:angel"].reachability)
        self.assertIn(rows["same:rook+angel"].status,
                      {"planned", "computing", "certified"})
        self.assertEqual(113_873_760, rows["same:rook+angel"].states)
        self.assertIn(rows["opposed:rook+angel"].status,
                      {"planned", "computing", "certified"})
        self.assertEqual(75_915_840, rows["opposed:rook+angel"].states)
        self.assertEqual("draw", rows["same:knight+angel"].status)
        self.assertEqual("draw", rows["opposed:bishop+angel"].status)
        for key in ("same:jester+angel", "opposed:jester+angel"):
            self.assertIn(rows[key].status, {"planned", "computing", "certified"})
        for key in ("same:ghost+angel", "opposed:ghost+angel"):
            self.assertIn(
                rows[key].status,
                {"planned", "computing", "preserving", "certified"},
            )
        self.assertEqual("information v2",
                         rows["same:ghost+angel"].result_kind)
        self.assertEqual("information v2",
                         rows["opposed:ghost+angel"].result_kind)
        self.assertIn(rows["same:copycat+angel"].status,
                      {"planned", "computing", "certified"})
        self.assertEqual(
            303_663_360, rows["same:copycat+angel"].states)
        self.assertIn(rows["opposed:copycat+angel"].status,
                      {"planned", "computing", "certified"})
        self.assertEqual(
            151_831_680, rows["opposed:copycat+angel"].states)
        self.assertEqual("draw", rows["same:angel+angel"].status)
        self.assertEqual("draw", rows["opposed:angel+angel"].status)

    def test_current_computation_hides_stale_result_and_hatches_plot(self):
        with tempfile.TemporaryDirectory() as directory:
            readme = Path(directory) / "README.md"
            readme.write_text(ledger.README.read_text())
            ledger.update(readme, ["kknightkghost.uftb=computing"], [])
            row = next(row for row in ledger.entries(readme.read_text())
                       if row.filename == "kknightkghost.uftb")
            self.assertEqual("computing", row.status)
            self.assertEqual("—", row.first)
            self.assertEqual("—", row.reachability)
            summary = plot.read_summary(readme)
            catalog = plot.OutcomeCatalog(summary)
            self.assertEqual(
                "computing", catalog.opposed("ghost", "knight").kind)

    def test_decommission_reclassifies_only_active_nonfinal_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            readme = Path(directory) / "README.md"
            readme.write_text(ledger.README.read_text())
            ledger.update(
                readme,
                ["opposed:queen+ghost=computing",
                 "same:bishop+devil=preserving"],
                [],
            )
            ledger.update(readme, [], [], decommission_active=True)
            rows = {row.key: row for row in ledger.entries(readme.read_text())}
            self.assertEqual("planned", rows["opposed:queen+ghost"].status)
            self.assertEqual("planned", rows["same:bishop+devil"].status)
            self.assertIn(
                "Fleet decommissioned",
                rows["opposed:queen+ghost"].storage)
            self.assertEqual("certified", rows["single:devil"].status)
            self.assertFalse(any(
                row.status in {"computing", "preserving"}
                for row in rows.values()
            ))

    def test_sleeping_wrappers_are_not_marked_computing(self):
        for row in ledger.entries(ledger.README.read_text()):
            if "sleeping" in row.storage.lower():
                self.assertNotEqual(
                    "computing", row.status,
                    f"{row.key} hatches the plot for a sleeping wrapper")

    def test_closed_preservation_queue_uses_final_or_planned_cells(self):
        catalog = plot.OutcomeCatalog(plot.read_summary(ledger.README))
        self.assertEqual("win", catalog.together("ghost", "rook").kind)
        self.assertEqual("win", catalog.together("prince", "ghost").kind)
        self.assertEqual("unknown", catalog.opposed("ghost", "pawn").kind)
        self.assertEqual("no_forced_loss",
                         catalog.opposed("ghost", "turtle").kind)
        self.assertEqual("win", catalog.opposed("ghost", "checker").kind)
        # The mirrored upper triangle remains deduplicated.
        self.assertEqual("duplicate", catalog.together("rook", "ghost").kind)

    def test_certified_ghost_angel_rows_preserve_dedup(self):
        catalog = plot.OutcomeCatalog(plot.read_summary(ledger.README))
        self.assertEqual("win", catalog.together("angel", "ghost").kind)
        self.assertEqual("mixed", catalog.opposed("ghost", "angel").kind)
        self.assertEqual("duplicate", catalog.together("ghost", "angel").kind)

    def test_s3_only_certified_ledger_result_is_plotted(self):
        summary = plot.read_summary(ledger.README)
        result = summary["kjesterjesterk.uftb"]
        self.assertEqual("certified", result.status)
        self.assertEqual(7_259_568, result.first_starts.wins)
        self.assertEqual(7_406_752, result.second_starts.losses)
        catalog = plot.OutcomeCatalog(summary)
        self.assertNotEqual(
            "unknown", catalog.together("jester", "jester").kind)

    def test_stateful_devil_certificate_plots_filtered_root_cohorts(self):
        summary = plot.read_summary(ledger.README)
        certificate = summary[
            "ultimate-devil-stateful-class-certificate.json"]
        self.assertEqual(certificate, summary["kdevilk.uftb"])
        cell = plot.OutcomeCatalog(summary).single("devil")
        self.assertEqual("mixed", cell.kind)
        self.assertEqual(
            plot.WDL(0, 4_129_343, 7_478_888_477), cell.first)
        self.assertEqual(
            plot.WDL(0, 21_773_540, 24_906_270_149), cell.second)
        self.assertEqual(7_483_017_820, cell.first.total)
        self.assertEqual(24_928_043_689, cell.second.total)
        self.assertEqual(32_411_061_509,
                         cell.first.total + cell.second.total)
        row = next(item for item in ledger.entries(ledger.README.read_text())
                   if item.key == "single:devil")
        first_admitted, first_excluded = ledger.parse_wdl(row.first)
        second_admitted, second_excluded = ledger.parse_wdl(row.second)
        self.assertEqual(
            row.states,
            first_admitted + first_excluded
            + second_admitted + second_excluded,
        )

    def test_every_certified_result_domain_requires_trivial_counts(self):
        readme = """
<!-- GENERATED_TABLE_START -->
| `seed.uftb` | 2 | 0 | 1 / 0 / 0 | 0 / 0 / 1 | `digest` |
<!-- GENERATED_TABLE_END -->
<!-- COMPUTATION_LEDGER_START -->
| `single:test` | Test | single | `test.uftb` | **CERTIFIED** | 2 | future exact domain | 1 / 0 / 0 | 0 / 0 / 1 | 1 / 0; 1 / 0 | stored |
<!-- COMPUTATION_LEDGER_END -->
"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "README.md"
            path.write_text(readme, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "trivial counts"):
                plot.read_summary(path)

        with self.assertRaisesRegex(ValueError, "trivial count"):
            plot.parse_wdl(
                "1 [0] / 0 / 0 [0]", require_trivial=True)

    def test_forced_colors_use_trivial_subtracted_display_counts(self):
        forced_win = plot.classify(
            plot.parse_wdl("100 [10] / 0 [0] / 0 [0]"),
            plot.parse_wdl("50 [5] / 0 [0] / 0 [0]"),
            allow_loss=True,
        )
        self.assertEqual("win", forced_win.kind)
        self.assertEqual("Win", plot.cell_text(forced_win))
        self.assertEqual("#5FAF32", plot.COLORS[forced_win.kind])

        forced_loss = plot.classify(
            plot.parse_wdl("0 [0] / 100 [10] / 0 [0]"),
            plot.parse_wdl("0 [0] / 50 [5] / 0 [0]"),
            allow_loss=True,
        )
        self.assertEqual("loss", forced_loss.kind)
        self.assertEqual("Loss", plot.cell_text(forced_loss))
        self.assertEqual("#D15B3B", plot.COLORS[forced_loss.kind])

    def test_empty_starting_side_is_visible_as_state_dependent_zeros(self):
        populated = plot.WDL(7, 2, 1)
        empty = plot.WDL(0, 0, 0)

        row_empty = plot.classify(empty, populated, allow_loss=True)
        self.assertEqual("mixed", row_empty.kind)
        self.assertEqual("W 0–70%\nL 0–20%\nD 0–10%", plot.cell_text(row_empty))

        column_empty = plot.classify(populated, empty, allow_loss=True)
        self.assertEqual("mixed", column_empty.kind)
        self.assertEqual("W 70–0%\nL 20–0%\nD 10–0%", plot.cell_text(column_empty))

        both_empty = plot.classify(empty, empty, allow_loss=True)
        self.assertEqual("mixed", both_empty.kind)
        self.assertEqual("W 0–0%\nL 0–0%\nD 0–0%", plot.cell_text(both_empty))

    def test_hidden_material_is_never_certified_from_concrete_wdl(self):
        for row in ledger.entries(ledger.README.read_text()):
            if (row.status == "certified" and row.filename and
                    ("jester" in row.filename or "ghost" in row.filename)):
                self.assertTrue(
                    row.result_kind.startswith("information"),
                    f"{row.filename} published concrete hidden-information WDL")

    def test_computing_hatch_is_clipped_parallel_diagonal_lines(self):
        hatch = plot.diagonal_hatch(30, 20, 10, 1)
        alpha = hatch.getchannel("A")
        for point in ((0, 0), (1, 1), (10, 0), (11, 1), (29, 19)):
            self.assertEqual(255, alpha.getpixel(point), point)
        for point in ((0, 5), (5, 0), (20, 5), (5, 19)):
            self.assertEqual(0, alpha.getpixel(point), point)

    def test_reachability_counts_include_all_outcome_buckets(self):
        self.assertEqual(
            "412,616 / 80,344; 492,960 / 0",
            ledger.reachability("412,616 (80,344) / 0 / 0",
                                "3,272 / 414,344 / 75,344"),
        )
        with self.assertRaisesRegex(ValueError, "malformed W/L/D"):
            ledger.reachability("not a result", "0 / 0 / 1")

    def test_trivial_counts_remain_admitted_but_are_removed_from_plot(self):
        cell = "100 [7] (11) / 50 [3] / 25 [25] (5)"
        self.assertEqual((175, 16), ledger.parse_wdl(cell))
        self.assertEqual(plot.WDL(93, 47, 0), plot.parse_wdl(cell))
        with self.assertRaisesRegex(ValueError, "trivial"):
            plot.parse_wdl("2 [3] / 0 / 0")

    def test_published_reachability_matches_parenthesized_wdl_buckets(self):
        checked = 0
        for row in ledger.entries(ledger.README.read_text()):
            if (row.status not in {"certified", "preserving"} or
                    row.reachability == "—"):
                continue
            self.assertEqual(
                ledger.reachability(row.first, row.second),
                row.reachability,
                row.key,
            )
            checked += 1
        self.assertGreaterEqual(checked, 413)

    def test_certified_concrete_wdl_conserves_each_encoded_side(self):
        checked = 0
        for row in ledger.entries(ledger.README.read_text()):
            if (row.status not in {"certified", "preserving"} or
                    row.result_kind != "concrete" or row.states is None or
                    row.reachability == "—"):
                continue
            side_totals = []
            for cell in (row.first, row.second):
                admitted, unreachable = ledger.parse_wdl(cell)
                side_totals.append(admitted + unreachable)
            self.assertEqual(side_totals[0], side_totals[1], row.key)
            pieces = row.key.split(":", 1)[1].split("+")
            prince_factor = 2 ** pieces.count("prince")
            self.assertEqual(
                row.states // (2 * prince_factor), side_totals[0], row.key)
            checked += 1
        self.assertGreater(checked, 300)

    def test_berserker_ninja_uses_admitted_not_omitted_counts(self):
        row = next(
            item for item in ledger.entries(ledger.README.read_text())
            if item.filename == "kberserkerkninja.uftb")
        self.assertEqual(
            "37,634,442 [21,426,156] (133,257,124) / "
            "6,593,328 [259,288] / 12,304,706 [208,544]",
            row.first)
        self.assertEqual(
            "43,467,936 [35,800,204] (52,591,000) / "
            "80,669,688 [3,051,264] / 13,060,976 [1,700,148]",
            row.second)
        self.assertIn("reachability v3 sha256:", row.storage)
        summary = plot.read_summary(ledger.README)
        self.assertEqual("mixed", plot.OutcomeCatalog(summary).opposed(
            "berserker", "ninja").kind)

    def test_opposed_berserker_pair_uses_sidecar_exclusions_as_parentheses(self):
        row = next(
            item for item in ledger.entries(ledger.README.read_text())
            if item.filename == "kberserkerkberserker.uftb")
        expected = (
            "262,700,200 [211,530,242] (1,332,571,240) / "
            "271,396,830 [9,450,088] / 31,227,730 [3,996,402]")
        self.assertEqual(expected, row.first)
        self.assertEqual(expected, row.second)
        self.assertEqual(
            "565,324,760 / 1,332,571,240; "
            "565,324,760 / 1,332,571,240",
            row.reachability)
        self.assertEqual("mixed", plot.OutcomeCatalog(
            plot.read_summary(ledger.README)).opposed(
                "berserker", "berserker").kind)

    def test_near_forced_outcomes_keep_wld_breakdown_and_lighter_color(self):
        catalog = plot.OutcomeCatalog(plot.read_summary(ledger.README))

        prince = catalog.opposed("prince", "bishop")
        self.assertEqual("win_mostly", prince.kind)
        self.assertEqual(0, prince.first.losses)
        self.assertEqual(0, prince.second.losses)
        self.assertGreaterEqual(
            prince.first.wins * 1000, prince.first.total * 985)
        self.assertTrue(plot.cell_text(prince).startswith("W >99–99%\nL 0–0%"))

        bishop = catalog.opposed("bishop", "prince")
        self.assertEqual("loss_mostly", bishop.kind)
        self.assertEqual(0, bishop.first.wins)
        self.assertEqual(0, bishop.second.wins)
        self.assertGreaterEqual(
            bishop.second.losses * 1000, bishop.second.total * 985)
        self.assertTrue(plot.cell_text(bishop).startswith("W 0–0%\nL 99–>99%"))

        self.assertEqual("#B8DA86", plot.COLORS["win_mostly"])
        self.assertEqual("#F5BE98", plot.COLORS["loss_mostly"])
        self.assertNotEqual(
            plot.COLORS["win_star"], plot.COLORS["win_mostly"])
        self.assertNotEqual(
            plot.COLORS["loss_star"], plot.COLORS["loss_mostly"])

    def test_near_forced_highlight_requires_no_opposite_outcome(self):
        almost = plot.WDL(985, 0, 15)
        no_loss = plot.WDL(700, 0, 300)
        self.assertEqual("win_mostly", plot.classify(
            almost, no_loss, allow_loss=True).kind)
        self.assertEqual("mixed", plot.classify(
            plot.WDL(985, 1, 14), no_loss, allow_loss=True).kind)

        almost_loss = plot.WDL(0, 985, 15)
        no_win = plot.WDL(0, 700, 300)
        self.assertEqual("loss_mostly", plot.classify(
            no_win, almost_loss, allow_loss=True).kind)
        self.assertEqual("mixed", plot.classify(
            no_win, plot.WDL(1, 985, 14), allow_loss=True).kind)

        below_threshold = plot.WDL(984, 0, 16)
        self.assertEqual("no_forced_loss", plot.classify(
            below_threshold, no_loss, allow_loss=True).kind)
        self.assertEqual("no_forced_win", plot.classify(
            no_win, plot.WDL(0, 984, 16), allow_loss=True).kind)

        self.assertEqual("#E0EFC4", plot.COLORS["no_forced_loss"])
        self.assertEqual("#FCE2CE", plot.COLORS["no_forced_win"])
        self.assertEqual(6, len({
            plot.COLORS[kind] for kind in (
                "win_star", "win_mostly", "no_forced_loss",
                "no_forced_win", "loss_mostly", "loss_star",
            )
        }))

    def test_legend_spacing_tracks_text_width_and_outcome_groups(self):
        widths = [71, 190, 83, 97, 151, 88, 203, 129, 101, 144]
        positions, total = plot.legend_positions(
            widths, normal_gap=44, group_gap=105)
        gaps = [
            positions[index + 1] - positions[index] - widths[index]
            for index in range(len(widths) - 1)
        ]
        self.assertEqual(
            [44, 44, 105, 44, 105, 44, 44, 105, 44], gaps)
        self.assertEqual(positions[-1] + widths[-1], total)

    def test_berserker_sniper_uses_admitted_not_omitted_counts(self):
        row = next(
            item for item in ledger.entries(ledger.README.read_text())
            if item.filename == "kberserkerksniper.uftb")
        self.assertEqual(
            "112,834,132 [43,082,802] (645,862,628) / 8,413 [0] (479) / "
            "222,407 [119,288] (230,341)",
            row.first)
        self.assertEqual(
            "126,266 [80,882] (73,752,811) / "
            "301,329,375 [12,310,654] (318,640,610) / "
            "36,606,979 [36,051,389] (28,702,359)",
            row.second)
        self.assertIn(
            "reachability v3 sha256:3eb5719205688ab9c980db29ef18f6f3b4773e5d8aef153674bc9eb39fe2153c",
            row.storage)

    def test_legacy_receipt_corrections_remain_canonical(self):
        rows = {row.filename: row
                for row in ledger.entries(ledger.README.read_text())}
        self.assertEqual(
            "372,224 [0] (108,184) / 0 [0] / 72 [0] (12,480)",
            rows["kcopycatk.uftb"].first)
        self.assertEqual(
            "0 [0] (40,776) / 374,136 [0] / 65,568 [65,352] (12,480)",
            rows["kcopycatk.uftb"].second)
        self.assertEqual(
            "30,656 [4,482] (3,121,728) / "
            "8,037,238 [229,674] (43,188) / "
            "2,052,834 [1,392,516] (5,693,316)",
            rows["kbombkgiant.uftb"].second)

    def test_stateful_penguin_sidecar_covers_every_power_substate(self):
        row = next(
            item for item in ledger.entries(ledger.README.read_text())
            if item.filename == "kpenguink.uftb")
        self.assertEqual(
            "1,264 [0] (52,024) / 192 [0] / 491,504 [0] (1,426,856)",
            row.first)
        self.assertEqual(
            "796 [0] (45,080) / 3,272 [0] / "
            "530,700 [78,584] (1,391,992)",
            row.second)
        for cell in (row.first, row.second):
            admitted, unreachable = ledger.parse_wdl(cell)
            self.assertEqual(row.states // 2, admitted + unreachable)

    def test_penguin_dragon_columns_follow_authenticated_file_owner(self):
        row = next(
            item for item in ledger.entries(ledger.README.read_text())
            if item.filename == "kpenguinkdragon.uftb")
        # The authenticated payload and the ledger both order Penguin first.
        # The obsolete Dragon-primary alias must not reverse these cells.
        self.assertEqual(
            "752,816 [34,664] (1,903,140) / 965,350 [287,284] (240) / "
            "20,469,414 [1,652,154] (127,740,720)",
            row.first)
        self.assertEqual(
            "6,345,962 [3,952,822] (5,766,916) / 206,072 [0] / "
            "11,875,666 [369,722] (127,637,064)",
            row.second)
        catalog = plot.OutcomeCatalog(plot.read_summary(ledger.README))
        self.assertEqual(
            "kpenguinkdragon.uftb",
            catalog.opposing[("penguin", "dragon")]["filename"])
        dragon = catalog.opposed("penguin", "dragon")
        self.assertEqual((718_152, 678_066, 18_817_260),
                         (dragon.first.wins, dragon.first.losses,
                          dragon.first.draws))
        self.assertEqual((206_072, 2_393_140, 11_505_944),
                         (dragon.second.wins, dragon.second.losses,
                          dragon.second.draws))

    def test_plot_catalog_maps_every_canonical_record(self):
        catalog = plot.OutcomeCatalog(plot.read_summary(ledger.README))
        for key, expected in ledger.record_catalog().items():
            domain, encoded = key.split(":", 1)
            names = encoded.split("+")
            if domain == "single":
                actual = catalog.singles.get(names[0])
            else:
                pair = tuple(sorted(names, key=plot.PIECE_INDEX.__getitem__))
                actual = (catalog.same_team if domain == "same"
                          else catalog.opposing).get(pair)
            self.assertIsNotNone(actual, key)
            self.assertEqual(expected["filename"], actual["filename"], key)

    def test_exact_certified_import_is_validated_and_persistent(self):
        rows = ledger.entries(ledger.README.read_text())
        encoded = json.dumps({
            "result_kind": "information v2",
            "first": "1 (1) / 0 / 0", "second": "0 / 1 / 1",
            "reachability": "1 / 1; 2 / 0", "storage": "S3 exact",
        })
        certified = ledger.parse_certified([f"kjesterjesterk.uftb={encoded}"])
        updated = ledger.apply_certified(rows, certified)
        row = next(item for item in updated
                   if item.filename == "kjesterjesterk.uftb")
        self.assertEqual("certified", row.status)
        self.assertEqual("information v2", row.result_kind)
        self.assertEqual("1 / 1; 2 / 0", row.reachability)
        with self.assertRaisesRegex(ValueError, "invalid certified result"):
            ledger.parse_certified([
                f"kjesterjesterk.uftb={encoded.replace('1 / 1; 2 / 0', 'bad')}"])

    def test_certified_concrete_cells_survive_unrelated_ledger_updates(self):
        with tempfile.TemporaryDirectory() as raw:
            readme = Path(raw) / "README.md"
            readme.write_text(ledger.README.read_text())
            result = {
                "result_kind": "concrete",
                "first": "1 (2) / 3 / 4",
                "second": "5 / 2 (1) / 2",
                "reachability": "8 / 2; 9 / 1",
                "storage": "S3 exact bound audit",
            }
            ledger.update(
                readme, [], [], certified_values=[
                    "kcopycatk.uftb=" + json.dumps(result)])
            ledger.update(
                readme, [], ["krk.uftb=S3 unrelated storage edit"])
            row = next(item for item in ledger.entries(readme.read_text())
                       if item.filename == "kcopycatk.uftb")
            self.assertEqual(result["first"], row.first)
            self.assertEqual(result["second"], row.second)
            self.assertEqual(result["reachability"], row.reachability)

    def test_launch_gate_refuses_certified_and_untracked_recomputation(self):
        with self.assertRaisesRegex(RuntimeError, "status certified"):
            ledger.check_launch(ledger.README, "krk.uftb", resume=False)
        with self.assertRaisesRegex(RuntimeError, "status certified"):
            ledger.check_launch(
                ledger.README, "kknightkghost.uftb", resume=False)
        with self.assertRaisesRegex(RuntimeError, "status certified"):
            ledger.check_launch(
                ledger.README, "kknightkghost.uftb", resume=True)
        # The live ledger can legitimately have no planned rows when every
        # non-deferred class is already running. Exercise the new-launch gate
        # and the resume gate against isolated copies instead of requiring
        # stale work in the canonical ledger.
        with tempfile.TemporaryDirectory() as directory:
            readme = Path(directory) / "README.md"
            readme.write_text(ledger.README.read_text())
            ledger.update(readme, ["kknightkghost.uftb=planned"], [])
            ledger.check_launch(readme, "kknightkghost.uftb", resume=False)
            ledger.update(readme, ["kknightkghost.uftb=computing"], [])
            with self.assertRaisesRegex(RuntimeError, "status computing"):
                ledger.check_launch(
                    readme, "kknightkghost.uftb", resume=False)
            ledger.check_launch(readme, "kknightkghost.uftb", resume=True)

    def test_archive_stream_round_trip_authenticates_every_physical_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tablebases").mkdir()
            first = root / "tablebases" / "sample.uftb"
            second = root / "tablebases" / "sample.uftb.part000"
            ignored = root / "tablebases" / "sample.checkpoint"
            first.write_bytes(b"header" * 100)
            second.write_bytes(bytes(range(251)) * 10)
            ignored.write_bytes(b"scratch")
            paths = archive.payload_paths(root)
            self.assertEqual([first, second], paths)
            manifest = {
                "schema": archive.SCHEMA,
                "source_commit": "0" * 40,
                "artifacts": [
                    {"path": path.relative_to(root).as_posix(),
                     "bytes": path.stat().st_size,
                     "sha256": archive.sha256_path(path)}
                    for path in paths
                ],
            }
            output = root / "snapshot.tar.zst"
            digest = archive.build_archive(root, manifest, output, 1)
            self.assertEqual(digest, archive.sha256_path(output))
            verified = archive.verify_archive(output)
            self.assertEqual(2, verified["files"])
            self.assertEqual(0, verified["stream_restore_residual"])

    def test_cleanup_requires_exact_restored_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tablebases").mkdir()
            payload = root / "tablebases" / "sample.uftb"
            payload.write_bytes(b"exact payload")
            certificate = root / "certificate.json"
            record = {
                "path": "tablebases/sample.uftb",
                "bytes": payload.stat().st_size,
                "sha256": archive.sha256_path(payload),
            }
            certificate.write_text(json.dumps({
                "schema": archive.SCHEMA,
                "safe_to_delete_local_payloads": True,
                "manifest": {"artifacts": [record]},
                "s3": {"version_id": "version", "head_residual": 0,
                       "download_residual": 0, "stream_restore_residual": 0},
            }))
            self.assertEqual(
                [payload], cleanup.certified_paths(root, certificate))
            (root / "tablebases" / "extra.ufiw").write_bytes(b"not certified")
            with self.assertRaisesRegex(RuntimeError, "inventory differs"):
                cleanup.certified_paths(root, certificate)


if __name__ == "__main__":
    unittest.main()
