import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))


def load(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, TOOLS / filename)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


package = load("package_ultimate_crossed_jester_ghost_aws",
               "package_ultimate_crossed_jester_ghost_aws.py")
runner = load("run_ultimate_crossed_jester_ghost_aws",
              "run_ultimate_crossed_jester_ghost_aws.py")


class CrossedJesterGhostAwsTests(unittest.TestCase):
    def test_manifest_is_exact_measure_first_and_restore_capable(self):
        manifest = package.build_manifest(require_committed=False)
        self.assertEqual(manifest["schema"],
                         "ultimate-crossed-jester-ghost-aws-v1")
        self.assertEqual(manifest["default_mode"], "measurement-only")
        self.assertEqual(manifest["raw_public_frames"], 38_450_880)
        self.assertEqual(manifest["source_sha256"], package.SOURCE_SHA256)
        self.assertEqual(manifest["lower_jester_overlay_sha256"],
                         package.LOWER_JESTER_OVERLAY_SHA256)
        self.assertEqual(manifest["lower_ghost_sidecar_sha256"],
                         package.LOWER_GHOST_SIDECAR_SHA256)
        self.assertEqual({record["path"] for record in manifest["files"]},
                         set(package.BUNDLE_FILES))
        self.assertIn(
            "src/ultimate/crossed_jester_ghost_information_sidecar.cpp",
            package.MODEL_SOURCES)
        source = (ROOT / "src/ultimate/"
                  "crossed_jester_ghost_information_tablebase.cpp").read_text()
        self.assertIn("--verify-sidecar", source)
        self.assertIn("crossed_sidecar_restore_certificate", source)

    def test_committed_source_gate_rejects_one_byte_drift(self):
        with mock.patch.object(package, "committed_source",
                               return_value=b"not-current"):
            with self.assertRaisesRegex(RuntimeError, "not committed"):
                package.require_committed_files(ROOT,
                                                (package.BUNDLE_FILES[0],))

    def test_archive_roundtrip_is_hash_exact_and_path_safe(self):
        if not runner.shutil.which("zstd"):
            self.skipTest("zstd is unavailable")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / "a"
            second = root / "b"
            first.write_bytes(b"alpha")
            second.write_bytes(b"beta")
            archive = root / "archive.tar.zst"
            digest = runner.write_archive(
                archive, {"proof/a": first, "tablebases/b": second},
                runner.RESULT_SCHEMA)
            restored = runner.restore_archive(
                archive, root / "restore", runner.RESULT_SCHEMA)
            self.assertEqual(digest, runner.sha256_path(archive))
            self.assertEqual(restored["proof/a"].read_bytes(), b"alpha")
            self.assertEqual(restored["tablebases/b"].read_bytes(), b"beta")

    def test_full_mode_requires_versioned_s3_and_all_resource_gates(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = package.build_manifest(require_committed=False)
            manifest["source_commit"] = "1" * 40
            manifest_path = root / "bundle-manifest.json"
            manifest_path.write_text(json.dumps(manifest))
            checkpoint = root / "graph.chk"
            checkpoint.write_bytes(b"checkpoint")
            overlay = root / "lower.ufiw"
            overlay.write_bytes(b"bad")
            with self.assertRaisesRegex(RuntimeError,
                                        "positive resource gates and S3"):
                runner.main([
                    "--work", str(root / "work"),
                    "--checkpoint", str(checkpoint),
                    "--checkpoint-sha256", runner.sha256_path(checkpoint),
                    "--lower-jester-overlay", str(overlay),
                    "--bundle-manifest", str(manifest_path), "--full",
                ])

    def test_preflight_parser_requires_both_targets_and_zero_residuals(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "measure.log"
            log.write_text(
                "crossed_solve_preflight target white nodes 7 atoms 8 gates 9 "
                "variables 17 tokens 20 reverse_edges 21 external_constants 2 "
                "peak_scratch_bytes 100 residual 0\n"
                "crossed_solve_preflight target black nodes 7 atoms 8 gates 10 "
                "variables 18 tokens 22 reverse_edges 23 external_constants 3 "
                "peak_scratch_bytes 120 residual 0\n"
                "crossed_solve_resources sidecar_bytes 30 required_free_bytes "
                "150 actual_free_bytes 1000 residual 0\n")
            parsed = runner.parse_preflight(log)
            self.assertEqual(parsed["nodes"], 7)
            self.assertEqual(parsed["peak_scratch_bytes"], 120)
            self.assertEqual(parsed["black_variables"], 18)


if __name__ == "__main__":
    unittest.main()
