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


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "double_jester_capture_runner",
    ROOT / "tools/tablebases/run_double_jester_information_capture.py",
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
            unrelated = root / "src/ultimate/tablebases/joint_jester_information_tablebase.cpp"
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
            self.assertNotIn("--expected-fresh-overlay", command)
            self.assertIn("results/kjesterjesterk.ufiw", command)
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

            def overlay(path: Path, bound_source: str, model: str,
                        payload: bytes) -> None:
                path.write_bytes(b"UFIW2\0\0\0" + b"\0" * 24 +
                                 bound_source.encode() + model.encode() + payload)

            lower_overlay = root / "lower.ufiw"
            authenticated = runner.AUDITED_LOWER_REBIND["to_model_sha256"]
            overlay(lower_overlay, lower_source, authenticated, b"lower payload")
            args = argparse.Namespace(
                input=source, lower_concrete=lower,
                lower_overlay=lower_overlay,
                allow_audited_lower_rebind=False)
            work = root / "work"
            with mock.patch.object(
                    runner.information, "solver_model_fingerprint",
                    return_value="9" * 64):
                staged, bindings = runner.stage_external_inputs(args, work)
            self.assertEqual(runner.sha256_path(source),
                             bindings["source_sha256"])
            self.assertEqual(runner.sha256_payload(lower_overlay),
                             bindings["lower_overlay_payload_sha256"])
            self.assertEqual(Path("inputs/kjesterjesterk.uftb"), staged["input"])
            self.assertEqual(0o444, (work / staged["input"]).stat().st_mode & 0o777)
            manifest = json.loads((work / "inputs/manifest.json").read_text())
            self.assertEqual("fresh-maximal-public-view-v2",
                             manifest["semantics"])
            self.assertNotIn("expected_overlay_sha256", bindings)

    def test_default_is_measurement_only_and_full_is_explicit(self) -> None:
        common = ["--work-directory", "work", "--lower-overlay", "lower"]
        self.assertFalse(runner.parse_args(common).full)
        self.assertTrue(runner.parse_args([*common, "--full"]).full)
        self.assertFalse(runner.parse_args(common).allow_audited_lower_rebind)

    def test_full_resource_gate_requires_s3_preservation(self) -> None:
        args = argparse.Namespace(
            s3_prefix=None, scratch_limit=1, resident_limit=1,
            required_free_bytes=1)
        with self.assertRaisesRegex(RuntimeError, "S3|s3"):
            runner.require_resource_gate({
                "raw_bytes_upper": 0, "resident_bytes_upper": 0,
                "recommended_free_bytes": 0, "portable_bytes_upper": 0,
            }, Path("."), args)

    def test_dense_overlay_and_completion_certificate_are_strict(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, model = "a" * 64, "b" * 64
            dense = root / "kjesterjesterk.ufiw"
            header = bytearray(b"UFIW2\0\0\0")
            for value in (2, 1, 1, 0, 4, 1):
                header.extend(value.to_bytes(4, "little"))
            header.extend(source.encode())
            header.extend(model.encode())
            dense.write_bytes(header + bytes((5, 4, 6, 4)))
            record = runner.inspect_dense_overlay(dense, source, model)
            self.assertEqual(4, record["concrete_states"])
            self.assertEqual(runner.sha256_payload(dense),
                             record["payload_sha256"])
            with self.assertRaisesRegex(RuntimeError, "binding"):
                runner.inspect_dense_overlay(dense, source, "c" * 64)

            log = root / "capture.log"
            residuals = (
                "bellman_residual 0 rank_residual 0 witness_residual 0 "
                "domain_bellman_residual 0 dual_win_residual 0 "
                "uniform_action_residual 0 singleton_residual 0 "
                "d2_residual 0 conservation_residual 0 overlay_residual 0 "
                "restore_residual 0")
            log.write_text(
                "capture_complete exhaustive 1 belief_cap none " + residuals +
                f" dense_sha256 {record['sha256']} dense_payload_sha256 "
                f"{record['payload_sha256']} sidecar_sha256 {'d' * 64}\n")
            certificate = runner.parse_capture_certificate(log)
            self.assertEqual("0", certificate["witness_residual"])
            log.write_text(log.read_text().replace(
                "conservation_residual 0", "conservation_residual 1"))
            with self.assertRaisesRegex(RuntimeError, "nonzero"):
                runner.parse_capture_certificate(log)

    def test_sidecar_header_cross_binds_dense_and_conservation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, model = "1" * 64, "2" * 64
            dense = {"concrete_states": 4, "sha256": "3" * 64,
                     "payload_sha256": "4" * 64}
            summaries = [
                {"side": 0, "win": 1, "loss": 0, "draw": 0,
                 "unreachable_win": 0, "unreachable_loss": 0,
                 "unreachable_draw": 1},
                {"side": 1, "win": 0, "loss": 1, "draw": 1,
                 "unreachable_win": 0, "unreachable_loss": 0,
                 "unreachable_draw": 0},
            ]
            header = bytearray(1024)
            header[:8] = b"UFICAP3\0"
            header[8:12] = (3).to_bytes(4, "little")
            header[12:16] = (1024).to_bytes(4, "little")
            header[16:20] = (1).to_bytes(4, "little")
            header[32:32 + len(runner.DOMAIN)] = runner.DOMAIN.encode()
            header[64:64 + len(runner.SEMANTICS)] = runner.SEMANTICS.encode()
            header[128:160] = bytes.fromhex(source)
            header[160:192] = bytes.fromhex(model)
            header[288:296] = (4).to_bytes(8, "little")
            header[672:704] = bytes.fromhex(str(dense["sha256"]))
            header[704:736] = bytes.fromhex(str(dense["payload_sha256"]))
            counts = [summaries[side][key] for side in range(2) for key in (
                "win", "loss", "draw", "unreachable_win",
                "unreachable_loss", "unreachable_draw")]
            for offset, value in zip(
                    (800, 808, 816, 824, 832, 840,
                     848, 856, 864, 872, 880, 888), counts):
                header[offset:offset + 8] = int(value).to_bytes(8, "little")
            header[936:944] = (8).to_bytes(8, "little")
            header[944:952] = (2).to_bytes(8, "little")
            header[992:1024] = runner.hashlib.sha256(header[:992]).digest()
            sidecar = root / "result.uficapture"
            sidecar.write_bytes(header)
            record = runner.inspect_sidecar_header(
                sidecar, source=source, model=model, dense=dense,
                manifest={"side_summaries": summaries})
            self.assertEqual(8, record["d2_states"])
            summaries[1]["draw"] = 0
            with self.assertRaisesRegex(RuntimeError, "cross-binding|conservation"):
                runner.inspect_sidecar_header(
                    sidecar, source=source, model=model, dense=dense,
                    manifest={"side_summaries": summaries})

    def test_audited_lower_rebind_preserves_provided_bytes_and_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            upper = root / "upper.uftb"
            lower = root / "lower.uftb"
            upper.write_bytes(b"upper")
            lower.write_bytes(b"lower")
            lower_sha = runner.sha256_path(lower)
            old_model, new_model = "1" * 64, "2" * 64
            lower_overlay = root / "lower.ufiw"
            lower_overlay.write_bytes(
                b"UFIW2\0\0\0" + b"\0" * 24 + lower_sha.encode() +
                old_model.encode() + b"same payload")
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
                lower_overlay=lower_overlay,
                allow_audited_lower_rebind=True)
            with mock.patch.object(runner, "AUDITED_LOWER_REBIND", audit), \
                 mock.patch.object(
                     runner.information, "solver_model_fingerprint",
                     return_value=new_model):
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

    def test_s3_preservation_requires_bucket_versioning(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "archive"
            source.write_bytes(b"archive")
            digest = runner.sha256_path(source)
            with mock.patch.object(runner.subprocess, "run"), \
                 mock.patch.object(runner.subprocess, "check_output",
                                   return_value=json.dumps({
                                       "ContentLength": source.stat().st_size,
                                       "Metadata": {"sha256": digest},
                                   })):
                with self.assertRaisesRegex(RuntimeError, "versioning"):
                    runner.upload_head_download_verify(
                        source=source, digest=digest,
                        extent=source.stat().st_size,
                        prefix="s3://bucket/base", key="results/hash/archive",
                        download=Path(temporary) / "download",
                        archive_schema=None)


if __name__ == "__main__":
    unittest.main()
