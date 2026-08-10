#!/usr/bin/env python3
"""Event-driven, fail-closed supervision for Ultimate Fish AWS jobs.

Normal ``--once`` runs are read-only.  They query the explicitly configured
fleet, systemd units, resource gates, checkpoint metadata, source bindings, and
S3 certificate objects.  Ordinary checkpoint growth is remembered but emits
``NO_CHANGE``.  A material transition, failure, resource warning, queue-ready
job, or two-hour heartbeat emits one compact JSON document.

Starting work is deliberately a separate ``--advance JOB`` operation.  It
refuses to start an already-running unit, a failed unit, or a job whose exact
dependencies/source bindings have not been certified.  It uses ``systemctl
start`` so the committed runner's own checkpoint/resume contract remains the
only restart mechanism.
"""

from __future__ import annotations

import argparse
import base64
import datetime as dt
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = ROOT / "tools/ultimate_aws_supervision.json"
SCHEMA = "ultimate-aws-supervision-v1"
STATE_SCHEMA = "ultimate-aws-supervision-state-v1"
REMOTE_PREFIX = "ULTIMATE_SUPERVISION_JSON="
# Run Command truncates StandardOutputContent at roughly 24 KiB.  Keep a
# deliberate margin for the sentinel and any provider-side decoration.
REMOTE_OUTPUT_BUDGET = 20_000
TERMINAL_OK = {"CERTIFIED"}
TERMINAL_BAD = {"FAILED", "SOURCE_MISMATCH", "RESOURCE_LIMIT"}
UNIT = re.compile(r"[A-Za-z0-9_.@-]+\.service")


def canonical_json(value: object) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def run(argv: list[str], *, timeout: int = 60) -> str:
    completed = subprocess.run(
        argv, check=False, capture_output=True, text=True, timeout=timeout)
    if completed.returncode:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise RuntimeError(f"command failed ({completed.returncode}): "
                           f"{' '.join(argv)}: {detail}")
    return completed.stdout


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise RuntimeError(f"{path} must contain one JSON object")
    return value


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


def validate_sha(value: object, label: str) -> str:
    text = str(value)
    if len(text) != 64 or any(character not in "0123456789abcdef"
                               for character in text):
        raise RuntimeError(f"{label} must be a lowercase SHA-256")
    return text


def validate_config(config: dict[str, Any]) -> None:
    if config.get("schema") != SCHEMA:
        raise RuntimeError(f"supervision config schema must be {SCHEMA}")
    if config.get("region") != "us-west-2":
        raise RuntimeError("Ultimate AWS supervision is pinned to us-west-2")
    budget = float(config.get("budget_usd", 0))
    if not 0 < budget <= 5_000:
        raise RuntimeError("budget_usd must be positive and at most 5000")
    instances = config.get("instances")
    jobs = config.get("jobs")
    if not isinstance(instances, list) or not instances or len(instances) > 5:
        raise RuntimeError("config must contain one to five explicit instances")
    if not isinstance(jobs, list):
        raise RuntimeError("config jobs must be a list")
    instance_ids: set[str] = set()
    for instance in instances:
        if not isinstance(instance, dict):
            raise RuntimeError("each instance must be an object")
        identifier = str(instance.get("instance_id", ""))
        if not identifier.startswith("i-") or identifier in instance_ids:
            raise RuntimeError("instance ids must be unique explicit EC2 ids")
        instance_ids.add(identifier)
        if float(instance.get("hourly_usd", 0)) <= 0:
            raise RuntimeError(f"{identifier} requires a positive hourly_usd")
        if instance.get("transport", "ssm") not in {"ssm", "local"}:
            raise RuntimeError(f"{identifier} has unsupported transport")
    job_ids: set[str] = set()
    for job in jobs:
        if not isinstance(job, dict):
            raise RuntimeError("each job must be an object")
        identifier = str(job.get("id", ""))
        if not identifier or identifier in job_ids:
            raise RuntimeError("job ids must be nonempty and unique")
        job_ids.add(identifier)
        if job.get("instance_id") not in instance_ids:
            raise RuntimeError(f"{identifier} references an unknown instance")
        unit = str(job.get("unit", ""))
        if not UNIT.fullmatch(unit):
            raise RuntimeError(f"{identifier} requires an explicit service unit")
        for source in job.get("source_bindings", []):
            validate_sha(source.get("sha256"), f"{identifier} source binding")
            if glob_magic(str(source.get("path", ""))):
                raise RuntimeError(
                    f"{identifier} source binding must be one explicit path")
        if job.get("advanceable") and not job.get("source_bindings"):
            raise RuntimeError(
                f"{identifier} is advanceable but has no source binding")
        for certificate in job.get("s3_certificates", []):
            validate_sha(certificate.get("sha256"),
                         f"{identifier} S3 certificate")
            if not certificate.get("bucket") or not certificate.get("key"):
                raise RuntimeError(f"{identifier} S3 certificate lacks bucket/key")
            if not certificate.get("version_id"):
                raise RuntimeError(f"{identifier} S3 certificate lacks VersionId")
    for job in jobs:
        for dependency in job.get("dependencies", []):
            if dependency not in job_ids or dependency == job["id"]:
                raise RuntimeError(f"{job['id']} has an invalid dependency")


