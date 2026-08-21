#!/usr/bin/env python3

from __future__ import annotations

import base64
import datetime as dt
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
import zlib
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "supervisor", ROOT / "tools/tablebases/supervise_ultimate_aws.py")
assert SPEC and SPEC.loader
SUPERVISOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SUPERVISOR)


SHA_A = "a" * 64
SHA_B = "b" * 64


def config() -> dict[str, object]:
    return {
        "schema": "ultimate-aws-supervision-v1",
        "region": "us-west-2",
        "budget_usd": 5000,
        "regular_report_seconds": 7200,
        "instances": [{
            "instance_id": "i-0123456789abcdef0", "name": "test",
            "hourly_usd": 2.0, "transport": "local", "mounts": ["/"],
            "vcpus": 32,
            "minimum_memory_available_bytes": 1,
            "minimum_disk_free_bytes": {"/": 1},
        }],
        "jobs": [{
            "id": "first", "instance_id": "i-0123456789abcdef0",
            "unit": "ultimatefish-first.service", "dependencies": [],
            "checkpoint_paths": ["/tmp/checkpoint"],
            "completion_paths": ["/tmp/result"],
            "source_bindings": [{"path": "/tmp/source", "sha256": SHA_A}],
            "s3_certificates": [{
                "bucket": "private", "key": "results/first", "version_id": "v1",
                "sha256": SHA_B, "size": 12,
            }],
        }, {
            "id": "second", "instance_id": "i-0123456789abcdef0",
            "unit": "ultimatefish-second.service", "dependencies": ["first"],
            "advanceable": True,
            "expected_allowed_cpus": "16",
            "resource_requirements": {
                "cpu_threads": 1, "memory_peak_bytes": 10,
                "disk_peak_bytes": {"/": 10},
            },
            "checkpoint_paths": [], "completion_paths": [],
            "source_bindings": [{"path": "/tmp/source", "sha256": SHA_A}],
            "s3_certificates": [],
        }, {
            "id": "third", "instance_id": "i-0123456789abcdef0",
            "unit": "ultimatefish-third.service", "dependencies": ["first"],
            "queue_stage": True,
            "checkpoint_paths": [], "completion_paths": [],
            "source_bindings": [], "s3_certificates": [],
        }],
    }


def remote(first: str = "active", complete: bool = False) -> dict[str, object]:
    return {
        "memory": {"MemAvailable": 100},
        "mounts": [{"path": "/", "free_bytes": 100, "total_bytes": 200}],
        "jobs": [{
            "id": "first",
            "unit": {"ActiveState": first, "Result": "success",
                     "ExecMainStatus": "0", "AllowedCPUs": "0-15",
                     "CPUUsageNSec": "10000000000",
                     "StateChangeTimestamp": "Sun 2026-08-10 10:00:00 UTC"},
            "checkpoints": [{"path": "/tmp/checkpoint", "exists": True,
                             "size": 100, "mtime_ns": 1}],
            "completion": ([{"path": "/tmp/result", "exists": True,
                              "size": 12, "mtime_ns": 2}] if complete else
                           [{"path": "/tmp/result", "exists": False}]),
            "sources": [SHA_A],
        }, {
            "id": "second",
            "unit": {"ActiveState": "inactive", "Result": "success",
                     "ExecMainStatus": "0", "AllowedCPUs": "16-31",
                     "CPUUsageNSec": "0",
                     "StateChangeTimestamp": "Sun 2026-08-10 10:00:00 UTC"},
            "checkpoints": [], "completion": [],
            "sources": [SHA_A],
        }, {
            "id": "third",
            "unit": {"ActiveState": "inactive", "Result": "success",
                     "ExecMainStatus": "0", "AllowedCPUs": "16-31"},
            "checkpoints": [], "completion": [], "sources": [],
        }],
    }


class SupervisionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.now = dt.datetime(2026, 8, 10, 12, tzinfo=dt.timezone.utc)
        self.ec2 = {"i-0123456789abcdef0": {
            "InstanceId": "i-0123456789abcdef0", "InstanceType": "r8gd.8xlarge",
            "LaunchTime": "2026-08-10T10:00:00Z", "State": {"Name": "running"},
        }}

    def test_config_is_explicit_and_budget_capped(self) -> None:
        SUPERVISOR.validate_config(config())
        invalid = config()
        invalid["budget_usd"] = 5001
        with self.assertRaisesRegex(RuntimeError, "at most 5000"):
            SUPERVISOR.validate_config(invalid)
        invalid = config()
        invalid["jobs"][0]["unit"] = "safe.service;reboot.service"
        with self.assertRaisesRegex(RuntimeError, "explicit service unit"):
            SUPERVISOR.validate_config(invalid)
        invalid = config()
        invalid["probe_workers"] = 5
        with self.assertRaisesRegex(RuntimeError, "between one and three"):
            SUPERVISOR.validate_config(invalid)
        invalid = config()
        invalid["jobs"][0]["ledger_files"] = ["kabk.uftb"]
        invalid["jobs"][0]["ledger_results"] = {
            "other.uftb": {"result_kind": "concrete"}}
        with self.assertRaisesRegex(RuntimeError, "invalid ledger_results"):
            SUPERVISOR.validate_config(invalid)
        invalid = config()
        invalid["jobs"][1]["superseded_by"] = "replacement"
        with self.assertRaisesRegex(RuntimeError, "superseded.*advanceable"):
            SUPERVISOR.validate_config(invalid)

    def test_current_queue_rejects_superseded_dependency(self) -> None:
        invalid = config()
        invalid["jobs"][0]["superseded_by"] = "replacement"
        replacement = json.loads(json.dumps(invalid["jobs"][0]))
        replacement["id"] = "replacement"
        replacement.pop("superseded_by")
        replacement["unit"] = "ultimatefish-replacement.service"
        invalid["jobs"].append(replacement)
        with self.assertRaisesRegex(
                RuntimeError,
                "third depends on superseded first.*replacement"):
            SUPERVISOR.validate_config(invalid)

    def test_ledger_readme_parser_rejects_conflicting_duplicate_rows(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "README.md"
            path.write_text(
                "| `opposed:bomb+ghost` | King+Bomb vs King+Ghost | opposed | "
                "`kbombkghost.uftb` | **COMPUTING** | 1 | information | — | — | — | — |\n")
            self.assertEqual(
                {"kbombkghost.uftb": "COMPUTING"},
                SUPERVISOR.ledger_readme_statuses(path))
            path.write_text(path.read_text() + (
                "| `duplicate` | duplicate | opposed | `kbombkghost.uftb` | "
                "**PRESERVING** | 1 | information | — | — | — | — |\n"))
            with self.assertRaisesRegex(RuntimeError, "conflicts"):
                SUPERVISOR.ledger_readme_statuses(path)

    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_active_job_with_stale_preserving_ledger_row_fails_closed(
            self, inventory: mock.Mock, probe: mock.Mock) -> None:
        inventory.return_value = self.ec2
        probe.return_value = remote()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "README.md"
            path.write_text(
                "| `opposed:bomb+ghost` | King+Bomb vs King+Ghost | opposed | "
                "`kbombkghost.uftb` | **PRESERVING** | 1 | information | — | — | — | — |\n")
            document = config()
            document["ledger_readme"] = str(path)
            document["jobs"][0]["ledger_files"] = ["kbombkghost.uftb"]
            output, state = SUPERVISOR.supervise(document, {}, self.now)

        self.assertTrue(output["delegate_sol"])
        self.assertEqual("error", output["severity"])
        self.assertEqual(
            "LEDGER_STATUS", state["report"]["errors"][0]["fleet"])
        self.assertIn("PRESERVING", state["report"]["errors"][0]["error"])

    def test_remote_probe_command_has_no_configured_shell_text(self) -> None:
        definition = config()["instances"][0]
        jobs = config()["jobs"]
        command = SUPERVISOR.remote_script(definition, jobs)
        self.assertNotIn("/tmp/source", command)
        self.assertNotIn("ultimatefish-first.service", command)
        self.assertIn("python3 -c", command)
        arguments = shlex.split(command)
        program = zlib.decompress(base64.b64decode(arguments[3])).decode()
        payload = json.loads(base64.b64decode(arguments[4]))
        self.assertIn("sys.argv[2]", program)
        self.assertEqual("first", payload["jobs"][0]["id"])
        # Keep the SSM response compact: these systemd properties are not
        # consumed by classification, resource safety, or scheduling.
        self.assertNotIn("MemoryPeak", program)
        self.assertNotIn("TasksCurrent", program)
        self.assertNotIn("SubState", program)
        self.assertNotIn("ExecMainCode", program)
        # CPU allocation is consumed only for active/activating units; an
        # inactive historical record must not carry its stale CPU assignment.
        self.assertIn("'ExecMainStatus'", program)

    @mock.patch.object(SUPERVISOR.subprocess, "run")
    def test_timeout_error_redacts_encoded_aws_payload(self, command: mock.Mock) -> None:
        command.side_effect = subprocess.TimeoutExpired(
            ["aws", "ssm", "send-command", "secret-payload"], 60)
        with self.assertRaisesRegex(RuntimeError, "aws ssm send-command") as caught:
            SUPERVISOR.run(
                ["aws", "ssm", "send-command", "secret-payload"])
        self.assertNotIn("secret-payload", str(caught.exception))

    @mock.patch.object(SUPERVISOR.time, "sleep")
    @mock.patch.object(SUPERVISOR, "run")
    def test_ssm_probe_has_bounded_retry_backoff(
            self, command: mock.Mock, sleep: mock.Mock) -> None:
        command_id = {"Command": {"CommandId": "cmd-1"}}
        result = {
            "Status": "Success",
            "StandardOutputContent": (
                'ULTIMATE_SUPERVISION_JSON={"memory":{},"mounts":[],"jobs":[]}'),
        }
        command.side_effect = [json.dumps(command_id),
                               RuntimeError("command timed out"),
                               RuntimeError("command timed out"),
                               json.dumps(result)]
        observed = SUPERVISOR.ssm_probe("us-west-2", "i-0123456789abcdef0",
                                        "read-only probe")
        self.assertEqual({"memory": {}, "mounts": [], "jobs": []}, observed)
        self.assertEqual(SUPERVISOR.SSM_SEND_TIMEOUT_SECONDS,
                         command.call_args_list[0].kwargs["timeout"])
        self.assertTrue(all(call.kwargs["timeout"] ==
                            SUPERVISOR.SSM_GET_TIMEOUT_SECONDS
                            for call in command.call_args_list[1:]))
        self.assertGreaterEqual(sleep.call_count, 2)

    @mock.patch.object(SUPERVISOR.time, "sleep")
    @mock.patch.object(SUPERVISOR, "run")
    def test_ssm_probe_polls_every_nonterminal_status(
            self, command: mock.Mock, sleep: mock.Mock) -> None:
        command.side_effect = [
            json.dumps({"Command": {"CommandId": "cmd-1"}}),
            json.dumps({"Status": "Pending"}),
            json.dumps({"Status": "InProgress"}),
            json.dumps({"Status": "Delayed"}),
            json.dumps({
                "Status": "Success",
                "StandardOutputContent": (
                    'ULTIMATE_SUPERVISION_JSON={"memory":{},'
                    '"mounts":[],"jobs":[]}'),
            }),
        ]
        observed = SUPERVISOR.ssm_probe(
            "us-west-2", "i-0123456789abcdef0", "read-only probe")
        self.assertEqual({"memory": {}, "mounts": [], "jobs": []}, observed)
        self.assertEqual(3, sleep.call_count)

    @mock.patch.object(SUPERVISOR.time, "monotonic",
                       side_effect=[0, 46])
    @mock.patch.object(SUPERVISOR, "run")
    def test_ssm_probe_reports_bounded_host_deadline(
            self, command: mock.Mock, monotonic: mock.Mock) -> None:
        command.side_effect = [
            json.dumps({"Command": {"CommandId": "cmd-1"}}),
            RuntimeError("command timed out after 15s: aws ssm get-command-invocation"),
        ]
        with self.assertRaisesRegex(
                RuntimeError,
                "SSM probe deadline exceeded.*i-0123456789abcdef0.*45s"):
            SUPERVISOR.ssm_probe(
                "us-west-2", "i-0123456789abcdef0", "read-only probe")
        self.assertEqual(2, monotonic.call_count)

    def test_large_checkpoint_glob_has_bounded_valid_remote_output(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checkpoint = root / "checkpoints"
            checkpoint.mkdir()
            for index in range(2_000):
                (checkpoint /
                 (f"checkpoint-{index:05d}-" + "x" * 96)).touch()
            definition = config()["instances"][0]
            jobs = config()["jobs"]
            jobs[0]["checkpoint_paths"] = [str(checkpoint / "*")]
            result = SUPERVISOR.local_probe(
                SUPERVISOR.remote_script(definition, jobs))
            self.assertLessEqual(result["probe_encoded_bytes"],
                                 SUPERVISOR.REMOTE_OUTPUT_BUDGET)
            record = result["jobs"][0]["checkpoints"][0]
            self.assertEqual(2_000, record["match_count"])
            self.assertRegex(record["metadata_sha256"], r"^[0-9a-f]{64}$")
            self.assertNotIn(
                "checkpoint-00000", SUPERVISOR.canonical_json(result))

    def test_busy_host_compacts_only_active_job_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            systemctl = root / "systemctl"
            systemctl.write_text(
                "#!/bin/sh\n"
                "if [ \"$1\" = show ]; then\n"
                "  if [ \"$2\" = ultimatefish-inactive.service ]; then\n"
                "    printf '%s\\n' 'LoadState=loaded' 'ActiveState=inactive' "
                "'Result=success' 'ExecMainStatus=0'\n"
                "  else\n"
                "    printf '%s\\n' 'LoadState=loaded' 'ActiveState=active' "
                "'Result=success' 'ExecMainStatus=0' 'MemoryCurrent=1024' "
                "'AllowedCPUs=0' 'CPUUsageNSec=1000000000'\n"
                "  fi\n"
                "elif [ \"$1\" = list-units ]; then\n"
                "  printf '%s\\n' 'ultimatefish-active.service loaded active "
                "running test'\n"
                "fi\n")
            systemctl.chmod(0o755)
            diagnostic = root / "diagnostic.log"
            diagnostic.write_text("x" * 2048)
            checkpoint = root / "checkpoint"
            checkpoint.write_text("checkpoint")
            completion = root / "completion"
            completion.write_text("completion")

            active = {
                "id": "active-0", "unit": "ultimatefish-active.service",
                "checkpoint_paths": [str(checkpoint)],
                "completion_paths": [str(completion)],
                "source_bindings": [],
                "diagnostic_sources": [{
                    "kind": "file", "path": str(diagnostic),
                    "max_bytes": 2048,
                }],
            }
            jobs = []
            for index in range(12):
                job = json.loads(json.dumps(active))
                job["id"] = f"active-{index}"
                jobs.append(job)
            inactive = json.loads(json.dumps(active))
            inactive["id"] = "inactive"
            inactive["unit"] = "ultimatefish-inactive.service"
            jobs.append(inactive)
            definition = config()["instances"][0]
            path = f"{root}:{os.environ['PATH']}"
            with mock.patch.dict(os.environ, {"PATH": path}):
                observed = SUPERVISOR.local_probe(
                    SUPERVISOR.remote_script(definition, jobs))

            self.assertNotIn("probe_error", observed)
            self.assertTrue(observed["probe_compacted"])
            self.assertLessEqual(observed["probe_encoded_bytes"],
                                 SUPERVISOR.REMOTE_OUTPUT_BUDGET)
            active_records = observed["jobs"][:-1]
            self.assertTrue(all(
                record["unit"]["ActiveState"] == "active"
                and record["checkpoints"] == []
                and record["completion"] == []
                and record["diagnostics"][0]["status"] == "ok"
                and len(record["diagnostics"][0]["tail"])
                    <= SUPERVISOR.ACTIVE_DIAGNOSTIC_TAIL_BYTES
                and record["diagnostics"][0]["sha256"]
                for record in active_records))
            inactive_record = observed["jobs"][-1]
            self.assertTrue(inactive_record["checkpoints"][0]["exists"])
            self.assertTrue(inactive_record["completion"][0]["exists"])
            self.assertEqual("ok", inactive_record["diagnostics"][0]["status"])

    def test_stopped_job_history_retains_completion_when_compacted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            systemctl = root / "systemctl"
            systemctl.write_text(
                "#!/bin/sh\n"
                "if [ \"$1\" = show ]; then\n"
                "  printf '%s\\n' 'LoadState=loaded' 'ActiveState=inactive' "
                "'Result=success' 'ExecMainStatus=0'\n"
                "fi\n")
            systemctl.chmod(0o755)
            diagnostic = root / "diagnostic.log"
            diagnostic.write_text("x" * 2048)
            checkpoint = root / "checkpoint"
            checkpoint.write_text("checkpoint")
            completion = root / "completion"
            completion.write_text("completion")
            jobs = [{
                "id": f"inactive-{index}",
                "unit": f"ultimatefish-inactive-{index}.service",
                "checkpoint_paths": [str(checkpoint)],
                "completion_paths": [str(completion)],
                "source_bindings": [],
                "diagnostic_sources": [{
                    "kind": "file", "path": str(diagnostic),
                    "max_bytes": 2048,
                }],
            } for index in range(12)]
            definition = config()["instances"][0]
            path = f"{root}:{os.environ['PATH']}"
            with mock.patch.dict(os.environ, {"PATH": path}):
                observed = SUPERVISOR.local_probe(
                    SUPERVISOR.remote_script(definition, jobs))

            self.assertNotIn("probe_error", observed)
            self.assertEqual(2, observed["probe_compaction_level"])
            self.assertLessEqual(observed["probe_encoded_bytes"],
                                 SUPERVISOR.REMOTE_OUTPUT_BUDGET)
            self.assertTrue(all(
                record["checkpoints"] == []
                and record["diagnostics"] == []
                and record["completion"][0]["exists"]
                for record in observed["jobs"]))

    def test_source_binding_globs_are_rejected(self) -> None:
        invalid = config()
        invalid["jobs"][0]["source_bindings"][0]["path"] = "/tmp/source-*"
        with self.assertRaisesRegex(RuntimeError, "one explicit path"):
            SUPERVISOR.validate_config(invalid)

    def test_diagnostic_sources_are_explicit_and_bounded(self) -> None:
        document = config()
        document["jobs"][0]["diagnostic_sources"] = [{
            "kind": "file", "path": "/tmp/failure.log", "max_bytes": 2048,
        }, {
            "kind": "journal", "unit": "ultimatefish-first.service",
            "max_bytes": 2048,
        }]
        SUPERVISOR.validate_config(document)
        invalid = config()
        invalid["jobs"][0]["diagnostic_sources"] = [{
            "kind": "file", "path": "/tmp/failure-*", "max_bytes": 2048,
        }]
        with self.assertRaisesRegex(RuntimeError, "not allowlisted"):
            SUPERVISOR.validate_config(invalid)
        invalid = config()
        invalid["jobs"][0]["diagnostic_sources"] = [{
            "kind": "journal", "unit": "other.service", "max_bytes": 2048,
        }]
        with self.assertRaisesRegex(RuntimeError, "journal unit"):
            SUPERVISOR.validate_config(invalid)
        invalid = config()
        invalid["jobs"][0]["diagnostic_sources"] = [{
            "kind": "file", "path": "/tmp/failure.log",
            "max_bytes": SUPERVISOR.DIAGNOSTIC_SOURCE_MAX_BYTES + 1,
        }]
        with self.assertRaisesRegex(RuntimeError, "max_bytes"):
            SUPERVISOR.validate_config(invalid)
        invalid = config()
        invalid["jobs"][0]["diagnostic_sources"] = [{
            "kind": "file", "path": f"/tmp/failure-{index}.log",
            "max_bytes": SUPERVISOR.DIAGNOSTIC_SOURCE_MAX_BYTES,
        } for index in range(3)]
        with self.assertRaisesRegex(RuntimeError, "byte cap"):
            SUPERVISOR.validate_config(invalid)

    def test_failed_diagnostic_tail_is_redacted_and_hashed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "failure.log"
            payload = (b"prefix\nAWS_SECRET_ACCESS_KEY=super-secret\n"
                       b"fatal: SIGBUS while finalizing transitions\n")
            path.write_bytes(payload)
            document = config()
            document["jobs"][0]["diagnostic_sources"] = [{
                "kind": "file", "path": str(path), "max_bytes": 2048,
            }]
            observed = SUPERVISOR.local_probe(
                SUPERVISOR.remote_script(document["instances"][0],
                                         document["jobs"]))
            diagnostic = observed["jobs"][0]["diagnostics"][0]
            self.assertEqual("ok", diagnostic["status"])
            self.assertEqual(len(payload), diagnostic["bytes"])
            self.assertEqual(SUPERVISOR.sha256_bytes(payload),
                             diagnostic["sha256"])
            self.assertIn("fatal: SIGBUS", diagnostic["tail"])
            self.assertIn("AWS_SECRET_ACCESS_KEY=[REDACTED]",
                          diagnostic["tail"])
            self.assertNotIn("super-secret", diagnostic["tail"])
            self.assertLessEqual(
                len(diagnostic["tail"].encode()), 2048)

    def test_source_exact_is_positional_and_fail_closed(self) -> None:
        job = {
            "source_bindings": [
                {"path": "/tmp/a", "sha256": SHA_A},
                {"path": "/tmp/b", "sha256": SHA_B},
            ]
        }
        self.assertTrue(SUPERVISOR.source_exact(job, {
            "sources": [SHA_A, SHA_B]}))
        for sources in (
                [None, SHA_B], ["c" * 64, SHA_B], [SHA_A],
                [SHA_A, SHA_B, SHA_A], [SHA_B, SHA_A], []):
            with self.subTest(sources=sources):
                self.assertFalse(SUPERVISOR.source_exact(
                    job, {"sources": sources}))
        self.assertTrue(SUPERVISOR.source_exact(
            {"source_bindings": []}, {"sources": []}))
        self.assertTrue(SUPERVISOR.source_exact(job, {
            "sources_exact": True, "source_binding_count": 2}))
        for compact in (
                {"sources_exact": False, "source_binding_count": 2},
                {"sources_exact": True, "source_binding_count": 1},
                {"sources_exact": 1, "source_binding_count": 2},
                {"source_binding_count": 2}):
            with self.subTest(compact=compact):
                self.assertFalse(SUPERVISOR.source_exact(job, compact))

    def test_compact_source_probe_fits_each_batch_host_budget(self) -> None:
        document = json.loads(
            (ROOT / "tools/tablebases/ultimate_aws_supervision.json").read_text())
        jobs_by_host = {}
        for job in document["jobs"]:
            if job["id"].startswith("concrete-wave0-batch-v4r2-"):
                copy = json.loads(json.dumps(job))
                jobs_by_host.setdefault(copy["instance_id"], []).append(copy)
        self.assertEqual(24, sum(map(len, jobs_by_host.values())))
        self.assertEqual(148, max(
            sum(len(job["source_bindings"]) for job in jobs)
            for jobs in jobs_by_host.values()))
        def check_host_payload(instance_id, jobs):
            definition = next(item for item in document["instances"]
                              if item["instance_id"] == instance_id)
            definition = json.loads(json.dumps(definition))
            definition["mounts"] = ["/"]
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                replacements = {}
                for job in jobs:
                    for binding in job["source_bindings"]:
                        path = binding["path"]
                        if path not in replacements:
                            target = root / f"source-{len(replacements):04d}"
                            target.write_bytes(b"compact-probe")
                            replacements[path] = str(target)
                probe_jobs = json.loads(json.dumps(jobs))
                for job in probe_jobs:
                    for binding in job["source_bindings"]:
                        binding["path"] = replacements[binding["path"]]
                        binding["sha256"] = SUPERVISOR.sha256_bytes(
                            b"compact-probe")
                observed = SUPERVISOR.local_probe(
                    SUPERVISOR.remote_script(definition, probe_jobs))
                self.assertNotIn("probe_error", observed)
                self.assertLessEqual(observed["probe_encoded_bytes"],
                                     SUPERVISOR.REMOTE_OUTPUT_BUDGET)
                self.assertEqual(
                    sum(len(job["source_bindings"]) for job in jobs),
                    sum(job["source_binding_count"]
                        for job in observed["jobs"]))
                self.assertTrue(all(
                    job["sources_exact"] for job in observed["jobs"]))
            return observed["probe_encoded_bytes"]

        for instance_id, jobs in jobs_by_host.items():
            check_host_payload(instance_id, jobs)
        # The promoted batch shares hosts with existing authenticated jobs.
        # Exercise the complete per-host probe too; otherwise a legacy job's
        # checkpoint metadata could push the combined response over the SSM
        # budget even when the 24 batch records fit in isolation.
        all_jobs_by_host = {}
        for job in document["jobs"]:
            if (not job.get("queue_stage") and
                    not job.get("superseded_by") and
                    not job.get("s3_only_certified")):
                all_jobs_by_host.setdefault(job["instance_id"], []).append(job)
        sizes = {instance_id: check_host_payload(instance_id, jobs)
                 for instance_id, jobs in all_jobs_by_host.items()}
        self.assertLessEqual(max(sizes.values()), SUPERVISOR.REMOTE_OUTPUT_BUDGET)

    def test_source_binding_table_deduplicates_worst_host_request(self) -> None:
        document = json.loads(
            (ROOT / "tools/tablebases/ultimate_aws_supervision.json").read_text())
        for definition in document["instances"]:
            jobs = [job for job in document["jobs"]
                    if job["instance_id"] == definition["instance_id"]
                    and not job.get("queue_stage")
                    and not job.get("superseded_by")
                    and not job.get("s3_only_certified")]
            table, compact_jobs = SUPERVISOR.compact_source_bindings(jobs)
            self.assertLess(
                len(table),
                sum(len(job.get("source_bindings", [])) for job in jobs))
            self.assertEqual(len(jobs), len(compact_jobs))
            for original, compact in zip(jobs, compact_jobs):
                references = compact["binding_refs"]
                self.assertEqual(len(original.get("source_bindings", [])),
                                 len(references))
                self.assertTrue(all(
                    isinstance(reference, int) and not isinstance(reference, bool)
                    and 0 <= reference < len(table)
                    for reference in references))
            command = SUPERVISOR.remote_script(definition, jobs)
            self.assertLess(len(command.encode()),
                            SUPERVISOR.REMOTE_REQUEST_BUDGET)
        worst = next(item for item in document["instances"]
                     if item["instance_id"] == "i-0b4523116b2f7765c")
        worst_jobs = [job for job in document["jobs"]
                      if job["instance_id"] == worst["instance_id"]
                      and not job.get("queue_stage")
                      and not job.get("superseded_by")
                      and not job.get("s3_only_certified")]
        table, _ = SUPERVISOR.compact_source_bindings(worst_jobs)
        # Completed and superseded jobs are retired from live probes; their
        # version-pinned ledger results and retained evidence remain
        # authoritative.  Only the current high-memory jobs contribute live
        # i0b bindings after the fleet reconciliation.  The current opposed
        # Checker recovery now runs on i024, while the Bomb and Ninja solves
        # bind only their small restored executables, wrappers, and markers
        # locally and authenticate their large migration archives through S3
        # instead of the bounded hash table.
        self.assertEqual(118, len(table))

    def test_cpu_allocation_reports_idle_capacity_and_overlap(self) -> None:
        definition = config()["instances"][0]
        report = SUPERVISOR.cpu_allocation(definition, remote())
        self.assertEqual(16, report["allocated_vcpus"])
        self.assertEqual(16, report["idle_vcpus"])
        self.assertTrue(report["allocation_known"])
        document = remote()
        document["jobs"][1]["unit"]["ActiveState"] = "active"
        document["jobs"][1]["unit"]["AllowedCPUs"] = "8-23"
        report = SUPERVISOR.cpu_allocation(definition, document)
        self.assertEqual(24, report["allocated_vcpus"])
        self.assertTrue(report["overlaps"])

    def test_cpu_allocation_reports_interval_utilization(self) -> None:
        definition = config()["instances"][0]
        before = remote()
        after = remote()
        after["jobs"][0]["unit"]["CPUUsageNSec"] = "130000000000"
        report = SUPERVISOR.cpu_allocation(
            definition, after, previous_remote=before, sample_seconds=60)
        sample = report["measured_jobs"]["first"]
        self.assertEqual(120_000_000_000, sample["cpu_delta_nsec"])
        self.assertEqual(2.0, sample["average_busy_vcpus"])
        self.assertEqual(12.5, sample["allocated_utilization_percent"])
        self.assertEqual(6.2, report["measured_fleet_capacity_percent"])
        self.assertTrue(report["measurement_complete"])

        after["jobs"][0]["unit"]["StateChangeTimestamp"] = "new activation"
        report = SUPERVISOR.cpu_allocation(
            definition, after, previous_remote=before, sample_seconds=60)
        self.assertFalse(report["measurement_complete"])
        self.assertEqual({}, report["measured_jobs"])

    def test_unconfigured_active_unit_disables_automatic_backfill(self) -> None:
        document = config()
        observed = remote(first="inactive", complete=True)
        observed["unconfigured_active_units"] = {
            "count": 1, "sha256": SHA_A,
            "sample": ["ultimatefish-current-retry.service"],
            "probe_status": 0,
        }
        allocation = SUPERVISOR.cpu_allocation(
            document["instances"][0], observed, document["jobs"])
        self.assertFalse(allocation["allocation_known"])
        self.assertFalse(allocation["measurement_complete"])
        report = {
            "instances": {"i-0123456789abcdef0": {
                "remote": observed, "cpu_allocation": allocation}},
            "jobs": {
                "first": {"status": "CERTIFIED"},
                "second": {"status": "READY"},
                "third": {"status": "AWAITING_STAGE"},
            },
        }
        schedule = SUPERVISOR.schedule_backfill(document, report)
        self.assertEqual([], schedule["selected"])

    def test_unconfigured_active_unit_utilization_is_measured_fail_closed(self) -> None:
        definition = config()["instances"][0]
        before = remote(first="inactive", complete=True)
        after = remote(first="inactive", complete=True)
        for document, cpu in ((before, "10000000000"),
                              (after, "130000000000")):
            document["unconfigured_active_units"] = {
                "count": 1, "sha256": SHA_A,
                "sample": ["ultimatefish-current-retry.service"],
                "probe_status": 0,
                "details": [{
                    "unit": "ultimatefish-current-retry.service",
                    "properties": {
                        "ActiveState": "active",
                        "CPUUsageNSec": cpu,
                        "MemoryCurrent": "4096",
                        "StateChangeTimestamp": "same activation",
                    },
                }],
            }
        allocation = SUPERVISOR.cpu_allocation(
            definition, after, previous_remote=before, sample_seconds=60)
        sample = allocation["unconfigured_measured_jobs"][
            "ultimatefish-current-retry.service"]
        self.assertEqual(2.0, sample["average_busy_vcpus"])
        self.assertEqual(4096, sample["memory_current_bytes"])
        self.assertEqual(6.2, allocation["measured_fleet_capacity_percent"])
        self.assertFalse(allocation["allocation_known"])
        self.assertFalse(allocation["measurement_complete"])

    def test_expected_cpu_partition_is_validated_and_monitored(self) -> None:
        document = config()
        document["jobs"][0]["expected_allowed_cpus"] = "0-15"
        SUPERVISOR.validate_config(document)
        report = SUPERVISOR.cpu_allocation(
            document["instances"][0], remote(), document["jobs"])
        self.assertEqual({}, report["allocation_mismatches"])
        document["jobs"][0]["expected_allowed_cpus"] = "16-31"
        report = SUPERVISOR.cpu_allocation(
            document["instances"][0], remote(), document["jobs"])
        self.assertIn("first", report["allocation_mismatches"])

        document["jobs"][0]["expected_allowed_cpus"] = "32"
        with self.assertRaisesRegex(RuntimeError, "expected_allowed_cpus"):
            SUPERVISOR.validate_config(document)

    def test_systemd_space_separated_cpu_ranges_are_canonical(self) -> None:
        document = config()
        document["jobs"][0]["expected_allowed_cpus"] = "0-7,9-15"
        observed = remote()
        observed["jobs"][0]["unit"]["AllowedCPUs"] = "0-7 9-15"
        report = SUPERVISOR.cpu_allocation(
            document["instances"][0], observed, document["jobs"])
        self.assertEqual({}, report["allocation_mismatches"])
        self.assertEqual(15, report["active_jobs"]["first"])
        self.assertEqual([], report["unknown_jobs"])

    def test_resource_scheduler_backfills_only_jobs_that_fit(self) -> None:
        document = config()
        fourth = {
            "id": "fourth", "instance_id": "i-0123456789abcdef0",
            "unit": "ultimatefish-fourth.service", "dependencies": [],
            "advanceable": True, "expected_allowed_cpus": "17",
            "resource_requirements": {
                "cpu_threads": 1, "memory_peak_bytes": 95,
                "disk_peak_bytes": {"/": 10},
            },
            "checkpoint_paths": [], "completion_paths": [],
            "source_bindings": [{"path": "/tmp/source", "sha256": SHA_A}],
            "s3_certificates": [],
        }
        document["jobs"].append(fourth)
        observed = remote(first="inactive", complete=True)
        observed["jobs"].append({
            "id": "fourth", "unit": {"ActiveState": "inactive"},
            "checkpoints": [], "completion": [], "sources": [],
        })
        report = {
            "instances": {"i-0123456789abcdef0": {
                "remote": observed,
                "cpu_allocation": SUPERVISOR.cpu_allocation(
                    document["instances"][0], observed, document["jobs"]),
            }},
            "jobs": {
                "first": {"status": "CERTIFIED"},
                "second": {"status": "READY"},
                "third": {"status": "AWAITING_STAGE"},
                "fourth": {"status": "READY"},
            },
        }
        schedule = SUPERVISOR.schedule_backfill(document, report)
        self.assertEqual(["second"], schedule["selected"])
        self.assertIn("memory", schedule["blocked"]["fourth"])

    @mock.patch.object(SUPERVISOR, "head_certificate")
    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_two_low_utilization_samples_raise_underutilized(
            self, inventory: mock.Mock, probe: mock.Mock,
            head: mock.Mock) -> None:
        document = config()
        document["underutilized_samples"] = 2
        inventory.return_value = self.ec2
        probe.return_value = remote(first="inactive", complete=True)
        head.return_value = {
            "bucket": "private", "key": "results/first", "version_id": "v1",
            "size": 12, "sha256": SHA_B, "exact": True,
        }
        first, state = SUPERVISOR.supervise(document, {}, self.now)
        self.assertFalse(first["report"]["scheduling"]["underutilized"])
        second, _ = SUPERVISOR.supervise(
            document, state, self.now + dt.timedelta(minutes=5))
        self.assertTrue(second["report"]["scheduling"]["underutilized"])
        self.assertTrue(second["delegate_sol"])
        self.assertEqual("UNDERUTILIZED",
                         second["report"]["errors"][-1]["fleet"])
        self.assertIn("second", second["ready_jobs"])
        self.assertEqual(
            "READY", second["report"]["jobs"]["second"]["status"])

    @mock.patch.object(SUPERVISOR, "local_probe")
    def test_uninstalled_queue_records_do_not_consume_remote_budget(
            self, probe: mock.Mock) -> None:
        document = config()
        definition = document["instances"][0]
        ec2 = self.ec2[definition["instance_id"]]
        probe.return_value = {
            "memory": {"MemAvailable": 100},
            "mounts": [{"path": "/", "free_bytes": 100,
                        "total_bytes": 200}],
            "jobs": [],
        }
        result, error = SUPERVISOR.probe_instance(
            document, definition, ec2, [document["jobs"][2]], self.now,
            None, None)
        self.assertIsNone(error)
        encoded = probe.call_args.args[0]
        self.assertNotIn("third", encoded)
        queued = result["remote"]["jobs"][0]
        self.assertEqual("third", queued["id"])
        self.assertEqual("not-found", queued["unit"]["LoadState"])
        self.assertEqual({}, result["cpu_allocation"]["active_jobs"])

    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_superseded_records_are_not_probed_or_reported_as_failures(
            self, inventory: mock.Mock, probe: mock.Mock) -> None:
        document = config()
        retired = document["jobs"][0]
        retired["superseded_by"] = "replacement"
        inventory.return_value = self.ec2
        observed = remote()
        observed["jobs"] = observed["jobs"][1:]
        probe.return_value = observed

        output, state = SUPERVISOR.supervise(document, {}, self.now)

        command = probe.call_args.args[0]
        self.assertNotIn('"id":"first"', command)
        self.assertEqual(
            "SUPERSEDED", output["report"]["jobs"]["first"]["status"])
        self.assertEqual("SUPERSEDED", state["report"]["jobs"]["first"]["status"])
        self.assertFalse(output["delegate_sol"])

    @mock.patch.object(SUPERVISOR, "head_certificate")
    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_quiet_checkpoint_growth_is_no_change(
            self, inventory: mock.Mock, probe: mock.Mock,
            head: mock.Mock) -> None:
        inventory.return_value = self.ec2
        probe.return_value = remote()
        first, state = SUPERVISOR.supervise(config(), {}, self.now)
        self.assertEqual("CHANGE", first["status"])
        probe.return_value = remote()
        probe.return_value["jobs"][0]["checkpoints"][0]["size"] = 200
        probe.return_value["jobs"][0]["unit"]["CPUUsageNSec"] = "70000000000"
        second, _ = SUPERVISOR.supervise(
            config(), state, self.now + dt.timedelta(minutes=5))
        self.assertEqual("NO_CHANGE", second["status"])
        measured = second["report"]["instances"]
        # Quiet polls do not emit a full instance report, while the returned
        # durable state still retains the utilization sample.
        self.assertEqual({}, measured)
        head.assert_not_called()

    @mock.patch.object(SUPERVISOR, "head_certificate")
    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_completion_certificate_unlocks_dependency(
            self, inventory: mock.Mock, probe: mock.Mock,
            head: mock.Mock) -> None:
        inventory.return_value = self.ec2
        probe.return_value = remote(first="inactive", complete=True)
        head.return_value = {
            "bucket": "private", "key": "results/first", "version_id": "v1",
            "size": 12, "sha256": SHA_B, "exact": True,
        }
        output, _ = SUPERVISOR.supervise(config(), {}, self.now)
        self.assertEqual("CERTIFIED", output["report"]["jobs"]["first"]["status"])
        self.assertEqual("READY", output["report"]["jobs"]["second"]["status"])
        self.assertEqual("AWAITING_STAGE",
                         output["report"]["jobs"]["third"]["status"])
        self.assertEqual(["second"], output["ready_jobs"])
        self.assertEqual(["third"], output["stage_jobs"])

    @mock.patch.object(SUPERVISOR, "head_certificate")
    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_source_only_s3_objects_cannot_certify_ledger_result(
            self, inventory: mock.Mock, probe: mock.Mock,
            head: mock.Mock) -> None:
        document = config()
        first = document["jobs"][0]
        first["ledger_files"] = ["first.uftb"]
        first["ledger_certifies"] = True
        first["s3_certificates"][0]["key"] = "sources/first-wrapper"
        inventory.return_value = self.ec2
        probe.return_value = remote(first="inactive", complete=True)
        head.return_value = {
            "bucket": "private", "key": "sources/first-wrapper",
            "version_id": "v1", "size": 12, "sha256": SHA_B,
            "exact": True,
        }
        output, state = SUPERVISOR.supervise(document, {}, self.now)
        observed = output["report"]["jobs"]["first"]
        self.assertEqual("COMPLETED_UNCERTIFIED", observed["status"])
        self.assertFalse(state["report"]["jobs"]["first"]
                         ["result_certificate_declared"])
        self.assertEqual("INACTIVE",
                         output["report"]["jobs"]["second"]["status"])
        self.assertEqual([], output["ready_jobs"])

    def test_result_certificate_namespaces_are_fail_closed(self) -> None:
        definition = config()["jobs"][0]
        definition["ledger_files"] = ["first.uftb"]
        definition["ledger_certifies"] = True
        definition["s3_certificates"][0]["key"] = "sources/first"
        self.assertFalse(SUPERVISOR.has_result_certificate(definition))
        definition["s3_certificates"][0]["key"] = "checkpoints/first"
        self.assertFalse(SUPERVISOR.has_result_certificate(definition))
        definition["s3_certificates"][0]["key"] = "results/first"
        self.assertTrue(SUPERVISOR.has_result_certificate(definition))
        definition["s3_certificates"][0]["key"] = (
            "hidden/first/existing-ufiw/sha256/result.tar.zst")
        self.assertTrue(SUPERVISOR.has_result_certificate(definition))
        definition["s3_certificates"][0]["key"] = (
            "results/dependency/dependency-result.tar.zst")
        definition["result_certificate_keys"] = {"first.uftb": []}
        self.assertFalse(SUPERVISOR.has_result_certificate(definition))
        definition["result_certificate_keys"] = {
            "first.uftb": ["results/dependency/dependency-result.tar.zst"]}
        self.assertTrue(SUPERVISOR.has_result_certificate(definition))
        definition["ledger_files"] = ["first.uftb", "second.uftb"]
        definition["s3_certificates"] = [
            {"key": "results/first/first-result.tar.zst"}]
        definition["result_certificate_keys"] = {
            "first.uftb": ["results/first/first-result.tar.zst"],
            "second.uftb": [],
        }
        self.assertFalse(SUPERVISOR.has_result_certificate(definition))
        definition["s3_certificates"].append(
            {"key": "results/second/second-result.tar.zst"})
        definition["result_certificate_keys"]["second.uftb"] = [
            "results/second/second-result.tar.zst"]
        self.assertTrue(SUPERVISOR.has_result_certificate(definition))

    @mock.patch.object(SUPERVISOR, "head_certificate")
    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_queued_source_bindings_wait_without_source_mismatch(
            self, inventory: mock.Mock, probe: mock.Mock,
            head: mock.Mock) -> None:
        document = config()
        document["jobs"][2]["source_bindings"] = [
            {"path": "/tmp/staged-source", "sha256": SHA_A}]
        inventory.return_value = self.ec2
        probe.return_value = remote(first="inactive", complete=True)
        head.return_value = {
            "bucket": "private", "key": "results/first", "version_id": "v1",
            "size": 12, "sha256": SHA_B, "exact": True,
        }
        output, _ = SUPERVISOR.supervise(document, {}, self.now)
        self.assertEqual(
            "AWAITING_STAGE", output["report"]["jobs"]["third"]["status"])
        self.assertNotIn("third", output["report"]["errors"])
        self.assertFalse(output["delegate_sol"])

    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_completed_result_survives_reclaimed_source_tree(
            self, inventory: mock.Mock, probe: mock.Mock) -> None:
        inventory.return_value = self.ec2
        finished = remote(first="inactive", complete=True)
        finished["jobs"][0]["sources"] = [None]
        probe.return_value = finished
        output, state = SUPERVISOR.supervise(config(), {}, self.now)
        observed = state["report"]["jobs"]["first"]
        self.assertEqual("COMPLETED_UNCERTIFIED", observed["status"])
        self.assertFalse(observed["source_exact"])
        self.assertEqual(
            "COMPLETED_UNCERTIFIED",
            output["report"]["jobs"]["first"]["status"])

    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_queued_replacement_does_not_inherit_disappeared_run_failure(
            self, inventory: mock.Mock, probe: mock.Mock) -> None:
        document = config()
        queued = document["jobs"][2]
        inventory.return_value = self.ec2
        current = remote(first="inactive", complete=True)
        current["jobs"][2]["unit"].update({
            "LoadState": "not-found", "ActiveState": "inactive",
            "Result": "success", "ExecMainStatus": "0",
        })
        probe.return_value = current
        previous = {"report": {"jobs": {"third": {"status": "RUNNING"}}}}
        output, state = SUPERVISOR.supervise(document, previous, self.now)
        self.assertEqual("INACTIVE", state["report"]["jobs"]["third"]["status"])
        self.assertNotEqual("FAILED", output["report"]["jobs"]["third"]["status"])

    @mock.patch.object(SUPERVISOR, "head_certificate")
    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_explicit_s3_only_archive_remains_certified_after_cleanup(
            self, inventory: mock.Mock, probe: mock.Mock,
            head: mock.Mock) -> None:
        document = config()
        first = document["jobs"][0]
        first["s3_only_certified"] = True
        inventory.return_value = self.ec2
        probe.return_value = remote(first="inactive", complete=False)
        probe.return_value["jobs"][0]["sources"] = []
        head.return_value = {
            "bucket": "private", "key": "results/first", "version_id": "v1",
            "size": 12, "sha256": SHA_B, "exact": True,
        }
        output, state = SUPERVISOR.supervise(document, {}, self.now)
        self.assertEqual("CERTIFIED", output["report"]["jobs"]["first"]["status"])
        arguments = shlex.split(probe.call_args.args[0])
        payload = json.loads(base64.b64decode(arguments[4]))
        self.assertNotIn("first", {job["id"] for job in payload["jobs"]})
        self.assertFalse(state["report"]["jobs"]["first"]["source_exact"])
        self.assertEqual("READY", output["report"]["jobs"]["second"]["status"])

    def test_s3_only_certification_is_archival_and_version_bound(self) -> None:
        document = config()
        first = document["jobs"][0]
        first["s3_only_certified"] = True
        SUPERVISOR.validate_config(document)
        first["advanceable"] = True
        with self.assertRaisesRegex(RuntimeError, "archival only"):
            SUPERVISOR.validate_config(document)
        first.pop("advanceable")
        first["s3_certificates"] = []
        with self.assertRaisesRegex(RuntimeError, "requires certificates"):
            SUPERVISOR.validate_config(document)

    def test_version_pinned_exact_certificate_is_cached(self) -> None:
        definition = config()["jobs"][0]
        previous = {"certificates": [{
            "bucket": "private", "key": "results/first", "version_id": "v1",
            "sha256": SHA_B, "size": 12, "exact": True,
        }]}
        self.assertEqual(
            previous["certificates"],
            SUPERVISOR.cached_certificates(definition, previous))
        previous["certificates"][0]["version_id"] = "wrong"
        self.assertEqual([], SUPERVISOR.cached_certificates(definition, previous))

    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_failure_requests_sol_without_restart(
            self, inventory: mock.Mock, probe: mock.Mock) -> None:
        inventory.return_value = self.ec2
        failed = remote(first="failed")
        failed["jobs"][0]["unit"]["Result"] = "exit-code"
        failed["jobs"][0]["unit"]["ExecMainStatus"] = "1"
        probe.return_value = failed
        output, _ = SUPERVISOR.supervise(config(), {}, self.now)
        self.assertEqual("FAILED", output["report"]["jobs"]["first"]["status"])
        self.assertTrue(output["delegate_sol"])
        self.assertEqual("error", output["severity"])

    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_garbage_collected_transient_cannot_fake_clean_exit(
            self, inventory: mock.Mock, probe: mock.Mock) -> None:
        inventory.return_value = self.ec2
        missing = remote(first="inactive")
        missing["jobs"][0]["unit"].update({
            "LoadState": "not-found", "Result": "success",
            "ExecMainCode": "0", "ExecMainStatus": "0",
        })
        probe.return_value = missing
        previous = {"report": {"jobs": {"first": {"status": "RUNNING"}}}}
        output, state = SUPERVISOR.supervise(config(), previous, self.now)
        self.assertEqual("FAILED", state["report"]["jobs"]["first"]["status"])
        self.assertEqual("FAILED", output["report"]["jobs"]["first"]["status"])
        self.assertTrue(output["delegate_sol"])
        self.assertIn("disappeared", state["report"]["jobs"]["first"]
                      ["unit"]["SupervisionFailure"])

    @mock.patch.object(SUPERVISOR, "local_probe")
    @mock.patch.object(SUPERVISOR, "ec2_inventory")
    def test_host_probe_error_never_fabricates_job_failures(
            self, inventory: mock.Mock, probe: mock.Mock) -> None:
        inventory.return_value = self.ec2
        probe.side_effect = subprocess.TimeoutExpired(["aws"], 10)
        output, _ = SUPERVISOR.supervise(config(), {}, self.now)
        self.assertEqual("UNKNOWN", output["report"]["jobs"]["first"]["status"])
        self.assertEqual("UNKNOWN", output["report"]["jobs"]["second"]["status"])
        self.assertTrue(output["delegate_sol"])
        self.assertIn("timed out", output["report"]["errors"][0]["error"])

    @mock.patch.object(SUPERVISOR, "run")
    def test_advance_refuses_nonready_and_starts_only_exact_unit(
            self, command: mock.Mock) -> None:
        state = {"report": {"jobs": {
            "first": {"status": "CERTIFIED"},
            "second": {"status": "READY", "source_exact": True},
        }, "scheduling": {"selected": ["second"]}}}
        result = SUPERVISOR.start_ready(config(), state, "second")
        self.assertEqual("STARTED", result["status"])
        command.assert_called_once_with(
            ["systemctl", "start", "--no-block",
             "ultimatefish-second.service"])
        state["report"]["jobs"]["second"]["status"] = "RUNNING"
        with self.assertRaisesRegex(RuntimeError, "not source-certified READY"):
            SUPERVISOR.start_ready(config(), state, "second")
        state["report"]["jobs"]["second"] = {
            "status": "READY", "source_exact": True}
        state["report"]["scheduling"]["selected"] = []
        with self.assertRaisesRegex(RuntimeError, "resource scheduler"):
            SUPERVISOR.start_ready(config(), state, "second")

    @mock.patch.object(SUPERVISOR, "ssm_probe")
    def test_remote_advance_never_waits_for_oneshot_completion(
            self, probe: mock.Mock) -> None:
        document = config()
        document["instances"][0]["transport"] = "ssm"
        state = {"report": {"jobs": {
            "first": {"status": "CERTIFIED"},
            "second": {"status": "READY", "source_exact": True},
        }, "scheduling": {"selected": ["second"]}}}
        result = SUPERVISOR.start_ready(document, state, "second")
        self.assertEqual(result["status"], "STARTED")
        command = probe.call_args.args[2]
        self.assertIn(
            "systemctl start --no-block ultimatefish-second.service", command)
        self.assertIn('test "$state" = activating', command)

    @mock.patch.object(SUPERVISOR, "run")
    def test_cpu_rebalance_shrinks_overlapping_live_allocations(
            self, command: mock.Mock) -> None:
        document = config()
        document["jobs"][0]["expected_allowed_cpus"] = "0"
        document["jobs"][0]["resource_requirements"] = {
            "cpu_threads": 1, "memory_peak_bytes": 10,
            "disk_peak_bytes": {"/": 10},
        }
        observed = remote()
        observed["jobs"][1]["unit"].update({
            "ActiveState": "active", "AllowedCPUs": "0-31"})
        state = {"report": {
            "jobs": {"first": {"status": "RUNNING"},
                     "second": {"status": "RUNNING"}},
            "instances": {"i-0123456789abcdef0": {"remote": observed}},
        }}
        command.side_effect = ["", "0\n"]
        result = SUPERVISOR.rebalance_cpu(document, state, "first")
        self.assertEqual("CPU_REBALANCED", result["status"])
        self.assertEqual("0", result["allowed_cpus"])
        command.assert_has_calls([
            mock.call(["systemctl", "set-property", "--runtime",
                       "ultimatefish-first.service", "AllowedCPUs=0"]),
            mock.call(["systemctl", "show", "ultimatefish-first.service",
                       "-p", "AllowedCPUs", "--value"]),
        ])


if __name__ == "__main__":
    unittest.main()
