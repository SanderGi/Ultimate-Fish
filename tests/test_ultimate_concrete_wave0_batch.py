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


class ConcreteWave0BatchTest(unittest.TestCase):
    def test_manifest_is_deterministic_and_fail_closed(self) -> None:
        document = batch.build_document(10)
        units = document["units"]
        self.assertEqual(document["schema"], batch.SCHEMA)
        self.assertEqual(document["status"], "plan-only-not-uploaded-not-installed-not-launched")
        self.assertEqual(len(units), 10)
        self.assertTrue(document["no_remote_side_effects"])
        self.assertTrue(document["never_delete"])
        self.assertTrue(document["launch_ready"])
        self.assertEqual(document["launch_blockers"], [])
        self.assertEqual(len({unit["unit"] for unit in units}), len(units))
        self.assertEqual(
            len({unit["work_directory"] for unit in units}), len(units))

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
            self.assertEqual(resources["cpu_count"], 1)
            self.assertEqual(resources["cpu_quota_percent"], 100)
            self.assertGreaterEqual(
                resources["scratch_limit_bytes"],
                resources["static_scratch_floor_bytes"] +
                resources["reverse_edge_bytes_limit"])
            self.assertGreaterEqual(
                resources["resident_limit_bytes"], resources["resident_floor_bytes"])
            self.assertGreaterEqual(resources["resident_limit_bytes"], 16 * batch.GIB)
            self.assertGreaterEqual(resources["reverse_edge_bytes_limit"], 16 * batch.GIB)
            self.assertGreaterEqual(resources["scratch_limit_bytes"], 20 * batch.GIB)
            self.assertGreaterEqual(resources["minimum_free_bytes"], 320 * batch.GIB)

            service = unit["service_text"]
            self.assertNotIn("@", service)
            self.assertIn(f"--range-begin {index}", service)
            self.assertIn(f"--range-end {index + 1}", service)
            self.assertIn("--full --aws-execution-ack EC2", service)
            self.assertNotIn("systemctl", service)
            self.assertNotIn("aws ", service)
            self.assertNotIn("ssh ", service)

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
