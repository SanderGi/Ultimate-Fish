import importlib.util
import argparse
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock


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
            reciprocal_ghost_extra_binary=root / "reciprocal-ghost-extra",
            reciprocal_ghost_extra_transitions=
                root / "reciprocal-ghost-extra-transitions",
            ghost_pair_binary=root / "ghost-pair",
            ghost_pair_transitions=root / "ghost-pair-transitions",
            jester_ghost_binary=root / "jester-ghost",
            jester_ghost_transitions=root / "jester-ghost-transitions",
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

    def test_arbitrary_reuse_requires_complete_authenticated_ufgg(self):
        source = "1" * 64
        model = "2" * 64
        payload = b"exact-correlated-roots"
        header = bytearray(928)
        header[:8] = b"UFGG1\0\0\0"
        struct.pack_into("<II", header, 8, 1, 928)
        struct.pack_into("<Q", header, 104, 928)
        struct.pack_into("<Q", header, 152, len(payload))
        header[160:224] = source.encode()
        header[224:288] = model.encode()
        header[288:352] = (
            generate.information.observation_model_fingerprint().encode())
        header[544:608] = (
            b"400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        header[800:864] = hashlib.sha256(payload).hexdigest().encode()
        semantics = b"correlated-unordered-pair-public-view-v1"
        header[864:864 + len(semantics)] = semantics
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "pair.ufgg"
            path.write_bytes(header + payload)
            self.assertTrue(generate.arbitrary_is_current(path, source, model))
            self.assertFalse(generate.arbitrary_is_current(
                path, "0" * 64, model))
            with path.open("r+b") as stream:
                stream.seek(928)
                stream.write(b"X")
            self.assertFalse(generate.arbitrary_is_current(path, source, model))

    def test_arbitrary_reuse_authenticates_ufjg_payload_and_dependencies(self):
        source, model = "1" * 64, "2" * 64
        payload = b"exact-product-roots"
        header = bytearray(816); header[:8] = b"UFJG1\0\0\0"
        struct.pack_into("<II", header, 8, 1, 816)
        struct.pack_into("<Q", header, 120, 816)
        struct.pack_into("<Q", header, 168, len(payload))
        header[176:240] = source.encode(); header[240:304] = model.encode()
        header[304:368] = generate.information.observation_model_fingerprint().encode()
        lower_jester = "4" * 64
        header[560:624] = lower_jester.encode()
        header[624:688] = b"400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b"
        header[688:752] = hashlib.sha256(payload).hexdigest().encode()
        semantics = b"king-jester-x-hidden-ghost-correlated-v1"
        header[752:752 + len(semantics)] = semantics
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "product.ufjg"
            path.write_bytes(header + payload)
            self.assertTrue(generate.arbitrary_is_current(
                path, source, model,
                lower_jester_overlay_sha256=lower_jester))
            self.assertFalse(generate.arbitrary_is_current(
                path, source, model,
                lower_jester_overlay_sha256="5" * 64))
            with path.open("r+b") as stream:
                stream.seek(560); stream.write(b"5" * 64)
            self.assertFalse(generate.arbitrary_is_current(
                path, source, model,
                lower_jester_overlay_sha256=lower_jester))
            with path.open("r+b") as stream:
                stream.seek(560); stream.write(lower_jester.encode())
            with path.open("r+b") as stream:
                stream.seek(816); stream.write(b"X")
            self.assertFalse(generate.arbitrary_is_current(
                path, source, model,
                lower_jester_overlay_sha256=lower_jester))

    def test_arbitrary_reuse_authenticates_reciprocal_ufgx(self):
        source, model = "1" * 64, "2" * 64
        payload = b"exact-reciprocal-public-extra-roots"
        header = bytearray(988)
        header[:8] = b"UFGX2\0\0\0"
        struct.pack_into("<II", header, 8, 2, 988)
        struct.pack_into("<Q", header, 92, 988)
        struct.pack_into("<Q", header, 148, len(payload))
        header[156:220] = source.encode()
        header[220:284] = model.encode()
        header[284:348] = (
            generate.information.observation_model_fingerprint().encode())
        header[348:412] = (
            b"400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
        header[860:924] = hashlib.sha256(payload).hexdigest().encode()
        semantics = b"fresh-maximal-public-view-v2:reciprocal-bishop-ghost"
        header[924:924 + len(semantics)] = semantics
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "reciprocal.ufgx"
            path.write_bytes(header + payload)
            self.assertTrue(generate.arbitrary_is_current(path, source, model))
            with path.open("r+b") as stream:
                stream.seek(348)
                stream.write(b"0" * 64)
            self.assertFalse(generate.arbitrary_is_current(path, source, model))
            with path.open("r+b") as stream:
                stream.seek(348)
                stream.write(
                    b"400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
                stream.seek(988)
                stream.write(b"X")
            self.assertFalse(generate.arbitrary_is_current(path, source, model))

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
            args.overlays.mkdir(parents=True)
            (args.overlays / "kjesterk.ufiw").write_bytes(b"lower")
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
                "kbishopkghost.uftb": (
                    str(args.reciprocal_ghost_extra_binary),
                    "--output-arbitrary"),
                "kghostghostk.uftb": (
                    str(args.ghost_pair_binary), "--output-arbitrary"),
                "kjesterghostk.uftb": (
                    str(args.jester_ghost_binary), "--output-arbitrary"),
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

            ghost_pair = generate.solver_command(
                args, records["kghostghostk.uftb"], root / "pair.ufiw",
                "204f4de6d0f9ff6da111d3d0c0a08c3562493cdec2130cc946b4eae7183012ee",
                generate.information.solver_model_fingerprint(
                    "kghostghostk.uftb"))
            self.assertEqual(ghost_pair[0], str(args.ghost_pair_binary))
            self.assertEqual(
                ghost_pair[ghost_pair.index("--lower-sidecar-sha256") + 1],
                "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b")
            self.assertEqual(
                ghost_pair[ghost_pair.index("--transition-prefix") + 1],
                str(args.ghost_pair_transitions))
            self.assertIn(str(args.overlays / "kghostghostk.ufgg"),
                          ghost_pair)

            reciprocal = generate.solver_command(
                args, records["kbishopkghost.uftb"], root / "reciprocal.ufiw",
                "0649b7859ba72c8534929a902f18b3ca1a74cd7d7b3713ac109633e83a85ac15",
                generate.information.solver_model_fingerprint(
                    "kbishopkghost.uftb"))
            self.assertEqual(
                reciprocal[0], str(args.reciprocal_ghost_extra_binary))
            self.assertEqual(
                reciprocal[reciprocal.index("--transition-prefix") + 1],
                str(args.reciprocal_ghost_extra_transitions))
            self.assertIn(str(args.overlays / "kbishopkghost.ufgx"),
                          reciprocal)

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

    def test_ghost_pair_route_rejects_stale_lower_sidecar(self):
        records = generate._records()  # pylint: disable=protected-access
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            lower = root / "tablebases" / "kghostk.ufgm"
            lower.parent.mkdir(parents=True)
            header = bytearray((ROOT / "tablebases" /
                                "kghostk.ufgm").read_bytes()[:320])
            header[-1] ^= 1
            lower.write_bytes(header)
            with mock.patch.object(generate, "ROOT", root):
                with self.assertRaisesRegex(RuntimeError,
                                            "stale lower UFGM SHA-256"):
                    generate.solver_command(
                        self._args(root), records["kghostghostk.uftb"],
                        root / "bad.ufiw", "1" * 64,
                        generate.information.solver_model_fingerprint(
                            "kghostghostk.uftb"))

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
