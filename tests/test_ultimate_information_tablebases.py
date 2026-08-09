import copy
import hashlib
import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "ultimate_information_tablebases",
    TOOLS / "ultimate_information_tablebases.py")
assert SPEC and SPEC.loader
info = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = info
SPEC.loader.exec_module(info)
ORIGINAL_SOLVER_FINGERPRINT = info.solver_model_fingerprint
README_SPEC = importlib.util.spec_from_file_location(
    "update_ultimate_tablebase_readme",
    TOOLS / "update_ultimate_tablebase_readme.py")
assert README_SPEC and README_SPEC.loader
readme = importlib.util.module_from_spec(README_SPEC)
sys.modules[README_SPEC.name] = readme
README_SPEC.loader.exec_module(readme)


class InformationTablebaseSchemaTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "tablebases").mkdir()
        records = info.affected_inventory(root=self.root, require_files=False)
        self.assertEqual(len(records), 45)
        self.document = {
            "schema_version": info.SCHEMA_VERSION,
            "semantics": dict(info.SEMANTICS),
            "inventory_sha256": info.inventory_fingerprint(records),
            "solver": {
                "name": "test-exact-information-solver",
                "version": info.SOLVER_VERSION,
                "observation_model_sha256": info.observation_model_fingerprint(),
                "exhaustive": True,
                "belief_cap": None,
            },
            "files": {},
        }
        for index, record in enumerate(records):
            filename = str(record["filename"])
            payload = f"logical table {index}: {filename}\n".encode()
            (self.root / "tablebases" / filename).write_bytes(payload)
            states = info.states_per_side(record)
            side = {
                "outcomes": {
                    "win": {"legal": 0, "unreachable": 0},
                    "loss": {"legal": 0, "unreachable": 0},
                    "draw": {"legal": states, "unreachable": 0},
                },
                "certificate": {
                    "information_sets": 1,
                    "concrete_realizations": states,
                    "legal_realizations": states,
                    "unreachable_realizations": 0,
                    "unresolved_information_sets": 0,
                    "partition_residual": 0,
                    "conservation_residual": 0,
                    "bellman_residual": 0,
                    "rank_residual": 0,
                    "observation_residual": 0,
                },
            }
            self.document["files"][filename] = {
                "tablebase_sha256": hashlib.sha256(payload).hexdigest(),
                "solver_model_sha256": self._future_solver_fingerprint(filename),
                "states_per_side": states,
                "sides": {"first": copy.deepcopy(side),
                          "second": copy.deepcopy(side)},
            }

    def tearDown(self):
        self.temp.cleanup()

    def assert_invalid(self, mutate, pattern):
        document = copy.deepcopy(self.document)
        mutate(document)
        with mock.patch.object(
                info, "solver_model_fingerprint",
                side_effect=self._future_solver_fingerprint):
            with self.assertRaisesRegex(info.SummaryValidationError, pattern):
                info.validate_summary(document, root=self.root)

    @staticmethod
    def _future_solver_fingerprint(filename, **kwargs):
        try:
            return ORIGINAL_SOLVER_FINGERPRINT(filename, **kwargs)
        except info.SummaryValidationError as error:
            if "unsupported information class" not in str(error):
                raise
            # Synthetic schema tests model a future material-correct solver.
            # Production validation never receives this test-only sentinel.
            return "f" * 64

    def validate_future_complete(self, document):
        with mock.patch.object(
                info, "solver_model_fingerprint",
                side_effect=self._future_solver_fingerprint):
            info.validate_summary(document, root=self.root)

    def test_inventory_is_exactly_all_stored_jester_or_ghost_rows(self):
        records = info.affected_inventory()
        self.assertEqual(len(records), 45)
        self.assertEqual(tuple(record["filename"] for record in records),
                         info.AFFECTED_FILENAMES)
        for record in records:
            pieces = {record["primary"], record["secondary"]}
            self.assertTrue(pieces & {"jester", "ghost"})
            self.assertTrue((ROOT / "tablebases" / record["filename"]).exists())

    def test_solver_inventory_classifies_all_45_exactly_once(self):
        supported = info.supported_solver_inventory()
        unsupported = info.unsupported_solver_inventory()
        names = [filename for filename, _ in supported] + list(unsupported)
        self.assertEqual(len(supported), 34)
        self.assertEqual(len(unsupported), 11)
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(set(names), set(info.AFFECTED_FILENAMES))
        counts = {}
        for filename, domain in supported:
            self.assertEqual(info.solver_domain(filename), domain)
            counts[domain] = counts.get(domain, 0) + 1
        self.assertEqual(counts, {
            "primary-jester": 23,
            "primary-jester-giant": 2,
            "ghost": 1,
            "double-jester": 1,
            "joint-jester": 1,
            "bishop-ghost": 1,
            "reciprocal-bishop-ghost": 1,
            "dragon-ghost-same": 1,
            "dragon-ghost-opposing": 1,
            "ghost-pair": 1,
            "jester-ghost": 1,
        })

    def test_unsupported_and_unknown_solver_domains_fail_closed(self):
        filename = info.unsupported_solver_inventory()[0]
        with self.assertRaisesRegex(
                info.SummaryValidationError, "unsupported information class"):
            info.solver_model_fingerprint(filename)
        with self.assertRaisesRegex(
                info.SummaryValidationError, "unknown information class"):
            info.solver_model_fingerprint("not-a-table.uftb")
        with self.assertRaisesRegex(
                info.SummaryValidationError, "unsupported information class"):
            info.validate_summary(self.document, root=self.root)

    def test_primary_jester_transitive_probe_dependencies_are_complete(self):
        forcing = {
            "queen": "kqk.uftb", "rook": "krk.uftb",
            "bomb": "kbombk.uftb", "ninja": "kninjak.uftb",
            "parasite": "kparasitek.uftb", "giant": "kgiantk.uftb",
            "dragon": "kdragonk.uftb",
        }
        records = {str(record["filename"]): record for record in
                   info.affected_inventory(root=ROOT, require_files=False)}
        for filename in (*info.PRIMARY_JESTER_FILENAMES,
                         *info.PRIMARY_JESTER_GIANT_FILENAMES):
            dependencies = info.solver_concrete_dependencies(filename)
            if filename == "kjesterk.uftb":
                self.assertEqual(dependencies, ())
                continue
            secondary = str(records[filename]["secondary"])
            expected = ("kjesterk.uftb",) + (
                (forcing[secondary],) if secondary in forcing else ())
            self.assertEqual(dependencies, expected)
            for dependency in dependencies:
                self.assertTrue((ROOT / "tablebases" / dependency).exists())

        # The owner-Bomb AWS run originally built millions of information
        # nodes before discovering this unstaged lower table.
        self.assertEqual(
            info.solver_concrete_dependencies("kjesterbombk.uftb"),
            ("kjesterk.uftb", "kbombk.uftb"))
        self.assertEqual(
            info.solver_concrete_dependencies("kjesterkbomb.uftb"),
            ("kjesterk.uftb", "kbombk.uftb"))
        self.assertEqual(
            info.solver_sidecar_dependencies("kghostghostk.uftb"),
            ("kghostk.ufgm",))
        self.assertEqual(
            info.solver_concrete_dependencies("kjesterghostk.uftb"),
            ("kjesterk.uftb",))
        self.assertEqual(
            info.solver_sidecar_dependencies("kjesterghostk.uftb"),
            ("kghostk.ufgm",))
        self.assertEqual(
            info.solver_sidecar_dependencies("kbishopkghost.uftb"),
            ("kghostk.ufgm",))
        self.assertEqual(
            info.solver_sidecar_dependencies("kghostdragonk.uftb"),
            ("kghostk.ufgm",))
        self.assertEqual(
            info.solver_sidecar_dependencies("kghostkdragon.uftb"),
            ("kghostk.ufgm",))
        self.assertEqual(
            info.solver_concrete_dependencies("kghostdragonk.uftb"),
            ("kdragonk.uftb",))
        self.assertEqual(
            info.solver_concrete_dependencies("kghostkdragon.uftb"),
            ("kdragonk.uftb",))
        self.assertEqual(len(info.concrete_tablebase_model_fingerprint(
            "kdragonk.uftb")), 64)
        self.assertTrue((ROOT / "tablebases" / "kghostk.ufgm").exists())

    def test_solver_fingerprints_are_isolated_by_implementation_domain(self):
        representatives = {
            "primary-jester": "kjesterk.uftb",
            "primary-jester-giant": "kjestergiantk.uftb",
            "ghost": "kghostk.uftb",
            "double-jester": "kjesterjesterk.uftb",
            "joint-jester": "kjesterkjester.uftb",
            "bishop-ghost": "kbishopghostk.uftb",
            "reciprocal-bishop-ghost": "kbishopkghost.uftb",
            "dragon-ghost-same": "kghostdragonk.uftb",
            "dragon-ghost-opposing": "kghostkdragon.uftb",
            "ghost-pair": "kghostghostk.uftb",
            "jester-ghost": "kjesterghostk.uftb",
        }
        with tempfile.TemporaryDirectory() as directory:
            source_root = Path(directory)
            all_sources = {
                path for paths in info.SOLVER_DOMAIN_SOURCES.values()
                for path in paths
            }
            for source in all_sources:
                destination = source_root / source.relative_to(ROOT)
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(source.read_bytes())
            before = {
                domain: info.solver_model_fingerprint(filename, root=source_root)
                for domain, filename in representatives.items()
            }
            self.assertNotEqual(before["primary-jester"],
                                before["primary-jester-giant"])

            ghost_source = (source_root /
                            info.GHOST_SOLVER_SOURCES[-1].relative_to(ROOT))
            ghost_source.write_bytes(ghost_source.read_bytes() + b"\n// drift\n")
            after_ghost = {
                domain: info.solver_model_fingerprint(filename, root=source_root)
                for domain, filename in representatives.items()
            }
            self.assertNotEqual(after_ghost["ghost"], before["ghost"])
            self.assertNotEqual(after_ghost["bishop-ghost"],
                                before["bishop-ghost"])
            self.assertNotEqual(after_ghost["reciprocal-bishop-ghost"],
                                before["reciprocal-bishop-ghost"])
            self.assertNotEqual(after_ghost["ghost-pair"],
                                before["ghost-pair"])
            self.assertNotEqual(after_ghost["jester-ghost"],
                                before["jester-ghost"])
            self.assertNotEqual(after_ghost["dragon-ghost-same"],
                                before["dragon-ghost-same"])
            self.assertNotEqual(after_ghost["dragon-ghost-opposing"],
                                before["dragon-ghost-opposing"])
            for domain in ("primary-jester", "primary-jester-giant",
                           "double-jester", "joint-jester"):
                self.assertEqual(after_ghost[domain], before[domain])

            shared_source = (source_root /
                             info.SHARED_SOLVER_SOURCES[0].relative_to(ROOT))
            shared_source.write_bytes(shared_source.read_bytes() + b"\n// drift\n")
            after_shared = {
                domain: info.solver_model_fingerprint(filename, root=source_root)
                for domain, filename in representatives.items()
            }
            for domain in representatives:
                self.assertNotEqual(after_shared[domain], after_ghost[domain])

            probe_source = (source_root / "src" / "ultimate" /
                            "tablebase_probe.cpp")
            probe_source.write_bytes(
                probe_source.read_bytes() + b"\n// probe drift\n")
            after_probe = {
                domain: info.solver_model_fingerprint(filename, root=source_root)
                for domain, filename in representatives.items()
            }
            self.assertNotEqual(after_probe["primary-jester"],
                                after_shared["primary-jester"])
            self.assertNotEqual(after_probe["primary-jester-giant"],
                                after_shared["primary-jester-giant"])
            for domain in ("ghost", "double-jester", "joint-jester",
                           "bishop-ghost", "reciprocal-bishop-ghost",
                           "ghost-pair", "jester-ghost",
                           "dragon-ghost-same", "dragon-ghost-opposing"):
                self.assertEqual(after_probe[domain], after_shared[domain])

            pair_relative = info.GHOST_PAIR_SOLVER_SOURCES[-1].relative_to(ROOT)
            pair_source = source_root / pair_relative
            pair_source.write_bytes(pair_source.read_bytes() +
                                    b"\n// pair-only drift\n")
            after_pair = {
                domain: info.solver_model_fingerprint(filename,
                                                      root=source_root)
                for domain, filename in representatives.items()
            }
            self.assertNotEqual(after_pair["ghost-pair"],
                                after_probe["ghost-pair"])
            for domain in representatives:
                if domain != "ghost-pair":
                    self.assertEqual(after_pair[domain], after_probe[domain])

            reciprocal_relative = (
                info.RECIPROCAL_BISHOP_GHOST_SOLVER_SOURCES[-1]
                .relative_to(ROOT))
            reciprocal_source = source_root / reciprocal_relative
            reciprocal_source.write_bytes(
                reciprocal_source.read_bytes() + b"\n// reciprocal-only drift\n")
            after_reciprocal = {
                domain: info.solver_model_fingerprint(filename,
                                                      root=source_root)
                for domain, filename in representatives.items()
            }
            self.assertNotEqual(
                after_reciprocal["reciprocal-bishop-ghost"],
                after_pair["reciprocal-bishop-ghost"])
            for domain in representatives:
                if domain != "reciprocal-bishop-ghost":
                    self.assertEqual(after_reciprocal[domain],
                                     after_pair[domain])

        self.assertEqual(
            info.solver_model_fingerprint("kbishopghostk.uftb"),
            "d59736789155c52d9b697f6e3f05c2fd0c565beba5d5cce49e3648719289da91")
        self.assertEqual(
            info.solver_model_fingerprint("kghostghostk.uftb"),
            "2280445ed5c6f024b0cd8d00fca45dd48e5359cd591d4e3d28ee0259f74ff286")

    def test_semantics_documents_fresh_maximal_public_view(self):
        self.assertEqual(info.SEMANTICS["id"],
                         "fresh-maximal-public-view-v2")
        self.assertEqual(info.SEMANTICS["outcome_weighting"],
                         "concrete-realizations")
        self.assertIn("all-causally-reachable",
                      info.SEMANTICS["hidden_ghosts"])
        self.assertIn("king-jester", info.SEMANTICS["royal_identity"])
        self.assertIn("mover-private",
                      info.SEMANTICS["pre_decision_legal_markers"])
        self.assertEqual(info.SEMANTICS["legal_marker_observer"],
                         "side-to-move-only")
        self.assertIn("owned-private", info.SEMANTICS["belief_update"])

    def test_rejects_every_v1_catalog_contract(self):
        self.assert_invalid(
            lambda document: document.update({"schema_version": 1}),
            r"schema_version.*expected 2")
        self.assert_invalid(
            lambda document: document["semantics"].update(
                {"id": "fresh-maximal-public-view-v1"}),
            r"semantics.*fresh-maximal-public-view-v2")
        self.assert_invalid(
            lambda document: document["solver"].update({"version": "1"}),
            r"solver.version.*expected 2")

    def test_model_hashes_bind_private_legal_marker_semantics(self):
        # Reconstruct the v1 source-only digest.  Even unchanged C++ bytes must
        # not authenticate a v2 catalog or UFIW2 overlay.
        digest = hashlib.sha256()
        for path in info.PRIMARY_JESTER_SOLVER_SOURCES:
            relative = path.relative_to(ROOT).as_posix().encode()
            payload = path.read_bytes()
            digest.update(len(relative).to_bytes(4, "little"))
            digest.update(relative)
            digest.update(len(payload).to_bytes(8, "little"))
            digest.update(payload)
        self.assertNotEqual(
            info.solver_model_fingerprint("kjesterk.uftb"),
            digest.hexdigest())
        self.assertEqual(len(info.semantics_fingerprint()), 64)

    def test_single_extra_legacy_codec_keeps_both_reflections(self):
        records = {record["filename"]: record for record in
                   info.affected_inventory(root=self.root, require_files=False)}
        self.assertEqual(info.states_per_side(records["kjesterk.uftb"]),
                         492_960)
        self.assertEqual(info.states_per_side(records["kghostk.uftb"]),
                         985_920)
        self.assertEqual(
            info.states_per_side(records["kjesterknightk.uftb"]),
            int(records["kjesterknightk.uftb"]["states"]) // 2)

    def test_complete_exact_sha_bound_document_validates(self):
        self.validate_future_complete(self.document)

    def test_rejects_any_belief_cap(self):
        self.assert_invalid(
            lambda document: document["solver"].update({"belief_cap": 64}),
            "belief_cap.*must be null")

    def test_rejects_non_exhaustive_solver(self):
        self.assert_invalid(
            lambda document: document["solver"].update({"exhaustive": False}),
            "exhaustive.*must be true")

    def test_rejects_stale_observation_projection(self):
        self.assert_invalid(
            lambda document: document["solver"].update(
                {"observation_model_sha256": "0" * 64}),
            "observation_model_sha256.*expected")

    def test_rejects_stale_solver_or_move_generation(self):
        filename = info.AFFECTED_FILENAMES[0]
        self.assert_invalid(
            lambda document: document["files"][filename].update(
                {"solver_model_sha256": "0" * 64}),
            "solver_model_sha256.*expected")

    def test_requires_all_45_rows(self):
        missing = info.AFFECTED_FILENAMES[-1]
        self.assert_invalid(
            lambda document: document["files"].pop(missing),
            "coverage mismatch")

    def test_rejects_stale_tablebase_sha(self):
        filename = info.AFFECTED_FILENAMES[0]
        self.assert_invalid(
            lambda document: document["files"][filename].update(
                {"tablebase_sha256": "0" * 64}),
            "source mismatch")

    def test_enforces_per_side_conservation(self):
        filename = info.AFFECTED_FILENAMES[0]
        self.assert_invalid(
            lambda document: document["files"][filename]["sides"]["second"]
                ["outcomes"]["draw"].update(
                    {"legal": document["files"][filename]["states_per_side"] - 1}),
            "do not conserve")

    def test_rejects_unresolved_information_sets(self):
        filename = info.AFFECTED_FILENAMES[0]
        self.assert_invalid(
            lambda document: document["files"][filename]["sides"]["first"]
                ["certificate"].update({"unresolved_information_sets": 1}),
            "unresolved_information_sets.*expected 0")

    def test_information_set_partition_cannot_exceed_realizations(self):
        filename = info.AFFECTED_FILENAMES[0]
        self.assert_invalid(
            lambda document: document["files"][filename]["sides"]["first"]
                ["certificate"].update({
                    "information_sets":
                        document["files"][filename]["states_per_side"] + 1}),
            "information_sets.*cannot exceed")

    def test_all_certificate_residuals_must_be_zero(self):
        filename = info.AFFECTED_FILENAMES[0]
        for residual in info.CERTIFICATE_RESIDUALS:
            with self.subTest(residual=residual):
                self.assert_invalid(
                    lambda document, field=residual:
                        document["files"][filename]["sides"]["first"]
                        ["certificate"].update({field: 1}),
                    f"{residual}.*expected 0")

    def test_inventory_fingerprint_is_order_and_layout_bound(self):
        records = list(info.affected_inventory(root=self.root, require_files=False))
        digest = info.inventory_fingerprint(records)
        records.reverse()
        self.assertNotEqual(info.inventory_fingerprint(records), digest)
        records.reverse()
        records[0] = dict(records[0], states=int(records[0]["states"]) + 2)
        self.assertNotEqual(info.inventory_fingerprint(records), digest)

    def _set_side_counts(self, filename, side, legal, unreachable):
        entry = self.document["files"][filename]["sides"][side]
        for index, outcome in enumerate(info.OUTCOMES):
            entry["outcomes"][outcome] = {
                "legal": legal[index], "unreachable": unreachable[index],
            }
        certificate = entry["certificate"]
        certificate["legal_realizations"] = sum(legal)
        certificate["unreachable_realizations"] = sum(unreachable)

    def test_readme_uses_exact_public_legal_and_unreachable_counts(self):
        filename = "kjesterjesterk.uftb"
        states = self.document["files"][filename]["states_per_side"]
        self._set_side_counts(
            filename, "first", (5, 7, states - 18), (2, 3, 1))
        self.validate_future_complete(self.document)
        totals = [[0, 999, 888, 777], [0, 0, 0, states]]
        # The exact admission additionally rejects one win, two losses, and one
        # draw beyond the older concrete audit.
        concrete_unreachable = [[0, 1, 1, 0], [0, 0, 0, 0]]
        first, second = readme.summary_cells(
            filename, totals, concrete_unreachable, self.document)
        self.assertEqual(first, f"5 (2) / 7 (3) / {states - 18:,} (1)")
        self.assertEqual(second, f"0 / 0 / {states:,}")

    def test_readme_never_falls_back_for_missing_affected_row(self):
        filename = "kjesterjesterk.uftb"
        self.document["files"].pop(filename)
        with self.assertRaisesRegex(
                info.SummaryValidationError,
                "missing validated public-information result"):
            readme.summary_cells(
                filename, [[0] * 4, [0] * 4], [[0] * 4, [0] * 4],
                self.document)

    def test_readme_rejects_native_unreachable_state_admitted_by_solver(self):
        filename = "kjesterjesterk.uftb"
        states = self.document["files"][filename]["states_per_side"]
        self._set_side_counts(filename, "first", (0, 0, states - 6),
                              (2, 3, 1))
        self.validate_future_complete(self.document)
        with self.assertRaisesRegex(
                info.SummaryValidationError,
                "native-audited unreachable"):
            readme.public_information_cell(
                self.document, filename, "first", [0, 3, 3, 0])

    def test_unaffected_readme_row_keeps_concrete_summary(self):
        totals = [[0, 10, 20, 30], [0, 40, 50, 60]]
        unreachable = [[0, 1, 2, 3], [0, 4, 5, 6]]
        self.assertEqual(
            readme.summary_cells("krk.uftb", totals, unreachable, {}),
            ("9 (1) / 18 (2) / 27 (3)",
             "36 (4) / 45 (5) / 54 (6)"))

    def test_catalog_timestamp_invalidates_cached_rows(self):
        path = self.root / "tablebases" / "information_summary.json"
        path.write_text("{}")
        future = time.time_ns() + 10_000_000_000
        os.utime(path, ns=(future, future))
        self.assertGreaterEqual(readme.dependency_mtime(path),
                                path.stat().st_mtime_ns)


if __name__ == "__main__":
    unittest.main()
