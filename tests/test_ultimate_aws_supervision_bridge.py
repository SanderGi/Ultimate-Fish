#!/usr/bin/env python3

from __future__ import annotations

import datetime as dt
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]


def module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


BRIDGE = module("bridge", ROOT / "tools/ultimate_aws_supervision_bridge.py")
INSTALLER = module(
    "installer", ROOT / "tools/install_ultimate_aws_supervision_launchd.py")


class BridgeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.health = self.root / "health.json"
        self.events = self.root / "events"
        self.cursor = self.root / "cursor.json"
        self.now = dt.datetime(2026, 8, 10, tzinfo=dt.timezone.utc)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    @mock.patch.object(BRIDGE, "run_supervisor")
    def test_quiet_collection_and_consumption_are_no_change(
            self, run: mock.Mock) -> None:
        run.return_value = (0, "NO_CHANGE", "")
        health = BRIDGE.collect(
            python=Path("/python"), supervisor=Path("/supervisor"),
            health=self.health, events=self.events, now=self.now)
        self.assertEqual("NO_CHANGE", health["supervisor_status"])
        self.assertFalse(self.events.exists())
        self.assertIsNone(BRIDGE.consume(
            health=self.health, events=self.events, cursor=self.cursor,
            now=self.now + dt.timedelta(minutes=5)))

    @mock.patch.object(BRIDGE, "run_supervisor")
    def test_material_event_is_durable_and_consumed_once(
            self, run: mock.Mock) -> None:
        event = {"status": "CHANGE", "severity": "info", "ready_jobs": [],
                 "report": {"jobs": {}}}
        run.return_value = (0, json.dumps(event), "")
        BRIDGE.collect(
            python=Path("/python"), supervisor=Path("/supervisor"),
            health=self.health, events=self.events, now=self.now)
        self.assertEqual(event, BRIDGE.consume(
            health=self.health, events=self.events, cursor=self.cursor,
            now=self.now))
        self.assertIsNone(BRIDGE.consume(
            health=self.health, events=self.events, cursor=self.cursor,
            now=self.now))
        self.assertEqual(1, len(list(self.events.glob("*.json"))))

    def test_event_backlog_coalesces_to_newest_spooled_snapshot(self) -> None:
        self.events.mkdir()
        BRIDGE.atomic_json(self.health, {
            "schema": BRIDGE.SCHEMA,
            "observed_epoch": self.now.timestamp(),
        })
        older = self.events / ("f" * 64 + ".json")
        newer = self.events / ("0" * 64 + ".json")
        BRIDGE.atomic_json(older, {
            "status": "CHANGE", "report": {"observed_at": "older"}})
        BRIDGE.atomic_json(newer, {
            "status": "CHANGE", "report": {"observed_at": "newer"}})
        os.utime(older, ns=(1_000_000_000, 1_000_000_000))
        os.utime(newer, ns=(2_000_000_000, 2_000_000_000))

        event = BRIDGE.consume(
            health=self.health, events=self.events, cursor=self.cursor,
            now=self.now)

        self.assertEqual("newer", event["report"]["observed_at"])
        cursor = json.loads(self.cursor.read_text())
        self.assertEqual({older.name, newer.name}, set(cursor["consumed"]))
        self.assertEqual(2_000_000_000, cursor["last_event_mtime_ns"])
        self.assertIsNone(BRIDGE.consume(
            health=self.health, events=self.events, cursor=self.cursor,
            now=self.now))

    @mock.patch.object(BRIDGE, "run_supervisor")
    def test_certified_ready_job_auto_advances_through_exact_gate(
            self, run: mock.Mock) -> None:
        event = {"status": "CHANGE", "severity": "info",
                 "ready_jobs": ["next"],
                 "report": {"jobs": {"next": {"status": "READY"}}}}
        run.side_effect = [
            (0, json.dumps(event), ""),
            (0, json.dumps({"status": "STARTED", "job": "next"}), ""),
        ]
        BRIDGE.collect(
            python=Path("/python"), supervisor=Path("/supervisor"),
            health=self.health, events=self.events, now=self.now)
        consumed = BRIDGE.consume(
            health=self.health, events=self.events, cursor=self.cursor,
            now=self.now)
        self.assertEqual("STARTED", consumed["host_actions"][0]["status"])
        self.assertEqual(
            ["--advance", "next", "--json"], run.call_args_list[1].args[2])

    @mock.patch.object(BRIDGE, "run_supervisor")
    def test_cpu_mismatch_is_rebalanced_before_backfill(
            self, run: mock.Mock) -> None:
        event = {"status": "CHANGE", "severity": "info",
                 "cpu_rebalance_jobs": ["oversized"],
                 "ready_jobs": ["next"],
                 "report": {"jobs": {"next": {"status": "READY"}}}}
        run.side_effect = [
            (0, json.dumps(event), ""),
            (0, json.dumps({"status": "CPU_REBALANCED",
                            "job": "oversized"}), ""),
            (0, json.dumps({"status": "STARTED", "job": "next"}), ""),
        ]
        BRIDGE.collect(
            python=Path("/python"), supervisor=Path("/supervisor"),
            health=self.health, events=self.events, now=self.now)
        consumed = BRIDGE.consume(
            health=self.health, events=self.events, cursor=self.cursor,
            now=self.now)
        self.assertEqual(
            ["CPU_REBALANCED", "STARTED"],
            [item["status"] for item in consumed["host_actions"]])
        self.assertEqual(
            ["--rebalance-cpu", "oversized", "--json"],
            run.call_args_list[1].args[2])
        self.assertEqual(
            ["--advance", "next", "--json"], run.call_args_list[2].args[2])

    @mock.patch.object(BRIDGE, "run_supervisor")
    def test_failed_cpu_rebalance_blocks_backfill(
            self, run: mock.Mock) -> None:
        event = {"status": "CHANGE", "severity": "info",
                 "cpu_rebalance_jobs": ["oversized"],
                 "ready_jobs": ["next"],
                 "report": {"jobs": {"next": {"status": "READY"}}}}
        run.side_effect = [
            (0, json.dumps(event), ""),
            (1, json.dumps({"status": "CPU_REBALANCE_ERROR",
                            "job": "oversized"}), "overlap"),
        ]
        BRIDGE.collect(
            python=Path("/python"), supervisor=Path("/supervisor"),
            health=self.health, events=self.events, now=self.now)
        consumed = BRIDGE.consume(
            health=self.health, events=self.events, cursor=self.cursor,
            now=self.now)
        self.assertEqual(2, run.call_count)
        self.assertEqual("error", consumed["severity"])
        self.assertTrue(consumed["delegate_sol"])

    def test_stale_collector_reports_once_without_claiming_fleet_failure(
            self) -> None:
        BRIDGE.atomic_json(self.health, {
            "observed_epoch": self.now.timestamp(), "schema": BRIDGE.SCHEMA})
        event = BRIDGE.consume(
            health=self.health, events=self.events, cursor=self.cursor,
            now=self.now + dt.timedelta(minutes=16))
        self.assertEqual("HOST_COLLECTOR_STALE", event["status"])
        self.assertIn("fleet state is unknown", event["error"])
        self.assertIsNone(BRIDGE.consume(
            health=self.health, events=self.events, cursor=self.cursor,
            now=self.now + dt.timedelta(minutes=17)))

    @mock.patch.object(BRIDGE.subprocess, "run")
    def test_event_reconciles_running_ledger_without_downgrading_certified(
            self, command: mock.Mock) -> None:
        repo = self.root / "repo"
        tablebases = repo / "tablebases"
        tablebases.mkdir(parents=True)
        (repo / "tools").mkdir()
        (repo / "tests").mkdir()
        (tablebases / "README.md").write_text(
            "| `same:a+b` | A | same | `kabk.uftb` | **PLANNED** | 1 | concrete | — | — | — | — |\n"
            "| `same:c+d` | C | same | `kcdk.uftb` | **CERTIFIED** | 1 | concrete | 1 / 0 / 0 | 1 / 0 / 0 | 1 / 0; 1 / 0 | S3 |\n")
        event = {"report": {"jobs": {"job": {
            "status": "RUNNING", "ledger_files": ["kabk.uftb", "kcdk.uftb"],
            "ledger_certifies": True,
        }}}}
        result = BRIDGE.reconcile_ledger(
            repo, event, commit=False, python=Path("/python"))
        self.assertTrue(result["changed"])
        self.assertEqual({"kabk.uftb": "computing"}, result["updates"])
        self.assertEqual(2, command.call_count)
        self.assertIn("kabk.uftb=computing", command.call_args_list[0].args[0])

    def test_certification_requires_exact_result_import(self) -> None:
        repo = self.root / "repo"
        tablebases = repo / "tablebases"
        tablebases.mkdir(parents=True)
        (tablebases / "README.md").write_text(
            "| `same:a+b` | A | same | `kabk.uftb` | **COMPUTING** | 1 | concrete | — | — | — | — |\n")
        event = {"report": {"jobs": {"job": {
            "status": "CERTIFIED", "ledger_files": ["kabk.uftb"],
            "ledger_certifies": True,
        }}}}
        result = BRIDGE.reconcile_ledger(
            repo, event, commit=False, python=Path("/python"))
        self.assertFalse(result["changed"])
        self.assertEqual(
            [{"job": "job", "filename": "kabk.uftb"}],
            result["pending_certification"])

    @mock.patch.object(BRIDGE.subprocess, "run")
    def test_certification_imports_exact_ledger_result(
            self, command: mock.Mock) -> None:
        repo = self.root / "repo"
        tablebases = repo / "tablebases"
        tablebases.mkdir(parents=True)
        (repo / "tools").mkdir()
        (tablebases / "README.md").write_text(
            "| `same:a+b` | A | same | `kabk.uftb` | **COMPUTING** | 2 | concrete | — | — | — | — |\n")
        exact = {
            "result_kind": "information v2", "first": "1 (1) / 0 / 0",
            "second": "0 / 1 / 1", "reachability": "1 / 1; 2 / 0",
            "storage": "S3 exact",
        }
        event = {"report": {"jobs": {"job": {
            "status": "CERTIFIED", "ledger_files": ["kabk.uftb"],
            "ledger_certifies": True,
            "ledger_results": {"kabk.uftb": exact},
        }}}}
        result = BRIDGE.reconcile_ledger(
            repo, event, commit=False, python=Path("/python"))
        self.assertTrue(result["changed"])
        self.assertEqual(["kabk.uftb"], result["certified"])
        arguments = command.call_args_list[0].args[0]
        self.assertIn("--set-certified", arguments)
        self.assertTrue(any(value.startswith("kabk.uftb={")
                            for value in arguments))

    def test_launch_agent_is_host_level_five_minute_singleton(self) -> None:
        value = INSTALLER.launch_agent(
            Path("/Application Support/UltimateFishAWS/commit"),
            Path("/python3"))
        self.assertEqual(300, value["StartInterval"])
        self.assertTrue(value["RunAtLoad"])
        self.assertEqual("org.ultimatefish.aws-supervisor", value["Label"])
        self.assertIn("collect", value["ProgramArguments"])
        self.assertIn("/private/tmp/ultimatefish-aws-supervision/health.json",
                      value["ProgramArguments"])
        self.assertNotIn(str(ROOT), value["ProgramArguments"])
        self.assertNotIn("aws_access_key", json.dumps(value).lower())


if __name__ == "__main__":
    unittest.main()