def glob_magic(path: str) -> bool:
    return any(character in path for character in "*?[")


def remote_script(instance: dict[str, Any], jobs: list[dict[str, Any]]) -> str:
    payload = {
        "mounts": instance.get("mounts", ["/"]),
        "jobs": [{
            "id": job["id"],
            "unit": job["unit"],
            "checkpoint_paths": job.get("checkpoint_paths", []),
            "completion_paths": job.get("completion_paths", []),
            "source_bindings": job.get("source_bindings", []),
        } for job in jobs],
    }
    encoded = base64.b64encode(canonical_json(payload).encode()).decode()
    program = r'''
import glob,hashlib,json,os,subprocess,sys
payload=json.loads(base64.b64decode(sys.argv[2]))
def command(argv):
 p=subprocess.run(argv,text=True,capture_output=True,check=False)
 return p.returncode,p.stdout.strip(),p.stderr.strip()
def props(unit):
 names=['LoadState','ActiveState','SubState','Result','ExecMainCode',
        'ExecMainStatus','MemoryCurrent','MemoryPeak','StateChangeTimestamp']
 try:
  rc,out,err=command(['systemctl','show',unit,*sum((['-p',n] for n in names),[])])
 except OSError as error:
  return {'probe_error':str(error)}
 if rc: return {'probe_error':err or out or f'systemctl rc={rc}'}
 result={}
 for line in out.splitlines():
  if '=' in line:
   key,value=line.split('=',1); result[key]=value
 return result
def aggregate(patterns):
 result=[]
 for pattern in patterns:
  matches=sorted(glob.glob(pattern))
  if not matches: result.append({'path':pattern,'exists':False}); continue
  digest=hashlib.sha256(); total=0; newest=0
  for path in matches:
   stat=os.stat(path); total+=stat.st_size; newest=max(newest,stat.st_mtime_ns)
   record=json.dumps([path,stat.st_size,stat.st_mtime_ns],
                     separators=(',',':')).encode()
   digest.update(len(record).to_bytes(8,'big')); digest.update(record)
  result.append({'path':pattern,'exists':True,'match_count':len(matches),
                 'total_size':total,'newest_mtime_ns':newest,
                 'metadata_sha256':digest.hexdigest()})
 return result
def sources(paths):
 result=[]
 for path in paths:
  if not os.path.exists(path):
   result.append({'path':path,'exists':False}); continue
  stat=os.stat(path); item={'path':path,'exists':True,'size':stat.st_size,
                            'mtime_ns':stat.st_mtime_ns}
  if stat.st_size>16*1024*1024:
   item['hash_error']='source binding exceeds 16 MiB'
  else:
   digest=hashlib.sha256()
   with open(path,'rb') as stream:
    for block in iter(lambda:stream.read(1024*1024),b''): digest.update(block)
   item['sha256']=digest.hexdigest()
  result.append(item)
 return result
memory={}
try:
 stream=open('/proc/meminfo',encoding='ascii')
except FileNotFoundError:
 stream=[]
for line in stream:
 key,value,*_=line.replace(':','').split()
 if key in {'MemTotal','MemAvailable','SwapTotal','SwapFree'}:
  memory[key]=int(value)*1024
if hasattr(stream,'close'): stream.close()
mounts=[]
for path in payload['mounts']:
 stat=os.statvfs(path)
 mounts.append({'path':path,'free_bytes':stat.f_bavail*stat.f_frsize,
                'total_bytes':stat.f_blocks*stat.f_frsize})
jobs=[]
for job in payload['jobs']:
 source_paths=[binding['path'] for binding in job['source_bindings']]
 jobs.append({'id':job['id'],'unit':props(job['unit']),
             'checkpoints':aggregate(job['checkpoint_paths']),
             'completion':aggregate(job['completion_paths']),
             'sources':sources(source_paths)})
document={'memory':memory,'mounts':mounts,'jobs':jobs}
encoded=json.dumps(document,sort_keys=True,separators=(',',':'))
if len(encoded.encode())>__REMOTE_OUTPUT_BUDGET__:
 encoded=json.dumps({'probe_error':'bounded remote output budget exceeded',
                     'jobs':[]},sort_keys=True,separators=(',',':'))
print('ULTIMATE_SUPERVISION_JSON='+encoded)
'''
    program = program.replace(
        "__REMOTE_OUTPUT_BUDGET__", str(REMOTE_OUTPUT_BUDGET))
    # The wrapper imports only the modules used by the embedded program.  The
    # source is immutable local text; no remote shell interpolation is used.
    encoded_program = base64.b64encode(program.encode()).decode()
    wrapper = (
        "python3 -c 'import base64,sys;exec(base64.b64decode(sys.argv[1]))' "
        f"{encoded_program} {encoded}")
    return wrapper


