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
from concurrent.futures import ThreadPoolExecutor, as_completed
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
# SSM rejects an oversized ``commands`` parameter before the remote program
# runs (the effective limit is just under 100 KiB in this path).  Keep a
# stricter local bound so a crowded host fails closed instead of producing a
# transport-only supervision error.  Source paths are deduplicated below,
# which keeps the concrete batch comfortably below this bound.
REMOTE_REQUEST_BUDGET = 97_000
TERMINAL_OK = {"CERTIFIED"}
TERMINAL_BAD = {"FAILED", "SOURCE_MISMATCH", "RESOURCE_LIMIT"}
UNIT = re.compile(r"[A-Za-z0-9_.@-]+\.service")
CPU_SET = re.compile(r"[0-9,-]+")
DEFAULT_TARGET_UTILIZATION = .70
DEFAULT_UNDERUTILIZED_FRACTION = .50
DEFAULT_UNDERUTILIZED_SAMPLES = 2


def canonical_json(value: object) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def run(argv: list[str], *, timeout: int = 60) -> str:
    try:
        completed = subprocess.run(
            argv, check=False, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        label = " ".join(argv[:3])
        raise RuntimeError(f"command timed out after {timeout}s: {label}") from None
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
    probe_workers = int(config.get("probe_workers", 2))
    if not 1 <= probe_workers <= 3:
        raise RuntimeError("probe_workers must be between one and three")
    target = float(config.get(
        "target_measured_utilization_fraction", DEFAULT_TARGET_UTILIZATION))
    underutilized = float(config.get(
        "underutilized_fraction", DEFAULT_UNDERUTILIZED_FRACTION))
    samples = int(config.get(
        "underutilized_samples", DEFAULT_UNDERUTILIZED_SAMPLES))
    if not 0 < underutilized < target <= 1:
        raise RuntimeError(
            "utilization fractions must satisfy 0 < underutilized < target <= 1")
    if samples < 2:
        raise RuntimeError("underutilized_samples must be at least two")
    instances = config.get("instances")
    jobs = config.get("jobs")
    if not isinstance(instances, list) or not instances or len(instances) > 5:
        raise RuntimeError("config must contain one to five explicit instances")
    if not isinstance(jobs, list):
        raise RuntimeError("config jobs must be a list")
    instance_ids: set[str] = set()
    capacities: dict[str, int] = {}
    for instance in instances:
        if not isinstance(instance, dict):
            raise RuntimeError("each instance must be an object")
        identifier = str(instance.get("instance_id", ""))
        if not identifier.startswith("i-") or identifier in instance_ids:
            raise RuntimeError("instance ids must be unique explicit EC2 ids")
        instance_ids.add(identifier)
        if float(instance.get("hourly_usd", 0)) <= 0:
            raise RuntimeError(f"{identifier} requires a positive hourly_usd")
        if int(instance.get("vcpus", 0)) <= 0:
            raise RuntimeError(f"{identifier} requires a positive vcpus count")
        capacities[identifier] = int(instance["vcpus"])
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
        if job.get("superseded_by") and job.get("advanceable"):
            raise RuntimeError(
                f"{identifier} is superseded but remains advanceable")
        for certificate in job.get("s3_certificates", []):
            validate_sha(certificate.get("sha256"),
                         f"{identifier} S3 certificate")
            if not certificate.get("bucket") or not certificate.get("key"):
                raise RuntimeError(f"{identifier} S3 certificate lacks bucket/key")
            if not certificate.get("version_id"):
                raise RuntimeError(f"{identifier} S3 certificate lacks VersionId")
        if job.get("s3_only_certified"):
            if not job.get("s3_certificates"):
                raise RuntimeError(
                    f"{identifier} S3-only certification requires certificates")
            if job.get("advanceable") or job.get("source_bindings"):
                raise RuntimeError(
                    f"{identifier} S3-only certification is archival only")
        ledger_files = job.get("ledger_files", [])
        if (not isinstance(ledger_files, list) or
                any(not re.fullmatch(r"[a-z0-9]+\.uftb", str(item))
                    for item in ledger_files)):
            raise RuntimeError(f"{identifier} has invalid ledger_files")
        ledger_results = job.get("ledger_results", {})
        if (not isinstance(ledger_results, dict) or
                set(ledger_results) - set(map(str, ledger_files))):
            raise RuntimeError(f"{identifier} has invalid ledger_results")
        for filename, result in ledger_results.items():
            if (not isinstance(result, dict) or set(result) != {
                    "result_kind", "first", "second", "reachability",
                    "storage"} or any(not isinstance(value, str) or not value
                                      for value in result.values())):
                raise RuntimeError(
                    f"{identifier} has invalid ledger result for {filename}")
        expected_cpus = job.get("expected_allowed_cpus")
        if expected_cpus is not None:
            try:
                parsed = parse_cpu_set(
                    expected_cpus, capacities[str(job["instance_id"])])
            except ValueError as error:
                raise RuntimeError(
                    f"{identifier} has invalid expected_allowed_cpus: {error}") from error
            if not parsed:
                raise RuntimeError(
                    f"{identifier} expected_allowed_cpus must not be empty")
        resources = job.get("resource_requirements")
        if resources is not None:
            if not isinstance(resources, dict) or set(resources) != {
                    "cpu_threads", "memory_peak_bytes", "disk_peak_bytes"}:
                raise RuntimeError(
                    f"{identifier} has invalid resource_requirements")
            threads = int(resources.get("cpu_threads", 0))
            memory = int(resources.get("memory_peak_bytes", 0))
            disks = resources.get("disk_peak_bytes")
            if (threads <= 0 or memory <= 0 or not isinstance(disks, dict) or
                    len(disks) != 1):
                raise RuntimeError(
                    f"{identifier} requires positive resources on one mount")
            instance = next(item for item in instances
                            if item["instance_id"] == job["instance_id"])
            mounts = set(map(str, instance.get("mounts", ["/"])))
            if (any(str(path) not in mounts or int(extent) <= 0
                    for path, extent in disks.items())):
                raise RuntimeError(
                    f"{identifier} resource disk requirements are invalid")
            if expected_cpus is not None and len(parsed) != threads:
                raise RuntimeError(
                    f"{identifier} CPU set must equal cpu_threads")
        if job.get("advanceable") and resources is None:
            raise RuntimeError(
                f"{identifier} advanceable job lacks resource_requirements")
        if job.get("advanceable") and expected_cpus is None:
            raise RuntimeError(
                f"{identifier} advanceable job lacks expected_allowed_cpus")
    for job in jobs:
        for dependency in job.get("dependencies", []):
            if dependency not in job_ids or dependency == job["id"]:
                raise RuntimeError(f"{job['id']} has an invalid dependency")


def glob_magic(path: str) -> bool:
    return any(character in path for character in "*?[")


def parse_cpu_set(value: object, capacity: int) -> set[int]:
    text = str(value or "").strip()
    if not text:
        return set()
    if not CPU_SET.fullmatch(text):
        raise ValueError(f"invalid CPU set: {text!r}")
    result: set[int] = set()
    for component in text.split(","):
        if "-" in component:
            first_text, last_text = component.split("-", 1)
            first, last = int(first_text), int(last_text)
            if first > last:
                raise ValueError(f"reversed CPU range: {component}")
            result.update(range(first, last + 1))
        else:
            result.add(int(component))
    if any(cpu < 0 or cpu >= capacity for cpu in result):
        raise ValueError(f"CPU set {text!r} exceeds capacity {capacity}")
    return result


def compact_source_bindings(
        jobs: list[dict[str, Any]]) -> tuple[list[dict[str, str]], list[dict[str, Any]]]:
    """Build one exact host binding table and per-job references.

    The same source files are authenticated by many jobs on a host.  Sending
    every ``{path, sha256}`` pair once per job can exceed SSM's command-size
    limit even though the remote probe's *response* is compact.  Keep one
    table entry for each exact path/digest pair and preserve each job's
    original order through integer references.  A path with two different
    expected digests intentionally gets two entries; the remote checker then
    hashes that path once and compares both expectations fail-closed.
    """
    table: list[dict[str, str]] = []
    indices: dict[tuple[str, str], int] = {}
    compact_jobs: list[dict[str, Any]] = []
    for job in jobs:
        references: list[int] = []
        for binding in job.get("source_bindings", []):
            if not isinstance(binding, dict):
                raise RuntimeError(f"{job.get('id', '<job>')} has invalid source binding")
            path = str(binding.get("path", ""))
            digest = validate_sha(
                binding.get("sha256"),
                f"{job.get('id', '<job>')} source binding")
            key = (path, digest)
            index = indices.get(key)
            if index is None:
                index = len(table)
                indices[key] = index
                table.append({"path": path, "sha256": digest})
            references.append(index)
        compact_jobs.append({
            "id": job["id"],
            "unit": job["unit"],
            "checkpoint_paths": job.get("checkpoint_paths", []),
            "completion_paths": job.get("completion_paths", []),
            "binding_refs": references,
        })
    return table, compact_jobs


def remote_script(instance: dict[str, Any], jobs: list[dict[str, Any]]) -> str:
    bindings, compact_jobs = compact_source_bindings(jobs)
    payload = {
        "mounts": instance.get("mounts", ["/"]),
        "bindings": bindings,
        "jobs": compact_jobs,
    }
    encoded = base64.b64encode(canonical_json(payload).encode()).decode()
    program = r'''
import glob,hashlib,json,os,subprocess,sys
payload=json.loads(base64.b64decode(sys.argv[2]))
def command(argv):
 p=subprocess.run(argv,text=True,capture_output=True,check=False)
 return p.returncode,p.stdout.strip(),p.stderr.strip()
def props(unit):
 names=['LoadState','ActiveState','Result',
        'ExecMainStatus','MemoryCurrent','StateChangeTimestamp',
        'AllowedCPUs','CPUUsageNSec']
 try:
  rc,out,err=command(['systemctl','show',unit,*sum((['-p',n] for n in names),[])])
 except OSError as error:
  return {'probe_error':str(error)}
 if rc: return {'probe_error':err or out or f'systemctl rc={rc}'}
 result={}
 for line in out.splitlines():
  if '=' in line:
   key,value=line.split('=',1); result[key]=value
 if result.get('ActiveState') not in {'active','activating','reloading'}:
  result={key:result[key] for key in (
   'LoadState','ActiveState','Result',
   'ExecMainStatus') if key in result}
 return result
def aggregate(patterns):
 result=[]
 for pattern in patterns:
  matches=sorted(glob.glob(pattern))
  if not matches: result.append([0]); continue
  digest=hashlib.sha256(); total=0; allocated=0; newest=0
  for path in matches:
   stat=os.stat(path); total+=stat.st_size; allocated+=stat.st_blocks*512
   newest=max(newest,stat.st_mtime_ns)
   record=json.dumps([path,stat.st_size,stat.st_mtime_ns],
                     separators=(',',':')).encode()
   digest.update(len(record).to_bytes(8,'big')); digest.update(record)
  result.append([1,len(matches),total,allocated,newest,digest.hexdigest()])
 return result
def checked_bindings(bindings):
 # Hash each physical path at most once, while retaining one result for each
 # exact path/digest table entry.  This accepts duplicate identical entries
 # and rejects a missing path, a divergent digest, or malformed table data.
 by_path={}; result=[]
 for binding in bindings:
  if not isinstance(binding,dict): result.append(False); continue
  path=binding.get('path'); expected=binding.get('sha256')
  if not isinstance(path,str) or not isinstance(expected,str):
   result.append(False); continue
  if path in by_path:
   actual=by_path[path]
  elif not os.path.isfile(path):
   actual=None; by_path[path]=actual
  else:
   stat=os.stat(path)
   if stat.st_size>16*1024*1024:
    actual=None; by_path[path]=actual
   else:
    digest=hashlib.sha256()
    with open(path,'rb') as stream:
     for block in iter(lambda:stream.read(1024*1024),b''): digest.update(block)
    actual=digest.hexdigest(); by_path[path]=actual
  result.append(actual==expected)
 return result

source_results=checked_bindings(payload.get('bindings',[]))
def sources_exact(references):
 if not isinstance(references,list): return False
 for reference in references:
  if isinstance(reference,bool) or not isinstance(reference,int): return False
  if reference<0 or reference>=len(source_results) or not source_results[reference]:
   return False
 return True
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
 jobs.append({'i':job['id'],'u':props(job['unit']),
              'k':aggregate(job['checkpoint_paths']),
              'c':aggregate(job['completion_paths']),
              'n':len(job.get('binding_refs',[])),
              'x':sources_exact(job.get('binding_refs',[]))})
document={'memory':memory,'mounts':mounts,'jobs':jobs}
encoded=json.dumps(document,sort_keys=True,separators=(',',':'))
document['b']=len(encoded.encode())
encoded=json.dumps(document,sort_keys=True,separators=(',',':'))
if len(encoded.encode())>__REMOTE_OUTPUT_BUDGET__:
 encoded=json.dumps({'probe_error':'bounded remote output budget exceeded',
                     'probe_encoded_bytes':len(encoded.encode()),
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
    request_bytes = len(wrapper.encode())
    if request_bytes >= REMOTE_REQUEST_BUDGET:
        raise RuntimeError(
            f"bounded remote request exceeded {REMOTE_REQUEST_BUDGET} bytes "
            f"({request_bytes})")
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
                return normalize_remote(json.loads(line[len(REMOTE_PREFIX):]))
        raise RuntimeError(f"SSM probe on {instance_id} returned no JSON sentinel")


def local_probe(command: str) -> dict[str, Any]:
    output = run(["/bin/sh", "-c", command])
    for line in output.splitlines():
        if line.startswith(REMOTE_PREFIX):
            return normalize_remote(json.loads(line[len(REMOTE_PREFIX):]))
    raise RuntimeError("local probe returned no JSON sentinel")


def normalize_remote(remote: dict[str, Any]) -> dict[str, Any]:
    """Expand the compact bounded wire format used by the remote probe."""
    if "b" in remote:
        remote["probe_encoded_bytes"] = remote.pop("b")

    def aggregate(records: object) -> list[dict[str, Any]]:
        if not isinstance(records, list):
            return []
        result: list[dict[str, Any]] = []
        for record in records:
            if not isinstance(record, list) or not record:
                result.append({"exists": False})
            elif record[0] == 0:
                result.append({"exists": False})
            elif len(record) == 6 and record[0] == 1:
                result.append({
                    "exists": True, "match_count": record[1],
                    "total_size": record[2], "allocated_bytes": record[3],
                    "newest_mtime_ns": record[4],
                    "metadata_sha256": record[5],
                })
            else:
                result.append({"exists": False})
        return result

    jobs = remote.get("jobs")
    if not isinstance(jobs, list):
        return remote
    normalized: list[dict[str, Any]] = []
    for job in jobs:
        if not isinstance(job, dict) or "i" not in job:
            normalized.append(job)
            continue
        normalized.append({
            "id": job.get("i"), "unit": job.get("u", {}),
            "checkpoints": aggregate(job.get("k")),
            "completion": aggregate(job.get("c")),
            "source_binding_count": job.get("n"),
            "sources_exact": job.get("x"),
        })
    remote["jobs"] = normalized
    return remote


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


def cached_certificates(definition: dict[str, Any], previous_job: dict[str, Any]
                       ) -> list[dict[str, Any]]:
    """Reuse proofs for immutable, version-pinned S3 objects."""
    expected = {(item["bucket"], item["key"], item["version_id"],
                 item["sha256"], int(item.get("size", -1)))
                for item in definition.get("s3_certificates", [])}
    cached = previous_job.get("certificates", [])
    actual = {(item.get("bucket"), item.get("key"), item.get("version_id"),
               item.get("sha256"), int(item.get("size", -1)))
              for item in cached if item.get("exact")}
    return list(cached) if expected and expected == actual else []


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
    expected = job.get("source_bindings", [])
    if not isinstance(expected, list):
        return False
    if not expected:
        return True
    if "sources_exact" in remote or "source_binding_count" in remote:
        return (remote.get("sources_exact") is True and
                remote.get("source_binding_count") == len(expected))
    actual = remote.get("sources")
    if not isinstance(actual, list) or len(actual) != len(expected):
        return False
    # The remote probe intentionally omits paths.  Positional comparison is
    # fail-closed for missing, truncated, extra, wrong, or reordered hashes.
    return all(
        isinstance(binding, dict) and
        isinstance(binding.get("sha256"), str) and
        isinstance(observed, str) and
        re.fullmatch(r"[0-9a-f]{64}", observed) is not None and
        observed == binding["sha256"]
        for binding, observed in zip(expected, actual))


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


def integer_property(unit: dict[str, Any], name: str) -> int | None:
    value = unit.get(name)
    try:
        result = int(value)
    except (TypeError, ValueError):
        return None
    return result if result >= 0 else None


def cpu_allocation(instance: dict[str, Any], remote: dict[str, Any],
                   job_definitions: list[dict[str, Any]] | None = None,
                   previous_remote: dict[str, Any] | None = None,
                   sample_seconds: float | None = None) -> dict[str, Any]:
    capacity = int(instance["vcpus"])
    active_sets: dict[str, set[int]] = {}
    unknown: list[str] = []
    previous_jobs = {
        str(job.get("id")): job for job in (previous_remote or {}).get("jobs", [])
    }
    utilization: dict[str, dict[str, Any]] = {}
    expected = {
        str(job["id"]): parse_cpu_set(job["expected_allowed_cpus"], capacity)
        for job in (job_definitions or [])
        if job.get("expected_allowed_cpus") is not None
    }
    for job in remote.get("jobs", []):
        unit = job.get("unit", {})
        if unit.get("ActiveState") not in {"active", "activating", "reloading"}:
            continue
        try:
            cpus = parse_cpu_set(unit.get("AllowedCPUs"), capacity)
        except ValueError:
            cpus = set()
        if cpus:
            active_sets[str(job["id"])] = cpus
        else:
            unknown.append(str(job["id"]))
        current_cpu = integer_property(unit, "CPUUsageNSec")
        previous_unit = previous_jobs.get(str(job["id"]), {}).get("unit", {})
        previous_cpu = integer_property(previous_unit, "CPUUsageNSec")
        same_activation = (
            unit.get("StateChangeTimestamp") and
            unit.get("StateChangeTimestamp") ==
            previous_unit.get("StateChangeTimestamp"))
        if (cpus and current_cpu is not None and previous_cpu is not None and
                current_cpu >= previous_cpu and same_activation and
                sample_seconds is not None and sample_seconds > 0):
            delta = current_cpu - previous_cpu
            busy = delta / (sample_seconds * 1_000_000_000)
            utilization[str(job["id"])] = {
                "cpu_usage_nsec": current_cpu,
                "cpu_delta_nsec": delta,
                "sample_seconds": round(sample_seconds, 3),
                "average_busy_vcpus": round(busy, 3),
                "allocated_utilization_percent": round(
                    100 * busy / len(cpus), 1),
            }
    allocated = set().union(*active_sets.values()) if active_sets else set()
    overlaps: list[str] = []
    names = sorted(active_sets)
    for index, first in enumerate(names):
        for second in names[index + 1:]:
            shared = active_sets[first] & active_sets[second]
            if shared:
                overlaps.append(
                    f"{first}+{second}:{','.join(map(str, sorted(shared)))}")
    measured_busy = sum(float(item["average_busy_vcpus"])
                        for item in utilization.values())
    mismatches = {
        name: {
            "expected": ",".join(map(str, sorted(cpus))),
            "actual": ",".join(map(str, sorted(active_sets.get(name, set())))),
        }
        for name, cpus in expected.items()
        if name in active_sets and active_sets[name] != cpus
    }
    return {
        "vcpus": capacity,
        "allocated_vcpus": len(allocated),
        "idle_vcpus": capacity - len(allocated),
        "allocation_known": not unknown,
        "unknown_jobs": unknown,
        "overlaps": overlaps,
        "active_jobs": {name: len(cpus) for name, cpus in active_sets.items()},
        "active_cpu_sets": {
            name: ",".join(map(str, sorted(cpus)))
            for name, cpus in active_sets.items()
        },
        "measured_jobs": utilization,
        "measured_busy_vcpus": round(measured_busy, 3),
        "measured_fleet_capacity_percent": round(
            100 * measured_busy / capacity, 1),
        # An entirely idle, successfully probed host is a complete sample too.
        # Treating it as unknown prevented the scheduler from backfilling the
        # most obviously idle machines.
        "measurement_complete": not unknown and
        set(utilization) == set(active_sets),
        "allocation_mismatches": mismatches,
    }


def _job_resources(job: dict[str, Any]) -> dict[str, Any]:
    resources = job.get("resource_requirements", {})
    return {
        "cpu_threads": int(resources.get("cpu_threads", 0)),
        "memory_peak_bytes": int(resources.get("memory_peak_bytes", 0)),
        "disk_peak_bytes": {
            str(path): int(extent)
            for path, extent in resources.get("disk_peak_bytes", {}).items()
        },
    }


def schedule_backfill(config: dict[str, Any], report: dict[str, Any]
                      ) -> dict[str, Any]:
    """Choose only dependency/source-certified jobs that fit measured hosts.

    Disk and memory requests are conservative peak *additional* reservations.
    For a running job we reserve only its remaining memory-to-peak and its full
    declared additional disk extent.  This deliberately favours correctness
    over optimistic overcommit while still allowing many single-threaded jobs
    to occupy otherwise idle CPUs.
    """
    target = float(config.get(
        "target_measured_utilization_fraction", DEFAULT_TARGET_UTILIZATION))
    definitions = {str(job["id"]): job for job in config["jobs"]}
    jobs = report["jobs"]
    instances = {str(item["instance_id"]): item for item in config["instances"]}
    candidates = [
        definition for definition in config["jobs"]
        if jobs.get(str(definition["id"]), {}).get("status") == "READY"
    ]
    # Smallest peak memory first backfills more independent solves; stable ids
    # make the decision deterministic and auditable.
    candidates.sort(key=lambda item: (
        int(_job_resources(item)["memory_peak_bytes"]), str(item["id"])))
    selected: list[str] = []
    blocked: dict[str, str] = {}
    host_plans: dict[str, dict[str, Any]] = {}
    for instance_id, instance in instances.items():
        observed = report["instances"].get(instance_id, {})
        remote = observed.get("remote", {})
        allocation = observed.get("cpu_allocation", {})
        mounts = {str(item["path"]): int(item.get("free_bytes", 0))
                  for item in remote.get("mounts", [])}
        available_memory = int(remote.get("memory", {}).get("MemAvailable", 0))
        memory_budget = max(0, available_memory - int(
            instance.get("minimum_memory_available_bytes", 0)))
        disk_budget = {
            path: max(0, free - int(
                instance.get("minimum_disk_free_bytes", {}).get(path, 0)))
            for path, free in mounts.items()
        }
        active_cpus: set[int] = set()
        reserved_memory = 0
        reserved_disk = {path: 0 for path in disk_budget}
        remote_jobs = {str(item["id"]): item for item in remote.get("jobs", [])}
        for job_id, result in jobs.items():
            definition = definitions[job_id]
            if definition["instance_id"] != instance_id or \
                    result["status"] != "RUNNING":
                continue
            unit = remote_jobs.get(job_id, {}).get("unit", {})
            try:
                active_cpus |= parse_cpu_set(
                    unit.get("AllowedCPUs"), int(instance["vcpus"]))
            except ValueError:
                pass
            resources = _job_resources(definition)
            current = integer_property(unit, "MemoryCurrent") or 0
            reserved_memory += max(0, resources["memory_peak_bytes"] - current)
            current_disk = sum(
                int(item.get("allocated_bytes", 0))
                for item in remote_jobs.get(job_id, {}).get("checkpoints", []))
            for path, extent in resources["disk_peak_bytes"].items():
                reserved_disk[path] = reserved_disk.get(path, 0) + max(
                    0, extent - current_disk)
        host_plans[instance_id] = {
            "measurement_complete": bool(allocation.get("measurement_complete")),
            "measured_busy_vcpus": float(
                allocation.get("measured_busy_vcpus", 0.0)),
            "target_busy_vcpus": round(target * int(instance["vcpus"]), 3),
            "selected": [], "memory_budget_bytes": memory_budget,
            "memory_reserved_bytes": reserved_memory,
            "disk_budget_bytes": disk_budget,
            "disk_reserved_bytes": reserved_disk,
        }
        if not allocation.get("measurement_complete"):
            continue
        host_candidates = [item for item in candidates
                           if item["instance_id"] == instance_id]
        predicted_busy = float(allocation.get("measured_busy_vcpus", 0.0))
        for job in host_candidates:
            identifier = str(job["id"])
            resources = _job_resources(job)
            cpus = parse_cpu_set(
                job.get("expected_allowed_cpus"), int(instance["vcpus"]))
            if predicted_busy >= target * int(instance["vcpus"]):
                blocked[identifier] = "host target utilization reached"
                continue
            if not cpus or cpus & active_cpus:
                blocked[identifier] = "no disjoint configured CPU set"
                continue
            if reserved_memory + resources["memory_peak_bytes"] > memory_budget:
                blocked[identifier] = "insufficient measured memory headroom"
                continue
            disk_failure = next((
                path for path, extent in resources["disk_peak_bytes"].items()
                if (path not in disk_budget or
                    reserved_disk.get(path, 0) + extent > disk_budget[path])
            ), None)
            if disk_failure is not None:
                blocked[identifier] = (
                    f"insufficient measured disk headroom on {disk_failure}")
                continue
            selected.append(identifier)
            host_plans[instance_id]["selected"].append(identifier)
            active_cpus |= cpus
            reserved_memory += resources["memory_peak_bytes"]
            for path, extent in resources["disk_peak_bytes"].items():
                reserved_disk[path] = reserved_disk.get(path, 0) + extent
            predicted_busy += resources["cpu_threads"]
        host_plans[instance_id]["predicted_busy_vcpus"] = round(predicted_busy, 3)
        host_plans[instance_id]["memory_reserved_bytes"] = reserved_memory
        host_plans[instance_id]["disk_reserved_bytes"] = reserved_disk
    return {"selected": selected, "blocked": blocked, "hosts": host_plans}


def probe_instance(config: dict[str, Any], definition: dict[str, Any],
                   ec2: dict[str, Any], jobs: list[dict[str, Any]],
                   now: dt.datetime, previous_remote: dict[str, Any] | None,
                   sample_seconds: float | None
                   ) -> tuple[dict[str, Any], str | None]:
    launch = parse_time(ec2["LaunchTime"])
    hours = max(0.0, (now - launch).total_seconds() / 3600)
    spend = hours * float(definition["hourly_usd"])
    state = ec2.get("State", {}).get("Name", "unknown")
    remote: dict[str, Any] = {}
    error: str | None = None
    queued = [job for job in jobs
              if job.get("queue_stage") and not job.get("advanceable") and
              not job.get("superseded_by")]
    superseded = [job for job in jobs if job.get("superseded_by")]
    omitted = queued + superseded
    probed = [job for job in jobs if job not in omitted]
    if state == "running":
        try:
            # Uninstalled queue records have no live state.  Sending their
            # future paths and synthetic systemd properties wastes the fixed
            # SSM output budget and can blind a busy host.  They are restored
            # locally as minimal inactive records after a successful probe.
            command = remote_script(definition, probed)
            remote = (local_probe(command)
                      if definition.get("transport", "ssm") == "local"
                      else ssm_probe(config["region"], definition["instance_id"],
                                     command))
            if remote.get("probe_error"):
                error = str(remote["probe_error"])
            else:
                remote.setdefault("jobs", []).extend({
                    "id": job["id"],
                    "unit": {"LoadState": "not-found",
                             "ActiveState": "inactive", "Result": "success",
                             "ExecMainStatus": "0"},
                    "checkpoints": [], "completion": [], "sources": [],
                } for job in queued)
        except Exception as exception:  # one host must not hide the other four
            error = str(exception)
    warnings = resource_warnings(definition, remote) if remote else []
    allocation = cpu_allocation(
        definition, remote, probed, previous_remote, sample_seconds) if remote else {
        "vcpus": int(definition["vcpus"]), "allocated_vcpus": 0,
        "idle_vcpus": int(definition["vcpus"]), "allocation_known": False,
        "unknown_jobs": [], "overlaps": [], "active_jobs": {},
        "active_cpu_sets": {},
        "measured_jobs": {}, "measured_busy_vcpus": 0.0,
        "measured_fleet_capacity_percent": 0.0,
        "measurement_complete": False,
        "allocation_mismatches": {},
    }
    if allocation["overlaps"]:
        warnings.append("cpu_overlap=" + ";".join(allocation["overlaps"]))
    if allocation["allocation_mismatches"]:
        warnings.append("cpu_allocation_mismatch=" + canonical_json(
            allocation["allocation_mismatches"]))
    return ({
        "name": definition.get("name", definition["instance_id"]),
        "ec2_state": state, "instance_type": ec2.get("InstanceType", ""),
        "launch_time": ec2.get("LaunchTime", ""),
        "estimated_spend_usd": round(spend, 2),
        "resource_warnings": warnings, "cpu_allocation": allocation,
        "remote": remote,
    }, error)


def supervise(config: dict[str, Any], previous: dict[str, Any],
              now: dt.datetime) -> tuple[dict[str, Any], dict[str, Any]]:
    inventory = ec2_inventory(config)
    previous_report = previous.get("report", {})
    previous_instances = previous_report.get("instances", {})
    sample_seconds: float | None = None
    try:
        sample_seconds = (now - parse_time(
            str(previous_report["observed_at"]))).total_seconds()
    except (KeyError, TypeError, ValueError):
        pass
    jobs_by_instance: dict[str, list[dict[str, Any]]] = {}
    for job in config["jobs"]:
        jobs_by_instance.setdefault(job["instance_id"], []).append(job)
    instance_results: dict[str, Any] = {}
    total_spend = 0.0
    errors: list[dict[str, str]] = []
    futures: dict[Any, tuple[str, dict[str, Any]]] = {}
    with ThreadPoolExecutor(
            max_workers=min(int(config.get("probe_workers", 2)),
                            len(config["instances"]))) as executor:
        for definition in config["instances"]:
            identifier = definition["instance_id"]
            ec2 = inventory.get(identifier)
            if not ec2:
                errors.append({"instance": identifier,
                               "error": "EC2 instance missing"})
                continue
            futures[executor.submit(
                probe_instance, config, definition, ec2,
                jobs_by_instance.get(identifier, []), now,
                previous_instances.get(identifier, {}).get("remote"),
                sample_seconds)] = (
                    identifier, definition)
        for future in as_completed(futures):
            identifier, definition = futures[future]
            try:
                result, error = future.result()
            except Exception as exception:
                errors.append({"instance": identifier, "error": str(exception)})
                continue
            instance_results[identifier] = result
            total_spend += float(result["estimated_spend_usd"])
            if error:
                errors.append({"instance": identifier, "error": error})

    # Preserve config order in emitted state despite parallel collection.
    instance_results = {
        definition["instance_id"]: instance_results[definition["instance_id"]]
        for definition in config["instances"]
        if definition["instance_id"] in instance_results
    }

    jobs: dict[str, Any] = {}
    definitions = {job["id"]: job for job in config["jobs"]}
    for identifier, definition in definitions.items():
        host = instance_results.get(definition["instance_id"], {})
        remote_jobs = {entry["id"]: entry for entry in
                       host.get("remote", {}).get("jobs", [])}
        remote = remote_jobs.get(identifier, {})
        superseded = bool(definition.get("superseded_by"))
        status = ("SUPERSEDED" if superseded else
                  unit_status(remote.get("unit", {})) if remote else "UNKNOWN")
        if not superseded and remote and not source_exact(definition, remote):
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
        if (not superseded and
                remote.get("unit", {}).get("LoadState") == "not-found" and
                previous_status == "RUNNING" and not completion):
            status = "FAILED"
            remote["unit"]["SupervisionFailure"] = (
                "transient unit disappeared after RUNNING without completion")
        certificates: list[dict[str, Any]] = []
        s3_only = bool(definition.get("s3_only_certified"))
        if (not superseded and (completion or s3_only) and
                definition.get("s3_certificates")):
            previous_job = previous.get("report", {}).get("jobs", {}).get(
                identifier, {})
            certificates = cached_certificates(definition, previous_job)
            if not certificates:
                try:
                    with ThreadPoolExecutor(
                            max_workers=len(definition["s3_certificates"])) as executor:
                        certificates = list(executor.map(
                            lambda certificate: head_certificate(
                                config["region"], certificate),
                            definition["s3_certificates"]))
                except Exception as error:
                    errors.append({"job": identifier, "error": str(error)})
        if (not superseded and (completion or s3_only) and certificates and
                all(item["exact"] for item in certificates)):
            status = "CERTIFIED"
        elif not superseded and completion and status == "INACTIVE":
            status = "COMPLETED_UNCERTIFIED"
        jobs[identifier] = {
            "status": status, "unit": remote.get("unit", {}),
            "checkpoints": remote.get("checkpoints", []),
            "completion": remote.get("completion", []),
            "source_exact": source_exact(definition, remote) if remote else False,
            "certificates": certificates,
            "dependencies": definition.get("dependencies", []),
            "ledger_files": definition.get("ledger_files", []),
            "ledger_certifies": bool(definition.get("ledger_certifies")),
            "ledger_results": definition.get("ledger_results", {}),
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
    scheduling = schedule_backfill(config, report)
    measured_instances = [
        value for value in instance_results.values()
        if value.get("ec2_state") == "running"
    ]
    measurement_complete = bool(measured_instances) and all(
        value["cpu_allocation"].get("measurement_complete")
        for value in measured_instances)
    measured_capacity = sum(int(value["cpu_allocation"]["vcpus"])
                            for value in measured_instances)
    measured_busy = sum(float(value["cpu_allocation"]["measured_busy_vcpus"])
                        for value in measured_instances)
    utilization_fraction = (measured_busy / measured_capacity
                            if measured_capacity else 0.0)
    stage_jobs = [identifier for identifier, value in jobs.items()
                  if value["status"] == "AWAITING_STAGE"]
    runnable = sorted(set(scheduling["selected"]) | set(stage_jobs))
    below = (measurement_complete and bool(runnable) and
             utilization_fraction < float(config.get(
                 "underutilized_fraction", DEFAULT_UNDERUTILIZED_FRACTION)))
    underutilized_limit = int(config.get(
        "underutilized_samples", DEFAULT_UNDERUTILIZED_SAMPLES))
    underutilized_samples = (min(
        underutilized_limit,
        int(previous.get("underutilized_samples", 0)) + 1) if below else 0)
    underutilized = underutilized_samples >= underutilized_limit
    if underutilized:
        errors.append({
            "fleet": "UNDERUTILIZED",
            "error": (f"measured fleet utilization {utilization_fraction:.3f} "
                      f"below threshold with {len(runnable)} runnable jobs for "
                      f"{underutilized_samples} samples"),
        })
    scheduling.update({
        "measurement_complete": measurement_complete,
        "measured_busy_vcpus": round(measured_busy, 3),
        "measured_capacity_vcpus": measured_capacity,
        "measured_utilization_percent": round(100 * utilization_fraction, 1),
        "underutilized_samples": underutilized_samples,
        "underutilized": underutilized,
        "stage_jobs": stage_jobs,
    })
    report["scheduling"] = scheduling
    # Utilization is sampled every poll and belongs in the durable state and
    # regular report, but it must not turn quiet progress into a five-minute
    # event stream.  Scheduling topology and warnings remain event-significant.
    event_view = {
        "instances": {identifier: {
            "ec2_state": value["ec2_state"],
            "resource_warnings": value["resource_warnings"],
            "cpu_allocation": {
                key: value["cpu_allocation"][key] for key in (
                    "vcpus", "allocated_vcpus", "idle_vcpus",
                    "allocation_known", "unknown_jobs", "overlaps",
                    "active_jobs", "active_cpu_sets",
                    "allocation_mismatches")
            },
        } for identifier, value in instance_results.items()},
        "jobs": {identifier: value["status"] for identifier, value in jobs.items()},
        "errors": errors,
        "scheduling": {
            "selected": scheduling["selected"],
            "stage_jobs": stage_jobs,
            "underutilized_samples": underutilized_samples,
            "underutilized": underutilized,
        },
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
    # Only the resource scheduler's safe subset reaches the automatic advance
    # gate.  Staged-but-not-installed work is reported separately for Sol.
    ready = list(scheduling["selected"])
    rebalance_jobs = sorted({
        job_id
        for value in instance_results.values()
        for job_id in value["cpu_allocation"]["allocation_mismatches"]
    })
    previous_jobs = previous.get("report", {}).get("jobs", {})
    changed_jobs = [identifier for identifier, value in jobs.items()
                    if previous_jobs.get(identifier, {}).get("status") !=
                    value["status"]]
    # The host bridge can advance only jobs present in this compact event.  A
    # READY job may remain READY across samples while measured capacity changes
    # and makes it newly selected; include every scheduler/rebalance selection
    # even when its classification itself did not change.
    selected_jobs = (sorted(jobs) if heartbeat or not last_digest else
                     sorted(set(changed_jobs) | set(ready) |
                            set(rebalance_jobs)))
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
            "ledger_files": jobs[identifier]["ledger_files"],
            "ledger_certifies": jobs[identifier]["ledger_certifies"],
            "ledger_results": jobs[identifier]["ledger_results"],
        } for identifier in selected_jobs
    }
    output = {
        "status": event_type, "severity": severity,
        "delegate_sol": severity == "error", "ready_jobs": ready,
        "cpu_rebalance_jobs": rebalance_jobs,
        "stage_jobs": stage_jobs,
        "report": {
            "observed_at": report["observed_at"],
            "estimated_spend_usd": report["estimated_spend_usd"],
            "fleet_burn_usd_per_hour": report["fleet_burn_usd_per_hour"],
            "instances": {identifier: {
                "ec2_state": value["ec2_state"],
                "resource_warnings": value["resource_warnings"],
                "cpu_allocation": value["cpu_allocation"],
            } for identifier, value in instance_results.items()
               if heartbeat or not last_digest or value["resource_warnings"]},
            "jobs": compact_jobs,
            "errors": errors,
            "scheduling": scheduling,
        },
    }
    state = {
        "schema": STATE_SCHEMA, "event_digest": digest,
        "last_observed_epoch": now.timestamp(),
        "last_regular_report_epoch": (now.timestamp() if heartbeat or changed
                                       else last_report),
        "underutilized_samples": underutilized_samples,
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
    selected = state.get("report", {}).get("scheduling", {}).get("selected", [])
    if job_id not in selected:
        raise RuntimeError(f"{job_id} was not selected by the resource scheduler")
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


def rebalance_cpu(config: dict[str, Any], state: dict[str, Any],
                  job_id: str) -> dict[str, Any]:
    jobs = {job["id"]: job for job in config["jobs"]}
    if job_id not in jobs:
        raise RuntimeError(f"unknown CPU rebalance job {job_id}")
    job = jobs[job_id]
    expected = str(job.get("expected_allowed_cpus", ""))
    if not expected:
        raise RuntimeError(f"{job_id} has no canonical CPU allocation")
    observed = state.get("report", {}).get("jobs", {}).get(job_id, {})
    if observed.get("status") != "RUNNING":
        raise RuntimeError(f"{job_id} is not RUNNING for CPU rebalance")
    instance = next(item for item in config["instances"]
                    if item["instance_id"] == job["instance_id"])
    desired = parse_cpu_set(expected, int(instance["vcpus"]))
    remote_jobs = state.get("report", {}).get("instances", {}).get(
        job["instance_id"], {}).get("remote", {}).get("jobs", [])
    definitions = {str(item["id"]): item for item in config["jobs"]}
    for other in remote_jobs:
        if other.get("id") == job_id or other.get("unit", {}).get(
                "ActiveState") not in {"active", "activating", "reloading"}:
            continue
        other_definition = definitions.get(str(other.get("id")), {})
        other_expected = other_definition.get("expected_allowed_cpus")
        try:
            # Compare canonical destinations when both jobs are being shrunk.
            # Comparing against the other job's current oversized set would
            # make an overlap impossible to repair without stopping work.
            actual = parse_cpu_set(
                other_expected or other.get("unit", {}).get("AllowedCPUs"),
                int(instance["vcpus"]))
        except ValueError:
            raise RuntimeError(
                f"{job_id} cannot rebalance around unknown {other.get('id')} CPUs")
        if desired & actual:
            raise RuntimeError(
                f"{job_id} canonical CPUs overlap canonical active "
                f"{other.get('id')}")
    unit = str(job["unit"])
    if instance.get("transport", "ssm") == "local":
        run(["systemctl", "set-property", "--runtime", unit,
             f"AllowedCPUs={expected}"])
        actual = run(["systemctl", "show", unit, "-p", "AllowedCPUs",
                      "--value"]).strip()
        if parse_cpu_set(actual, int(instance["vcpus"])) != desired:
            raise RuntimeError(f"{job_id} CPU rebalance readback residual")
    else:
        command = (
            f"systemctl set-property --runtime {unit} AllowedCPUs={expected} && "
            f"actual=$(systemctl show {unit} -p AllowedCPUs --value) && "
            f'test "$actual" = {expected} && echo {REMOTE_PREFIX}{{}}')
        ssm_probe(config["region"], job["instance_id"], command)
    return {"status": "CPU_REBALANCED", "job": job_id,
            "instance_id": job["instance_id"], "unit": unit,
            "allowed_cpus": expected}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--state", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--once", action="store_true")
    mode.add_argument("--advance", metavar="JOB")
    mode.add_argument("--rebalance-cpu", metavar="JOB")
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
        if args.rebalance_cpu:
            result = rebalance_cpu(config, previous, args.rebalance_cpu)
            print(canonical_json(result) if args.json else
                  f"CPU_REBALANCED {args.rebalance_cpu}")
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
