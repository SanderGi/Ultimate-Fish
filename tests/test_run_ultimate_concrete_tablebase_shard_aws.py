#!/usr/bin/env python3
"""Standalone tests for the complete concrete K+K+2 AWS shard runner."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "concrete_aws", ROOT / "tools/run_ultimate_concrete_tablebase_shard_aws.py")
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class ConcreteAwsRunnerTest(unittest.TestCase):
    def test_inventory_and_wave_conservation(self) -> None:
        self.assertEqual(232, len(runner.closed_inventory()))
        rows = runner.supported_inventory()
        self.assertEqual(268, len(rows))
        self.assertEqual(72_736_864_200,
                         sum(int(row["packed_bytes"]) for row in rows))
        expected = ((226, 65_050_385_400),
                    (40, 7_401_794_400),
                    (2, 284_684_400))
        self.assertEqual(expected, tuple(
            (len(runner.wave_inventory(wave)),
             sum(int(row["packed_bytes"])
                 for row in runner.wave_inventory(wave)))
            for wave in range(3)))

    def test_balanced_ranges_are_exact_contiguous_coverage(self) -> None:
        for wave in range(3):
            rows = runner.wave_inventory(wave)
            count = min(4, len(rows))
            ranges = runner.balanced_ranges(rows, count)
            self.assertEqual(0, ranges[0]["begin"])
            self.assertEqual(len(rows), ranges[-1]["end"])
            self.assertTrue(all(left["end"] == right["begin"]
                                for left, right in zip(ranges, ranges[1:])))
            self.assertEqual(sum(int(row["states"]) for row in rows),
                             sum(item["states"] for item in ranges))

    def test_copycat_scope_and_dynamic_deferral_are_explicit(self) -> None:
        self.assertEqual({"devil", "sludge", "angel"},
                         set(runner.DEFERRED_DYNAMIC))
        self.assertEqual(36, len(runner.mirror_copycat_inventory()))
        self.assertFalse(any(bool(row["truncates_native_separation"])
                             for row in runner.mirror_copycat_inventory()))
        self.assertTrue(all(row["secondary"] not in runner.plan.COPYCAT_SEPARATORS
                            for row in runner.mirror_copycat_inventory()))
        deferred = runner.deferred_domain_plan()
        self.assertEqual(90, deferred["classes"])
        self.assertEqual(90, len(deferred["inventory"]))
        self.assertIn("incomplete", deferred["completeness"])

    def test_pawn_dependency_waves(self) -> None:
        for wave in range(3):
            for row in runner.wave_inventory(wave):
                self.assertEqual(wave, runner.dependency_wave(row))
            self.assertTrue(set(runner.required_dependency_filenames(wave)).isdisjoint(
                {str(row["filename"]) for row in runner.wave_inventory(wave)}))
        wave_one = set(runner.required_dependency_filenames(1))
        self.assertTrue({str(row["filename"])
                         for row in runner.wave_inventory(0)} <= wave_one)

    def test_default_is_plan_only(self) -> None:
        arguments = [
            "--work-directory", "work", "--dependencies", "dependencies",
            "--dependency-manifest", "manifest", "--wave", "0",
            "--range-begin", "0", "--range-end", "1",
        ]
        self.assertFalse(runner.parse_args(arguments).full)
        self.assertTrue(runner.parse_args([*arguments, "--full"]).full)

    def test_aws_source_preserves_named_scratch(self) -> None:
        source = (ROOT / "src/ultimate/tablebase.cpp").read_text()
        self.assertIn("ULTIMATE_TABLEBASE_PRESERVE_SCRATCH", source)
        runner_source = (ROOT /
                         "tools/run_ultimate_concrete_tablebase_shard_aws.py").read_text()
        self.assertIn(
            'environment["ULTIMATE_TABLEBASE_PRESERVE_SCRATCH"] = "1"',
            runner_source)

    def test_full_is_forbidden_on_mac_and_requires_explicit_gates(self) -> None:
        args = mock.Mock(
            aws_execution_ack="EC2", s3_prefix="s3://bucket/base",
            scratch_limit=1 << 40, resident_limit=1 << 40,
            reverse_edge_bytes_limit=1 << 30,
            minimum_free_bytes=1)
        measurement = {
            "static_scratch_floor_bytes": 1, "resident_floor_bytes": 1,
            "packed_bytes": 1,
        }
        with tempfile.TemporaryDirectory() as temporary, \
             mock.patch.object(runner.sys, "platform", "darwin"):
            with self.assertRaisesRegex(RuntimeError, "forbidden"):
                runner.require_aws_full(args, measurement, Path(temporary))

    def test_resource_limits_fail_closed(self) -> None:
        healthy = {
            "active_file_bytes": 10, "rss_bytes": 20,
            "reverse_edge_bytes": 5, "filesystem_free_bytes": 100,
        }
        arguments = {
            "scratch_limit": 10, "resident_limit": 20,
            "reverse_edge_bytes_limit": 5, "minimum_free_bytes": 100,
        }
        self.assertIsNone(runner.resource_limit_violation(healthy, **arguments))
        violations = (
            {**healthy, "active_file_bytes": 11},
            {**healthy, "rss_bytes": 21},
            {**healthy, "reverse_edge_bytes": 6},
            {**healthy, "filesystem_free_bytes": 99},
        )
        for damaged in violations:
            self.assertIsNotNone(
                runner.resource_limit_violation(damaged, **arguments))

    def test_structural_and_log_verification(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "kbombkghost.uftb"
            record = {
                "filename": output.name, "primary": "bomb",
                "secondary": "ghost", "opposing": True,
                "states": 4, "packed_bytes": 5, "shards": 1,
                "phase": "test",
            }
            header = struct.pack(
                "<8sIIIIIIIIII", b"UFTB1\0\0\0", 5,
                runner.PIECE_INDEX["bomb"], 4, 7, 2, 1, 4, 0,
                runner.PIECE_INDEX["ghost"], 1)
            output.write_bytes(header + bytes([0b_11_10_01_01]) + bytes(4))
            log = root / "proof.log"
            log.write_text(
                f"verifyok states 4\noutput outputs/{output.name} edges 7 "
                "win 1 loss 1 draw 2\ncomplete states 4/4 elapsed 1s\n")
            verified = runner.parse_uftb(output, record, log)
            self.assertEqual(0, verified["verification_residual"])
            self.assertEqual(4, verified["states"])
            damaged = bytearray(output.read_bytes())
            damaged[len(header)] = 0
            output.write_bytes(damaged)
            with self.assertRaisesRegex(RuntimeError, "WDL"):
                runner.parse_uftb(output, record, log)
            output.write_bytes(header + bytes([0b_11_10_01_01]) + bytes(3))
            with self.assertRaisesRegex(RuntimeError, "extent"):
                runner.parse_uftb(output, record, log)

    def test_dependency_manifest_rejects_duplicates(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "manifest.json"
            record = {"filename": "krk.uftb", "bytes": 1,
                      "sha256": "1" * 64}
            path.write_text(json.dumps({
                "schema": runner.DEPENDENCY_SCHEMA,
                "files": [record, record],
            }))
            with self.assertRaisesRegex(RuntimeError, "malformed"):
                runner.load_dependency_manifest(path)


if __name__ == "__main__":
    unittest.main()
