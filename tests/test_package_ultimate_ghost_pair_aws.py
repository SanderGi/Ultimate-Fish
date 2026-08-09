import hashlib
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
    "package_ultimate_ghost_pair_aws",
    TOOLS / "package_ultimate_ghost_pair_aws.py")
assert SPEC and SPEC.loader
package = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = package
SPEC.loader.exec_module(package)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1 << 20):
            digest.update(chunk)
    return digest.hexdigest()


class GhostPairAwsBundleTests(unittest.TestCase):
    def test_manifest_has_exact_authenticated_32_shard_cover(self):
        manifest = package.build_manifest()
        self.assertEqual(manifest["schema"],
                         "ultimate-ghost-pair-aws-v1")
        self.assertEqual(manifest["shards"], 32)
        self.assertEqual(manifest["raw_per_shard"], 1_217_390)
        self.assertEqual(manifest["side_half_boundary"], 19_478_240)
        commands = manifest["commands"]["shards"]
        self.assertEqual(len(commands), 32)
        for index, command in enumerate(commands):
            begin = int(command[command.index("--raw-begin") + 1])
            count = int(command[command.index("--raw-count") + 1])
            self.assertEqual(begin, index * 1_217_390)
            self.assertEqual(count, 1_217_390)
            self.assertIn(manifest["source_sha256"], command)
            self.assertIn(manifest["model_sha256"], command)
            self.assertIn(manifest["observation_sha256"], command)
        self.assertEqual(
            manifest["source_sha256"],
            "204f4de6d0f9ff6da111d3d0c0a08c3562493cdec2130cc946b4eae7183012ee")
        self.assertEqual(
            manifest["lower_sidecar_sha256"],
            "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        self.assertIn("--output-arbitrary", manifest["commands"]["solve"])
        self.assertIn("work/results/kghostghostk.ufgg",
                      manifest["artifacts"])

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
            self.assertNotIn("src/ultimate/ghost_extra_information_tablebase.cpp",
                             names)


if __name__ == "__main__":
    unittest.main()
