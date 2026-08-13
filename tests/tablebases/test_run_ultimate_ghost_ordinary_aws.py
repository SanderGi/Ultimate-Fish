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
    "run_ultimate_ghost_ordinary_aws",
    ROOT / "tools/tablebases/run_ultimate_ghost_ordinary_aws.py")
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class OrdinaryGhostManifestTests(unittest.TestCase):
    def arguments(self) -> SimpleNamespace:
        return SimpleNamespace(
            filename="kknightghostk.uftb", piece="knight",
            orientation="same", source_sha256="1" * 64,
            model_sha256="2" * 64, lower_sha256="3" * 64,
            lower_model_sha256="4" * 64)

    def test_manifest_keeps_results_and_proof_logs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            results = work / "work/results"
            logs = work / "work/logs"
            results.mkdir(parents=True)
            logs.mkdir(parents=True)
            (results / "kknightghostk.ufiw").write_bytes(b"overlay")
            (results / "kknightghostk.ufgd").write_bytes(b"beliefs")
            (logs / "build.log").write_text("build proof\n")
            (logs / "solve.log").write_text("fixed point proof\n")
            runner.write_manifest(work, self.arguments())
            manifest = json.loads(
                (work / "work/artifact-manifest.json").read_text())
            self.assertEqual(set(manifest["files"]), {
                "work/results/kknightghostk.ufiw",
                "work/results/kknightghostk.ufgd",
                "work/logs/build.log", "work/logs/solve.log",
            })

    def test_incomplete_result_proof_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            (work / "work/results").mkdir(parents=True)
            (work / "work/logs").mkdir(parents=True)
            (work / "work/results/kknightghostk.ufiw").write_bytes(b"overlay")
            with self.assertRaisesRegex(RuntimeError, "inventory is incomplete"):
                runner.write_manifest(work, self.arguments())


class OrdinaryGhostGeometryTests(unittest.TestCase):
    def test_base_piece_geometry_count(self) -> None:
        self.assertEqual(runner.geometry_count("knight"), 492_960)

    def test_substate_and_horizontal_factors_are_both_applied(self) -> None:
        self.assertEqual(runner.geometry_count("pawn"), 1_971_840)
        self.assertEqual(runner.geometry_count("checker"), 3_943_680)
        self.assertEqual(runner.geometry_count("sniper"), 3_943_680)

    def test_substate_only_geometry_count(self) -> None:
        self.assertEqual(runner.geometry_count("berserker"), 4_929_600)
        self.assertEqual(runner.geometry_count("penguin"), 3_943_680)

    def test_ranges_cover_checker_domain_exactly(self) -> None:
        geometries = runner.geometry_count("checker")
        shards = runner.ranges(geometries)
        self.assertEqual(len(shards), runner.SHARDS)
        self.assertEqual(shards[0], (0, 61_620))
        self.assertEqual(shards[-1], (3_882_060, 61_620))

    def test_unknown_piece_fails_closed(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown ordinary Ghost piece"):
            runner.geometry_count("devil")


if __name__ == "__main__":
    unittest.main()
