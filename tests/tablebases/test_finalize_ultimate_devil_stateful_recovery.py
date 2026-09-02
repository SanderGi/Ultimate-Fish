import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
SPEC = importlib.util.spec_from_file_location(
    "finalize_devil_stateful",
    TOOLS / "finalize_ultimate_devil_stateful_recovery.py")
assert SPEC and SPEC.loader
finalize = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(finalize)


class FinalizeUltimateDevilStatefulRecoveryTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads(finalize.DEFAULT_MANIFEST.read_text())

    def _complete(self):
        data = copy.deepcopy(self.manifest)
        template = next(row for row in data["partitions"]
                        if "census_sha256" in row)
        for row in data["partitions"]:
            if row["status"] == "PRESERVED" and "census_sha256" in row:
                continue
            row.clear()
            row.update(copy.deepcopy(template))
            row["label"] = {3: "D1", 10: "C2", 19: "D3"}.get(
                row.get("square"), "X")
        # Reconstruct squares because clear() discarded them.
        for row, square in zip(data["partitions"], finalize.FIXED_SQUARES):
            row["square"] = square
            row["label"] = f"{chr(ord('A') + square % 8)}{square // 8 + 1}"
        return data

    def test_incomplete_campaign_fails_closed(self):
        incomplete = copy.deepcopy(self.manifest)
        incomplete["partitions"][-1]["status"] = "RUNNING"
        with self.assertRaisesRegex(ValueError, "lacks census counts|not preserved"):
            finalize.build_certificate(incomplete)

    def test_complete_certificate_has_exact_coverage_and_conservation(self):
        certificate = finalize.build_certificate(self._complete())
        self.assertEqual(list(finalize.FIXED_SQUARES),
                         certificate["fixed_squares"])
        self.assertTrue(certificate["entry_slice_excluded_as_class_proof"])
        self.assertEqual(0, certificate["aggregate"]["conservation_residual"])
        self.assertEqual(12, len(certificate["partitions"]))
        self.assertEqual(
            sum(row["states"] for row in certificate["partitions"]),
            certificate["aggregate"]["states"],
        )

    def test_duplicate_or_missing_square_fails_closed(self):
        data = self._complete()
        data["partitions"][-1]["square"] = 18
        with self.assertRaisesRegex(ValueError, "coverage mismatch"):
            finalize.build_certificate(data)

    def test_census_conservation_failure_is_rejected(self):
        data = self._complete()
        data["partitions"][0]["census_draws"] += 1
        with self.assertRaisesRegex(ValueError, "conservation failure"):
            finalize.build_certificate(data)


if __name__ == "__main__":
    unittest.main()
