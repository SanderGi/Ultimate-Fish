import importlib.util
import copy
from pathlib import Path
import sys
import struct
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "package_ultimate_jester_ghost_aws",
    TOOLS / "package_ultimate_jester_ghost_aws.py")
assert SPEC and SPEC.loader
package = importlib.util.module_from_spec(SPEC); SPEC.loader.exec_module(package)
RUN_SPEC = importlib.util.spec_from_file_location(
    "run_ultimate_jester_ghost_aws",
    TOOLS / "run_ultimate_jester_ghost_aws.py")
assert RUN_SPEC and RUN_SPEC.loader
runner = importlib.util.module_from_spec(RUN_SPEC); RUN_SPEC.loader.exec_module(runner)


class JesterGhostAwsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lower = Path(
          "/private/tmp/ultimatefish-information-overlays.exact-progress-v2/kjesterk.ufiw")
        if not cls.lower.exists():
            raise unittest.SkipTest("current exact lower Jester overlay unavailable")

    def test_manifest_is_gap_free_visibility_aligned_and_bound(self):
        manifest = package.build_manifest(self.lower)
        runner.validate_manifest(manifest)
        self.assertEqual(manifest["schema"], "ultimate-jester-ghost-aws-v2")
        self.assertEqual(manifest["active_jobs"], 40)
        self.assertEqual(manifest["parallelism"], 29)
        self.assertEqual(manifest["bootstrap_inputs"], 40)
        self.assertEqual(manifest["merge_inputs"], 80)
        self.assertEqual(manifest["raw_per_shard"] % 78, 0)
        self.assertEqual(manifest["half_raw"], 320_424)
        commands = manifest["commands"]["shards"]
        self.assertEqual(len(commands), 40)
        self.assertEqual(len(manifest["commands"]["bootstrap"]), 40)
        cursor = 0
        counts = {320_424: 0, 640_848: 0}
        names = []
        for name, begin, count in manifest["merge_ranges"]:
            self.assertEqual(begin, cursor)
            self.assertEqual(begin % 78, 0); self.assertEqual(count % 78, 0)
            names.append(name); counts[count] += 1
            cursor += count
        self.assertEqual(cursor, package.RAW_GEOMETRIES)
        self.assertEqual(counts, {320_424: 40, 640_848: 40})
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(
          [item[0] for item in manifest["active_ranges"][:4]],
          ["shard-00a", "shard-00b", "shard-01a", "shard-01b"])
        for command in manifest["commands"]["bootstrap"]:
            self.assertIn("--verify-transitions", command)
            self.assertIn("--allow-partial-merge", command)
        self.assertIn("--output-arbitrary", manifest["commands"]["solve"])
        self.assertIn("work/results/kjesterghostk.ufjg", manifest["artifacts"])
        self.assertEqual(
          package.information.solver_model_fingerprint("kbishopghostk.uftb"),
          package.BISHOP_GHOST_FINGERPRINT)
        self.assertEqual(
          package.information.solver_model_fingerprint("kghostghostk.uftb"),
          package.GHOST_PAIR_FINGERPRINT)
        self.assertEqual(manifest["model_sha256"],
          package.information.solver_model_fingerprint("kjesterghostk.uftb"))

    def test_runner_rejects_range_drift_and_missing_bootstrap(self):
        manifest = package.build_manifest(self.lower)
        corrupt = copy.deepcopy(manifest)
        corrupt["active_ranges"][0][2] -= 78
        with self.assertRaisesRegex(RuntimeError, "range inventory drift"):
            runner.validate_manifest(corrupt)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError,
                                         "bootstrap transition ranges are missing"):
                runner.authenticate_bootstrap(
                  manifest["commands"]["bootstrap"], Path(directory), 1)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            command = manifest["commands"]["bootstrap"][0]
            base = root / runner.prefix(command)
            base.parent.mkdir(parents=True)
            for suffix in runner.SUFFIXES:
                Path(f"{base}{suffix}").write_bytes(b"")
            Path(f"{base}.header").write_bytes(struct.pack(
              "<8sIIII", b"UFJGT2\0\0", 2, 0,
              int(runner.option(command, "--raw-begin")) + 78,
              int(runner.option(command, "--raw-count"))))
            with self.assertRaisesRegex(RuntimeError,
                                         "bootstrap range mismatch"):
                runner.authenticate_bootstrap([command], root, 1)

    def test_tar_is_deterministic_and_contains_authenticated_lower(self):
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "a.tar"; second = Path(directory) / "b.tar"
            package.build_bundle(first, self.lower); package.build_bundle(second, self.lower)
            self.assertEqual(package.sha(first.read_bytes()), package.sha(second.read_bytes()))
            with tarfile.open(first) as archive:
                names = set(archive.getnames())
            self.assertIn("tablebases/kjesterk.ufiw", names)
            self.assertIn("bundle-manifest.json", names)


class JesterGhostRunnerResumeTests(unittest.TestCase):
    def test_failed_logs_and_completed_merge_are_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "work/logs/measure.log"
            with self.assertRaisesRegex(RuntimeError, "failed-0001"):
                runner.run([
                    sys.executable, "-c",
                    "print('late semantic failure'); raise SystemExit(4)",
                ], root, log)
            self.assertFalse(log.exists())
            self.assertEqual(
                "late semantic failure\n",
                (log.parent / "measure.failed-0001.log").read_text())

            runner.prepare(root)
            command = ["solver", "--transition-prefix",
                       "work/transitions/merged"]
            base = root / "work/transitions/merged"
            for suffix in runner.SUFFIXES:
                Path(f"{base}{suffix}").touch()
            self.assertFalse(
                runner.merged_transition_is_complete(root, command))
            (root / "work/logs/merge.log").touch()
            self.assertTrue(
                runner.merged_transition_is_complete(root, command))


if __name__ == "__main__": unittest.main()
