import hashlib
import importlib.util
from collections import Counter
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

from artifact_support import require_artifacts


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "package_ultimate_ghost_pair_aws",
    TOOLS / "package_ultimate_ghost_pair_aws.py")
assert SPEC and SPEC.loader
package = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = package
SPEC.loader.exec_module(package)
RUNNER_SPEC = importlib.util.spec_from_file_location(
    "run_ultimate_ghost_pair_aws",
    TOOLS / "run_ultimate_ghost_pair_aws.py")
assert RUNNER_SPEC and RUNNER_SPEC.loader
runner = importlib.util.module_from_spec(RUNNER_SPEC)
sys.modules[RUNNER_SPEC.name] = runner
RUNNER_SPEC.loader.exec_module(runner)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1 << 20):
            digest.update(chunk)
    return digest.hexdigest()


class GhostPairAwsBundleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        require_artifacts(
            ROOT, "tablebases/kghostk.ufgm", "tablebases/kghostghostk.uftb")

    @staticmethod
    def command_range(command):
        prefix = command[command.index("--transition-prefix") + 1]
        begin = int(command[command.index("--raw-begin") + 1])
        count = int(command[command.index("--raw-count") + 1])
        return Path(prefix).name, begin, count

    def test_manifest_has_exact_authenticated_balanced_cover(self):
        manifest = package.build_manifest()
        self.assertEqual(manifest["schema"],
                         "ultimate-ghost-pair-aws-v3")
        self.assertEqual(manifest["active_jobs"], 60)
        self.assertEqual(manifest["long_jobs"], 30)
        self.assertEqual(manifest["tail_jobs"], 30)
        self.assertEqual(manifest["zero_bootstrap_jobs"], 24)
        self.assertEqual(manifest["merge_inputs"], 84)
        self.assertEqual(manifest["parallelism"], 30)
        self.assertEqual(manifest["raw_per_shard"], 1_217_390)
        self.assertEqual(manifest["side_half_boundary"], 19_478_240)
        zero_commands = manifest["commands"]["zero_shards"]
        active_commands = manifest["commands"]["shards"]
        self.assertEqual(len(zero_commands), 24)
        self.assertEqual(len(active_commands), 60)
        ranges = {
            name: (begin, count)
            for name, begin, count in map(
                self.command_range, zero_commands + active_commands)
        }
        self.assertEqual(len(ranges), 84)
        expected_zero = {
            *(f"shard-{index:02d}" for index in
              (*range(8, 16), *range(24, 32))),
            *(f"split-{label}" for label in package.ZERO_HALVES),
        }
        self.assertEqual(
            {self.command_range(command)[0] for command in zero_commands},
            expected_zero)
        dense_names = {
            f"resume-{label}q{quarter}"
            for label in package.DENSE_HALVES for quarter in range(2)
        }
        expected_halves = {
            "resume-00bh", "resume-03ah", "resume-04bh", "resume-05bh",
            "resume-06ah", "resume-07ah",
            "resume-16bh", "resume-17bh", "resume-18ah", "resume-19ah",
            "resume-20bh", "resume-21bh", "resume-22ah", "resume-23ah",
        }
        tail_names = {
            f"tail-{label}-{part:02d}"
            for label in package.TAIL_HALVES
            for part in range(package.TAIL_PARTS)
        }
        self.assertEqual(
            {self.command_range(command)[0] for command in active_commands},
            dense_names | expected_halves | tail_names)
        self.assertEqual(Counter(count for _, count in ranges.values()),
                         Counter({1_217_390: 16, 608_695: 22,
                                  304_347: 8, 304_348: 8,
                                  40_580: 20, 40_579: 10}))
        active_names = [self.command_range(command)[0]
                        for command in active_commands]
        expected_long_order = [
            "resume-00aq0", "resume-00aq1", "resume-00bh",
            "resume-03ah", "resume-03bq0", "resume-03bq1",
            "resume-04bh", "resume-05aq0", "resume-05aq1",
            "resume-05bh", "resume-06ah", "resume-06bq0",
            "resume-06bq1", "resume-07ah", "resume-16aq0",
            "resume-16aq1", "resume-16bh", "resume-17bh",
            "resume-18ah", "resume-19ah", "resume-19bq0",
            "resume-19bq1", "resume-20bh", "resume-21aq0",
            "resume-21aq1", "resume-21bh", "resume-22ah",
            "resume-22bq0", "resume-22bq1", "resume-23ah",
        ]
        expected_tail_order = [
            *(f"tail-01b-{part:02d}" for part in range(15)),
            *(f"tail-02a-{part:02d}" for part in range(15)),
        ]
        self.assertEqual(active_names,
                         expected_long_order + expected_tail_order)
        self.assertEqual(set(active_names[:30]), dense_names | expected_halves)
        self.assertEqual(set(active_names[30:]), tail_names)
        self.assertFalse(any(name.startswith("tail-")
                             for name in active_names[:30]))
        self.assertTrue(all(name.startswith("tail-")
                            for name in active_names[30:]))
        cursor = 0
        for begin, count in sorted(ranges.values()):
            self.assertEqual(begin, cursor)
            cursor += count
        self.assertEqual(cursor, package.RAW_GEOMETRIES)

        merge = manifest["commands"]["merge"]
        merge_prefixes = [Path(merge[index + 1]).name
                          for index, argument in enumerate(merge)
                          if argument == "--shard"]
        self.assertEqual(len(merge_prefixes), 84)
        self.assertEqual(merge_prefixes,
                         [name for name, _, _ in package.balanced_ranges()[2]])
        for command in zero_commands + active_commands:
            self.assertIn(manifest["source_sha256"], command)
            self.assertIn(manifest["model_sha256"], command)
            self.assertIn(manifest["observation_sha256"], command)
        self.assertEqual(
            manifest["source_sha256"],
            "204f4de6d0f9ff6da111d3d0c0a08c3562493cdec2130cc946b4eae7183012ee")
        self.assertEqual(
            manifest["lower_sidecar_sha256"],
            "472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb")
        self.assertIn("--output-arbitrary", manifest["commands"]["solve"])
        self.assertIn("work/results/kghostghostk.ufgg",
                      manifest["artifacts"])
        self.assertEqual(
            manifest["model_sha256"], package.GHOST_PAIR_FINGERPRINT)
        self.assertEqual(
            package.information.solver_model_fingerprint(
                "kbishopghostk.uftb"), package.BISHOP_GHOST_FINGERPRINT)

    def test_runner_creates_every_measurement_parent_and_reuses_only_complete(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            runner.prepare_workdirs(root)
            for name in ("transitions", "solve", "results", "logs"):
                self.assertTrue((root / "work" / name).is_dir())
            command = ["solver", "--transition-prefix",
                       "work/transitions/resume-00aq0"]
            self.assertFalse(runner.transition_is_complete(root, command))
            prefix = root / "work" / "transitions" / "resume-00aq0"
            for suffix in runner.TRANSITION_SUFFIXES:
                Path(f"{prefix}{suffix}").touch()
            self.assertTrue(runner.transition_is_complete(root, command))
            with self.assertRaisesRegex(RuntimeError, "escapes"):
                runner.transition_prefix([
                    "solver", "--transition-prefix",
                    "work/transitions/../../outside"])

            merged = ["solver", "--transition-prefix",
                      "work/transitions/merged"]
            merged_prefix = root / "work" / "transitions" / "merged"
            for suffix in runner.TRANSITION_SUFFIXES:
                Path(f"{merged_prefix}{suffix}").touch()
            self.assertFalse(
                runner.merged_transition_is_complete(root, merged))
            (root / "work" / "logs" / "merge.log").touch()
            self.assertTrue(
                runner.merged_transition_is_complete(root, merged))

    def test_runner_preserves_failed_measurement_log(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "work" / "logs" / "measure.log"
            with self.assertRaisesRegex(RuntimeError, "failed-0001"):
                runner.run([
                    sys.executable, "-c",
                    "print('semantic failure'); raise SystemExit(3)",
                ], root, log)
            self.assertFalse(log.exists())
            self.assertEqual(
                "semantic failure\n",
                (log.parent / "measure.failed-0001.log").read_text())

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
            self.assertNotIn("src/ultimate/tablebases/ghost_extra_information_tablebase.cpp",
                             names)


if __name__ == "__main__":
    unittest.main()
