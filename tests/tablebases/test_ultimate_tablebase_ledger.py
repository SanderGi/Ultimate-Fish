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
    def test_inventory_has_every_unique_plot_cell_exactly_once(self):
        rows = ledger.entries(ledger.README.read_text())
        self.assertEqual(624, len(rows))
        self.assertEqual(624, len({row.key for row in rows}))
        self.assertEqual(24, sum(row.domain == "single" for row in rows))
        self.assertEqual(300, sum(row.domain == "same" for row in rows))
        self.assertEqual(300, sum(row.domain == "opposed" for row in rows))
        self.assertEqual(147, sum(row.status == "deferred" for row in rows))
        self.assertEqual(477, sum(row.status != "deferred" for row in rows))
        self.assertEqual(
            477,
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

        Encode the requested campaign boundary independently of the ledger
        implementation: Devil, Sludge, and Angel are not closed material, and
        only Penguin, Mage, or Fisherman can split the simplified symmetric
        Copycat compound within a four-model class.
        """
        dynamic = {"devil", "sludge", "angel"}
        copycat_separators = {"penguin", "mage", "fisherman"}
        expected_dynamic = set()
        expected_copycat = set()
        pieces = [piece.name for piece in ledger.plan.PIECES]
        for first_index, first in enumerate(pieces):
            if first in dynamic:
                expected_dynamic.add(f"single:{first}")
            for second in pieces[first_index:]:
                names = {first, second}
                target = None
                if names & dynamic:
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
        self.assertEqual(141, len(expected_dynamic))
        self.assertEqual(6, len(expected_copycat))
        self.assertEqual(expected_dynamic | expected_copycat, actual)

    def test_current_computation_hides_stale_result_and_hatches_plot(self):
        text = ledger.README.read_text()
        rows = ledger.apply_overrides(
            ledger.entries(text), {"kknightkghost.uftb": "computing"}, {})
        row = next(row for row in rows if row.filename == "kknightkghost.uftb")
        self.assertEqual("computing", row.status)
        self.assertEqual("—", row.first)
        self.assertEqual("—", row.reachability)
        summary = plot.read_summary(ledger.README)
        catalog = plot.OutcomeCatalog(summary)
        self.assertEqual("computing", catalog.opposed("ghost", "knight").kind)

    def test_sleeping_wrappers_are_not_marked_computing(self):
        for row in ledger.entries(ledger.README.read_text()):
            if "sleeping" in row.storage.lower():
                self.assertNotEqual(
                    "computing", row.status,
                    f"{row.key} hatches the plot for a sleeping wrapper")

    def test_s3_only_certified_ledger_result_is_plotted(self):
        summary = plot.read_summary(ledger.README)
        result = summary["kjesterjesterk.uftb"]
        self.assertEqual("certified", result.status)
        self.assertEqual(7_259_568, result.first_starts.wins)
        self.assertEqual(8_682_564, result.second_starts.losses)
        catalog = plot.OutcomeCatalog(summary)
        self.assertNotEqual(
            "unknown", catalog.together("jester", "jester").kind)

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
        self.assertEqual(410, checked)

    def test_certified_concrete_wdl_conserves_each_encoded_side(self):
        checked = 0
        for row in ledger.entries(ledger.README.read_text()):
            if (row.status not in {"certified", "preserving"} or
                    row.result_kind != "concrete" or row.states is None):
                continue
            side_totals = []
            for cell in (row.first, row.second):
                admitted, unreachable = ledger.parse_wdl(cell)
                side_totals.append(admitted + unreachable)
            self.assertEqual(side_totals[0], side_totals[1], row.key)
            self.assertEqual(row.states // 2, side_totals[0], row.key)
            checked += 1
        self.assertGreater(checked, 300)

    def test_berserker_ninja_uses_admitted_not_omitted_counts(self):
        row = next(
            item for item in ledger.entries(ledger.README.read_text())
            if item.filename == "kberserkerkninja.uftb")
        self.assertEqual(
            "37,634,442 (133,257,124) / 6,593,328 / 12,304,706",
            row.first)
        self.assertEqual(
            "43,467,936 (52,591,000) / 80,669,688 / 13,060,976",
            row.second)
        self.assertIn("reachability sha256:", row.storage)
        summary = plot.read_summary(ledger.README)
        self.assertEqual("mixed", plot.OutcomeCatalog(summary).opposed(
            "berserker", "ninja").kind)

    def test_berserker_sniper_uses_admitted_not_omitted_counts(self):
        row = next(
            item for item in ledger.entries(ledger.README.read_text())
            if item.filename == "kberserkerksniper.uftb")
        self.assertEqual(
            "112,834,132 (645,862,628) / 8,413 (479) / "
            "222,407 (230,341)",
            row.first)
        self.assertEqual(
            "126,266 (73,752,811) / 301,329,375 (318,640,610) / "
            "36,606,979 (28,702,359)",
            row.second)
        self.assertIn(
            "reachability sha256:590ebdc7051d453605f5153024986c02718f5af22973ab99fccd22373c04167c",
            row.storage)

    def test_legacy_receipt_corrections_remain_canonical(self):
        rows = {row.filename: row
                for row in ledger.entries(ledger.README.read_text())}
        self.assertEqual(
            "372,224 (108,184) / 0 / 72 (12,480)",
            rows["kcopycatk.uftb"].first)
        self.assertEqual(
            "0 (40,776) / 374,136 / 65,568 (12,480)",
            rows["kcopycatk.uftb"].second)
        self.assertEqual(
            "30,670 (3,121,728) / 7,645,846 (43,188) / "
            "2,444,212 (5,693,316)",
            rows["kbombkgiant.uftb"].second)

    def test_stateful_penguin_sidecar_covers_every_power_substate(self):
        row = next(
            item for item in ledger.entries(ledger.README.read_text())
            if item.filename == "kpenguink.uftb")
        self.assertEqual(
            "1,264 (52,024) / 192 / 491,504 (1,426,856)",
            row.first)
        self.assertEqual(
            "796 (45,080) / 3,272 / 530,700 (1,391,992)",
            row.second)
        for cell in (row.first, row.second):
            admitted, unreachable = ledger.parse_wdl(cell)
            self.assertEqual(row.states // 2, admitted + unreachable)

    def test_dragon_penguin_columns_follow_authenticated_file_owner(self):
        row = next(
            item for item in ledger.entries(ledger.README.read_text())
            if item.filename == "kdragonkpenguin.uftb")
        # This filename's authenticated header orders Dragon first even though
        # the display key is sorted as Penguin+Dragon.  Reordering these cells
        # to match the key silently reverses both cells in the opposed plot.
        self.assertEqual(
            "752,816 (1,903,140) / 965,350 (240) / "
            "20,469,414 (127,740,720)",
            row.first)
        self.assertEqual(
            "6,345,962 (5,766,916) / 206,072 / "
            "11,875,666 (127,637,064)",
            row.second)
        catalog = plot.OutcomeCatalog(plot.read_summary(ledger.README))
        self.assertEqual(
            "kdragonkpenguin.uftb",
            catalog.opposing[("penguin", "dragon")]["filename"])
        dragon = catalog.opposed("dragon", "penguin")
        self.assertEqual((752_816, 965_350, 20_469_414),
                         (dragon.first.wins, dragon.first.losses,
                          dragon.first.draws))
        self.assertEqual((206_072, 6_345_962, 11_875_666),
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
        with self.assertRaisesRegex(RuntimeError, "status computing"):
            ledger.check_launch(
                ledger.README, "kknightkghost.uftb", resume=False)
        ledger.check_launch(
            ledger.README, "kknightkghost.uftb", resume=True)
        planned = next(
            entry.filename for entry in ledger.entries(ledger.README.read_text())
            if entry.filename and entry.status == "planned")
        ledger.check_launch(ledger.README, planned, resume=False)

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