def ssm_probe(region: str, instance_id: str, command: str) -> dict[str, Any]:
    request = run([
        "aws", "ssm", "send-command", "--region", region,
        "--instance-ids", instance_id,
        "--document-name", "AWS-RunShellScript",
        "--parameters", json.dumps({"commands": [command]}),
        "--comment", "Ultimate Fish read-only supervision",
        "--timeout-seconds", "45", "--output", "json",
    ])
    command_id = json.loads(request)["Command"]["CommandId"]
    deadline = time.monotonic() + 45
    while True:
        try:
            response = json.loads(run([
                "aws", "ssm", "get-command-invocation", "--region", region,
                "--command-id", command_id, "--instance-id", instance_id,
                "--output", "json"], timeout=10))
        except (RuntimeError, subprocess.TimeoutExpired):
            if time.monotonic() >= deadline:
                raise
            time.sleep(1)
            continue
        status = response.get("Status")
        if status in {"Pending", "InProgress", "Delayed"}:
            if time.monotonic() >= deadline:
                raise RuntimeError(f"SSM probe timed out on {instance_id}")
            time.sleep(1)
            continue
        if status != "Success":
            raise RuntimeError(
                f"SSM probe {status} on {instance_id}: " +
                str(response.get("StandardErrorContent", "")))
        for line in str(response.get("StandardOutputContent", "")).splitlines():
            if line.startswith(REMOTE_PREFIX):
                return json.loads(line[len(REMOTE_PREFIX):])
        raise RuntimeError(f"SSM probe on {instance_id} returned no JSON sentinel")


def local_probe(command: str) -> dict[str, Any]:
    output = run(["/bin/sh", "-c", command])
    for line in output.splitlines():
        if line.startswith(REMOTE_PREFIX):
            return json.loads(line[len(REMOTE_PREFIX):])
    raise RuntimeError("local probe returned no JSON sentinel")


def ec2_inventory(config: dict[str, Any]) -> dict[str, dict[str, Any]]:
    identifiers = [entry["instance_id"] for entry in config["instances"]]
    response = json.loads(run([
        "aws", "ec2", "describe-instances", "--region", config["region"],
        "--instance-ids", *identifiers, "--output", "json"]))
    result: dict[str, dict[str, Any]] = {}
    for reservation in response.get("Reservations", []):
        for instance in reservation.get("Instances", []):
            result[instance["InstanceId"]] = instance
    return result


def parse_time(value: str) -> dt.datetime:
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def head_certificate(region: str, certificate: dict[str, Any]) -> dict[str, Any]:
    argv = [
        "aws", "s3api", "head-object", "--region", region,
        "--bucket", certificate["bucket"], "--key", certificate["key"],
        "--version-id", certificate["version_id"], "--output", "json",
    ]
    head = json.loads(run(argv))
    metadata = head.get("Metadata", {})
    expected_size = int(certificate.get("size", head.get("ContentLength", -1)))
    exact = (head.get("VersionId") == certificate["version_id"] and
             int(head.get("ContentLength", -1)) == expected_size and
             metadata.get("sha256") == certificate["sha256"])
    return {
        "bucket": certificate["bucket"], "key": certificate["key"],
        "version_id": head.get("VersionId"),
        "size": int(head.get("ContentLength", -1)),
        "sha256": metadata.get("sha256", ""), "exact": exact,
    }


