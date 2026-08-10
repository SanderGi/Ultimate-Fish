#!/usr/bin/env python3

import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


builder = load(
    "jg_binary_compat",
    ROOT / "tools/build_ultimate_jester_ghost_measurement_compat_aws.py")
measurement = load(
    "jg_binary_compat_measurement",
    ROOT / "tools/run_ultimate_jester_ghost_measurement_aws.py")


class MeasurementBinaryCompatibilityTest(unittest.TestCase):
    def test_builder_accepts_canonical_sha1_commit_ids(self):
        self.assertIsNotNone(builder.COMMIT.fullmatch("a" * 40))
        self.assertIsNone(builder.COMMIT.fullmatch("a" * 64))

    def test_committed_loader_patch_is_exactly_allowlisted(self):
        expected = [
            ("src/ultimate/jester_ghost_information_solver.cpp",
             builder.OLD_SOLVER_SHA256, builder.NEW_SOLVER_SHA256),
            ("src/ultimate/jester_ghost_information_solver.h",
             builder.OLD_HEADER_SHA256, builder.NEW_HEADER_SHA256),
            ("src/ultimate/jester_ghost_information_tablebase.cpp",
             builder.OLD_CLI_SHA256, builder.NEW_CLI_SHA256),
        ]
        patches = []
        for relative, old_sha, new_sha in expected:
            old = subprocess.check_output(
                ["git", "show", f"{builder.OLD_BUNDLE_COMMIT}:{relative}"],
                cwd=ROOT)
            new = (ROOT / relative).read_bytes()
            self.assertEqual(old_sha, builder.hashlib.sha256(old).hexdigest())
            self.assertEqual(new_sha, builder.hashlib.sha256(new).hexdigest())
            patches.append((relative, old, new))
        self.assertEqual(builder.PATCH_SHA256,
                         builder.loader_patch_sha(patches))

    def test_runner_authenticates_semantic_transition_compatibility(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "certificate.json"
            binary_sha = "a" * 64
            certificate = {
                "schema": builder.SCHEMA, "status": builder.STATUS,
                "new_binary": {"sha256": binary_sha},
                "source_sha256": builder.SOURCE_SHA256,
                "semantic_transition_model_sha256":
                    builder.SEMANTIC_MODEL_SHA256,
                "observation_sha256": builder.OBSERVATION_SHA256,
                "residuals": {"bundle": 0, "loader_patch": 0,
                              "build": 0, "selftest": 0,
                              "transition_semantics": 0},
            }
            path.write_text(json.dumps(certificate))
            args = argparse.Namespace(
                binary_compatibility_certificate=path,
                binary_compatibility_certificate_sha256=
                    measurement.sha256_file(path),
                binary_sha256=binary_sha)
            manifest = {"source_sha256": builder.SOURCE_SHA256,
                        "model_sha256": builder.SEMANTIC_MODEL_SHA256,
                        "observation_sha256": builder.OBSERVATION_SHA256}
            self.assertEqual(measurement.sha256_file(path),
                             measurement.authenticate_binary_compatibility(
                                 args, manifest))

    def test_runner_rejects_new_model_as_transition_semantics(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "certificate.json"
            certificate = {
                "schema": builder.SCHEMA, "status": builder.STATUS,
                "new_binary": {"sha256": "a" * 64},
                "source_sha256": builder.SOURCE_SHA256,
                "semantic_transition_model_sha256":
                    builder.FULL_SOURCE_MODEL_SHA256,
                "observation_sha256": builder.OBSERVATION_SHA256,
                "residuals": {"bundle": 0, "loader_patch": 0,
                              "build": 0, "selftest": 0,
                              "transition_semantics": 0},
            }
            path.write_text(json.dumps(certificate))
            args = argparse.Namespace(
                binary_compatibility_certificate=path,
                binary_compatibility_certificate_sha256=
                    measurement.sha256_file(path), binary_sha256="a" * 64)
            manifest = {"source_sha256": builder.SOURCE_SHA256,
                        "model_sha256": builder.SEMANTIC_MODEL_SHA256,
                        "observation_sha256": builder.OBSERVATION_SHA256}
            with self.assertRaisesRegex(ValueError, "compatibility"):
                measurement.authenticate_binary_compatibility(args, manifest)


if __name__ == "__main__":
    unittest.main()
