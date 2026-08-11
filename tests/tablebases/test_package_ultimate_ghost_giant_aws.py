#!/usr/bin/env python3
"""Tests for deterministic exact Giant/Ghost AWS bundles."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

from artifact_support import require_artifacts


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))


def load(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, TOOLS / filename)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


package = load("package_ghost_giant", "package_ultimate_ghost_giant_aws.py")
runner = load("run_ghost_giant", "run_ultimate_ghost_giant_aws.py")


class GhostGiantPackageTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        require_artifacts(
            ROOT, "tablebases/kghostk.ufgm", "tablebases/kgiantk.uftb",
            "tablebases/kghostgiantk.uftb", "tablebases/kghostkgiant.uftb")

    def test_exact_models_dependencies_and_ranges(self) -> None:
        expected = {
            "kghostgiantk.uftb":
                "31ca4c394fb1ff94a338b66c2ea6856eac05278241ed6f5aefdd26bd66dd2f48",
            "kghostkgiant.uftb":
                "8be6f990ed4f788523e6b8f116b6d7db4428a6847f7db810de9213980237e979",
        }
        self.assertEqual(package.LOWER_GIANT_SHA256,
                         package.sha256_bytes(
                             (ROOT / "tablebases/kgiantk.uftb").read_bytes()))
        self.assertEqual(package.LOWER_GIANT_MODEL_SHA256,
                         package.information.concrete_tablebase_model_fingerprint(
                             "kgiantk.uftb"))
        ranges = package.shard_ranges()
        self.assertEqual(64, len(ranges))
        self.assertEqual(0, ranges[0][1])
        self.assertEqual(package.GEOMETRIES,
                         ranges[-1][1] + ranges[-1][2])
        self.assertTrue(all(left[1] + left[2] == right[1]
                            for left, right in zip(ranges, ranges[1:])))
        for filename, model in expected.items():
            with self.subTest(filename=filename):
                manifest = package.build_manifest(filename)
                self.assertEqual(model, manifest["model_sha256"])
                self.assertEqual(package.LOWER_GIANT_SHA256,
                                 manifest["lower_giant_full_sha256"])
                self.assertEqual(package.LOWER_GIANT_MODEL_SHA256,
                                 manifest["lower_giant_model_sha256"])
                runner.validate_manifest(manifest)
                self.assertEqual(64, len(manifest["commands"]["shards"]))
                self.assertEqual("1",
                                 manifest["commands"]["measure"][2])
                self.assertEqual("1",
                                 manifest["commands"]["solve"][-1])
                self.assertIn("work/results/" + Path(filename).stem + ".ufgi",
                              manifest["artifacts"])

    def test_bundle_is_deterministic_and_manifest_authenticates_every_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / "first.tar"
            second = root / "second.tar"
            package.build_bundle("kghostgiantk.uftb", first)
            package.build_bundle("kghostgiantk.uftb", second)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            import tarfile
            with tarfile.open(first) as archive:
                manifest = json.loads(
                    archive.extractfile("bundle-manifest.json").read())
                names = set(archive.getnames())
                for record in manifest["files"]:
                    self.assertIn(record["path"], names)
                    payload = archive.extractfile(record["path"]).read()
                    self.assertEqual(record["bytes"], len(payload))
                    self.assertEqual(record["sha256"],
                                     package.sha256_bytes(payload))

    def test_manifest_rejects_orientation_or_geometry_drift(self) -> None:
        manifest = package.build_manifest("kghostkgiant.uftb")
        damaged = json.loads(json.dumps(manifest))
        damaged["orientation"] = "same"
        with self.assertRaisesRegex(RuntimeError, "invalid Giant/Ghost"):
            runner.validate_manifest(damaged)
        damaged = json.loads(json.dumps(manifest))
        damaged["geometries"] += 1
        with self.assertRaisesRegex(RuntimeError, "invalid Giant/Ghost"):
            runner.validate_manifest(damaged)


if __name__ == "__main__":
    unittest.main()