def unit_status(properties: dict[str, Any]) -> str:
    if properties.get("probe_error"):
        return "FAILED"
    active = properties.get("ActiveState")
    result = properties.get("Result")
    exit_status = properties.get("ExecMainStatus")
    if active in {"active", "activating", "reloading"}:
        return "RUNNING"
    if active == "failed" or (result not in {"", "success"}) or exit_status not in {
            None, "", "0"}:
        return "FAILED"
    return "INACTIVE"


def all_paths_exist(records: list[dict[str, Any]]) -> bool:
    return bool(records) and all(bool(record.get("exists")) for record in records)


def source_exact(job: dict[str, Any], remote: dict[str, Any]) -> bool:
    expected = {binding["path"]: binding["sha256"]
                for binding in job.get("source_bindings", [])}
    actual = {record["path"]: record.get("sha256")
              for record in remote.get("sources", []) if record.get("exists")}
    return not expected or expected == actual


def resource_warnings(instance: dict[str, Any], remote: dict[str, Any]) -> list[str]:
    warnings: list[str] = []
    available = int(remote.get("memory", {}).get("MemAvailable", 0))
    minimum_memory = int(instance.get("minimum_memory_available_bytes", 0))
    if minimum_memory and available < minimum_memory:
        warnings.append(f"memory_available={available}<{minimum_memory}")
    mounts = {record["path"]: record for record in remote.get("mounts", [])}
    for path, minimum in instance.get("minimum_disk_free_bytes", {}).items():
        free = int(mounts.get(path, {}).get("free_bytes", 0))
        if free < int(minimum):
            warnings.append(f"disk_free[{path}]={free}<{minimum}")
    return warnings


