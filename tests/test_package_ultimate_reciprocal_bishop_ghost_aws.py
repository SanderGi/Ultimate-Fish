import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "package_ultimate_reciprocal_bishop_ghost_aws",
    TOOLS / "package_ultimate_reciprocal_bishop_ghost_aws.py")
assert SPEC and SPEC.loader
package = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = package
SPEC.loader.exec_module(package)
RUNNER_SPEC = importlib.util.spec_from_file_location(
    "run_ultimate_reciprocal_bishop_ghost_aws",
    TOOLS / "run_ultimate_reciprocal_bishop_ghost_aws.py")
assert RUNNER_SPEC and RUNNER_SPEC.loader
runner = importlib.util.module_from_spec(RUNNER_SPEC)
sys.modules[RUNNER_SPEC.name] = runner
RUNNER_SPEC.loader.exec_module(runner)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class ReciprocalBishopGhostAwsBundleTests(unittest.TestCase):
    @staticmethod
    def command_range(command):
        prefix = command[command.index("--transition-prefix") + 1]
        begin = int(command[command.index("--geometry-begin") + 1])
        count = int(command[command.index("--geometry-count") + 1])
        return Path(prefix).name, begin, count

    def test_manifest_is_exact_gap_free_and_fingerprint_isolated(self):
        manifest = package.build_manifest()
        self.assertEqual(
            manifest["schema"],
            "ultimate-reciprocal-bishop-ghost-aws-v1")
        self.assertEqual(manifest["geometries"], 492_960)
        self.assertEqual(manifest["shards"], 32)
        self.assertEqual(manifest["geometries_per_shard"], 15_405)
        self.assertEqual(manifest["parallelism"], 29)
        commands = manifest["commands"]["shards"]
        ranges = list(map(self.command_range, commands))
        self.assertEqual([name for name, _, _ in ranges],
                         [f"shard-{index:02d}" for index in range(32)])
        cursor = 0
        for _, begin, count in ranges:
            self.assertEqual(begin, cursor)
            self.assertEqual(count, 15_405)
            cursor += count
        self.assertEqual(cursor, 492_960)
        merge = manifest["commands"]["merge"]
        merge_inputs = [Path(merge[index + 1]).name
                        for index, argument in enumerate(merge)
                        if argument == "--shard"]
        self.assertEqual(merge_inputs, [item[0] for item in ranges])
        self.assertEqual(manifest["source_sha256"], package.SOURCE_SHA256)
        self.assertEqual(manifest["lower_sidecar_sha256"],
                         package.LOWER_SHA256)
        self.assertEqual(manifest["model_sha256"],
                         package.RECIPROCAL_FINGERPRINT)
        self.assertEqual(
            package.information.solver_model_fingerprint(
                "kbishopghostk.uftb"), package.BISHOP_GHOST_FINGERPRINT)
        self.assertEqual(
            package.information.solver_model_fingerprint(
                "kghostghostk.uftb"), package.GHOST_PAIR_FINGERPRINT)
        self.assertIn("work/results/kbishopkghost.ufgx",
                      manifest["artifacts"])
        self.assertIn("--output-arbitrary", manifest["commands"]["solve"])
        self.assertEqual(manifest["commands"]["build"][0], "clang++")
        build = manifest["commands"]["build"]
        self.assertGreater(
            build.index("-Wno-error=range-loop-construct"),
            build.index("-Werror"))
        for command in (manifest["commands"]["measure"],
                        manifest["commands"]["solve"]):
            self.assertEqual(command[command.index("--compact-every") + 1],
                             "1")
        for log in ("build", "self-test", "merge",
                    *(f"shard-{index:02d}" for index in range(32))):
            self.assertIn(f"work/logs/{log}.log", manifest["artifacts"])

    def test_runner_reuses_only_complete_transition_ranges(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            runner.prepare_workdirs(root)
            command = ["solver", "--transition-prefix",
                       "work/transitions/shard-00"]
            self.assertFalse(runner.transition_is_complete(root, command))
            prefix = root / "work" / "transitions" / "shard-00"
            for suffix in runner.TRANSITION_SUFFIXES:
                Path(f"{prefix}{suffix}").touch()
            self.assertFalse(runner.transition_is_complete(root, command))
            (root / "work" / "logs" / "shard-00.log").touch()
            self.assertTrue(runner.transition_is_complete(root, command))
            with self.assertRaisesRegex(RuntimeError, "escapes"):
                runner.transition_prefix([
                    "solver", "--transition-prefix",
                    "work/transitions/../../outside"])
            manifest = {
                "source_sha256": "1" * 64,
                "model_sha256": "2" * 64,
                "observation_sha256": "3" * 64,
                "lower_sidecar_sha256": "4" * 64,
                "lower_source_sha256": "5" * 64,
                "lower_model_sha256": "6" * 64,
                "lower_observation_sha256": "7" * 64,
                "artifacts": [],
            }
            runner.artifact_manifest(root, manifest)
            recorded = json.loads(
                (root / "work" / "artifact-manifest.json").read_text())
            for key in ("lower_sidecar_sha256", "lower_source_sha256",
                        "lower_model_sha256", "lower_observation_sha256"):
                self.assertEqual(recorded[key], manifest[key])

    def test_runner_preserves_failed_and_prior_phase_logs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "work" / "logs" / "measure.log"
            with self.assertRaisesRegex(RuntimeError, "failed-0001"):
                runner.run([
                    sys.executable, "-c",
                    "print('first failure'); raise SystemExit(7)",
                ], root, log)
            self.assertFalse(log.exists())
            self.assertEqual(
                "first failure\n",
                (log.parent / "measure.failed-0001.log").read_text())

            runner.run([sys.executable, "-c", "print('success')"],
                       root, log)
            self.assertEqual("success\n", log.read_text())
            runner.run([sys.executable, "-c", "print('replacement')"],
                       root, log)
            self.assertEqual("success\n",
                             (log.parent / "measure.prior-0001.log").read_text())
            self.assertEqual("replacement\n", log.read_text())

    def test_runner_reuses_only_complete_merged_transition(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            runner.prepare_workdirs(root)
            command = ["solver", "--transition-prefix",
                       "work/transitions/merged"]
            prefix = root / "work" / "transitions" / "merged"
            for suffix in runner.TRANSITION_SUFFIXES:
                Path(f"{prefix}{suffix}").touch()
            self.assertFalse(
                runner.merged_transition_is_complete(root, command))
            (root / "work" / "logs" / "merge.log").touch()
            self.assertTrue(
                runner.merged_transition_is_complete(root, command))
            Path(f"{prefix}.verified").unlink()
            self.assertFalse(
                runner.merged_transition_is_complete(root, command))

    def test_tar_is_deterministic_and_minimal(self):
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "first.tar"
            second = Path(directory) / "second.tar"
            package.build_bundle(first)
            package.build_bundle(second)
            self.assertEqual(sha256(first), sha256(second))
            with tarfile.open(first) as archive:
                names = archive.getnames()
            self.assertEqual(names[0], "bundle-manifest.json")
            self.assertEqual(set(names[1:]), set(package.BUILD_INPUTS))
            self.assertIn(
                "src/ultimate/ghost_extra_information_tablebase.cpp", names)


if __name__ == "__main__":
    unittest.main()
