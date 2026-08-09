import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "generate_ultimate_information_tablebases",
    TOOLS / "generate_ultimate_information_tablebases.py")
assert SPEC and SPEC.loader
generate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = generate
SPEC.loader.exec_module(generate)


class InformationGenerationDriverTests(unittest.TestCase):
    def test_summary_line_requires_exact_uncapped_certificate(self):
        line = (
            "information_summary side 1 win 120 loss 417496 draw 75344 "
            "unreachable_win 0 unreachable_loss 0 unreachable_draw 0 "
            "sets 246600 concrete 492960 bellman_residual 0 "
            "rank_residual 0 belief_cap none exhaustive 1")
        match = generate.SUMMARY_RE.fullmatch(line)
        self.assertIsNotNone(match)
        assert match is not None
        self.assertEqual(int(match["win"]), 120)
        self.assertEqual(int(match["concrete"]), 492_960)
        self.assertIsNone(generate.SUMMARY_RE.fullmatch(
            line.replace("belief_cap none", "belief_cap 64")))
        self.assertIsNone(generate.SUMMARY_RE.fullmatch(
            line.replace("exhaustive 1", "exhaustive 0")))

    def test_checkpoint_is_bound_to_current_solver_sources(self):
        document = generate._empty_document()  # pylint: disable=protected-access
        solver = document["solver"]
        self.assertEqual(
            solver["observation_model_sha256"],
            generate.information.observation_model_fingerprint())
        self.assertTrue(solver["exhaustive"])
        self.assertIsNone(solver["belief_cap"])
        self.assertEqual(solver["version"], "2")
        self.assertEqual(document["schema_version"], 2)
        self.assertEqual(document["semantics"]["id"],
                         "fresh-maximal-public-view-v2")

    def test_v2_defaults_never_reuse_preserved_v1_artifacts(self):
        self.assertIn("legal-dots-v2", str(generate.DEFAULT_CHECKPOINT))
        self.assertIn("legal-dots-v2", str(generate.DEFAULT_OVERLAYS))
        self.assertNotIn("pre-legal-dots-v1",
                         str(generate.DEFAULT_CHECKPOINT))
        self.assertNotIn("pre-legal-dots-v1", str(generate.DEFAULT_OVERLAYS))

    def test_partial_checkpoint_rejects_source_drift(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "partial.json"
            document = generate._empty_document()  # pylint: disable=protected-access
            document["files"]["kjesterk.uftb"] = {
                "solver_model_sha256": "0" * 64,
            }
            generate.save_checkpoint(path, document)
            with self.assertRaisesRegex(RuntimeError, "stale solver"):
                generate.load_checkpoint(path)

    def test_partial_checkpoint_rejects_v1_semantics_before_entries(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "pre-legal-dots-v1.partial.json"
            document = generate._empty_document()  # pylint: disable=protected-access
            document["schema_version"] = 1
            document["semantics"]["id"] = "fresh-maximal-public-view-v1"
            document["solver"]["version"] = "1"
            generate.save_checkpoint(path, document)
            with self.assertRaisesRegex(RuntimeError, "stale schema_version"):
                generate.load_checkpoint(path)

    def test_overlay_reuse_requires_both_hashes_and_exact_domain(self):
        record = generate._records()["kjesterk.uftb"]  # pylint: disable=protected-access
        source = "1" * 64
        model = "2" * 64
        count = generate.information.states_per_side(record) * 2
        header = (struct.pack("<8s6I", b"UFIW2\0\0\0", 2, 1, 30, 0,
                              count, 1) + source.encode() + model.encode())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "overlay.ufiw"
            with path.open("wb") as stream:
                stream.write(header)
                stream.truncate(160 + count)
            self.assertTrue(generate.overlay_is_current(
                path, record, source, model))
            self.assertFalse(generate.overlay_is_current(
                path, record, "0" * 64, model))
            # A valid v1 source hash cannot rescue an overlay whose model hash
            # predates the private pre-decision legal-marker semantics.
            self.assertFalse(generate.overlay_is_current(
                path, record, source,
                generate.information.solver_model_fingerprint(
                    "kjesterk.uftb")))
            with path.open("r+b") as stream:
                stream.truncate(159 + count)
            self.assertFalse(generate.overlay_is_current(
                path, record, source, model))

    def test_checkpoint_merge_preserves_independent_completed_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "partial.json"
            first = {
                "solver_model_sha256":
                    generate.information.solver_model_fingerprint(
                        "kjesterk.uftb"),
                "marker": "first",
            }
            second = {
                "solver_model_sha256":
                    generate.information.solver_model_fingerprint(
                        "kjesterknightk.uftb"),
                "marker": "second",
            }
            generate.merge_checkpoint_entry(path, "kjesterk.uftb", first)
            document = generate.merge_checkpoint_entry(
                path, "kjesterknightk.uftb", second)
            self.assertEqual(
                document["files"]["kjesterk.uftb"]["marker"], "first")
            self.assertEqual(
                document["files"]["kjesterknightk.uftb"]["marker"], "second")


if __name__ == "__main__":
    unittest.main()