def supervise(config: dict[str, Any], previous: dict[str, Any],
              now: dt.datetime) -> tuple[dict[str, Any], dict[str, Any]]:
    inventory = ec2_inventory(config)
    jobs_by_instance: dict[str, list[dict[str, Any]]] = {}
    for job in config["jobs"]:
        jobs_by_instance.setdefault(job["instance_id"], []).append(job)
    instance_results: dict[str, Any] = {}
    total_spend = 0.0
    errors: list[dict[str, str]] = []
    for definition in config["instances"]:
        identifier = definition["instance_id"]
        ec2 = inventory.get(identifier)
        if not ec2:
            errors.append({"instance": identifier, "error": "EC2 instance missing"})
            continue
        launch = parse_time(ec2["LaunchTime"])
        hours = max(0.0, (now - launch).total_seconds() / 3600)
        spend = hours * float(definition["hourly_usd"])
        total_spend += spend
        state = ec2.get("State", {}).get("Name", "unknown")
        remote: dict[str, Any] = {}
        if state == "running":
            try:
                command = remote_script(definition, jobs_by_instance.get(identifier, []))
                remote = (local_probe(command)
                          if definition.get("transport", "ssm") == "local"
                          else ssm_probe(config["region"], identifier, command))
                if remote.get("probe_error"):
                    errors.append({"instance": identifier,
                                   "error": str(remote["probe_error"])})
            except Exception as error:  # one host must not hide the other four
                errors.append({"instance": identifier, "error": str(error)})
        warnings = resource_warnings(definition, remote) if remote else []
        instance_results[identifier] = {
            "name": definition.get("name", identifier), "ec2_state": state,
            "instance_type": ec2.get("InstanceType", ""),
            "launch_time": ec2.get("LaunchTime", ""),
            "estimated_spend_usd": round(spend, 2),
            "resource_warnings": warnings, "remote": remote,
        }

    jobs: dict[str, Any] = {}
    definitions = {job["id"]: job for job in config["jobs"]}
    for identifier, definition in definitions.items():
        host = instance_results.get(definition["instance_id"], {})
        remote_jobs = {entry["id"]: entry for entry in
                       host.get("remote", {}).get("jobs", [])}
        remote = remote_jobs.get(identifier, {})
        status = unit_status(remote.get("unit", {})) if remote else "UNKNOWN"
        if remote and not source_exact(definition, remote):
            status = "SOURCE_MISMATCH"
        completion = all_paths_exist(remote.get("completion", []))
        previous_status = previous.get("report", {}).get("jobs", {}).get(
            identifier, {}).get("status")
        # systemd garbage-collects successful and failed transient units.  Once
        # gone, `systemctl show` fabricates innocuous-looking defaults
        # (LoadState=not-found, Result=success, ExecMainStatus=0).  A tracked
        # RUNNING job may therefore look like a clean inactive exit even though
        # its journal records failure.  Completion evidence may still certify
        # it below; without that evidence, fail closed and delegate diagnosis.
        if (remote.get("unit", {}).get("LoadState") == "not-found" and
                previous_status == "RUNNING" and not completion):
            status = "FAILED"
            remote["unit"]["SupervisionFailure"] = (
                "transient unit disappeared after RUNNING without completion")
        certificates: list[dict[str, Any]] = []
        if completion and definition.get("s3_certificates"):
            try:
                certificates = [head_certificate(config["region"], certificate)
                                for certificate in definition["s3_certificates"]]
            except Exception as error:
                errors.append({"job": identifier, "error": str(error)})
        if completion and certificates and all(item["exact"] for item in certificates):
            status = "CERTIFIED"
        elif completion and status == "INACTIVE":
            status = "COMPLETED_UNCERTIFIED"
        jobs[identifier] = {
            "status": status, "unit": remote.get("unit", {}),
            "checkpoints": remote.get("checkpoints", []),
            "completion": remote.get("completion", []),
            "source_exact": source_exact(definition, remote) if remote else False,
            "certificates": certificates,
            "dependencies": definition.get("dependencies", []),
        }

    # Resolve readiness only after every job's observed status is known.
    for identifier, result in jobs.items():
        if result["status"] != "INACTIVE":
            continue
        dependencies = result["dependencies"]
        if (definitions[identifier].get("advanceable") and
                all(jobs.get(dependency, {}).get("status") in TERMINAL_OK
                    for dependency in dependencies)):
            result["status"] = "READY"
        elif (definitions[identifier].get("queue_stage") and
              all(jobs.get(dependency, {}).get("status") in TERMINAL_OK
                  for dependency in dependencies)):
            result["status"] = "AWAITING_STAGE"

    budget = float(config["budget_usd"])
    if total_spend >= budget:
        errors.append({"fleet": "budget", "error":
                       f"estimated spend {total_spend:.2f} reached {budget:.2f}"})
    elif total_spend >= budget * float(config.get("budget_warning_fraction", .9)):
        errors.append({"fleet": "budget-warning", "error":
                       f"estimated spend {total_spend:.2f} approaches {budget:.2f}"})

    report = {
        "schema": SCHEMA, "observed_at": now.isoformat(),
        "estimated_spend_usd": round(total_spend, 2),
        "fleet_burn_usd_per_hour": round(sum(float(instance["hourly_usd"])
                                              for instance in config["instances"]), 2),
        "instances": instance_results, "jobs": jobs, "errors": errors,
    }
    event_view = {
        "instances": {identifier: {
            "ec2_state": value["ec2_state"],
            "resource_warnings": value["resource_warnings"],
        } for identifier, value in instance_results.items()},
        "jobs": {identifier: value["status"] for identifier, value in jobs.items()},
        "errors": errors,
    }
    digest = sha256_bytes(canonical_json(event_view).encode())
    last_digest = previous.get("event_digest")
    last_report = float(previous.get("last_regular_report_epoch", 0))
    report_interval = int(config.get("regular_report_seconds", 7200))
    heartbeat = now.timestamp() - last_report >= report_interval
    changed = digest != last_digest
    event_type = "CHANGE" if changed else "HEARTBEAT" if heartbeat else "NO_CHANGE"
    resource_risk = any(value["resource_warnings"]
                        for value in instance_results.values())
    severity = ("error" if errors or any(
        result["status"] in TERMINAL_BAD for result in jobs.values())
                else "warning" if resource_risk else "info")
    ready = [identifier for identifier, value in jobs.items()
             if value["status"] in {"READY", "AWAITING_STAGE"}]
    previous_jobs = previous.get("report", {}).get("jobs", {})
    changed_jobs = [identifier for identifier, value in jobs.items()
                    if previous_jobs.get(identifier, {}).get("status") !=
                    value["status"]]
    selected_jobs = sorted(jobs) if heartbeat or not last_digest else changed_jobs
    compact_jobs = {
        identifier: {
            "status": jobs[identifier]["status"],
            "previous_status": previous_jobs.get(identifier, {}).get("status"),
            "unit_active": jobs[identifier]["unit"].get("ActiveState", ""),
            "unit_result": jobs[identifier]["unit"].get("Result", ""),
            "exit_status": jobs[identifier]["unit"].get("ExecMainStatus", ""),
            "checkpoint_files": sum(
                int(record.get("match_count",
                               bool(record.get("exists"))))
                for record in jobs[identifier]["checkpoints"]),
            "completion_files": sum(
                int(record.get("match_count",
                               bool(record.get("exists"))))
                for record in jobs[identifier]["completion"]),
            "certificates": [{
                "key": certificate["key"],
                "version_id": certificate["version_id"],
                "sha256": certificate["sha256"],
                "size": certificate["size"],
                "exact": certificate["exact"],
            } for certificate in jobs[identifier]["certificates"]],
        } for identifier in selected_jobs
    }
    output = {
        "status": event_type, "severity": severity,
        "delegate_sol": severity == "error", "ready_jobs": ready,
        "report": {
            "observed_at": report["observed_at"],
            "estimated_spend_usd": report["estimated_spend_usd"],
            "fleet_burn_usd_per_hour": report["fleet_burn_usd_per_hour"],
            "instances": {identifier: {
                "ec2_state": value["ec2_state"],
                "resource_warnings": value["resource_warnings"],
            } for identifier, value in instance_results.items()
               if heartbeat or not last_digest or value["resource_warnings"]},
            "jobs": compact_jobs,
            "errors": errors,
        },
    }
    state = {
        "schema": STATE_SCHEMA, "event_digest": digest,
        "last_observed_epoch": now.timestamp(),
        "last_regular_report_epoch": (now.timestamp() if heartbeat or changed
                                       else last_report),
        "report": report,
    }
    return output, state


