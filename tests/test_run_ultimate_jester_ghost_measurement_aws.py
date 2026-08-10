#!/usr/bin/env python3

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


measure = load(
    "jg_measure", ROOT / "tools/run_ultimate_jester_ghost_measurement_aws.py")
certify = load(
    "jg_merge_certify",
    ROOT / "tools/certify_ultimate_jester_ghost_merge_aws.py")


class JesterGhostMeasurementTest(unittest.TestCase):
    def test_exact_one_sweep_certificate_is_required(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "measure.log"
            log.write_text(
                "information_summary side 0 win 0 loss 0 draw 0 "
                "unreachable_win 0 unreachable_loss 0 unreachable_draw 0 "
                "sets 0 concrete 0 bellman_residual 0 rank_residual 0 "
                "belief_cap none exhaustive 1\n"
                "information_summary side 1 win 0 loss 0 draw 0 "
                "unreachable_win 0 unreachable_loss 0 unreachable_draw 0 "
                "sets 0 concrete 0 bellman_residual 0 rank_residual 0 "
                "belief_cap none exhaustive 1\n"
                "information_symbolic_certificate iterations 1 "
                "upper_nodes 123 lower_nodes 45 bellman_residual 0 "
                "monotonicity_residual 0 singleton_residual 0 belief_cap "
                "none powerset_exact 1\n"
                "jester_ghost_artifacts overlay_sha256  arbitrary_sha256  "
                "arbitrary_structural_residual 0 arbitrary_singleton_residual "
                "0 arbitrary_root_residual 0\n")
            parsed = measure.parse_log(log)
            self.assertEqual(1, parsed["iterations"])
            self.assertEqual(123, parsed["upper_nodes"])
            self.assertEqual(45, parsed["lower_nodes"])

    def test_nonzero_measurement_residual_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "measure.log"
            log.write_text(
                "information_summary side 0 belief_cap none exhaustive 1\n"
                "information_summary side 1 belief_cap none exhaustive 1\n"
                "information_symbolic_certificate iterations 1 upper_nodes "
                "2 lower_nodes 2 bellman_residual 1 monotonicity_residual 0 "
                "singleton_residual 0 belief_cap none powerset_exact 1\n"
                "jester_ghost_artifacts empty\n")
            with self.assertRaisesRegex(ValueError, "residual"):
                measure.parse_log(log)

    def test_certifier_binds_exact_production_totals(self):
        self.assertEqual({
            "raw": 38_450_880, "canonical": 9_612_720,
            "worlds": 37_957_920, "edges": 479_456_062,
        }, certify.EXPECTED)

    def test_supervisor_keeps_full_solve_out_of_measurement_stage(self):
        config = json.loads((
            ROOT / "tools/ultimate_aws_supervision.json").read_text())
        jobs = {job["id"]: job for job in config["jobs"]}
        self.assertEqual(["jester-ghost-merge"],
                         jobs["jester-ghost-measure"]["dependencies"])

    def test_production_unit_is_measurement_only_and_resource_bounded(self):
        unit = (ROOT / "tools/ultimatefish-jg7328-measure.service").read_text()
        self.assertIn("--merge-evidence-sha256 "
                      "83f4ee24c9c19672bb62899f55aacdf230e6d4848684e96322211d04add2fcdd",
                      unit)
        self.assertIn("--maximum-resident-bytes 47244640256", unit)
        self.assertIn("--maximum-disk-bytes 483183820800", unit)
        self.assertIn("MemoryMax=46G", unit)
        self.assertIn("AllowedCPUs=1", unit)
        self.assertIn("Restart=no", unit)
        self.assertNotIn("--solve", unit)
        self.assertNotIn("--full", unit)
        self.assertIn("ReadOnlyPaths=/mnt/ultimatefish/jester-ghost-migration-7328-prep/merge-7328-third-work", unit)


if __name__ == "__main__":
    unittest.main()
