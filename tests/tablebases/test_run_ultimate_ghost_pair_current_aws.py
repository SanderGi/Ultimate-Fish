#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "run_ultimate_ghost_pair_current_aws",
    ROOT / "tools/tablebases/run_ultimate_ghost_pair_current_aws.py")
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class CurrentGhostPairRunnerTests(unittest.TestCase):
    def test_prebuilt_model_binding_is_strict_and_optional(self) -> None:
        executable = Path("solver")
        runner.validate_prebuilt_binding(executable, "a" * 64, "", "b" * 64)
        runner.validate_prebuilt_binding(
            executable, "a" * 64, "b" * 64, "b" * 64)
        with self.assertRaisesRegex(RuntimeError, "must equal"):
            runner.validate_prebuilt_binding(
                executable, "a" * 64, "c" * 64, "b" * 64)
        with self.assertRaisesRegex(RuntimeError, "must equal"):
            runner.validate_prebuilt_binding(
                None, "", "b" * 64, "b" * 64)

    def test_manifest_requires_both_sidecars(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            results = work / "work/results"
            logs = work / "work/logs"
            results.mkdir(parents=True)
            logs.mkdir(parents=True)
            (results / "kghostghostk.ufiw").write_bytes(b"overlay")
            (results / "kghostghostk.ufgg").write_bytes(b"beliefs")
            (logs / "solve.log").write_text("fixed point proof\n")
            args = SimpleNamespace(
                source_sha256="1" * 64, model_sha256="2" * 64,
                observation_sha256="3" * 64,
                lower_ghost_sha256="4" * 64)
            runner.write_manifest(work, args)
            manifest = json.loads(
                (work / "work/artifact-manifest.json").read_text())
            self.assertEqual(manifest["filename"], "kghostghostk.uftb")
            self.assertEqual({Path(path).suffix for path in manifest["files"]
                              if path.startswith("work/results/")},
                             {".ufiw", ".ufgg"})

    def test_transition_manifest_binds_exact_model(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            (work / "work").mkdir()
            args = SimpleNamespace(
                source_sha256="1" * 64, model_sha256="2" * 64,
                observation_sha256="3" * 64,
                lower_ghost_sha256="4" * 64)
            runner.write_transition_manifest(work, args)
            marker = json.loads(
                (work / "work/transitions-ready.json").read_text())
            self.assertEqual(marker["schema"],
                             "ultimate-ghost-pair-current-transitions-v1")
            self.assertEqual(marker["model_sha256"], "2" * 64)

    def test_transition_manifest_validation_rejects_any_drift(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            path = work / "work/transitions-ready.json"
            args = SimpleNamespace(
                source_sha256="1" * 64, model_sha256="2" * 64,
                observation_sha256="3" * 64,
                lower_ghost_sha256="4" * 64)
            with self.assertRaisesRegex(RuntimeError, "lacks"):
                runner.validate_transition_manifest(path, args)
            (work / "work").mkdir()
            runner.write_transition_manifest(work, args)
            generated = work / "work/transitions-ready.json"
            runner.validate_transition_manifest(generated, args)
            marker = json.loads(generated.read_text())
            marker["model_sha256"] = "5" * 64
            generated.write_text(json.dumps(marker))
            with self.assertRaisesRegex(RuntimeError, "binding mismatch"):
                runner.validate_transition_manifest(generated, args)


if __name__ == "__main__":
    unittest.main()
