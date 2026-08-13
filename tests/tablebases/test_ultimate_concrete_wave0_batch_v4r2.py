"""Regression tests for the add-only v4r2 concrete namespace."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "tablebases"))
import build_ultimate_concrete_wave0_batch_v4r2 as r2  # noqa: E402
import supervise_ultimate_aws as supervisor  # noqa: E402


class ConcreteWave0BatchV4R2Test(unittest.TestCase):
    def historical_document(self):
        # v4r2 is also a frozen pre-simplification namespace.  The current
        # planner correctly excludes separator-asymmetric Copycat classes and
        # therefore must not be used to recreate this historical manifest.
        return json.loads(r2.MANIFEST.read_text())

    def test_namespace_is_add_only_and_source_pinned(self) -> None:
        document = self.historical_document()
        self.assertEqual(r2.SCHEMA, document["schema"])
        self.assertEqual("v4r2", document["batch_version"])
        self.assertEqual(24, len(document["units"]))
        for unit in document["units"]:
            self.assertIn("-v4r2-", str(unit["unit"]))
            self.assertIn("/source-v4r2/", str(unit["source_root"]))
            self.assertIn("/dependencies-v4r2/", str(unit["dependency_root"]))
            self.assertIn("batch-v4r2-", str(unit["work_directory"]))
            # These hashes pin the historical source payload; comparing them
            # to today's helper files would erase that provenance boundary.
            self.assertTrue(unit["source_hashes"])
            for digest in unit["source_hashes"].values():
                self.assertRegex(digest, r"^[0-9a-f]{64}$")
        self.assertEqual(
            r2.DEPENDENCY_ARCHIVE["version_id"],
            document["dependency_archive"]["version_id"])

    def test_fragment_has_distinct_source_pinned_ids(self) -> None:
        document = self.historical_document()
        fragment = r2.build_supervision_jobs(document)
        self.assertEqual(24, len(fragment["jobs"]))
        self.assertTrue(all(str(job["id"]).startswith(
            "concrete-wave0-batch-v4r2-") for job in fragment["jobs"]))
        self.assertTrue(all(any(str(binding["path"]).endswith(
            r2.STAGING_HELPER_RELATIVE)
            for binding in job["staging_source_bindings"])
            for job in fragment["jobs"]))
        self.assertTrue(all("supersedes_job_id" not in job
                            for job in fragment["jobs"]))

    def test_config_merge_replaces_only_current_namespace_and_validates(self) -> None:
        document = self.historical_document()
        fragment = r2.build_supervision_jobs(document)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            config = root / "config.json"
            fragment_path = root / "jobs.json"
            base_config = json.loads(r2.SUPERVISION_CONFIG.read_text())
            # The committed config already contains v4r2; exercise the merge
            # against its pre-publication shape to avoid synthetic duplicates.
            base_config["jobs"] = [
                job for job in base_config["jobs"]
                if not str(job.get("id", "")).startswith(
                    "concrete-wave0-batch-v4r2-")]
            config.write_text(json.dumps(base_config, indent=2) + "\n")
            fragment_path.write_text(json.dumps(fragment, indent=2, sort_keys=True) + "\n")
            old_fragment = r2.JOB_FRAGMENT
            try:
                r2.JOB_FRAGMENT = fragment_path
                r2.merge_supervision_config(document, config)
            finally:
                r2.JOB_FRAGMENT = old_fragment
            merged = json.loads(config.read_text())
        supervisor.validate_config(merged)
        jobs = {str(job["id"]): job for job in merged["jobs"]}
        r2_jobs = [job for job in jobs.values()
                   if str(job["id"]).startswith("concrete-wave0-batch-v4r2-")]
        self.assertEqual(24, len(r2_jobs))


if __name__ == "__main__":
    unittest.main()
