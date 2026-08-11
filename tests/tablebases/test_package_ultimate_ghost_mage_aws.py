import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

from artifact_support import require_artifacts


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


package = load("package_ultimate_ghost_mage_aws",
               TOOLS / "package_ultimate_ghost_mage_aws.py")
runner = load("run_ultimate_ghost_mage_aws",
              TOOLS / "run_ultimate_ghost_mage_aws.py")


class MageGhostAwsBundleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        require_artifacts(
            ROOT, "tablebases/kghostk.ufgm", "tablebases/kmagek.uftb",
            "tablebases/kghostmagek.uftb", "tablebases/kghostkmage.uftb")

    def test_ranges_are_balanced_gap_free_and_exact(self):
        ranges = package.shard_ranges()
        self.assertEqual(len(ranges), 64)
        cursor = 0
        counts = {}
        for index, (name, begin, count) in enumerate(ranges):
            self.assertEqual(name, f"shard-{index:02d}")
            self.assertEqual(begin, cursor)
            cursor += count
            counts[count] = counts.get(count, 0) + 1
        self.assertEqual(cursor, 492_960)
        self.assertEqual(counts, {7_703: 32, 7_702: 32})

    def test_both_manifests_bind_inputs_and_measure_first(self):
        for filename, orientation, model in (
                ("kghostmagek.uftb", "same",
                 "21e04690c947749e1bcc044513f22fc66a6882688e066cd5a2b500c74f52d205"),
                ("kghostkmage.uftb", "opposing",
                 "7071380483a55c200a6e33cb47819e234ef735070533b137483e16969ff2053c")):
            manifest = package.build_manifest(filename)
            runner.validate_manifest(manifest)
            self.assertEqual(manifest["orientation"], orientation)
            self.assertEqual(manifest["model_sha256"], model)
            self.assertEqual(manifest["parallelism"], 29)
            self.assertEqual(manifest["lower_sidecar_sha256"],
                             package.LOWER_SHA256)
            paths = {record["path"] for record in manifest["files"]}
            self.assertIn(f"tablebases/{filename}", paths)
            self.assertIn("tablebases/kghostk.ufgm", paths)
            self.assertNotIn("tablebases/kmagek.uftb", paths)
            build = manifest["commands"]["build"]
            self.assertEqual(build[0], "clang++")
            self.assertIn("-Werror", build)
            self.assertIn("-Wno-error=range-loop-construct", build)
            self.assertEqual(build[build.index("-include") + 1], "sstream")
            for command in (manifest["commands"]["measure"],
                            manifest["commands"]["solve"]):
                self.assertEqual(
                    command[command.index("--compact-every") + 1], "1")
                self.assertIn(f"work/results/{Path(filename).stem}.ufmg",
                              command)

    def test_deterministic_bundle_and_authenticated_members(self):
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "first.tar"
            second = Path(directory) / "second.tar"
            package.build_bundle("kghostmagek.uftb", first)
            package.build_bundle("kghostmagek.uftb", second)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            with tarfile.open(first) as archive:
                manifest = json.loads(
                    archive.extractfile("bundle-manifest.json").read())
                runner.validate_manifest(manifest)
                for record in manifest["files"]:
                    payload = archive.extractfile(record["path"]).read()
                    self.assertEqual(len(payload), record["bytes"])
                    self.assertEqual(hashlib.sha256(payload).hexdigest(),
                                     record["sha256"])

    def test_manifest_validation_fails_closed(self):
        manifest = package.build_manifest("kghostkmage.uftb")
        manifest["commands"]["shards"][1][
            manifest["commands"]["shards"][1].index("--geometry-begin") + 1
        ] = "0"
        with self.assertRaisesRegex(RuntimeError, "gap-free"):
            runner.validate_manifest(manifest)
        manifest = package.build_manifest("kghostkmage.uftb")
        measure = manifest["commands"]["measure"]
        measure[measure.index("--compact-every") + 1] = "4"
        with self.assertRaisesRegex(RuntimeError, "cadence"):
            runner.validate_manifest(manifest)


if __name__ == "__main__":
    unittest.main()
