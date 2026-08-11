#!/usr/bin/env python3
"""Regression tests for the immutable v4 concrete wave-0 planner."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import build_ultimate_concrete_wave0_batch_v4 as batch  # noqa: E402
import run_ultimate_concrete_tablebase_shard_aws as runner  # noqa: E402
import supervise_ultimate_aws as supervisor  # noqa: E402
import ultimate_information_tablebases as information  # noqa: E402
import update_ultimate_tablebase_ledger as ledger  # noqa: E402


class ConcreteWave0BatchV4Test(unittest.TestCase):
    def test_v4_is_disjoint_and_source_pinned(self) -> None:
        document = batch.build_document()
        self.assertEqual(batch.SCHEMA, document["schema"])
        self.assertEqual(batch.BATCH_VERSION, document["batch_version"])
        self.assertEqual(24, len(document["units"]))
        self.assertTrue(document["launch_ready"])
        self.assertEqual([], document["launch_blockers"])
        legacy = batch._current_v3_filenames()
        filenames = [str(unit["filename"]) for unit in document["units"]]
        self.assertEqual(24, len(set(filenames)))
        self.assertTrue(legacy.isdisjoint(filenames))
        self.assertTrue(set(filenames).isdisjoint(information.AFFECTED_FILENAMES))
        statuses = {entry.filename: entry.status
                    for entry in ledger.entries(batch.base.committed_ledger_text())}
        self.assertTrue(all(statuses[name] == "planned" for name in filenames))
        artifact_names = set(batch.base.dependency_artifact_records())
        for unit in document["units"]:
            self.assertTrue(unit["unit"].startswith(
                "ultimatefish-concrete-wave0-batch-v4-"))
            self.assertIn("/concrete-wave0-batch/batch-v4-", unit["work_directory"])
            self.assertNotIn("batch-v3-", unit["work_directory"])
            expected = tuple(runner.class_dependency_filenames(unit["record"]))
            self.assertEqual(list(expected), unit["dependencies"])
            self.assertTrue(set(expected).issubset(artifact_names))
            self.assertEqual(unit["source_hashes"][batch.WRAPPER_RELATIVE],
                             batch._sha256_path(Path(batch.__file__)))
            self.assertNotIn("StandardOutput=append:", unit["service_text"])
            self.assertNotIn("StandardError=append:", unit["service_text"])
        encoded = dict(document)
        claimed = encoded.pop("manifest_sha256")
        expected_digest = hashlib.sha256(
            json.dumps(encoded, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        self.assertEqual(expected_digest, claimed)

    def test_priority_slots_are_unique_and_measured_safe_is_first(self) -> None:
        document = batch.build_document()
        units = document["units"]
        self.assertEqual(
            ["i-0986ed3d272721f02"] * 6 +
            ["i-024a2073283e4336e"] * 6 +
            ["i-0b4523116b2f7765c"] * 6 +
            ["i-03c81f90d2c59a2e7"] * 6,
            [str(unit["instance_id"]) for unit in units],
        )
        for instance_id in batch.HOST_SPECS:
            cpus = [unit["expected_allowed_cpus"] for unit in units
                    if unit["instance_id"] == instance_id]
            self.assertEqual(len(cpus), len(set(cpus)))
            for cpu in cpus:
                self.assertIn(int(cpu), batch.HOST_SPECS[instance_id]["cpu_pool"])
        self.assertGreaterEqual(
            min(int(unit["runner_limits"]["resident_limit_bytes"]) for unit in units),
            16 * batch.GIB)
        self.assertGreaterEqual(
            min(int(unit["runner_limits"]["reverse_edge_bytes_limit"]) for unit in units),
            16 * batch.GIB)
        self.assertGreaterEqual(
            min(int(unit["runner_limits"]["scratch_limit_bytes"]) for unit in units),
            20 * batch.GIB)

    def test_fragment_and_merge_preserve_v3(self) -> None:
        document = batch.build_document()
        fragment = batch.build_supervision_jobs(document)
        self.assertEqual(24, len(fragment["jobs"]))
        self.assertEqual(
            f"{batch.SOURCE_ROOT}/tools/{batch.MANIFEST.name}",
            fragment["batch_manifest"]["path"],
        )
        self.assertTrue(all(job["id"].startswith("concrete-wave0-batch-v4-")
                            for job in fragment["jobs"]))
        self.assertTrue(all(job["queue_stage"] for job in fragment["jobs"]))
        self.assertTrue(all(not job["source_bindings"] for job in fragment["jobs"]))
        self.assertTrue(all(any(path.endswith(batch.WRAPPER_RELATIVE)
                                for path in (binding["path"]
                                             for binding in job["staging_source_bindings"]))
                            for job in fragment["jobs"]))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            config = root / "config.json"
            fragment_path = root / "fragment.json"
            config.write_text(batch.SUPERVISION_CONFIG.read_text())
            fragment_path.write_text(json.dumps(fragment, indent=2, sort_keys=True) + "\n")
            old_fragment = batch.JOB_FRAGMENT
            try:
                batch.JOB_FRAGMENT = fragment_path
                batch.merge_supervision_config(document, config)
            finally:
                batch.JOB_FRAGMENT = old_fragment
            merged = json.loads(config.read_text())
        jobs = {str(job["id"]): job for job in merged["jobs"]}
        self.assertNotIn("concrete-wave0-remaining", jobs)
        v3 = [job for job in jobs if job.startswith("concrete-wave0-batch-v3-")]
        self.assertEqual(len(batch._current_v3_filenames()), len(v3))
        self.assertEqual(24, len([job for job in jobs if job.startswith(
            "concrete-wave0-batch-v4-")]))
        supervisor.validate_config(merged)

    def test_count_is_bounded(self) -> None:
        with self.assertRaises(RuntimeError):
            batch.build_document(0)
        with self.assertRaises(RuntimeError):
            batch.build_document(batch.MAX_CLASSES + 1)


if __name__ == "__main__":
    unittest.main()
