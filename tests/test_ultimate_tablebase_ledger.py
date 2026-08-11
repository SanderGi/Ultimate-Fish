import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
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
        # These six stateful opposing cells are exact insufficient draws but
        # were formerly missing from the 450-class compute-scope count.
        stateful_draws = {
            "opposed:knight+checker", "opposed:bishop+checker",
            "opposed:turtle+checker", "opposed:mage+checker",
            "opposed:checker+checker", "opposed:checker+fisherman",
        }
        self.assertTrue(stateful_draws <= {
            row.key for row in rows if row.status == "draw"})

    def test_current_computation_hides_stale_result_and_hatches_plot(self):
        text = ledger.README.read_text()
        rows = ledger.apply_overrides(
            ledger.entries(text), {"kghostghostk.uftb": "computing"}, {})
        row = next(row for row in rows if row.filename == "kghostghostk.uftb")
        self.assertEqual("computing", row.status)
        self.assertEqual("—", row.first)
        self.assertEqual("—", row.reachability)
        summary = plot.read_summary(ledger.README)
        catalog = plot.OutcomeCatalog(summary)
        self.assertEqual("computing", catalog.together("ghost", "ghost").kind)

    def test_s3_only_certified_ledger_result_is_plotted(self):
        summary = plot.read_summary(ledger.README)
        result = summary["kjesterjesterk.uftb"]
        self.assertEqual("certified", result.status)
        self.assertEqual(7_259_568, result.first_starts.wins)
        self.assertEqual(8_682_564, result.second_starts.losses)
        catalog = plot.OutcomeCatalog(summary)
        self.assertNotEqual(
            "unknown", catalog.together("jester", "jester").kind)

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

    def test_launch_gate_refuses_certified_and_untracked_recomputation(self):
        with self.assertRaisesRegex(RuntimeError, "status preserving"):
            ledger.check_launch(ledger.README, "krk.uftb", resume=False)
        with self.assertRaisesRegex(RuntimeError, "status computing"):
            ledger.check_launch(
                ledger.README, "kghostghostk.uftb", resume=False)
        ledger.check_launch(
            ledger.README, "kghostghostk.uftb", resume=True)
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
