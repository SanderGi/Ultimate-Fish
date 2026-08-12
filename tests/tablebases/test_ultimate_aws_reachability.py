import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, TOOLS / filename)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


audit = load("ultimate_aws_reachability_audits",
             "run_ultimate_aws_reachability_audits.py")
receipts = load("ultimate_aws_reachability_import",
                "import_ultimate_aws_reachability_sidecars.py")
legacy = load("ultimate_legacy_reachability_import",
              "import_ultimate_legacy_reachability.py")
finalize = load("ultimate_aws_concrete_finalize",
                "finalize_ultimate_aws_concrete_class.py")
launch = load("ultimate_aws_concrete_launch",
              "launch_ultimate_aws_concrete_class.py")
primary_jester = load("ultimate_primary_jester_certification",
                      "certify_ultimate_primary_jester_archive.py")
jester_ghost_measure = load(
    "ultimate_jester_ghost_measurement",
    "run_ultimate_jester_ghost_measurement_aws.py")
ledger = load("ultimate_reachability_ledger",
              "update_ultimate_tablebase_ledger.py")


class UltimateAwsReachabilityTests(unittest.TestCase):
    def test_audit_command_is_idempotent_and_preserves_output(self):
        job = {
            "filename": "krookbishopk.uftb",
            "work_root": "/mnt/ultimatefish/job",
            "record": {"primary": "rook", "secondary": "bishop",
                       "opposing": False},
        }
        command = audit.audit_command(job, 7)
        self.assertIn("test -s /mnt/ultimatefish/job/certificates/"
                      "reachability-v1.txt", command)
        self.assertIn("--property=AllowedCPUs=7", command)
        self.assertIn("--piece rook --piece2 bishop", command)
        self.assertIn("--audit-reachability "
                      "/mnt/ultimatefish/job/outputs/krookbishopk.uftb",
                      command)
        self.assertNotIn("--generate", command)

    def test_opposing_audit_uses_exact_material_orientation(self):
        job = {
            "filename": "krookkbishop.uftb",
            "work_root": "/mnt/ultimatefish/job",
            "record": {"primary": "rook", "secondary": "bishop",
                       "opposing": True},
        }
        self.assertIn("--piece2 bishop --opposing --audit-reachability",
                      audit.audit_command(job, 1))

    def test_wld_cell_parenthesizes_only_unreachable_states(self):
        self.assertEqual(
            "8 (2) / 20 / 25 (5)",
            receipts.cell([0, 10, 20, 30], [0, 2, 0, 5]))
        with self.assertRaisesRegex(ValueError, "unknown states"):
            receipts.cell([1, 10, 20, 30], [0, 0, 0, 0])
        with self.assertRaisesRegex(ValueError, "exceeds"):
            receipts.cell([0, 10, 20, 30], [0, 11, 0, 0])

    def test_legacy_import_replaces_old_reachability_by_outcome(self):
        totals = legacy.components("8 (2) / 20 / 25 (5)")
        self.assertEqual([10, 20, 30], totals)
        audit_rows = legacy.unreachable(
            "reachability side 0 unknown 0 win 1 loss 2 draw 3\n"
            "reachability side 1 unknown 0 win 0 loss 4 draw 5\n")
        self.assertEqual([0, 1, 2, 3], audit_rows[0])
        self.assertEqual("9 (1) / 18 (2) / 27 (3)",
                         legacy.rendered(totals, audit_rows[0]))

    def test_single_class_finalizer_conserves_and_parenthesizes_wld(self):
        text = (
            "reachability side 0 unknown 0 win 2 loss 0 draw 5\n"
            "reachability side 1 unknown 0 win 1 loss 4 draw 0\n"
            "reachability_total side 0 unknown 0 win 10 loss 20 draw 30\n"
            "reachability_total side 1 unknown 0 win 11 loss 22 draw 33\n")
        omitted = finalize.counts(finalize.AUDIT, text)
        totals = finalize.counts(finalize.TOTAL, text)
        self.assertEqual("8 (2) / 20 / 25 (5)",
                         finalize.render(totals[0], omitted[0]))
        self.assertEqual("10 (1) / 18 (4) / 33",
                         finalize.render(totals[1], omitted[1]))

    def test_single_class_finalizer_resolves_supported_record(self):
        record = finalize.record_for("kknightkprince.uftb")
        self.assertEqual("knight", record["primary"])
        self.assertEqual("prince", record["secondary"])
        self.assertTrue(record["opposing"])

    def test_single_class_finalizer_coalesces_requested_penguin_alias(self):
        record = finalize.record_for("kbishopkpenguin.uftb")
        self.assertEqual("bishop", record["primary"])
        self.assertEqual("penguin", record["secondary"])
        self.assertTrue(record["opposing"])
        self.assertEqual(303_663_360, record["states"])

    def test_single_class_finalizer_rejects_certified_before_remote_upload(self):
        with tempfile.TemporaryDirectory() as directory:
            readme = Path(directory) / "README.md"
            readme.write_text("unused")
            row = SimpleNamespace(filename="kknightprincek.uftb",
                                  result_kind="concrete", status="certified")
            with mock.patch.object(finalize.ledger, "entries", return_value=[row]):
                with self.assertRaisesRegex(ValueError, "already CERTIFIED"):
                    finalize.preflight(SimpleNamespace(
                        filename="kknightprincek.uftb", readme=readme))

    def test_single_class_launcher_preflights_exact_index_before_systemd(self):
        args = SimpleNamespace(
            source_root="/mnt/source", dependencies="/mnt/dependencies",
            dependency_manifest="/mnt/dependencies/manifest.json",
            work_directory="/mnt/work", wave=0, index=12,
            scratch_limit=100, resident_limit=90,
            reverse_edge_bytes_limit=80, minimum_free_bytes=70,
            monitor_interval=10, s3_prefix="s3://bucket/results",
            unit="ultimatefish-wave0-12", cpu=3,
            expected_model_sha256="a" * 64)
        commands = launch.remote_commands(
            args, {"filename": "kexample.uftb"})
        self.assertIn("--range-begin 12 --range-end 13", commands[4])
        self.assertIn("kexample.uftb", commands[5])
        self.assertIn("--property=AllowedCPUs=3", commands[6])
        self.assertIn("--scratch-limit 100", commands[6])
        self.assertLess(commands.index(commands[4]), commands.index(commands[6]))

    def test_single_class_launcher_uses_ledger_assignment_syntax(self):
        source = (TOOLS / "launch_ultimate_aws_concrete_class.py").read_text()
        self.assertIn("record[\"filename\"]}=computing", source)
        self.assertNotIn("[(str(record[\"filename\"]), \"computing\")]", source)

    def test_primary_jester_outcome_cell_parenthesizes_unreachable(self):
        entry = {"sides": {"first": {"outcomes": {
            "win": {"legal": 8, "unreachable": 2},
            "loss": {"legal": 20, "unreachable": 0},
            "draw": {"legal": 25, "unreachable": 5},
        }}}}
        self.assertEqual("8 (2) / 20 / 25 (5)",
                         primary_jester.outcome_cell(entry, "first"))

    def test_jester_ghost_measurement_accepts_explicit_larger_bdd_gate(self):
        jester_ghost_measure.validate_upper_bdd_gates(1_000_000_000, 1 << 31)
        with self.assertRaisesRegex(ValueError, "ProductRobdd"):
            jester_ghost_measure.validate_upper_bdd_gates(
                1_000_000_000, (1 << 31) - 1)

    def test_all_jester_and_ghost_material_requires_information(self):
        # Checker+Jester is outside the legacy information-v2 filename set.
        # It must still fail closed instead of exposing a concrete-world WLD.
        hidden = {"primary": "jester", "secondary": "checker"}
        public = {"primary": "rook", "secondary": "checker"}
        self.assertTrue(ledger._requires_information(hidden))
        self.assertFalse(ledger._requires_information(public))
        self.assertTrue(receipts.hidden(hidden))
        self.assertFalse(receipts.hidden(public))

    def test_supervision_import_separates_hidden_dependency(self):
        base = {
            "filename": "krookbishopk.uftb", "totals": [[0, 2, 3, 5]] * 2,
            "unreachable": [[0, 0, 1, 0]] * 2,
            "table_archive": {"bucket": "bucket", "key": "table", "version_id": "t",
                              "sha256": "a" * 64, "bytes": 10},
            "wave_certificate": {"key": "wave", "version_id": "w",
                                 "sha256": "b" * 64, "bytes": 20},
            "reachability_s3": {"key": "reach", "version_id": "r",
                                "sha256": "c" * 64, "bytes": 30},
        }
        hidden = dict(base, filename="kjestercheckerk.uftb")
        configuration = {"jobs": [
            {"id": "public", "ledger_files": [base["filename"]]},
            {"id": "hidden", "ledger_files": [hidden["filename"]],
             "ledger_certifies": True, "ledger_results": {"stale": {}}},
        ]}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "supervision.json"
            path.write_text(json.dumps(configuration))
            self.assertEqual(2, receipts.update_supervision([base, hidden], path))
            jobs = json.loads(path.read_text())["jobs"]
        self.assertEqual(3, len(jobs[0]["s3_certificates"]))
        self.assertTrue(jobs[0]["ledger_certifies"])
        self.assertIn(base["filename"], jobs[0]["ledger_results"])
        self.assertFalse(jobs[1]["ledger_certifies"])
        self.assertNotIn("ledger_results", jobs[1])


if __name__ == "__main__":
    unittest.main()
