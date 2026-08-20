import hashlib
import importlib.util
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
    "package_ultimate_ghost_parasite_aws",
    TOOLS / "package_ultimate_ghost_parasite_aws.py")
assert SPEC and SPEC.loader
package = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = package
SPEC.loader.exec_module(package)
RUNNER_SPEC = importlib.util.spec_from_file_location(
    "run_ultimate_ghost_parasite_aws",
    TOOLS / "run_ultimate_ghost_parasite_aws.py")
assert RUNNER_SPEC and RUNNER_SPEC.loader
runner = importlib.util.module_from_spec(RUNNER_SPEC)
sys.modules[RUNNER_SPEC.name] = runner
RUNNER_SPEC.loader.exec_module(runner)


class ParasiteGhostAwsBundleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        require_artifacts(
            ROOT, "tablebases/kghostk.ufgm", "tablebases/kparasitek.uftb",
            "tablebases/kghostk-tracked.uftb",
            "tablebases/kghostparasitek.uftb",
            "tablebases/kghostkparasite.uftb")

    @staticmethod
    def command_range(command):
        prefix = command[command.index("--transition-prefix") + 1]
        begin = int(command[command.index("--geometry-begin") + 1])
        count = int(command[command.index("--geometry-count") + 1])
        return Path(prefix).name, begin, count

    def test_both_manifests_are_gap_free_and_fingerprint_isolated(self):
        self.assertIn(
            package.information.concrete_tablebase_model_fingerprint(
                "kparasitek.uftb"),
            package.LOWER_PARASITE_COMPATIBLE_GENERATOR_MODELS)
        self.assertIn(package.LOWER_PARASITE_MODEL_SHA256,
                      package.LOWER_PARASITE_COMPATIBLE_GENERATOR_MODELS)
        self.assertIn(
            package.information.tracked_ghost_tablebase_model_fingerprint(),
            package.LOWER_TRACKED_GHOST_COMPATIBLE_GENERATOR_MODELS)
        self.assertIn(package.LOWER_TRACKED_GHOST_MODEL_SHA256,
                      package.LOWER_TRACKED_GHOST_COMPATIBLE_GENERATOR_MODELS)
        for filename, orientation in (("kghostparasitek.uftb", "same"),
                                      ("kghostkparasite.uftb", "opposing")):
            with self.subTest(filename=filename):
                manifest = package.build_manifest(filename)
                self.assertEqual(manifest["orientation"], orientation)
                self.assertEqual(manifest["shards"], 64)
                self.assertEqual(manifest["parallelism"], 29)
                ranges = list(map(self.command_range,
                                  manifest["commands"]["shards"]))
                self.assertEqual([item[0] for item in ranges],
                                 [f"shard-{index:02d}"
                                  for index in range(64)])
                cursor = 0
                for index, (_, begin, count) in enumerate(ranges):
                    self.assertEqual(begin, cursor)
                    self.assertEqual(count, 7_703 if index < 32 else 7_702)
                    cursor += count
                self.assertEqual(cursor, 492_960)
                merge = manifest["commands"]["merge"]
                merged = [Path(merge[index + 1]).name
                          for index, argument in enumerate(merge)
                          if argument == "--shard"]
                self.assertEqual(merged, [item[0] for item in ranges])
                for command in (manifest["commands"]["measure"],
                                manifest["commands"]["solve"]):
                    self.assertEqual(
                        command[command.index("--compact-every") + 1], "1")
                build = manifest["commands"]["build"]
                self.assertEqual(build[:14], [
                    "clang++", "-std=c++17", "-O3", "-DNDEBUG",
                    "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                    "-Wno-error=range-loop-construct",
                    "-include", "sstream", "-Isrc/ultimate",
                    "-Isrc/ultimate/tablebases",
                    "src/ultimate/tablebases/ghost_parasite_information_tablebase.cpp",
                ])
                stem = Path(filename).stem
                self.assertIn(f"work/results/{stem}.ufgp",
                              manifest["artifacts"])
                self.assertEqual(manifest["lower_parasite_full_sha256"],
                                 package.LOWER_PARASITE_SHA256)
                self.assertEqual(manifest["lower_parasite_model_sha256"],
                                 package.LOWER_PARASITE_MODEL_SHA256)
                self.assertEqual(manifest["lower_tracked_ghost_full_sha256"],
                                 package.LOWER_TRACKED_GHOST_SHA256)
                self.assertIn("--lower-parasite-table",
                              manifest["commands"]["shards"][0])
                self.assertIn("--lower-ghost-sidecar",
                              manifest["commands"]["shards"][0])
                self.assertIn("--lower-tracked-ghost-table",
                              manifest["commands"]["shards"][0])
                for index in range(64):
                    self.assertIn(f"work/logs/shard-{index:02d}.log",
                                  manifest["artifacts"])
                runner.validate_manifest(manifest)

    def test_bundles_are_deterministic_and_minimal(self):
        for filename in package.ROWS:
            with self.subTest(filename=filename), \
                 tempfile.TemporaryDirectory() as directory:
                first = Path(directory) / "first.tar"
                second = Path(directory) / "second.tar"
                package.build_bundle(filename, first)
                package.build_bundle(filename, second)
                self.assertEqual(hashlib.sha256(first.read_bytes()).digest(),
                                 hashlib.sha256(second.read_bytes()).digest())
                with tarfile.open(first) as archive:
                    names = archive.getnames()
                self.assertEqual(names[0], "bundle-manifest.json")
                self.assertIn(f"tablebases/{filename}", names)
                self.assertIn("tablebases/kparasitek.uftb", names)
                self.assertIn("tablebases/kghostk-tracked.uftb", names)
                self.assertIn("tools/tablebases/run_ultimate_ghost_parasite_aws.py", names)
                self.assertNotIn(
                    "src/ultimate/tablebases/ghost_public_extra_information_tablebase.cpp",
                    names)

    def test_runner_fails_closed_on_range_or_orientation_drift(self):
        manifest = package.build_manifest("kghostparasitek.uftb")
        for key, value in (("orientation", "opposing"),
                           ("shard_count_distribution", {"7703": 31,
                                                          "7702": 33}),
                           ("shards", 63)):
            corrupted = dict(manifest)
            corrupted[key] = value
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                runner.validate_manifest(corrupted)


if __name__ == "__main__":
    unittest.main()
