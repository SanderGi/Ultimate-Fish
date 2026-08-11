#!/usr/bin/env python3
"""Tests for the source-pinned concrete wave-0 batch planner."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import build_ultimate_concrete_wave0_batch as batch  # noqa: E402
import run_ultimate_concrete_tablebase_shard_aws as runner  # noqa: E402
import supervise_ultimate_aws as supervisor  # noqa: E402


class ConcreteWave0BatchTest(unittest.TestCase):
    def test_manifest_is_deterministic_and_fail_closed(self) -> None:
        document = batch.build_document(24)
        units = document["units"]
        self.assertEqual(document["schema"], batch.SCHEMA)
        self.assertEqual(document["status"], "plan-only-not-uploaded-not-installed-not-launched")
        self.assertEqual(len(units), 24)
        self.assertTrue(document["no_remote_side_effects"])
        self.assertTrue(document["never_delete"])
        self.assertTrue(document["launch_ready"])
        self.assertEqual(document["launch_blockers"], [])
        fragment = batch.build_supervision_jobs(document)
        self.assertEqual(fragment["schema"], "ultimate-aws-supervision-job-fragment-v1")
        self.assertEqual(len(fragment["jobs"]), 24)
        self.assertEqual(fragment["replace_job_ids"], ["concrete-wave0-remaining"])
        self.assertEqual(
            [update["id"] for update in fragment["retained_placeholder_updates"]],
            ["concrete-wave1", "concrete-wave2"])
        merged = json.loads(
            (ROOT / "tools/ultimate_aws_supervision.json").read_text())
        jobs_by_id = {job["id"]: job for job in merged["jobs"]}
        jobs_by_id.pop("concrete-wave0-remaining", None)
        for update in fragment["retained_placeholder_updates"]:
            jobs_by_id[update["id"]]["dependencies"] = update["dependencies"]
        jobs_by_id.update({job["id"]: job for job in fragment["jobs"]})
        merged["jobs"] = list(jobs_by_id.values())
        supervisor.validate_config(merged)
        self.assertEqual(len({unit["unit"] for unit in units}), len(units))
        self.assertEqual(
            len({unit["work_directory"] for unit in units}), len(units))
        hosts = {host["instance_id"]: host
                 for host in document["configured_hosts"]}
        self.assertEqual(set(hosts), set(batch.HOST_SPECS))
        for instance_id, spec in batch.HOST_SPECS.items():
            self.assertEqual(hosts[instance_id]["name"], spec["name"])
            self.assertEqual(hosts[instance_id]["capacity_class"], spec["capacity_class"])
            self.assertEqual(hosts[instance_id]["memory_capacity_bytes"],
                             spec["memory_capacity_bytes"])
            self.assertEqual(hosts[instance_id]["cpu_pool"], list(spec["cpu_pool"]))
        self.assertTrue(document["scheduler_policy"]["simultaneous_launch_forbidden"])
        cpu_by_host = {}

        inventory = runner.wave_inventory(0)
        rows = {str(row["filename"]): (index, row)
                for index, row in enumerate(inventory)}
        selected_indices = []
        for unit in units:
            filename = unit["filename"]
            self.assertIn(filename, rows)
            index, row = rows[filename]
            self.assertEqual(unit["inventory_index"], index)
            self.assertEqual(unit["record"], runner.normalized_record(row))
            selected_indices.append(index)
            self.assertNotIn(row["primary"], runner.DEFERRED_DYNAMIC)
            self.assertNotIn(row["secondary"], runner.DEFERRED_DYNAMIC)
            if row.get("mirror_simplification"):
                self.assertNotIn(row["secondary"], {"penguin", "mage", "fisherman"})

            expected_dependencies = list(runner.class_dependency_filenames(row))
            self.assertEqual(unit["dependencies"], expected_dependencies)
            self.assertEqual(unit["dependencies"], sorted(set(unit["dependencies"])))
            self.assertEqual(
                [entry["filename"] for entry in unit["dependency_records"]],
                expected_dependencies)
            for dependency in unit["dependency_records"]:
                self.assertRegex(dependency["sha256"], r"^[0-9a-f]{64}$")
                self.assertTrue(dependency["authenticated"])

            resources = unit["resource_requirements"]
            runner_limits = unit["runner_limits"]
            self.assertEqual(set(resources), {
                "cpu_threads", "memory_peak_bytes", "disk_peak_bytes"})
            self.assertEqual(runner_limits["cpu_count"], 1)
            self.assertEqual(runner_limits["cpu_quota_percent"], 100)
            self.assertEqual(resources["cpu_threads"], 1)
            self.assertEqual(resources["memory_peak_bytes"],
                             runner_limits["resident_limit_bytes"])
            self.assertRegex(unit["expected_allowed_cpus"], r"^[0-9]+$")
            self.assertIn(int(unit["expected_allowed_cpus"]),
                          batch.HOST_SPECS[unit["instance_id"]]["cpu_pool"])
            self.assertIn(unit["work_mount"],
                          batch.HOST_SPECS[unit["instance_id"]]["mount_rotation"])
            self.assertTrue(unit["work_directory"].startswith(unit["work_mount"] + "/"))
            cpu_by_host.setdefault(unit["instance_id"], []).append(
                unit["expected_allowed_cpus"])
            self.assertEqual(unit["work_mount"], next(iter(resources["disk_peak_bytes"])))
            self.assertEqual(
                resources["disk_peak_bytes"][unit["work_mount"]],
                runner_limits["scratch_limit_bytes"] + 5 * runner_limits["packed_bytes"])
            self.assertGreaterEqual(
                runner_limits["scratch_limit_bytes"],
                runner_limits["static_scratch_floor_bytes"] +
                runner_limits["reverse_edge_bytes_limit"])
            self.assertGreaterEqual(
                runner_limits["resident_limit_bytes"], runner_limits["resident_floor_bytes"])
            self.assertGreaterEqual(runner_limits["resident_limit_bytes"], 16 * batch.GIB)
            self.assertGreaterEqual(runner_limits["reverse_edge_bytes_limit"], 16 * batch.GIB)
            self.assertGreaterEqual(runner_limits["scratch_limit_bytes"], 20 * batch.GIB)
            self.assertGreater(runner_limits["minimum_free_bytes"], 0)

            service = unit["service_text"]
            self.assertNotIn("@", service)
            self.assertIn(f"--range-begin {index}", service)
            self.assertIn(f"--range-end {index + 1}", service)
            self.assertIn("--full --aws-execution-ack EC2", service)
            self.assertIn(f"AllowedCPUs={unit['expected_allowed_cpus']}", service)
            self.assertNotIn("systemctl", service)
            self.assertNotIn("aws ", service)
            self.assertNotIn("ssh ", service)

        self.assertEqual(
            {host: len(cpus) for host, cpus in cpu_by_host.items()},
            {"i-03c81f90d2c59a2e7": 8, "i-0b4523116b2f7765c": 7,
             "i-0986ed3d272721f02": 3, "i-024a2073283e4336e": 3,
             "i-08c0f44a1776cb34a": 3})
        for cpus in cpu_by_host.values():
            self.assertEqual(len(cpus), len(set(cpus)))

        for job in fragment["jobs"]:
            self.assertTrue(job["advanceable"])
            self.assertTrue(job["queue_stage"])
            self.assertEqual(job["dependencies"], [])
            self.assertTrue(job["ledger_certifies"])
            self.assertEqual(set(job["resource_requirements"]), {
                "cpu_threads", "memory_peak_bytes", "disk_peak_bytes"})
            self.assertEqual(job["s3_certificates"], [])
            paths = [binding["path"] for binding in job["source_bindings"]]
            self.assertEqual(len(paths), len(set(paths)))
            self.assertTrue(any(path.endswith("run_ultimate_concrete_tablebase_shard_aws.py")
                                for path in paths))
            self.assertTrue(any(path.endswith("ultimate_concrete_wave0_batch.json")
                                for path in paths))
            self.assertTrue(any(path.startswith("/etc/systemd/system/")
                                for path in paths))
            self.assertTrue(any(path.endswith("/dependencies/manifest.json")
                                for path in paths))
            for binding in job["source_bindings"]:
                self.assertRegex(binding["sha256"], r"^[0-9a-f]{64}$")
                self.assertFalse(any(character in binding["path"]
                                     for character in "*?[]"))

        self.assertEqual(document["all_dependencies"], sorted({
            dependency for unit in units for dependency in unit["dependencies"]
        }))
        self.assertEqual(
            document["all_dependency_count"], len(document["all_dependencies"]))
        self.assertEqual(
            selected_indices,
            sorted(selected_indices,
                   key=lambda index: (
                       int(inventory[index]["packed_bytes"]),
                       str(inventory[index]["filename"]), index)))

        encoded = dict(document)
        claimed = encoded.pop("manifest_sha256")
        expected = hashlib.sha256(
            json.dumps(encoded, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        self.assertEqual(claimed, expected)

    def test_count_is_bounded(self) -> None:
        with self.assertRaises(RuntimeError):
            batch.build_document(0)
        with self.assertRaises(RuntimeError):
            batch.build_document(batch.MAX_CLASSES + 1)


if __name__ == "__main__":
    unittest.main()
