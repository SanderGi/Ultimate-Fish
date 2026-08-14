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
SPEC = importlib.util.spec_from_file_location(
    "package_ultimate_ghost_bomb_aws",
    TOOLS / "package_ultimate_ghost_bomb_aws.py")
assert SPEC and SPEC.loader
package = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = package
SPEC.loader.exec_module(package)
RUNNER_SPEC = importlib.util.spec_from_file_location(
    "run_ultimate_ghost_bomb_aws",
    TOOLS / "run_ultimate_ghost_bomb_aws.py")
assert RUNNER_SPEC and RUNNER_SPEC.loader
runner = importlib.util.module_from_spec(RUNNER_SPEC)
sys.modules[RUNNER_SPEC.name] = runner
RUNNER_SPEC.loader.exec_module(runner)
STAGE_SPEC = importlib.util.spec_from_file_location(
    "stage_ultimate_ghost_bomb_resume_aws",
    TOOLS / "stage_ultimate_ghost_bomb_resume_aws.py")
assert STAGE_SPEC and STAGE_SPEC.loader
stage = importlib.util.module_from_spec(STAGE_SPEC)
sys.modules[STAGE_SPEC.name] = stage
STAGE_SPEC.loader.exec_module(stage)


class BombGhostResumeUnitTests(unittest.TestCase):
    def manifest(self, filename="kbombghostk.uftb", orientation="same"):
        return {
            "schema": "ultimate-bomb-ghost-aws-v3",
            "canonical_commit": "d" * 40,
            "filename": filename,
            "orientation": orientation,
            "implementation_sha256": "a" * 64,
            "model_sha256": "b" * 64,
            "observation_sha256": "c" * 64,
            "geometries": 492_960,
            "shards": 64,
            "shard_count_distribution": {"7703": 32, "7702": 32},
            "parallelism": 29,
            "commands": {"shards": [[] for _ in range(64)]},
        }

    def test_resume_authenticates_every_extent_and_digest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            prefix = root / "work" / "transitions" / "kbombghost"
            prefix.parent.mkdir(parents=True)
            paths = [Path(f"{prefix}{suffix}")
                     for suffix in runner.shared.TRANSITION_SUFFIXES]
            (root / "work" / "logs").mkdir()
            paths.append(root / "work" / "logs" / "merge.log")
            for index, path in enumerate(paths):
                path.write_bytes(f"proof-{index}".encode())
            resume = {
                "schema": "ultimate-bomb-ghost-transition-resume-v1",
                "filename": "kbombghostk.uftb", "orientation": "same",
                "model_sha256": "b" * 64,
                "observation_sha256": "c" * 64,
                "source_prefix": str(prefix),
                "files": [{"name": path.name, "bytes": path.stat().st_size,
                           "sha256": runner.shared.sha256(path)}
                          for path in paths],
            }
            previous = runner.RESUME_PREFIXES["kbombghostk.uftb"]
            runner.RESUME_PREFIXES["kbombghostk.uftb"] = prefix
            try:
                self.assertEqual(runner.validate_transition_resume(
                    resume, self.manifest()), prefix)
                paths[3].write_bytes(b"changed")
                with self.assertRaises(RuntimeError):
                    runner.validate_transition_resume(resume, self.manifest())
            finally:
                runner.RESUME_PREFIXES["kbombghostk.uftb"] = previous

    def test_resume_prefixes_match_preserved_merged_artifact_names(self):
        self.assertEqual(
            runner.RESUME_PREFIXES["kbombghostk.uftb"].name,
            "kbombghostk")
        self.assertEqual(
            stage.ROWS["kbombghostk.uftb"]["source_prefix"].name,
            "kbombghostk")
        self.assertEqual(
            runner.RESUME_PREFIXES["kbombkghost.uftb"].name,
            "kbombkghost")
        self.assertEqual(
            stage.ROWS["kbombkghost.uftb"]["source_prefix"].name,
            "kbombkghost")

    def test_measure_rewrites_only_the_transition_prefix(self):
        command = ["solver", "--measure", "1", "--transition-prefix",
                   "work/transitions/kbombghost", "--output", "result"]
        rewritten = runner.resumed_measure_command(
            command, Path("/read-only/kbombghost"))
        self.assertEqual(command[4], "work/transitions/kbombghost")
        self.assertEqual(rewritten[4], "/read-only/kbombghost")
        self.assertEqual(rewritten[:4] + rewritten[5:],
                         command[:4] + command[5:])

    def test_exact_solve_gets_an_authenticated_writable_transition_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "preserved" / "kbombghostk"
            source.parent.mkdir()
            for index, suffix in enumerate(runner.shared.TRANSITION_SUFFIXES):
                Path(f"{source}{suffix}").write_bytes(
                    f"transition-{index}".encode())
            command = ["solver", "--transition-prefix",
                       "work/transitions/kbombghostk"]
            target = runner.prepare_writable_transition_copy(
                root, source, command)
            for suffix in runner.shared.TRANSITION_SUFFIXES:
                self.assertEqual(Path(f"{target}{suffix}").read_bytes(),
                                 Path(f"{source}{suffix}").read_bytes())
            Path(f"{target}.blocks").write_bytes(b"patched")
            self.assertNotEqual(Path(f"{target}.blocks").read_bytes(),
                                Path(f"{source}.blocks").read_bytes())

    def test_retry_solve_creates_a_new_scratch_parent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            command = ["solver", "--scratch", "work/solve-1b/kbombkghost"]
            scratch = runner.prepare_solve_scratch(root, command)
            self.assertEqual(scratch, root / "work/solve-1b/kbombkghost")
            self.assertTrue(scratch.parent.is_dir())
            with self.assertRaises(RuntimeError):
                runner.prepare_solve_scratch(
                    root, ["solver", "--scratch", "../outside"])

    def test_completed_measurement_requires_authenticated_zero_residual_proof(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "ultimate_ghost_bomb_information_tablebase"
            executable.write_text("binary")
            executable.chmod(0o755)
            build = root / "work/logs/build.log"
            build.parent.mkdir(parents=True)
            build.write_text("successful build\n")
            self_test = root / "work/logs/self-test.log"
            self_test.write_text(
                "ghost_bomb_exact_self_test codec_states 151831680 "
                "remap_residual 0 belief_cap none\n"
                "bomb_ghost_resource geometries 492960 concrete_worlds "
                "37957920 owner_roots 39436800\n")
            log = root / "work/logs/measure.log"
            log.write_text(
                "reciprocal_ghost_extra_measurement iterations 1 "
                "bdd_nodes 190808071 peak_rss_bytes 7278997504 "
                "proof_complete 0 overlay_written 0\n"
                "bomb_ghost_certificate dual_force_residual 0 "
                "structural_residual 0 singleton_residual 0 "
                "source_remap_residual 0 normalized_source_sha256 "
                f"{'a' * 64} transition_payload_sha256  arbitrary_sha256 \n")
            artifact = {
                "filename": "kbombghostk.uftb",
                "model_sha256": "b" * 64,
                "artifacts": [
                    {"path": str(path.relative_to(root)),
                     "bytes": path.stat().st_size,
                     "sha256": runner.shared.sha256(path)}
                    for path in (build, self_test, log)
                ],
            }
            (root / "work/artifact-manifest.json").write_text(
                json.dumps(artifact))
            runner.validate_completed_setup(root, self.manifest())
            log.write_text(log.read_text().replace(
                "structural_residual 0", "structural_residual 1"))
            with self.assertRaises(RuntimeError):
                runner.validate_completed_setup(root, self.manifest())

    def test_runner_rejects_old_schema(self):
        manifest = self.manifest()
        manifest["schema"] = "ultimate-bomb-ghost-aws-v2"
        with self.assertRaises(RuntimeError):
            runner.validate_manifest(manifest)

    def test_safe_extract_rejects_links_and_preserves_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "unsafe.tar"
            with tarfile.open(archive, "w") as output:
                member = tarfile.TarInfo("link")
                member.type = tarfile.SYMTYPE
                member.linkname = "/etc/passwd"
                output.addfile(member)
            destination = root / "destination"
            with self.assertRaises(RuntimeError):
                stage.safe_extract(archive, destination)
            self.assertFalse(destination.exists())

    def test_legacy_observation_digest_survives_only_the_move(self):
        self.assertEqual(package.legacy_observation_fingerprint(),
                         "af09ebab834599de83d546f8729b8329dbe5ba8ff1cc7f24be3ac63086273adf")
        self.assertNotEqual(package.legacy_observation_fingerprint(),
                            package.OBSERVATION_SHA256)
        self.assertEqual(package.legacy_lower_bomb_model_fingerprint(),
                         package.LOWER_BOMB_MODEL_SHA256)

    def test_v3_services_are_single_cpu_and_keep_v2_read_only(self):
        cases = (
            ("kbombghostk.uftb", "AllowedCPUs=0",
             "/hidden-kbombghostk-7af5dec2/"),
            ("kbombkghost.uftb", "AllowedCPUs=1",
             "/hidden-kbombkghost-7af5dec2/"),
        )
        for filename, cpu, root in cases:
            with self.subTest(filename=filename):
                service = (ROOT / package.SERVICE_FILES[filename]).read_text()
                self.assertIn(cpu, service)
                self.assertIn("CPUQuota=100%", service)
                self.assertIn(root + "resume-v2", service)
                self.assertIn("ReadOnlyPaths=", service)
                self.assertIn(root + "resume-v3", service)
                self.assertIn("--transition-resume-manifest", service)
                self.assertNotIn("solvefix-v2", service)


class BombGhostAwsBundleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        require_artifacts(
            ROOT, "tablebases/kghostk.ufgm", "tablebases/kbombk.uftb",
            "tablebases/kbombghostk.uftb", "tablebases/kbombkghost.uftb")

    @staticmethod
    def command_range(command):
        prefix = command[command.index("--transition-prefix") + 1]
        begin = int(command[command.index("--geometry-begin") + 1])
        count = int(command[command.index("--geometry-count") + 1])
        return Path(prefix).name, begin, count

    def test_both_manifests_are_gap_free_and_fingerprint_isolated(self):
        for filename, orientation in (("kbombghostk.uftb", "same"),
                                      ("kbombkghost.uftb", "opposing")):
            with self.subTest(filename=filename):
                manifest = package.build_manifest(filename)
                self.assertEqual(manifest["schema"],
                                 "ultimate-bomb-ghost-aws-v3")
                self.assertEqual(len(manifest["canonical_commit"]), 40)
                self.assertEqual(manifest["implementation_sha256"],
                                 package.ROWS[filename][
                                     "implementation_sha256"])
                self.assertEqual(manifest["model_sha256"],
                                 package.ROWS[filename]["model_sha256"])
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
                    "c++", "-std=c++17", "-O3", "-DNDEBUG",
                    "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                    "-Wno-error=range-loop-construct",
                    "-include", "sstream", "-Isrc/ultimate",
                    "-Isrc/ultimate/tablebases",
                    "src/ultimate/tablebases/ghost_bomb_information_tablebase.cpp",
                ])
                stem = Path(filename).stem
                self.assertIn(f"work/results/{stem}.ufgb",
                              manifest["artifacts"])
                self.assertEqual(manifest["lower_bomb_full_sha256"],
                                 package.LOWER_BOMB_SHA256)
                self.assertEqual(manifest["lower_bomb_model_sha256"],
                                 package.LOWER_BOMB_MODEL_SHA256)
                self.assertIn("--lower-bomb-table",
                              manifest["commands"]["shards"][0])
                for index in range(64):
                    self.assertIn(f"work/logs/shard-{index:02d}.log",
                                  manifest["artifacts"])
                runner.validate_manifest(manifest)
        for frozen, expected in package.FROZEN_FINGERPRINTS.items():
            self.assertEqual(
                package.information.solver_model_fingerprint(frozen), expected)

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
                self.assertIn("tablebases/kbombk.uftb", names)
                self.assertIn("tools/tablebases/run_ultimate_ghost_bomb_aws.py", names)
                self.assertIn(
                    "tools/tablebases/stage_ultimate_ghost_bomb_resume_aws.py",
                    names)
                self.assertNotIn(
                    "src/ultimate/tablebases/ghost_public_extra_information_tablebase.cpp",
                    names)

    def test_runner_fails_closed_on_range_or_orientation_drift(self):
        manifest = package.build_manifest("kbombghostk.uftb")
        for key, value in (("orientation", "opposing"),
                           ("implementation_sha256", "0" * 63),
                           ("shard_count_distribution", {"7703": 31,
                                                          "7702": 33}),
                           ("shards", 63)):
            corrupted = dict(manifest)
            corrupted[key] = value
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                runner.validate_manifest(corrupted)

    def test_runner_preserves_completed_merged_transition(self):
        manifest = package.build_manifest("kbombkghost.uftb")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            prefix = root / "work/transitions/kbombkghost"
            prefix.parent.mkdir(parents=True)
            for suffix in runner.shared.TRANSITION_SUFFIXES:
                Path(f"{prefix}{suffix}").write_bytes(b"preserved")
            (root / "work/logs").mkdir(parents=True)
            (root / "work/logs/merge.log").write_text("certified\n")
            self.assertTrue(runner.merged_transition_is_complete(
                root, manifest["commands"]["merge"]))
            Path(f"{prefix}.verified").unlink()
            self.assertFalse(runner.merged_transition_is_complete(
                root, manifest["commands"]["merge"]))


if __name__ == "__main__":
    unittest.main()
