import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools/tablebases"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "run_ultimate_existing_ghost_measurement_exact_aws",
    TOOLS / "run_ultimate_existing_ghost_measurement_exact_aws.py")
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class ExistingGhostMeasurementTests(unittest.TestCase):
    def test_authenticates_graph_and_zero_residual_measurement(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "work/logs").mkdir(parents=True)
            (root / "work/transitions").mkdir()
            executable = root / "solver"
            executable.write_text("binary")
            executable.chmod(0o755)
            files = [root / "work/logs/build.log",
                     root / "work/logs/self-test.log",
                     root / "work/logs/merge.log"]
            for path in files:
                path.write_text("proof\n")
            measure = root / "work/logs/measure.log"
            measure.write_text(
                "reciprocal_ghost_extra_measurement iterations 1 "
                "bdd_nodes 12 peak_rss_bytes 34 proof_complete 0 "
                "overlay_written 0\n"
                "mage_ghost_certificate dual_force_residual 0 "
                "structural_residual 0 singleton_residual 0 "
                "source_remap_residual 0 proof\n")
            files.append(measure)
            for suffix in runner.shared.TRANSITION_SUFFIXES:
                path = Path(f"{root}/work/transitions/kghostmagek{suffix}")
                path.write_bytes(suffix.encode())
                files.append(path)
            manifest = {
                "schema": "ultimate-mage-ghost-aws-v1",
                "filename": "kghostmagek.uftb",
                "source_sha256": "a" * 64,
                "model_sha256": "b" * 64,
                "commands": {"solve": ["./solver"]},
            }
            artifact = {
                "source_sha256": "a" * 64,
                "model_sha256": "b" * 64,
                "artifacts": [
                    {"path": str(path.relative_to(root)),
                     "bytes": path.stat().st_size,
                     "sha256": runner.shared.sha256(path)}
                    for path in files
                ],
            }
            artifact_path = root / "work/artifact-manifest.json"
            artifact_path.write_text(json.dumps(artifact))
            self.assertEqual(
                runner.validate_completed_measurement(root, manifest),
                runner.shared.sha256(artifact_path))
            Path(f"{root}/work/transitions/kghostmagek.blocks").write_bytes(
                b"changed")
            with self.assertRaisesRegex(RuntimeError, "binding residual"):
                runner.validate_completed_measurement(root, manifest)


if __name__ == "__main__":
    unittest.main()
