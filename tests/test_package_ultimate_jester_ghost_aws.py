import importlib.util
from pathlib import Path
import sys
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


class JesterGhostAwsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lower = Path(
          "/private/tmp/ultimatefish-information-overlays.exact-progress-v2/kjesterk.ufiw")
        if not cls.lower.exists():
            raise unittest.SkipTest("current exact lower Jester overlay unavailable")

    def test_manifest_is_gap_free_visibility_aligned_and_bound(self):
        manifest = package.build_manifest(self.lower)
        self.assertEqual(manifest["schema"], "ultimate-jester-ghost-aws-v1")
        self.assertEqual(manifest["active_jobs"], 60)
        self.assertEqual(manifest["parallelism"], 30)
        self.assertEqual(manifest["raw_per_shard"] % 78, 0)
        commands = manifest["commands"]["shards"]
        self.assertEqual(len(commands), 60)
        cursor = 0
        for command in commands:
            begin = int(command[command.index("--raw-begin") + 1])
            count = int(command[command.index("--raw-count") + 1])
            self.assertEqual(begin, cursor); self.assertEqual(count, 640_848)
            cursor += count
        self.assertEqual(cursor, package.RAW_GEOMETRIES)
        self.assertIn("--output-arbitrary", manifest["commands"]["solve"])
        self.assertIn("work/results/kjesterghostk.ufjg", manifest["artifacts"])
        self.assertEqual(
          package.information.solver_model_fingerprint("kbishopghostk.uftb"),
          package.BISHOP_GHOST_FINGERPRINT)
        self.assertEqual(
          package.information.solver_model_fingerprint("kghostghostk.uftb"),
          package.GHOST_PAIR_FINGERPRINT)

    def test_tar_is_deterministic_and_contains_authenticated_lower(self):
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "a.tar"; second = Path(directory) / "b.tar"
            package.build_bundle(first, self.lower); package.build_bundle(second, self.lower)
            self.assertEqual(package.sha(first.read_bytes()), package.sha(second.read_bytes()))
            with tarfile.open(first) as archive:
                names = set(archive.getnames())
            self.assertIn("tablebases/kjesterk.ufiw", names)
            self.assertIn("bundle-manifest.json", names)


if __name__ == "__main__": unittest.main()
