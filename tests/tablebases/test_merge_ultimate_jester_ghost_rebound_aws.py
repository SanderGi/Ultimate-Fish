#!/usr/bin/env python3

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

from test_certify_ultimate_jester_ghost_verification_aws import (
    CertificationTest,
)


ROOT = Path(__file__).resolve().parents[2]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


merge = load("jg_merge", ROOT / "tools/tablebases/merge_ultimate_jester_ghost_rebound_aws.py")


class MergeTest(unittest.TestCase):
    def fixture(self, root: Path):
        certification = CertificationTest().fixture(root)
        audit_module = load(
            "jg_audit_fixture",
            ROOT / "tools/tablebases/certify_ultimate_jester_ghost_verification_aws.py")
        audit_module.certify(certification)
        runner = ROOT / "tools/tablebases/merge_ultimate_jester_ghost_rebound_aws.py"
        return certification, type("Arguments", (), {
            "runner_source": runner,
            "runner_sha256": merge.sha256_file(runner),
            "verifier": certification.verifier,
            "verifier_sha256": certification.verifier_sha256,
            "bundle_manifest": certification.bundle_manifest,
            "rebind_manifest": certification.rebind_manifest,
            "transition_tree": certification.transition_tree,
            "verification_certificate": certification.certificate_output,
            "verification_certificate_sha256": merge.sha256_file(
                certification.certificate_output),
            "verification_summary": certification.state_dir /
            "verification-summary.json",
            "binary": certification.binary,
            "binary_sha256": certification.binary_sha256,
            "output_dir": root / "fresh-third-output",
        })()

    def test_exact_evidence_builds_exact_80_shard_command(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, args = self.fixture(root)
            verifier = merge.load_verifier(args.verifier,
                                            args.verifier_sha256)
            bundle, shards, _, _ = merge.preflight(args, verifier)
            command = merge.merge_command(
                args, bundle, args.output_dir / "kjesterghostk")
            self.assertEqual(80, len(shards))
            self.assertEqual(80, command.count("--shard"))
            self.assertNotIn(str(args.transition_tree / "kjesterghostk"),
                             command[:4])
            self.assertEqual("--model-sha256", command[-4])

    def test_existing_output_fails_before_merge(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, args = self.fixture(root)
            args.output_dir.mkdir()
            verifier = merge.load_verifier(args.verifier,
                                            args.verifier_sha256)
            with self.assertRaisesRegex(ValueError, "already exists"):
                merge.preflight(args, verifier)

    def test_production_unit_is_bounded_nonrestarting_and_read_only(self):
        unit = (ROOT / "tools/tablebases/ultimatefish-jg7328-merge.service").read_text()
        self.assertIn("AllowedCPUs=1", unit)
        self.assertIn("MemoryMax=4G", unit)
        self.assertIn("Restart=no", unit)
        self.assertIn("ProtectSystem=strict", unit)
        self.assertIn("ReadOnlyPaths=/mnt/ultimatefish/jester-ghost-migration-7328-prep/rebound-7328", unit)
        self.assertIn("merge-7328-third-work/output", unit)
        self.assertNotIn("RUNNER_SHA256", unit)
        config = json.loads((
            ROOT / "tools/tablebases/ultimate_aws_supervision.json").read_text())
        jobs = {job["id"]: job for job in config["jobs"]}
        verification = jobs["jester-ghost-verify-shards"]
        self.assertEqual(
            "tAdN0u.rzOc.IA_wXc1xM.rhFPlifb8m",
            verification["s3_certificates"][0]["version_id"])
        self.assertTrue(verification["s3_only_certified"])
        self.assertEqual([], verification["source_bindings"])
        self.assertTrue(jobs["jester-ghost-merge"]["s3_only_certified"])
        self.assertEqual([], jobs["jester-ghost-merge"]["source_bindings"])


if __name__ == "__main__":
    unittest.main()
