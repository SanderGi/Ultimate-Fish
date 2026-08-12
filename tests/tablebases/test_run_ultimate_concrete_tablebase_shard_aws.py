#!/usr/bin/env python3
"""Standalone tests for the complete concrete K+K+2 AWS shard runner."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "concrete_aws", ROOT / "tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py")
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class ConcreteAwsRunnerTest(unittest.TestCase):
    def test_staged_source_inventory_has_a_closed_python_import_graph(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative in runner.MODEL_SOURCES:
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / relative, target)
            subprocess.run([
                sys.executable,
                str(root / "tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py"),
                "--help",
            ], cwd=root, check=True, stdout=subprocess.DEVNULL)

    def test_inventory_and_wave_conservation(self) -> None:
        self.assertEqual(232, len(runner.closed_inventory()))
        rows = runner.supported_inventory()
        self.assertEqual(268, len(rows))
        self.assertEqual(94_657_563_000,
                         sum(int(row["packed_bytes"]) for row in rows))
        expected = ((226, 85_832_346_600),
                    (40, 8_540_532_000),
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
        singles = set(runner.base_dependency_filenames())
        self.assertEqual(15, len(singles))
        self.assertNotIn("kjesterkjester.uftb", singles)
        for wave in range(3):
            for row in runner.wave_inventory(wave):
                self.assertEqual(wave, runner.dependency_wave(row))
                dependencies = set(runner.class_dependency_filenames(row))
                self.assertEqual(1 if wave else 0,
                                 len(dependencies - singles))
                self.assertNotIn(str(row["filename"]), dependencies)
            self.assertTrue(set(runner.required_dependency_filenames(wave)).isdisjoint(
                {str(row["filename"]) for row in runner.wave_inventory(wave)}))
        bomb_berserker = next(
            row for row in runner.wave_inventory(0)
            if row["filename"] == "kberserkerbombk.uftb")
        self.assertEqual(
            {"kberserkerk.uftb", "kbombk.uftb"},
            set(runner.class_dependency_filenames(bomb_berserker)))
        pawn_bomb = next(
            row for row in runner.wave_inventory(1)
            if row["filename"] == "kpawnbombk.uftb")
        self.assertEqual(
            {"kpawnk.uftb", "kqk.uftb", "kbombk.uftb",
             "kqueenbombk.uftb"},
            set(runner.class_dependency_filenames(pawn_bomb)))
        pawn_pair = next(
            row for row in runner.wave_inventory(2)
            if row["filename"] == "kpawnpawnk.uftb")
        self.assertEqual(
            {"kpawnk.uftb", "kqk.uftb", "kpawnqueenk.uftb"},
            set(runner.class_dependency_filenames(pawn_pair)))

    def test_default_is_plan_only(self) -> None:
        arguments = [
            "--work-directory", "work", "--dependencies", "dependencies",
            "--dependency-manifest", "manifest", "--wave", "0",
            "--range-begin", "0", "--range-end", "1",
        ]
        self.assertFalse(runner.parse_args(arguments).full)
        self.assertTrue(runner.parse_args([*arguments, "--full"]).full)

    def test_penguin_bootstrap_is_exact_and_range_exclusive(self) -> None:
        selected, measurement = runner.penguin_bootstrap_plan()
        self.assertEqual("kpenguink.uftb", selected[0]["filename"])
        self.assertEqual(3_943_680, selected[0]["states"])
        self.assertEqual("bootstrap-penguin", measurement["wave"])
        args = runner.parse_args([
            "--work-directory", "work", "--dependencies", "dependencies",
            "--dependency-manifest", "manifest", "--bootstrap-penguin",
        ])
        self.assertTrue(args.bootstrap_penguin)
        with self.assertRaisesRegex(RuntimeError, "exclusive"):
            runner.main([
                "--work-directory", "work", "--dependencies", "dependencies",
                "--dependency-manifest", "manifest", "--bootstrap-penguin",
                "--wave", "0", "--range-begin", "0", "--range-end", "1",
            ])

    def test_penguin_bootstrap_v4_header_uses_four_causal_states(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "kpenguink.uftb"
            record = dict(runner.penguin_bootstrap_plan()[0][0])
            record["states"] = 4
            header = struct.pack(
                "<8sIIIIIIII", b"UFTB1\0\0\0", 4,
                runner.PIECE_INDEX["penguin"], 4, 7, 4, 1, 4, 0)
            output.write_bytes(header + bytes([0b_11_10_01_01]) + bytes(4))
            log = root / "proof.log"
            log.write_text(
                f"verifyok states 4\noutput outputs/{output.name} edges 7 "
                "win 1 loss 1 draw 2\ncomplete states 4/4 elapsed 1s\n")
            self.assertEqual(4, runner.parse_uftb(
                output, record, log)["substates"])

    def test_aws_source_preserves_named_scratch(self) -> None:
        source = (ROOT / "src/ultimate/tablebases/tablebase.cpp").read_text()
        self.assertIn("ULTIMATE_TABLEBASE_PRESERVE_SCRATCH", source)
        runner_source = (ROOT /
                         "tools/tablebases/run_ultimate_concrete_tablebase_shard_aws.py").read_text()
        self.assertIn(
            'environment["ULTIMATE_TABLEBASE_PRESERVE_SCRATCH"] = "1"',
            runner_source)

    def test_authenticated_dependency_source_accepts_identical_replicas(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = b"authenticated dependency\n"
            first = root / "batch-02" / "kcopycatk.uftb"
            second = root / "batch-01" / "kcopycatk.uftb"
            first.parent.mkdir(parents=True)
            second.parent.mkdir(parents=True)
            first.write_bytes(payload)
            second.write_bytes(payload)
            expected = runner.sha256_path(first)
            selected = runner.authenticated_dependency_source(
                root, "kcopycatk.uftb", len(payload), expected)
            self.assertEqual(second, selected)

    def test_authenticated_dependency_source_rejects_divergent_replica(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            good = root / "batch-01" / "kcopycatk.uftb"
            bad = root / "batch-02" / "kcopycatk.uftb"
            good.parent.mkdir(parents=True)
            bad.parent.mkdir(parents=True)
            good.write_bytes(b"good")
            bad.write_bytes(b"bad")
            with self.assertRaisesRegex(RuntimeError, "divergent"):
                runner.authenticated_dependency_source(
                    root, "kcopycatk.uftb", 4, runner.sha256_path(good))

    def test_authenticated_dependency_source_rejects_missing_replica(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(RuntimeError, "no authenticated"):
                runner.authenticated_dependency_source(
                    Path(temporary), "kcopycatk.uftb", 4, "0" * 64)

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

    def test_penguin_header_requires_material_specific_causal_factor(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "kpenguinbombk.uftb"
            record = {
                "filename": output.name, "primary": "penguin",
                "secondary": "bomb", "opposing": False,
                "states": 4, "packed_bytes": 5, "shards": 1,
                "phase": "test",
            }
            header = struct.pack(
                "<8sIIIIIIIIII", b"UFTB1\0\0\0", 5,
                runner.PIECE_INDEX["penguin"], 4, 7, 8, 1, 4, 0,
                runner.PIECE_INDEX["bomb"], 0)
            output.write_bytes(header + bytes([0b_11_10_01_01]) + bytes(4))
            log = root / "proof.log"
            log.write_text(
                f"verifyok states 4\noutput outputs/{output.name} edges 7 "
                "win 1 loss 1 draw 2\ncomplete states 4/4 elapsed 1s\n")
            self.assertEqual(8, runner.parse_uftb(
                output, record, log)["substates"])
            damaged = bytearray(output.read_bytes())
            struct.pack_into("<I", damaged, 24, 2)
            output.write_bytes(damaged)
            with self.assertRaisesRegex(RuntimeError, "codec header"):
                runner.parse_uftb(output, record, log)

    def test_uftb_extent_uses_versioned_header_not_planner_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for version, extra, expected_header in (
                    (4, b"", 40),
                    (5, struct.pack("<II", runner.PIECE_INDEX["queen"], 0), 48)):
                path = root / f"synthetic-v{version}.uftb"
                header = struct.pack(
                    "<8sIIIIIIII", b"UFTB1\0\0\0", version,
                    runner.PIECE_INDEX["bishop"], 4, 7, 1, 1, 4, 0)
                path.write_bytes(header + extra + bytes(5))
                extent = runner.uftb_extent(path)
                self.assertEqual(expected_header, extent["header_bytes"])
                self.assertEqual(5, extent["payload_bytes"])
                self.assertEqual(expected_header + 5, extent["file_bytes"])
                self.assertEqual(path.stat().st_size, extent["file_bytes"])
                path.write_bytes(path.read_bytes() + b"x")
                with self.assertRaisesRegex(RuntimeError, "extent residual"):
                    runner.uftb_extent(path)

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
