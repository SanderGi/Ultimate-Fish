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
preservation_resume = load(
    "ultimate_aws_concrete_preservation_resume",
    "resume_ultimate_concrete_preservation_aws.py")
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
        self.assertIn("reachability-v2.txt", command)
        self.assertIn(
            "reachability_binding filename krookbishopk.uftb "
            "output_sha256 $table_sha", command)
        self.assertIn("INVALID_BINDING:krookbishopk.uftb", command)
        self.assertIn("--property=AllowedCPUs=7", command)
        self.assertIn("--piece rook --piece2 bishop", command)
        self.assertIn("--audit-reachability "
                      "/mnt/ultimatefish/job/outputs/krookbishopk.uftb",
                      command)
        self.assertNotIn("--generate", command)

    def test_single_class_reused_sidecar_requires_table_binding(self):
        args = SimpleNamespace(
            filename="krookbishopk.uftb",
            audit_binary=None,
            reuse_reachability_sidecar=True,
            unit=None,
            work_directory="/mnt/work",
            s3_prefix="results/current",
            bucket="bucket",
            region="us-west-2",
        )
        command = finalize.remote_script(
            args, {"primary": "rook", "secondary": "bishop",
                   "opposing": False})
        self.assertIn(
            "reachability_binding filename krookbishopk.uftb "
            "output_sha256 $table_sha", command)
        self.assertIn("filename=krookbishopk.uftb", command)
        self.assertIn("output_sha256=$table_sha", command)
        self.assertIn('--version-id "$side_version"', command)
        self.assertIn('cat "$work/reachability-head.json"', command)

    def test_single_class_finalizer_can_use_idle_audit_cpus(self):
        args = SimpleNamespace(
            filename="kcopycatangelk.uftb",
            audit_binary="/mnt/auditor/ultimate_tablebase",
            audit_workers=7,
            reuse_reachability_sidecar=False,
            unit=None,
            work_directory="/mnt/work",
            s3_prefix="results/current",
            bucket="bucket",
            region="us-west-2",
        )
        command = finalize.remote_script(
            args, {"primary": "copycat", "secondary": "angel",
                   "opposing": False})
        self.assertIn(
            "/mnt/auditor/ultimate_tablebase --piece copycat --workers 7 "
            "--checkpoint-every 0 --piece2 angel --audit-reachability",
            command)

    def test_single_class_finalizer_uses_pinned_s3_sidecar_size(self):
        source = (TOOLS / "finalize_ultimate_aws_concrete_class.py").read_text()
        self.assertIn('"size": int(side_put["ContentLength"])', source)
        self.assertNotIn('"size": len(audit_text.encode("utf-8"))', source)

    def test_opposing_audit_uses_exact_material_orientation(self):
        job = {
            "filename": "krookkbishop.uftb",
            "work_root": "/mnt/ultimatefish/job",
            "record": {"primary": "rook", "secondary": "bishop",
                       "opposing": True},
        }
        self.assertIn("--piece2 bishop --opposing --audit-reachability",
                      audit.audit_command(job, 1))

    def test_collection_rechecks_sidecar_filename_and_output_sha(self):
        job = {
            "filename": "krookbishopk.uftb",
            "work_root": "/mnt/ultimatefish/job",
        }
        command = audit.compact_collection_command(job)
        self.assertIn(
            "assert q[0]=='reachability_binding filename '+z['filename']",
            command)
        self.assertIn("+' output_sha256 '+z['output']['sha256']", command)

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

    def test_single_class_finalizer_cross_checks_explicit_reachability_semantics(self):
        text = (
            "reachability side 0 unknown 0 win 2 loss 0 draw 5\n"
            "reachability side 1 unknown 0 win 1 loss 4 draw 0\n"
            "reachability_total side 0 unknown 0 win 10 loss 20 draw 30\n"
            "reachability_total side 1 unknown 0 win 11 loss 22 draw 33\n"
            "reachability_excluded side 0 unknown 0 win 2 loss 0 draw 5\n"
            "reachability_excluded side 1 unknown 0 win 1 loss 4 draw 0\n"
            "reachability_admitted side 0 unknown 0 win 8 loss 20 draw 25\n"
            "reachability_admitted side 1 unknown 0 win 10 loss 18 draw 33\n")
        totals = finalize.counts(finalize.TOTAL, text)
        omitted = finalize.counts(finalize.AUDIT, text)
        finalize.validate_explicit_reachability_semantics(
            text, totals, omitted)
        with self.assertRaisesRegex(ValueError, "admission residual"):
            finalize.validate_explicit_reachability_semantics(
                text.replace("win 8 loss 20", "win 9 loss 20"),
                totals, omitted)

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

    def test_single_class_finalizer_swaps_encoded_alias_side_rows(self):
        record = finalize.encoded_record_for("kdragonkpenguin.uftb")
        self.assertEqual("kpenguinkdragon.uftb", record["filename"])
        self.assertEqual((1, 0), finalize.ledger_side_order(
            "kdragonkpenguin.uftb"))
        self.assertEqual((0, 1), finalize.ledger_side_order(
            "kpenguinksniper.uftb"))

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

    def test_single_class_finalizer_releases_supervision_reservation(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "supervision.json"
            config.write_text(json.dumps({"jobs": [{
                "id": "job", "unit": "unit.service",
                "ledger_files": ["krookbishopk.uftb"],
                "ledger_certifies": False,
                "s3_certificates": [{
                    "bucket": "bucket", "key": "source", "version_id": "1",
                    "sha256": "a" * 64, "size": 1,
                }],
            }]}))
            args = SimpleNamespace(
                filename="krookbishopk.uftb", unit="unit.service",
                supervision_config=config)
            value = {"result_kind": "concrete", "first": "1 / 2 / 3",
                     "second": "4 / 5 / 6", "reachability": "7; 8",
                     "storage": "pinned"}
            result = {
                "bucket": "bucket",
                "key": "results/krookbishopk/krookbishopk-result.tar.zst",
                "version_id": "2", "sha256": "b" * 64, "size": 2}
            finalize.update_supervision(args, value, [result, result])
            job = json.loads(config.read_text())["jobs"][0]
            self.assertTrue(job["ledger_certifies"])
            self.assertTrue(job["s3_only_certified"])
            self.assertEqual(value, job["ledger_results"][args.filename])
            self.assertEqual(2, len(job["s3_certificates"]))
            self.assertEqual(
                ["results/krookbishopk/krookbishopk-result.tar.zst"],
                job["result_certificate_keys"][args.filename])

    def test_finalizer_retires_multi_result_job_only_after_every_result(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "supervision.json"
            config.write_text(json.dumps({"jobs": [{
                "id": "job", "unit": "unit.service",
                "ledger_files": ["krookbishopk.uftb", "krookknightk.uftb"],
                "ledger_certifies": False, "s3_certificates": [],
            }]}))
            args = SimpleNamespace(
                filename="krookbishopk.uftb", unit="unit.service",
                supervision_config=config)
            value = {"result_kind": "concrete", "first": "1 / 2 / 3",
                     "second": "4 / 5 / 6", "reachability": "7; 8",
                     "storage": "pinned"}
            result = {
                "bucket": "bucket",
                "key": "results/krookbishopk/krookbishopk-result.tar.zst",
                "version_id": "2", "sha256": "b" * 64, "size": 2}
            finalize.update_supervision(args, value, [result])
            job = json.loads(config.read_text())["jobs"][0]
            self.assertNotIn("s3_only_certified", job)
            self.assertEqual([], job["result_certificate_keys"]
                             ["krookknightk.uftb"])

    def test_hidden_dependency_preservation_is_explicit(self):
        with tempfile.TemporaryDirectory() as directory:
            readme = Path(directory) / "README.md"
            readme.write_text("unused")
            row = SimpleNamespace(filename="kjesterprincek.uftb",
                                  result_kind="information required",
                                  status="computing")
            with mock.patch.object(finalize.ledger, "entries", return_value=[row]):
                finalize.preflight(SimpleNamespace(
                    filename="kjesterprincek.uftb", readme=readme,
                    preserve_information_dependency=True))
                with self.assertRaisesRegex(ValueError, "cannot be concrete-certified"):
                    finalize.preflight(SimpleNamespace(
                        filename="kjesterprincek.uftb", readme=readme,
                        preserve_information_dependency=False))

    def test_single_class_launcher_preflights_exact_index_before_systemd(self):
        args = SimpleNamespace(
            source_root="/mnt/source", dependencies="/mnt/dependencies",
            dependency_manifest="/mnt/dependencies/manifest.json",
            work_directory="/mnt/work", wave=0, index=12,
            scratch_limit=100, resident_limit=90, memory_max=95,
            reverse_edge_bytes_limit=80, minimum_free_bytes=70,
            monitor_interval=10, s3_prefix="s3://bucket/results",
            unit="ultimatefish-wave0-12", cpu=3, cpu_count=4,
            expected_model_sha256="a" * 64)
        commands = launch.remote_commands(
            args, {"filename": "kexample.uftb", "primary": "rook",
                   "secondary": "angel", "opposing": True})
        self.assertIn("test -f /mnt/dependencies/krk.uftb", commands)
        plan_command = next(command for command in commands
                            if "--range-begin 12 --range-end 13" in command)
        assertion = next(command for command in commands
                         if "kexample.uftb" in command and "python3 -c" in command)
        unit = next(command for command in commands
                    if command.startswith("systemd-run "))
        self.assertIn("kexample.uftb", assertion)
        self.assertIn("--property=AllowedCPUs=3-6", unit)
        self.assertIn("--property=CPUQuota=400%", unit)
        self.assertIn("--property=MemoryMax=95", unit)
        self.assertIn("--scratch-limit 100", unit)
        self.assertIn("--workers 4", plan_command)
        self.assertIn("--workers 4", unit)
        self.assertNotIn("--collect", unit)
        probe = next(command for command in commands
                     if "systemctl" in command and "ActiveState" in command)
        self.assertIn("ActiveState", probe)
        self.assertIn("wave-certificate.json", probe)
        self.assertLess(commands.index(plan_command), commands.index(unit))
        self.assertLess(commands.index(unit), commands.index(probe))

    def test_single_class_launcher_rejects_non_uri_s3_prefix(self):
        args = SimpleNamespace(
            source_root="/mnt/source", dependencies="/mnt/dependencies",
            dependency_manifest="/mnt/dependencies/manifest.json",
            work_directory="/mnt/work", unit="ultimatefish-wave0-12",
            cpu=3, cpu_count=4, scratch_limit=100, resident_limit=90,
            memory_max=95, reverse_edge_bytes_limit=80,
            minimum_free_bytes=70, s3_prefix="results/not-an-uri")
        with self.assertRaisesRegex(ValueError, "must start with s3://"):
            launch.validate(args)

    def test_single_class_launcher_accepts_all_eight_advertised_workers(self):
        args = SimpleNamespace(
            source_root="/mnt/source", dependencies="/mnt/dependencies",
            dependency_manifest="/mnt/dependencies/manifest.json",
            work_directory="/mnt/work", unit="ultimatefish-wave0-12",
            cpu=3, cpu_count=8, scratch_limit=100, resident_limit=90,
            memory_max=95, reverse_edge_bytes_limit=80,
            minimum_free_bytes=70, s3_prefix="s3://bucket/results")
        launch.validate(args)
        args.cpu_count = 9
        with self.assertRaisesRegex(ValueError, "invalid unit"):
            launch.validate(args)

    def test_fresh_angel_v5_stage_contains_remaining_dependencies(self):
        source = (TOOLS / "ultimatefish-stage-angel-graph-v5.sh").read_text()
        self.assertIn("kqueenkangel.uftb", source)
        self.assertIn("kqueenangelk.uftb", source)
        self.assertIn("kberserkerk.uftb", source)
        self.assertIn(
            "f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1",
            source)
        self.assertIn("manifest-pawn-angel-v1.json", source)
        self.assertIn("manifest-pawn-angel-same-v1.json", source)
        self.assertIn(
            "b7953747687e0a10d7069bc50cc469388fdefa4fb265b198adb755ff7aa40db2",
            source)

    def test_preservation_resume_uses_numbered_non_destructive_attempts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "restore-preservation"
            first = preservation_resume.unused_attempt(root, "kbishopangelk")
            self.assertEqual("restore-preservation-0001", first.parent.name)
            first.mkdir(parents=True)
            second = preservation_resume.unused_attempt(root, "kbishopangelk")
            self.assertEqual("restore-preservation-0002", second.parent.name)
        source = (TOOLS / "resume_ultimate_concrete_preservation_aws.py").read_text()
        self.assertNotIn("generator.generate", source)
        self.assertIn("preservation_only_resume", source)

    def test_single_class_launcher_uses_ledger_assignment_syntax(self):
        source = (TOOLS / "launch_ultimate_aws_concrete_class.py").read_text()
        self.assertIn("ledger_filename(str(record[\"filename\"]))}=computing",
                      source)
        self.assertNotIn("[(str(record[\"filename\"]), \"computing\")]", source)
        self.assertEqual("kdragonkpenguin.uftb",
                         launch.ledger_filename("kpenguinkdragon.uftb"))
        self.assertEqual("kexample.uftb",
                         launch.ledger_filename("kexample.uftb"))

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
        def receipt(filename):
            states = int(receipts.catalog()[filename]["states"])
            reach_sha = "c" * 64
            return {
                "schema": receipts.SCHEMA,
                "predicate": receipts.PREDICATE,
                "filename": filename,
                "output_sha256": "d" * 64,
                "states": states,
                "totals": [[0, 2, 3, states // 2 - 5]] * 2,
                "unreachable": [[0, 0, 1, 0]] * 2,
                "table_archive": {
                    "bucket": "bucket", "key": "table", "version_id": "t",
                    "sha256": "a" * 64, "bytes": 10},
                "wave_certificate": {
                    "key": "wave", "version_id": "w",
                    "sha256": "b" * 64, "bytes": 20},
                "reachability_s3": {
                    "bucket": "bucket",
                    "key": (f"prefix/sha256/{reach_sha}/"
                            f"{Path(filename).stem}.reachability-v1.json"),
                    "version_id": "r", "sha256": reach_sha, "bytes": 30},
            }

        base = receipt("krookbishopk.uftb")
        hidden = receipt("kjestercheckerk.uftb")
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

    def test_reachability_import_rejects_cross_bound_sidecar(self):
        filename = "krookbishopk.uftb"
        states = int(receipts.catalog()[filename]["states"])
        record = {
            "schema": receipts.SCHEMA,
            "predicate": receipts.PREDICATE,
            "filename": filename,
            "output_sha256": "d" * 64,
            "states": states,
            "totals": [[0, 2, 3, states // 2 - 5]] * 2,
            "unreachable": [[0, 0, 1, 0]] * 2,
            "table_archive": {"key": "table", "version_id": "t",
                              "sha256": "a" * 64, "bytes": 10},
            "wave_certificate": {"key": "wave", "version_id": "w",
                                 "sha256": "b" * 64, "bytes": 20},
            "reachability_s3": {
                "key": ("prefix/sha256/" + "c" * 64 +
                        "/kberserkerninjak.reachability-v1.json"),
                "version_id": "r", "sha256": "c" * 64, "bytes": 30},
        }
        with self.assertRaisesRegex(ValueError, "sidecar filename residual"):
            receipts.validate_receipt(record)


if __name__ == "__main__":
    unittest.main()
