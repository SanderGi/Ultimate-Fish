import importlib.util
import argparse
from pathlib import Path
import struct
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "generate_ultimate_information_tablebases",
    TOOLS / "generate_ultimate_information_tablebases.py")
assert SPEC and SPEC.loader
generate = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = generate
SPEC.loader.exec_module(generate)


class InformationGenerationDriverTests(unittest.TestCase):
    @staticmethod
    def _args(root):
        return argparse.Namespace(
            binary=root / "primary",
            ghost_binary=root / "ghost",
            double_jester_binary=root / "double",
            joint_jester_binary=root / "joint",
            ghost_extra_binary=root / "ghost-extra",
            ghost_extra_transitions=root / "ghost-extra-transitions",
            overlays=root / "overlays",
            scratch=root / "scratch",
        )

    def test_summary_line_requires_exact_uncapped_certificate(self):
        line = (
            "information_summary side 1 win 120 loss 417496 draw 75344 "
            "unreachable_win 0 unreachable_loss 0 unreachable_draw 0 "
            "sets 246600 concrete 492960 bellman_residual 0 "
            "rank_residual 0 belief_cap none exhaustive 1")
        match = generate.SUMMARY_RE.fullmatch(line)
        self.assertIsNotNone(match)
        assert match is not None
        self.assertEqual(int(match["win"]), 120)
        self.assertEqual(int(match["concrete"]), 492_960)
        self.assertIsNone(generate.SUMMARY_RE.fullmatch(
            line.replace("belief_cap none", "belief_cap 64")))
        self.assertIsNone(generate.SUMMARY_RE.fullmatch(
            line.replace("exhaustive 1", "exhaustive 0")))

    def test_symbolic_ghost_certificate_requires_zero_exactness_residuals(self):
        line = (
            "information_symbolic_certificate iterations 17 bdd_nodes 1234 "
            "bellman_residual 0 monotonicity_residual 0 "
            "singleton_residual 0 belief_cap none powerset_exact 1")
        match = generate.SYMBOLIC_RE.fullmatch(line)
        self.assertIsNotNone(match)
        assert match is not None
        self.assertTrue(all(int(match[field]) == 0 for field in
                            ("bellman", "monotonicity", "singleton")))
        bad = generate.SYMBOLIC_RE.fullmatch(
            line.replace("singleton_residual 0", "singleton_residual 1"))
        self.assertIsNotNone(bad)
        assert bad is not None
        self.assertEqual(int(bad["singleton"]), 1)

    def test_checkpoint_is_bound_to_current_solver_sources(self):
        document = generate._empty_document()  # pylint: disable=protected-access
        solver = document["solver"]
        self.assertEqual(
            solver["observation_model_sha256"],
            generate.information.observation_model_fingerprint())
        self.assertTrue(solver["exhaustive"])
        self.assertIsNone(solver["belief_cap"])
        self.assertEqual(solver["version"], "2")
        self.assertEqual(document["schema_version"], 2)
        self.assertEqual(document["semantics"]["id"],
                         "fresh-maximal-public-view-v2")

    def test_v2_defaults_never_reuse_preserved_v1_artifacts(self):
        self.assertIn("legal-dots-v2", str(generate.DEFAULT_CHECKPOINT))
        self.assertIn("legal-dots-v2", str(generate.DEFAULT_OVERLAYS))
        self.assertNotIn("pre-legal-dots-v1",
                         str(generate.DEFAULT_CHECKPOINT))
        self.assertNotIn("pre-legal-dots-v1", str(generate.DEFAULT_OVERLAYS))

    def test_partial_checkpoint_rejects_source_drift(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "partial.json"
            document = generate._empty_document()  # pylint: disable=protected-access
            document["files"]["kjesterk.uftb"] = {
                "solver_model_sha256": "0" * 64,
            }
            generate.save_checkpoint(path, document)
            with self.assertRaisesRegex(RuntimeError, "stale solver"):
                generate.load_checkpoint(path)

    def test_partial_checkpoint_rejects_v1_semantics_before_entries(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "pre-legal-dots-v1.partial.json"
            document = generate._empty_document()  # pylint: disable=protected-access
            document["schema_version"] = 1
            document["semantics"]["id"] = "fresh-maximal-public-view-v1"
            document["solver"]["version"] = "1"
            generate.save_checkpoint(path, document)
            with self.assertRaisesRegex(RuntimeError, "stale schema_version"):
                generate.load_checkpoint(path)

    def test_overlay_reuse_requires_both_hashes_and_exact_domain(self):
        record = generate._records()["kjesterk.uftb"]  # pylint: disable=protected-access
        source = "1" * 64
        model = "2" * 64
        count = generate.information.states_per_side(record) * 2
        header = (struct.pack("<8s6I", b"UFIW2\0\0\0", 2, 1, 30, 0,
                              count, 1) + source.encode() + model.encode())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "overlay.ufiw"
            with path.open("wb") as stream:
                stream.write(header)
                stream.truncate(160 + count)
            self.assertTrue(generate.overlay_is_current(
                path, record, source, model))
            self.assertFalse(generate.overlay_is_current(
                path, record, "0" * 64, model))
            # A valid v1 source hash cannot rescue an overlay whose model hash
            # predates the private pre-decision legal-marker semantics.
            self.assertFalse(generate.overlay_is_current(
                path, record, source,
                generate.information.solver_model_fingerprint(
                    "kjesterk.uftb")))
            with path.open("r+b") as stream:
                stream.truncate(159 + count)
            self.assertFalse(generate.overlay_is_current(
                path, record, source, model))

    def test_checkpoint_merge_preserves_independent_completed_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "partial.json"
            first = {
                "solver_model_sha256":
                    generate.information.solver_model_fingerprint(
                        "kjesterk.uftb"),
                "marker": "first",
            }
            second = {
                "solver_model_sha256":
                    generate.information.solver_model_fingerprint(
                        "kjesterknightk.uftb"),
                "marker": "second",
            }
            generate.merge_checkpoint_entry(path, "kjesterk.uftb", first)
            document = generate.merge_checkpoint_entry(
                path, "kjesterknightk.uftb", second)
            self.assertEqual(
                document["files"]["kjesterk.uftb"]["marker"], "first")
            self.assertEqual(
                document["files"]["kjesterknightk.uftb"]["marker"], "second")

    def test_commands_route_to_each_exact_solver_domain(self):
        records = generate._records()  # pylint: disable=protected-access
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            args = self._args(root)
            cases = {
                "kjesterk.uftb": (str(args.binary), "--solve-jester-information"),
                "kjestergiantk.uftb": (
                    str(args.binary), "--solve-jester-information"),
                "kghostk.uftb": (str(args.ghost_binary), "--input"),
                "kjesterjesterk.uftb": (
                    str(args.double_jester_binary),
                    "--lower-information-model-sha256"),
                "kjesterkjester.uftb": (
                    str(args.joint_jester_binary), "--semantics-id"),
                "kbishopghostk.uftb": (
                    str(args.ghost_extra_binary), "--solve-external"),
            }
            for filename, (binary, required) in cases.items():
                with self.subTest(filename=filename):
                    command = generate.solver_command(
                        args, records[filename], root / f"{filename}.ufiw",
                        "1" * 64,
                        generate.information.solver_model_fingerprint(filename))
                    self.assertEqual(command[0], binary)
                    self.assertIn(required, command)
                    self.assertIn("1" * 64, command)

            double = generate.solver_command(
                args, records["kjesterjesterk.uftb"], root / "double.ufiw",
                "1" * 64,
                generate.information.solver_model_fingerprint(
                    "kjesterjesterk.uftb"))
            lower_index = double.index("--lower-information-model-sha256") + 1
            self.assertEqual(
                double[lower_index],
                generate.information.solver_model_fingerprint("kjesterk.uftb"))
            self.assertNotEqual(
                double[lower_index],
                generate.information.solver_model_fingerprint(
                    "kjesterjesterk.uftb"))

    def test_primary_jester_secondary_routing_preserves_material_layout(self):
        records = generate._records()  # pylint: disable=protected-access
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            args = self._args(root)
            command = generate.solver_command(
                args, records["kjesterkknight.uftb"], root / "out.ufiw",
                "1" * 64,
                generate.information.solver_model_fingerprint(
                    "kjesterkknight.uftb"))
            self.assertEqual(command[command.index("--piece2") + 1], "knight")
            self.assertIn("--opposing", command)
            lower_model = command[
                command.index("--lower-information-model-sha256") + 1]
            self.assertEqual(
                lower_model,
                generate.information.solver_model_fingerprint("kjesterk.uftb"))

            giant = generate.solver_command(
                args, records["kjestergiantk.uftb"], root / "giant.ufiw",
                "1" * 64,
                generate.information.solver_model_fingerprint(
                    "kjestergiantk.uftb"))
            self.assertEqual(
                giant[giant.index("--lower-information-model-sha256") + 1],
                generate.information.solver_model_fingerprint("kjesterk.uftb"))
            self.assertNotEqual(
                generate.information.solver_model_fingerprint(
                    "kjestergiantk.uftb"),
                generate.information.solver_model_fingerprint("kjesterk.uftb"))

    def test_unsupported_material_never_routes_to_a_nearby_solver(self):
        records = generate._records()  # pylint: disable=protected-access
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(RuntimeError,
                                        "unsupported information class"):
                generate.solver_command(
                    self._args(root), records["kghostdragonk.uftb"],
                    root / "bad.ufiw", "1" * 64, "2" * 64)


if __name__ == "__main__":
    unittest.main()
