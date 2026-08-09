import importlib.util
from pathlib import Path
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


if __name__ == "__main__":
    unittest.main()
