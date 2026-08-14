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
    "run_ultimate_ghost_mage_current_aws",
    ROOT / "tools/tablebases/run_ultimate_ghost_mage_current_aws.py")
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class CurrentMageRunnerTests(unittest.TestCase):
    def test_ranges_cover_the_domain_once(self) -> None:
        ranges = runner.ranges()
        self.assertEqual(len(ranges), 64)
        self.assertEqual(ranges[0], (0, 7_703))
        self.assertEqual(ranges[-1], (485_258, 7_702))

    def test_manifest_requires_both_sidecars_and_normalization(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            results = work / "work/results"
            logs = work / "work/logs"
            results.mkdir(parents=True)
            logs.mkdir(parents=True)
            (results / "kghostmagek.ufiw").write_bytes(b"overlay")
            (results / "kghostmagek.ufmg").write_bytes(b"beliefs")
            normalized = "a" * 64
            (logs / "self-test.log").write_text(
                "mage_ghost_normalized_source_sha256 " + normalized + "\n")
            (logs / "solve.log").write_text("fixed point proof\n")
            args = SimpleNamespace(
                kind="mage",
                filename="kghostmagek.uftb", orientation="same",
                source_sha256="1" * 64, model_sha256="2" * 64,
                observation_sha256="3" * 64,
                lower_ghost_sha256="4" * 64)
            runner.write_manifest(work, args)
            manifest = json.loads(
                (work / "work/artifact-manifest.json").read_text())
            self.assertEqual(manifest["normalized_source_sha256"], normalized)
            self.assertEqual({Path(path).suffix for path in manifest["files"]
                              if path.startswith("work/results/")},
                             {".ufiw", ".ufmg"})

    def test_fisherman_uses_collision_specific_sidecar(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            results = work / "work/results"
            logs = work / "work/logs"
            results.mkdir(parents=True)
            logs.mkdir(parents=True)
            (results / "kghostfishermank.ufiw").write_bytes(b"overlay")
            (results / "kghostfishermank.ufgf").write_bytes(b"beliefs")
            normalized = "b" * 64
            (logs / "self-test.log").write_text(
                "fisherman_ghost_normalized_source_sha256 details "
                + normalized + "\n")
            (logs / "solve.log").write_text("fixed point proof\n")
            args = SimpleNamespace(
                kind="fisherman", filename="kghostfishermank.uftb",
                orientation="same", source_sha256="1" * 64,
                model_sha256="2" * 64, observation_sha256="3" * 64,
                lower_ghost_sha256="4" * 64)
            runner.write_manifest(work, args)
            manifest = json.loads(
                (work / "work/artifact-manifest.json").read_text())
            self.assertEqual(manifest["kind"], "fisherman")
            self.assertEqual(manifest["normalized_source_sha256"], normalized)


if __name__ == "__main__":
    unittest.main()
