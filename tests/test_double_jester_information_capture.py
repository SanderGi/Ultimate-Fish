#!/usr/bin/env python3
"""Standalone tests for the persistent double-Jester capture runner."""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "double_jester_capture_runner",
    ROOT / "tools/run_double_jester_information_capture.py",
)
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class DoubleJesterCaptureRunnerTest(unittest.TestCase):
    def test_capture_fingerprint_is_domain_scoped(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for index, relative in enumerate(runner.CAPTURE_SOURCES):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(f"source-{index}\n".encode())
            before = runner.capture_model_sha256(root)
            unrelated = root / "src/ultimate/joint_jester_information_tablebase.cpp"
            unrelated.write_text("unrelated\n")
            self.assertEqual(before, runner.capture_model_sha256(root))
            owned = root / runner.CAPTURE_SOURCES[0]
            owned.write_text("changed capture source\n")
            self.assertNotEqual(before, runner.capture_model_sha256(root))

    def test_overlay_binding_is_strict(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            overlay = Path(temporary) / "lower.ufiw"
            source = "a" * 64
            model = "b" * 64
            overlay.write_bytes(b"UFIW2\0\0\0" + b"\0" * 24 +
                                source.encode() + model.encode())
            self.assertEqual((source, model), runner.overlay_binding(overlay))
            overlay.write_bytes(b"bad")
            with self.assertRaises(ValueError):
                runner.overlay_binding(overlay)

    def test_capture_command_does_not_mutate_plan_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary) / "capture"
            args = argparse.Namespace(
                required_free_bytes=123,
                scratch_limit=456,
            )
            staged = {
                "input": Path("inputs/input.uftb"),
                "lower_overlay": Path("inputs/lower.ufiw"),
                "lower_concrete": Path("inputs/lower.uftb"),
                "expected_overlay": Path("inputs/expected.ufiw"),
            }
            bindings = {
                "source_sha256": "d" * 64,
                "lower_source_sha256": "e" * 64,
                "lower_model_sha256": "f" * 64,
            }
            command = runner.relative_capture_command(
                staged, "c" * 64, bindings, args)
            self.assertFalse(work.exists())
            self.assertIn("--raw-directory", command)
            self.assertIn("raw-live", command)
            self.assertIn("--expected-fresh-overlay", command)
            self.assertTrue(all(not value.startswith(str(work))
                                for value in command))

    def test_staged_sources_are_catalog_hash_bound(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            model = runner.capture_model_sha256()
            bundle = runner.stage_source_bundle(work, model)
            self.assertEqual(model, runner.capture_model_sha256(bundle))
            self.assertEqual(0o444, (bundle / runner.CAPTURE_SOURCES[0]).stat().st_mode & 0o777)

    def test_external_inputs_are_staged_with_full_and_payload_bindings(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source.uftb"
            source.write_bytes(b"upper table")
            lower = root / "lower.uftb"
            lower.write_bytes(b"lower table")
            lower_source = runner.sha256_path(lower)
            source_sha = runner.sha256_path(source)

            def overlay(path: Path, bound_source: str, model: str,
                        payload: bytes) -> None:
                path.write_bytes(b"UFIW2\0\0\0" + b"\0" * 24 +
                                 bound_source.encode() + model.encode() + payload)

            lower_overlay = root / "lower.ufiw"
            expected = root / "expected.ufiw"
            overlay(lower_overlay, lower_source, "1" * 64, b"lower payload")
            overlay(expected, source_sha, "2" * 64, b"expected payload")
            args = argparse.Namespace(
                input=source, lower_concrete=lower,
                lower_overlay=lower_overlay, expected_overlay=expected,
                allow_audited_lower_rebind=False)
            work = root / "work"
            with mock.patch.object(
                    runner.information, "solver_model_fingerprint",
                    side_effect=lambda filename: (
                        "1" * 64 if filename == "kjesterk.uftb" else "2" * 64)):
                staged, bindings = runner.stage_external_inputs(args, work)
            self.assertEqual(source_sha, bindings["source_sha256"])
            self.assertEqual(runner.sha256_path(expected),
                             bindings["expected_overlay_sha256"])
            self.assertEqual(runner.sha256_payload(lower_overlay),
                             bindings["lower_overlay_payload_sha256"])
            self.assertEqual(Path("inputs/kjesterjesterk.uftb"), staged["input"])
            self.assertEqual(0o444, (work / staged["input"]).stat().st_mode & 0o777)
            manifest = json.loads((work / "inputs/manifest.json").read_text())
            self.assertEqual("fresh-maximal-public-view-v2",
                             manifest["semantics"])
            with mock.patch.object(
                    runner.information, "solver_model_fingerprint",
                    return_value="9" * 64):
                with self.assertRaisesRegex(RuntimeError, "model is stale"):
                    runner.stage_external_inputs(args, root / "stale-work")

    def test_default_is_measurement_only_and_full_is_explicit(self) -> None:
        common = ["--work-directory", "work", "--lower-overlay", "lower",
                  "--expected-overlay", "expected"]
        self.assertFalse(runner.parse_args(common).full)
        self.assertTrue(runner.parse_args([*common, "--full"]).full)
        self.assertFalse(runner.parse_args(common).allow_audited_lower_rebind)

    def test_audited_lower_rebind_preserves_provided_bytes_and_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            upper = root / "upper.uftb"
            lower = root / "lower.uftb"
            upper.write_bytes(b"upper")
            lower.write_bytes(b"lower")
            upper_sha = runner.sha256_path(upper)
            lower_sha = runner.sha256_path(lower)
            old_model, new_model, normal_model = "1" * 64, "2" * 64, "3" * 64
            lower_overlay = root / "lower.ufiw"
            expected = root / "expected.ufiw"
            lower_overlay.write_bytes(
                b"UFIW2\0\0\0" + b"\0" * 24 + lower_sha.encode() +
                old_model.encode() + b"same payload")
            expected.write_bytes(
                b"UFIW2\0\0\0" + b"\0" * 24 + upper_sha.encode() +
                normal_model.encode() + b"upper payload")
            audit = {
                "from_model_sha256": old_model,
                "to_model_sha256": new_model,
                "source_sha256": lower_sha,
                "provided_full_sha256": runner.sha256_path(lower_overlay),
                "rebound_full_sha256": "",
                "payload_sha256": runner.sha256_payload(lower_overlay),
            }
            rebound_bytes = bytearray(lower_overlay.read_bytes())
            rebound_bytes[96:160] = new_model.encode()
            audit["rebound_full_sha256"] = runner.hashlib.sha256(
                rebound_bytes).hexdigest()
            args = argparse.Namespace(
                input=upper, lower_concrete=lower,
                lower_overlay=lower_overlay, expected_overlay=expected,
                allow_audited_lower_rebind=True)
            with mock.patch.object(runner, "AUDITED_LOWER_REBIND", audit), \
                 mock.patch.object(
                     runner.information, "solver_model_fingerprint",
                     side_effect=lambda filename: (
                         new_model if filename == "kjesterk.uftb" else normal_model)):
                _, bindings = runner.stage_external_inputs(args, root / "work")
            provided = root / "work/inputs/kjesterk.provided-095d.ufiw"
            rebound = root / "work/inputs/kjesterk.ufiw"
            self.assertEqual(audit["provided_full_sha256"],
                             runner.sha256_path(provided))
            self.assertEqual(audit["payload_sha256"],
                             runner.sha256_payload(rebound))
            self.assertEqual(new_model, runner.overlay_binding(rebound)[1])
            self.assertEqual(0, bindings["lower_header_rebind"]["payload_residual"])

    @unittest.skipUnless(runner.shutil.which("zstd"), "zstd is unavailable")
    def test_archive_is_deterministic_and_restores_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            one = root / "one"
            two = root / "two"
            one.write_bytes(b"one\n")
            two.write_bytes(b"two\n")
            files = {"proof/two": two, "binary/one": one}
            first = root / "first.tar.zst"
            second = root / "second.tar.zst"
            first_sha = runner.write_zstd_archive(
                first, files, runner.RAW_ARCHIVE_SCHEMA)
            second_sha = runner.write_zstd_archive(
                second, files, runner.RAW_ARCHIVE_SCHEMA)
            self.assertEqual(first_sha, second_sha)
            restored = runner.restore_zstd_archive(
                first, root / "restored", runner.RAW_ARCHIVE_SCHEMA)
            self.assertEqual(b"one\n", restored["binary/one"].read_bytes())
            self.assertEqual(b"two\n", restored["proof/two"].read_bytes())

    def test_s3_keys_are_content_addressable(self) -> None:
        self.assertEqual(
            ("bucket", "prefix/results/hash/file", "s3://bucket/prefix/results/hash/file"),
            runner.s3_object("s3://bucket/prefix", "results/hash/file"))

    def test_s3_head_must_match_full_sha_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "archive"
            source.write_bytes(b"archive")
            with mock.patch.object(runner.subprocess, "run"), \
                 mock.patch.object(runner.subprocess, "check_output",
                                   return_value=json.dumps({
                                       "ContentLength": len(b"archive"),
                                       "Metadata": {"sha256": "0" * 64},
                                   })):
                with self.assertRaisesRegex(RuntimeError, "HEAD"):
                    runner.upload_head_download_verify(
                        source=source, digest=runner.sha256_path(source),
                        extent=source.stat().st_size, prefix="s3://bucket/base",
                        key="results/hash/archive",
                        download=Path(temporary) / "download",
                        archive_schema=None)


if __name__ == "__main__":
    unittest.main()
