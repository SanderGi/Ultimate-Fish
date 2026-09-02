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
    def test_copyfile_allow_same_accepts_retained_dependency(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            retained = Path(temporary) / "retained.uftb"
            retained.write_bytes(b"authenticated retained dependency")
            runner.copyfile_allow_same(retained, retained)
            self.assertEqual(
                retained.read_bytes(), b"authenticated retained dependency")

    def test_prebuilt_install_replaces_stale_solve_existing_binary(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "prebuilt"
            destination = root / "work-binary"
            source.write_bytes(b"parallel-current")
            destination.write_bytes(b"stale-serial")
            expected = runner.sha256_path(source)

            runner.install_prebuilt_executable(
                source, destination, expected)

            self.assertEqual(source.read_bytes(), destination.read_bytes())
            self.assertEqual(0o755, destination.stat().st_mode & 0o777)

    def test_legacy_single_worker_omits_unsupported_flag(self) -> None:
        self.assertEqual(runner.solve_worker_arguments(1), [])
        self.assertEqual(
            runner.solve_worker_arguments(10), ["--workers", "10"])

    def test_retained_roots_require_explicit_resume_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            scratch = Path(directory) / "retained"
            Path(str(scratch) + ".owner-current").write_bytes(b"root")
            with self.assertRaisesRegex(RuntimeError, "explicit resume"):
                runner.fixed_point_resume_arguments(
                    scratch, solve_existing=True, enabled=False, iteration=0,
                    current_slot="current", bdd_slot="a")

    def test_complete_resume_metadata_is_forwarded_exactly(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            scratch = Path(directory) / "retained"
            for suffix in runner.FIXED_POINT_ROOT_SUFFIXES:
                Path(str(scratch) + suffix).write_bytes(b"root")
            for suffix in ("nodes", "unique"):
                Path(str(scratch) + f".bdd-b.{suffix}").write_bytes(b"bdd")
            self.assertEqual(
                runner.fixed_point_resume_arguments(
                    scratch, solve_existing=True, enabled=True, iteration=31,
                    current_slot="next", bdd_slot="b"),
                ["--resume-fixed-point", "--resume-iteration", "31",
                 "--resume-current-slot", "next", "--resume-bdd-slot", "b"],
            )

    def test_resume_requires_solve_existing_and_complete_files(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            scratch = Path(directory) / "retained"
            with self.assertRaisesRegex(RuntimeError, "metadata is invalid"):
                runner.fixed_point_resume_arguments(
                    scratch, solve_existing=False, enabled=True, iteration=1,
                    current_slot="current", bdd_slot="a")
            with self.assertRaisesRegex(RuntimeError, "file is missing"):
                runner.fixed_point_resume_arguments(
                    scratch, solve_existing=True, enabled=True, iteration=1,
                    current_slot="current", bdd_slot="a")

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

    def arguments(self) -> SimpleNamespace:
        return SimpleNamespace(
            filename="kknightghostk.uftb", piece="knight",
            orientation="same", source_sha256="1" * 64,
            model_sha256="2" * 64, observation_sha256="3" * 64,
            lower_sha256="4" * 64, lower_model_sha256="5" * 64,
            lower_ghost_sha256="6" * 64,
            lower_ghost_source_sha256="7" * 64,
            lower_ghost_model_sha256="8" * 64,
            lower_ghost_observation_sha256="9" * 64)

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
            self.assertEqual(manifest["observation_sha256"], "3" * 64)
            self.assertEqual(manifest["lower_ghost_sidecar_sha256"], "6" * 64)
            self.assertEqual(manifest["lower_ghost_source_sha256"], "7" * 64)
            self.assertEqual(manifest["lower_ghost_model_sha256"], "8" * 64)
            self.assertEqual(
                manifest["lower_ghost_observation_sha256"], "9" * 64)

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
        self.assertEqual(runner.geometry_count("dragon"), 492_960)
        self.assertEqual(runner.PIECES["dragon"], "Dragon")

    def test_substate_and_horizontal_factors_are_both_applied(self) -> None:
        self.assertEqual(runner.geometry_count("pawn"), 1_971_840)
        self.assertEqual(runner.geometry_count("checker"), 3_943_680)
        self.assertEqual(runner.geometry_count("sniper"), 3_943_680)

    def test_substate_only_geometry_count(self) -> None:
        self.assertEqual(runner.geometry_count("berserker"), 4_929_600)
        self.assertEqual(runner.geometry_count("penguin"), 3_943_680)

    def test_angel_geometry_count_is_orientation_bound(self) -> None:
        self.assertEqual(runner.geometry_count("angel", "same"), 1_478_880)
        self.assertEqual(
            runner.geometry_count("angel", "opposing"), 985_920)
        with self.assertRaisesRegex(ValueError, "requires an orientation"):
            runner.geometry_count("angel")

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