def start_ready(config: dict[str, Any], state: dict[str, Any], job_id: str) -> dict[str, Any]:
    jobs = {job["id"]: job for job in config["jobs"]}
    if job_id not in jobs:
        raise RuntimeError(f"unknown queued job {job_id}")
    observed = state.get("report", {}).get("jobs", {}).get(job_id, {})
    if observed.get("status") != "READY" or not observed.get("source_exact"):
        raise RuntimeError(f"{job_id} is not source-certified READY")
    job = jobs[job_id]
    if not job.get("advanceable"):
        raise RuntimeError(f"{job_id} is monitor-only and cannot be advanced")
    for dependency in job.get("dependencies", []):
        dependency_status = state.get("report", {}).get("jobs", {}).get(
            dependency, {}).get("status")
        if dependency_status != "CERTIFIED":
            raise RuntimeError(f"{job_id} dependency {dependency} is not certified")
    instance = next(item for item in config["instances"]
                    if item["instance_id"] == job["instance_id"])
    if instance.get("transport", "ssm") == "local":
        run(["systemctl", "start", "--no-block", job["unit"]])
    else:
        # A synchronous start waits for Type=oneshot completion and can exceed
        # the bounded SSM probe even though the unit started successfully.
        # Queue the exact validated unit, then accept only active/activating.
        unit = job["unit"]
        ssm_probe(config["region"], job["instance_id"],
                  f"systemctl start --no-block {unit} && "
                  f"state=$(systemctl show {unit} "
                  "--property=ActiveState --value) && "
                  '{ test "$state" = active || '
                  'test "$state" = activating; } && '
                  f"echo {REMOTE_PREFIX}{{}}")
    return {"status": "STARTED", "job": job_id,
            "instance_id": job["instance_id"], "unit": job["unit"],
            "resumable": True}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--state", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--once", action="store_true")
    mode.add_argument("--advance", metavar="JOB")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    config = load_json(args.config.resolve())
    validate_config(config)
    state_path = (args.state or
                  Path(config.get("state_path", ".git/ultimate-aws-supervision.json")))
    if not state_path.is_absolute():
        state_path = ROOT / state_path
    lock_path = state_path.with_suffix(state_path.suffix + ".lock")
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    with lock_path.open("w", encoding="utf-8") as lock:
        try:
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            print("NO_CHANGE")
            return 0
        previous = load_json(state_path) if state_path.exists() else {}
        if args.advance:
            result = start_ready(config, previous, args.advance)
            print(canonical_json(result) if args.json else
                  f"STARTED {args.advance}")
            return 0
        output, state = supervise(
            config, previous, dt.datetime.now(dt.timezone.utc))
        atomic_json(state_path, state)
        if output["status"] == "NO_CHANGE":
            print("NO_CHANGE")
        else:
            print(canonical_json(output) if args.json else
                  f"{output['status']} severity={output['severity']}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(canonical_json({"status": "SUPERVISOR_ERROR", "severity": "error",
                              "delegate_sol": True, "error": str(error)}))
        raise SystemExit(2)
