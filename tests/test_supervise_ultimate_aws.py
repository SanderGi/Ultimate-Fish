#!/usr/bin/env python3

from __future__ import annotations

import base64
import datetime as dt
import importlib.util
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "supervisor", ROOT / "tools/supervise_ultimate_aws.py")
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
                     "ExecMainStatus": "0"},
            "checkpoints": [{"path": "/tmp/checkpoint", "exists": True,
                             "size": 100, "mtime_ns": 1}],
            "completion": ([{"path": "/tmp/result", "exists": True,
                              "size": 12, "mtime_ns": 2}] if complete else
                           [{"path": "/tmp/result", "exists": False}]),
            "sources": [{"path": "/tmp/source", "exists": True,
                         "sha256": SHA_A}],
        }, {
            "id": "second",
            "unit": {"ActiveState": "inactive", "Result": "success",
                     "ExecMainStatus": "0"},
            "checkpoints": [], "completion": [],
            "sources": [{"path": "/tmp/source", "exists": True,
                         "sha256": SHA_A}],
        }, {
            "id": "third",
            "unit": {"ActiveState": "inactive", "Result": "success",
                     "ExecMainStatus": "0"},
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

    def test_remote_probe_command_has_no_configured_shell_text(self) -> None:
        definition = config()["instances"][0]
        jobs = config()["jobs"]
        command = SUPERVISOR.remote_script(definition, jobs)
        self.assertNotIn("/tmp/source", command)
        self.assertNotIn("ultimatefish-first.service", command)
        self.assertIn("python3 -c", command)
        arguments = shlex.split(command)
        program = base64.b64decode(arguments[3]).decode()
        payload = json.loads(base64.b64decode(arguments[4]))
        self.assertIn("sys.argv[2]", program)
        self.assertEqual("first", payload["jobs"][0]["id"])

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
            encoded = SUPERVISOR.canonical_json(result).encode()
            self.assertLessEqual(len(encoded), SUPERVISOR.REMOTE_OUTPUT_BUDGET)
            record = result["jobs"][0]["checkpoints"][0]
            self.assertEqual(2_000, record["match_count"])
            self.assertRegex(record["metadata_sha256"], r"^[0-9a-f]{64}$")
            self.assertNotIn("checkpoint-00000", encoded.decode())

    def test_source_binding_globs_are_rejected(self) -> None:
        invalid = config()
        invalid["jobs"][0]["source_bindings"][0]["path"] = "/tmp/source-*"
        with self.assertRaisesRegex(RuntimeError, "one explicit path"):
            SUPERVISOR.validate_config(invalid)

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
        second, _ = SUPERVISOR.supervise(
            config(), state, self.now + dt.timedelta(minutes=5))
        self.assertEqual("NO_CHANGE", second["status"])
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
        self.assertEqual(["second", "third"], output["ready_jobs"])

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
        }}}
        result = SUPERVISOR.start_ready(config(), state, "second")
        self.assertEqual("STARTED", result["status"])
        command.assert_called_once_with(
            ["systemctl", "start", "ultimatefish-second.service"])
        state["report"]["jobs"]["second"]["status"] = "RUNNING"
        with self.assertRaisesRegex(RuntimeError, "not source-certified READY"):
            SUPERVISOR.start_ready(config(), state, "second")


if __name__ == "__main__":
    unittest.main()
