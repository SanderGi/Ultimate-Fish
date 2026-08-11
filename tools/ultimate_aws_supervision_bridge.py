#!/usr/bin/env python3
"""Bridge host-network AWS supervision into a local Codex event consumer.

``collect`` is intended for a host scheduler with normal AWS credentials.  It
runs the committed supervisor, records a small health heartbeat, durably spools
only material events, and advances already-certified READY jobs through the
supervisor's existing fail-closed gate.  ``consume`` performs no network I/O;
it returns each durable event once and otherwise prints exactly ``NO_CHANGE``.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "ultimate-aws-supervision-bridge-v1"
DEFAULT_HEALTH = ROOT / ".git/ultimate-aws-supervision-health.json"
DEFAULT_EVENTS = ROOT / ".git/ultimate-aws-supervision-events"
DEFAULT_CURSOR = ROOT / ".git/ultimate-aws-luna-cursor.json"
DEFAULT_SUPERVISOR = ROOT / "tools/supervise_ultimate_aws.py"


def canonical_json(value: object) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def atomic_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix=path.name + ".", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(value, stream, sort_keys=True, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass


def load_object(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise RuntimeError(f"{path} must contain one JSON object")
    return value


def run_supervisor(python: Path, supervisor: Path,
                   arguments: list[str]) -> tuple[int, str, str]:
    environment = os.environ.copy()
    path_parts = ["/usr/local/bin", "/opt/homebrew/bin", "/usr/bin", "/bin"]
    existing = environment.get("PATH", "")
    environment["PATH"] = ":".join(path_parts + ([existing] if existing else []))
    completed = subprocess.run(
        [str(python), str(supervisor), *arguments], cwd=ROOT,
        env=environment, text=True, capture_output=True, check=False,
        timeout=240)
    return completed.returncode, completed.stdout.strip(), completed.stderr.strip()


def parse_supervisor_output(output: str) -> dict[str, Any] | None:
    if output == "NO_CHANGE":
        return None
    lines = [line for line in output.splitlines() if line.strip()]
    if len(lines) != 1:
        raise RuntimeError("supervisor must emit exactly one nonempty line")
    value = json.loads(lines[0])
    if not isinstance(value, dict) or not value.get("status"):
        raise RuntimeError("supervisor output must be a status object")
    return value


def ledger_statuses(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.startswith("| `"):
            continue
        fields = [field.strip() for field in line.strip().strip("|").split("|")]
        if len(fields) != 11 or fields[3] == "—":
            continue
        result[fields[3].strip("`")] = fields[4].strip("*").lower()
    return result


def reconcile_ledger(repo: Path, event: dict[str, Any], *, commit: bool,
                     python: Path) -> dict[str, Any]:
    """Apply monotone material state transitions from one durable event."""
    readme = repo / "tablebases/README.md"
    plot = repo / "tablebases/ultimate-tablebase-grid.png"
    current = ledger_statuses(readme)
    updates: dict[str, str] = {}
    pending: list[dict[str, str]] = []
    terminal = {"certified", "draw", "deferred"}
    for job_id, job in event.get("report", {}).get("jobs", {}).items():
        status = str(job.get("status", ""))
        for filename in job.get("ledger_files", []):
            existing = current.get(str(filename))
            if existing is None or existing in terminal:
                continue
            if status == "RUNNING":
                updates[str(filename)] = "computing"
            elif status in {"FAILED", "SOURCE_MISMATCH", "RESOURCE_LIMIT"}:
                updates[str(filename)] = "blocked"
            elif status == "CERTIFIED" and job.get("ledger_certifies"):
                # A VersionId proves storage, not the per-side W/L/D and
                # reachability cells.  Keep the current state until the exact
                # result certificate is imported instead of exposing a stale
                # pre-information result as current.
                pending.append({"job": str(job_id), "filename": str(filename)})
    updates = {filename: status for filename, status in updates.items()
               if current.get(filename) != status}
    if not updates:
        return {"changed": False, "updates": {},
                "pending_certification": pending, "commit": ""}
    if commit:
        branch = subprocess.run(
            ["git", "branch", "--show-current"], cwd=repo, text=True,
            capture_output=True, check=True).stdout.strip()
        head = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=repo, text=True,
            capture_output=True, check=True).stdout.strip()
        upstream = subprocess.run(
            ["git", "rev-parse", "origin/master"], cwd=repo, text=True,
            capture_output=True, check=True).stdout.strip()
        dirty = subprocess.run(
            ["git", "status", "--porcelain", "--", str(readme), str(plot)],
            cwd=repo, text=True, capture_output=True, check=True).stdout.strip()
        if branch != "master" or head != upstream or dirty:
            raise RuntimeError(
                "ledger auto-commit requires clean canonical master==origin/master")
    command = [str(python), str(repo / "tools/update_ultimate_tablebase_ledger.py")]
    for filename, status in sorted(updates.items()):
        command.extend(["--set-status", f"{filename}={status}"])
    subprocess.run(command, cwd=repo, check=True)
    subprocess.run([
        str(python), str(repo / "tools/plot_ultimate_tablebases.py"),
        "--output", str(plot)], cwd=repo, check=True)
    commit_sha = ""
    if commit:
        subprocess.run([str(python), str(repo / "tests/test_ultimate_tablebase_ledger.py")],
                       cwd=repo, check=True)
        subprocess.run(["git", "add", "--", str(readme), str(plot)],
                       cwd=repo, check=True)
        subprocess.run([
            "git", "commit", "-m", "tablebases: reconcile AWS ledger",
            "--", str(readme), str(plot)], cwd=repo, check=True)
        commit_sha = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=repo, text=True,
            capture_output=True, check=True).stdout.strip()
        subprocess.run(["git", "push", "origin", "master"], cwd=repo, check=True)
    return {"changed": True, "updates": updates,
            "pending_certification": pending, "commit": commit_sha}


def spool_event(events: Path, event: dict[str, Any]) -> str:
    payload = canonical_json(event).encode("utf-8")
    digest = hashlib.sha256(payload).hexdigest()
    destination = events / f"{digest}.json"
    if not destination.exists():
        atomic_json(destination, event)
    return digest


def collect(*, python: Path, supervisor: Path, health: Path, events: Path,
            auto_advance: bool = True,
            now: dt.datetime | None = None) -> dict[str, Any]:
    observed = now or dt.datetime.now(dt.timezone.utc)
    code, stdout, stderr = run_supervisor(
        python, supervisor, ["--once", "--json"])
    event: dict[str, Any] | None
    try:
        event = parse_supervisor_output(stdout)
    except Exception as error:
        event = {
            "status": "HOST_COLLECTOR_ERROR", "severity": "error",
            "delegate_sol": True,
            "error": f"malformed supervisor output: {error}",
            "supervisor_exit_code": code,
        }
    if code and event is None:
        event = {
            "status": "HOST_COLLECTOR_ERROR", "severity": "error",
            "delegate_sol": True,
            "error": stderr or f"supervisor exited {code} without an event",
            "supervisor_exit_code": code,
        }

    actions: list[dict[str, Any]] = []
    if event is not None and auto_advance and code == 0:
        jobs = event.get("report", {}).get("jobs", {})
        for job_id in event.get("ready_jobs", []):
            if jobs.get(job_id, {}).get("status") != "READY":
                continue
            action_code, action_stdout, action_stderr = run_supervisor(
                python, supervisor, ["--advance", str(job_id), "--json"])
            try:
                action = parse_supervisor_output(action_stdout)
            except Exception as error:
                action = {
                    "status": "ADVANCE_ERROR", "job": job_id,
                    "error": f"malformed advance output: {error}",
                }
            if action is None:
                action = {"status": "ADVANCE_ERROR", "job": job_id,
                          "error": "advance emitted NO_CHANGE"}
            action["exit_code"] = action_code
            if action_stderr:
                action["stderr"] = action_stderr
            actions.append(action)
        if actions:
            event = dict(event)
            event["host_actions"] = actions
            if any(action.get("status") != "STARTED" or action.get("exit_code")
                   for action in actions):
                event["severity"] = "error"
                event["delegate_sol"] = True

    digest = spool_event(events, event) if event is not None else ""
    # Tests can pin time; production health records completion so a long but
    # successful bounded poll does not look stale immediately after it exits.
    completed = observed if now is not None else dt.datetime.now(dt.timezone.utc)
    health_value = {
        "schema": SCHEMA, "observed_at": completed.isoformat(),
        "observed_epoch": completed.timestamp(),
        "supervisor_exit_code": code,
        "supervisor_status": event.get("status") if event else "NO_CHANGE",
        "event_digest": digest,
        "event_spooled": bool(event),
    }
    if stderr:
        health_value["supervisor_stderr"] = stderr
    atomic_json(health, health_value)
    return health_value


def consume(*, health: Path, events: Path, cursor: Path,
            stale_seconds: int = 900,
            now: dt.datetime | None = None) -> dict[str, Any] | None:
    cursor_value = load_object(cursor) if cursor.exists() else {
        "schema": SCHEMA, "consumed": [], "stale_health_epoch": None}
    consumed = set(str(item) for item in cursor_value.get("consumed", []))
    for event_path in sorted(events.glob("*.json")) if events.exists() else []:
        if event_path.name in consumed:
            continue
        event = load_object(event_path)
        consumed.add(event_path.name)
        cursor_value["consumed"] = sorted(consumed)
        atomic_json(cursor, cursor_value)
        return event

    current = now or dt.datetime.now(dt.timezone.utc)
    health_value = load_object(health) if health.exists() else {}
    observed_epoch = float(health_value.get("observed_epoch", 0))
    if current.timestamp() - observed_epoch > stale_seconds:
        last_stale = cursor_value.get("stale_health_epoch")
        if last_stale != observed_epoch:
            cursor_value["stale_health_epoch"] = observed_epoch
            atomic_json(cursor, cursor_value)
            return {
                "status": "HOST_COLLECTOR_STALE", "severity": "error",
                "delegate_sol": True,
                "error": ("host AWS collector health is missing or older than "
                          f"{stale_seconds} seconds; fleet state is unknown"),
                "last_observed_epoch": observed_epoch,
            }
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("collect", "consume"))
    parser.add_argument("--python", type=Path, default=Path(os.sys.executable))
    parser.add_argument("--supervisor", type=Path, default=DEFAULT_SUPERVISOR)
    parser.add_argument("--health", type=Path, default=DEFAULT_HEALTH)
    parser.add_argument("--events", type=Path, default=DEFAULT_EVENTS)
    parser.add_argument("--cursor", type=Path, default=DEFAULT_CURSOR)
    parser.add_argument("--stale-seconds", type=int, default=900)
    parser.add_argument("--no-auto-advance", action="store_true")
    parser.add_argument("--reconcile-ledger", action="store_true")
    parser.add_argument("--commit-ledger", action="store_true")
    parser.add_argument("--ledger-repo", type=Path, default=ROOT)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    if args.mode == "collect":
        result = collect(
            python=args.python.resolve(), supervisor=args.supervisor.resolve(),
            health=args.health.resolve(), events=args.events.resolve(),
            auto_advance=not args.no_auto_advance)
        if args.json:
            print(canonical_json(result))
        return 0
    event = consume(
        health=args.health.resolve(), events=args.events.resolve(),
        cursor=args.cursor.resolve(), stale_seconds=args.stale_seconds)
    if args.commit_ledger and not args.reconcile_ledger:
        parser.error("--commit-ledger requires --reconcile-ledger")
    if event is not None and args.reconcile_ledger:
        event = dict(event)
        event["ledger"] = reconcile_ledger(
            args.ledger_repo.resolve(), event, commit=args.commit_ledger,
            python=args.python.resolve())
    print(canonical_json(event) if event is not None else "NO_CHANGE")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(canonical_json({
            "status": "BRIDGE_ERROR", "severity": "error",
            "delegate_sol": True, "error": str(error)}))
        raise SystemExit(2)
