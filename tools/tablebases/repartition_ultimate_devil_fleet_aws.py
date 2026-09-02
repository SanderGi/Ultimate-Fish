#!/usr/bin/env python3
"""Safely repartition unfinished authenticated Devil census shards onto v6."""

from __future__ import annotations

import argparse
import base64
from concurrent.futures import ThreadPoolExecutor, as_completed
import gzip
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
from typing import Callable

import launch_ultimate_devil_fleet_aws as fleet


SCHEMA = 1
CAMPAIGN = "highcap-200m-repartition-v2"
PLAN_KEY_PREFIX = "plans/devil-closure-v6/highcap-200m-repartition-v2/sha256"
SOURCE_SHA256 = "eeb0ef6c2c9fc1f7e89724012e3b57f5c8c7de653034e3cf0d4cb6f93f344e83"
SOURCE_VERSION = "Fcog2hi.OHbq9CAD0yAe_iMhdxHs4e8m"
SOURCE_KEY = (f"sources/bundles/devil-closure-v6/sha256/{SOURCE_SHA256}/"
              "ultimatefish-devil-closure-v6-source.tar")
BINARY_SHA256 = "90e33f98201f43a2ba1f78d836395e7cb22d11ca72264dc385099c85bb5052e7"
BINARY_VERSION = "5WDv1T8jpmTvSwr0Jsq8Xf50_zDHaUIc"
BINARY_KEY = (f"sources/binaries/devil-closure-v6/sha256/{BINARY_SHA256}/"
              "ultimate_tablebase-devil-v6")
RUNNER_SHA256 = "9aa4324a70855e7dcf156278ba6e1cab6dadce0394578a939b7bd5998ec70f87"
RUNNER_VERSION = "6KfDhuacsZQNXyYvN1vP9eNqjQUgbOD7"
RUNNER_KEY = (f"sources/runners/devil-closure-v6/sha256/{RUNNER_SHA256}/"
              "run_ultimate_devil_repartition_worker.py")
HOST_RUNNER_SHA256 = "0eba2b39365621cebc5922b117a2ca2f4682360e2a1c8cf48571a48c4da5be44"
HOST_RUNNER_VERSION = "EXiR4P_J.RSEauKPYbA4TXL_LBYWg_Z7"
HOST_RUNNER_KEY = (
    f"sources/runners/devil-closure-v6/sha256/{HOST_RUNNER_SHA256}/"
    "run_ultimate_devil_repartition_host.py")
SEMANTICS = (
    "no-preexisting-minions-devil-first-three-ranks-cooldown-"
    "exact-bitboard-codec-flat-visited-set-v6"
)
V5_PROVENANCE = {
    "source_sha256": fleet.SOURCE_SHA256,
    "binary_sha256": fleet.BINARY_SHA256,
    "semantics": fleet.SEMANTICS,
    "equivalence_oracle": {
        "shard": 0,
        "shards": 992,
        "depth": 2,
        "state_limit": 100000,
        "visited": 100000,
        "boundary": 4101,
        "max_minions": 1,
        "capped": 1,
    },
}
OLD_PARENT = "ultimatefish-devil-orchestrator-v5-highcap-200m.service"
OLD_CHILD = "ultimatefish-devil-fleet-v5-highcap-200m-"
PARENT = "ultimatefish-devil-orchestrator-v6-repartition-v2"
CHILD = "ultimatefish-devil-fleet-v6-repartition-v2-"
ROOT = f"/mnt/ultimatefish/devil-closure-v6-{SOURCE_SHA256[:8]}"
LOG_ROOT = f"{ROOT}/fleet-repartition-v2"
RECEIPT_PREFIX = "runs/devil-closure-v6/repartition-v2"
PLAN_PATH = f"{ROOT}/highcap-200m-repartition-v2-plan.json"
RECEIPT = re.compile(
    r"^DEVIL_CLOSURE_CENSUS_OK shard ([0-9]+)/992 visited 200000000 "
    r"boundary ([0-9]+) max_minions ([0-9]+) capped 1$")
TASK_MARKER = re.compile(
    r"^DEVIL_TASK_OK kind=(single|pair) class=([^ ]+) shard=([0-9]+) "
    r"receipt_sha256=([0-9a-f]{64})$")

# These companions have no independent cooldown, continuation, linked-pair,
# attachment, or aura state.  The v6 exact key therefore represents their
# complete spawned-only closure today.  Stateful companions remain planned
# while their additional substate bits are added; they are not silently run
# through the stateless codec.
STATELESS_DEVIL_COMPANIONS = (
    ("jester", "jester+devil"),
    ("knight", "knight+devil"),
    ("queen", "queen+devil"),
    ("rook", "rook+devil"),
    ("bishop", "bishop+devil"),
    ("bomb", "bomb+devil"),
    ("ninja", "ninja+devil"),
    ("turtle", "turtle+devil"),
    ("mage", "mage+devil"),
    ("parasite", "parasite+devil"),
    ("giant", "devil+giant"),
    ("fisherman", "devil+fisherman"),
    ("dragon", "devil+dragon"),
)
PAIR_CAMPAIGNS = tuple(
    {"id": f"{orientation}:{class_name}", "piece2": piece,
     "opposing": orientation == "opposed"}
    for orientation in ("same", "opposed")
    for piece, class_name in STATELESS_DEVIL_COMPANIONS)

# All five hosts are r8gd.8xlarge.  These assignments use every CPU not owned
# by a current Ghost unit.  V6's flat visited table and 32-bit frontier indices
# remove the old 12--16 GiB node-set gate: 133 Devil workers plus 27 Ghost CPUs
# cover the complete 160-vCPU quota without overlap.
HOSTS = (
    ("i-03c81f90d2c59a2e7", (0, *range(3, 14), *range(15, 32))),
    ("i-0b4523116b2f7765c", (*range(13, 21), 22, 23,
                              *range(25, 29), 30, 31)),
    ("i-024a2073283e4336e", tuple(range(4, 32))),
    ("i-08c0f44a1776cb34a", (*range(0, 14), *range(15, 32))),
    ("i-0986ed3d272721f02", tuple(range(3, 32))),
)
assert sum(len(cpus) for _, cpus in HOSTS) == 133


def canonical(value: object) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def parallel(action: Callable[[str], object]) -> dict[str, object]:
    result: dict[str, object] = {}
    with ThreadPoolExecutor(max_workers=5) as executor:
        futures = {executor.submit(action, instance): instance
                   for instance, _ in HOSTS}
        for future in as_completed(futures):
            instance = futures[future]
            result[instance] = future.result()
    return result


def receipt_lines(instance: str) -> list[str]:
    command = (
        "set -o pipefail; { grep -H -E "
        "'^(DEVIL_CLOSURE_CENSUS_OK |DEVIL_TASK_OK )' "
        f"{fleet.ROOT}/fleet-116-highcap-200m/slot-*.log "
        f"{LOG_ROOT}/worker-*.log 2>/dev/null || true; }} | "
        "gzip -c | base64 -w0")
    encoded = fleet.send(instance, [command]).strip()
    try:
        payload = gzip.decompress(base64.b64decode(encoded, validate=True))
        text = payload.decode("utf-8")
    except (ValueError, OSError, UnicodeDecodeError) as error:
        raise RuntimeError(
            f"invalid compressed Devil receipt inventory on {instance}") from error
    singles: list[str] = []
    pending: dict[str, str] = {}
    for tagged in text.splitlines():
        if not tagged:
            continue
        source, separator, line = tagged.partition(":")
        if not separator:
            raise RuntimeError(
                f"untagged Devil receipt inventory on {instance}")
        if "/fleet-116-highcap-200m/" in source:
            if RECEIPT.fullmatch(line) is None:
                raise RuntimeError(
                    f"invalid v5 Devil receipt on {instance}: {line}")
            singles.append(line)
            continue
        if line.startswith("DEVIL_CLOSURE_CENSUS_OK "):
            if RECEIPT.fullmatch(line) is None:
                raise RuntimeError(
                    f"invalid v6 Devil receipt on {instance}: {line}")
            pending[source] = line
            continue
        marker = TASK_MARKER.fullmatch(line)
        receipt = pending.pop(source, None)
        if marker is None or receipt is None:
            raise RuntimeError(
                f"unpaired v6 Devil marker on {instance}: {line}")
        kind, class_id, raw_shard, digest = marker.groups()
        parsed = RECEIPT.fullmatch(receipt)
        assert parsed is not None
        if (int(parsed.group(1)) != int(raw_shard) or
                hashlib.sha256(receipt.encode()).hexdigest() != digest or
                (kind == "single" and class_id != "single:devil")):
            raise RuntimeError(
                f"v6 Devil receipt binding residual on {instance}: {line}")
        if kind == "single":
            singles.append(receipt)
    if pending:
        raise RuntimeError(
            f"partial v6 Devil receipts on {instance}: {sorted(pending)}")
    return singles


def inventory() -> tuple[dict[int, dict[str, object]], dict[str, int]]:
    by_instance = parallel(receipt_lines)
    receipts: dict[int, dict[str, object]] = {}
    counts: dict[str, int] = {}
    for instance, raw in by_instance.items():
        lines = list(raw)  # type: ignore[arg-type]
        counts[instance] = len(lines)
        for line in lines:
            match = RECEIPT.fullmatch(line)
            if match is None:
                raise RuntimeError(f"invalid Devil receipt on {instance}: {line}")
            shard = int(match.group(1))
            if not 0 <= shard < fleet.SHARDS:
                raise RuntimeError(f"Devil shard out of range: {shard}")
            if shard in receipts:
                raise RuntimeError(
                    f"duplicate Devil shard {shard} on {instance} and "
                    f"{receipts[shard]['instance']}")
            receipts[shard] = {
                "instance": instance,
                "line": line,
                "boundary": int(match.group(2)),
                "max_minions": int(match.group(3)),
            }
    return receipts, counts


def stop_old(instance: str) -> str:
    command = (
        "set -euo pipefail; "
        f"systemctl stop {OLD_PARENT} || true; "
        f"systemctl stop '{OLD_CHILD}*.service' || true; "
        f"systemctl stop {PARENT}.service || true; "
        f"systemctl stop '{CHILD}*.service' || true; "
        f"test \"$(systemctl list-units '{OLD_CHILD}*.service' "
        "--state=active --no-legend | wc -l)\" -eq 0; "
        f"test \"$(systemctl list-units '{CHILD}*.service' "
        "--state=active --no-legend | wc -l)\" -eq 0; echo STOPPED")
    return fleet.send(instance, [command])


def make_plan(path: Path) -> dict[str, object]:
    stopped = parallel(stop_old)
    if any(str(value).strip() != "STOPPED" for value in stopped.values()):
        raise RuntimeError(f"Devil stop residual: {stopped}")
    receipts, counts = inventory()
    remaining = sorted(set(range(fleet.SHARDS)) - set(receipts))
    workers = [(instance, cpu) for instance, cpus in HOSTS for cpu in cpus]
    assignments: dict[str, list[dict[str, object]]] = {
        instance: [] for instance, _ in HOSTS}
    shard_lists: list[list[int]] = [[] for _ in workers]
    for index, shard in enumerate(remaining):
        shard_lists[index % len(workers)].append(shard)
    for worker, ((instance, cpu), shards) in enumerate(zip(workers, shard_lists)):
        assignments[instance].append({
            "worker": worker, "cpu": cpu, "shards": shards,
        })
    plan: dict[str, object] = {
        "schema": SCHEMA,
        "campaign": CAMPAIGN,
        "source_sha256": SOURCE_SHA256,
        "source_version_id": SOURCE_VERSION,
        "binary_sha256": BINARY_SHA256,
        "binary_version_id": BINARY_VERSION,
        "runner_sha256": RUNNER_SHA256,
        "runner_version_id": RUNNER_VERSION,
        "host_runner_sha256": HOST_RUNNER_SHA256,
        "host_runner_version_id": HOST_RUNNER_VERSION,
        "semantics": SEMANTICS,
        "completed_v5_provenance": V5_PROVENANCE,
        "shards": fleet.SHARDS,
        "state_limit": 200_000_000,
        "completed": sorted(receipts),
        "completed_counts": counts,
        "remaining": remaining,
        "pair_campaigns": list(PAIR_CAMPAIGNS),
        "assignments": assignments,
    }
    path.write_text(json.dumps(plan, indent=2, sort_keys=True) + "\n")
    return plan


def validate_plan(plan: dict[str, object]) -> None:
    if (plan.get("schema") != SCHEMA or plan.get("campaign") != CAMPAIGN or
            plan.get("source_sha256") != SOURCE_SHA256 or
            plan.get("source_version_id") != SOURCE_VERSION or
            plan.get("binary_sha256") != BINARY_SHA256 or
            plan.get("binary_version_id") != BINARY_VERSION or
            plan.get("runner_sha256") != RUNNER_SHA256 or
            plan.get("runner_version_id") != RUNNER_VERSION or
            plan.get("host_runner_sha256") != HOST_RUNNER_SHA256 or
            plan.get("host_runner_version_id") != HOST_RUNNER_VERSION or
            plan.get("semantics") != SEMANTICS or
            plan.get("completed_v5_provenance") != V5_PROVENANCE or
            plan.get("shards") != fleet.SHARDS or
            plan.get("state_limit") != 200_000_000):
        raise RuntimeError("Devil repartition plan provenance residual")
    if plan.get("pair_campaigns") != list(PAIR_CAMPAIGNS):
        raise RuntimeError("Devil stateless-pair campaign residual")
    completed = list(map(int, plan.get("completed", [])))
    remaining = list(map(int, plan.get("remaining", [])))
    if (completed != sorted(set(completed)) or
            remaining != sorted(set(remaining)) or
            sorted(completed + remaining) != list(range(fleet.SHARDS))):
        raise RuntimeError("Devil repartition coverage residual")
    assignments = plan.get("assignments")
    if not isinstance(assignments, dict):
        raise RuntimeError("Devil repartition assignments missing")
    assigned: list[int] = []
    worker_ids: list[int] = []
    for instance, cpus in HOSTS:
        rows = assignments.get(instance)
        if not isinstance(rows, list) or len(rows) != len(cpus):
            raise RuntimeError(f"Devil repartition host residual: {instance}")
        for row, cpu in zip(rows, cpus):
            if not isinstance(row, dict) or row.get("cpu") != cpu:
                raise RuntimeError(f"Devil repartition CPU residual: {instance}")
            worker_ids.append(int(row.get("worker", -1)))
            assigned.extend(map(int, row.get("shards", [])))
    if sorted(worker_ids) != list(range(len(worker_ids))):
        raise RuntimeError("Devil repartition worker-id residual")
    if sorted(assigned) != remaining or len(assigned) != len(set(assigned)):
        raise RuntimeError("Devil repartition overlap/gap residual")


def upload_plan(path: Path) -> tuple[str, str, str]:
    payload = path.read_bytes()
    digest = sha256_bytes(payload)
    key = f"{PLAN_KEY_PREFIX}/{digest}/{path.name}"
    response = fleet.aws_json([
        "aws", "s3api", "put-object", "--region", fleet.REGION,
        "--bucket", fleet.BUCKET, "--key", key, "--body", str(path),
        "--metadata", f"sha256={digest}", "--output", "json",
    ])
    version = str(response.get("VersionId", ""))
    if not version:
        raise RuntimeError("versioned Devil repartition plan upload residual")
    return key, version, digest


def launch_host(instance: str, plan: dict[str, object], key: str,
                version: str, digest: str) -> str:
    assignments = plan["assignments"]  # type: ignore[index]
    rows = assignments[instance]  # type: ignore[index]
    encoded = base64.b64encode(canonical(rows).encode()).decode()
    binary = f"{ROOT}/ultimate_tablebase-devil-v6"
    runner = f"{ROOT}/run_ultimate_devil_repartition_worker.py"
    host_runner = f"{ROOT}/run_ultimate_devil_repartition_host.py"
    worker_count = sum(len(cpus) for _, cpus in HOSTS)
    cpus = ",".join(str(row["cpu"]) for row in rows)
    expected = sum(len(row["shards"]) for row in rows) + sum(
        len(range(int(row["worker"]), fleet.SHARDS, worker_count))
        for row in rows) * len(PAIR_CAMPAIGNS)
    command = "\n".join((
        "set -euo pipefail",
        f"mkdir -p {ROOT} {LOG_ROOT}",
        f"aws s3api get-object --region {fleet.REGION} --bucket {fleet.BUCKET} "
        f"--key {shlex.quote(SOURCE_KEY)} --version-id {SOURCE_VERSION} "
        f"{ROOT}/source.tar.new >/dev/null",
        f"test \"$(sha256sum {ROOT}/source.tar.new | cut -d' ' -f1)\" = "
        f"{SOURCE_SHA256}",
        f"mv {ROOT}/source.tar.new {ROOT}/source.tar",
        f"aws s3api get-object --region {fleet.REGION} --bucket {fleet.BUCKET} "
        f"--key {shlex.quote(BINARY_KEY)} --version-id {BINARY_VERSION} "
        f"{binary}.new >/dev/null",
        f"test \"$(sha256sum {binary}.new | cut -d' ' -f1)\" = "
        f"{BINARY_SHA256}",
        f"mv {binary}.new {binary}",
        f"chmod 755 {binary}",
        f"aws s3api get-object --region {fleet.REGION} --bucket {fleet.BUCKET} "
        f"--key {shlex.quote(RUNNER_KEY)} --version-id {RUNNER_VERSION} "
        f"{runner}.new >/dev/null",
        f"test \"$(sha256sum {runner}.new | cut -d' ' -f1)\" = "
        f"{RUNNER_SHA256}",
        f"mv {runner}.new {runner}",
        f"chmod 755 {runner}",
        f"aws s3api get-object --region {fleet.REGION} --bucket {fleet.BUCKET} "
        f"--key {shlex.quote(HOST_RUNNER_KEY)} "
        f"--version-id {HOST_RUNNER_VERSION} {host_runner}.new >/dev/null",
        f"test \"$(sha256sum {host_runner}.new | cut -d' ' -f1)\" = "
        f"{HOST_RUNNER_SHA256}",
        f"mv {host_runner}.new {host_runner}",
        f"chmod 755 {host_runner}",
        f"aws s3api get-object --region {fleet.REGION} --bucket {fleet.BUCKET} "
        f"--key {shlex.quote(key)} --version-id {shlex.quote(version)} "
        f"{PLAN_PATH}.new >/dev/null",
        f"test \"$(sha256sum {PLAN_PATH}.new | cut -d' ' -f1)\" = {digest}",
        f"mv {PLAN_PATH}.new {PLAN_PATH}",
        f"test \"$(sha256sum {binary} | cut -d' ' -f1)\" = "
        f"{BINARY_SHA256}",
        f"test \"$(sha256sum {runner} | cut -d' ' -f1)\" = "
        f"{RUNNER_SHA256}",
        f"test \"$(sha256sum {host_runner} | cut -d' ' -f1)\" = "
        f"{HOST_RUNNER_SHA256}",
        f"test \"$(python3 -c 'import json,sys; sys.stdout.write(json.dumps(json.load(open(sys.argv[1]))[\"assignments\"][sys.argv[2]],sort_keys=True,separators=(\",\",\":\")))' {PLAN_PATH} {instance} | base64 -w0)\" = {encoded}",
        f"systemctl is-active --quiet {PARENT}.service || "
        f"systemd-run --quiet --collect --unit={PARENT} "
        f"--property=AllowedCPUs={cpus} --property=Nice=10 "
        f"--property=OOMPolicy=stop /usr/bin/python3 {host_runner} "
        f"--plan {PLAN_PATH} --plan-sha256 {digest} "
        f"--plan-version {shlex.quote(version)} --instance {instance} "
        f"--binary {binary} --binary-sha256 {BINARY_SHA256} "
        f"--worker-runner {runner} --worker-runner-sha256 {RUNNER_SHA256} "
        f"--log-root {LOG_ROOT} --bucket {fleet.BUCKET} "
        f"--receipt-prefix {RECEIPT_PREFIX}",
        f"echo STARTED plan_sha256={digest} expected={expected}",
    ))
    return fleet.send(instance, [command])


def status_host(instance: str) -> str:
    command = (
        f"echo instance={instance}; systemctl show {PARENT}.service "
        "-p ActiveState -p SubState -p Result -p ExecMainStatus -p AllowedCPUs "
        "-p CPUUsageNSec -p MemoryCurrent --no-pager; "
        f"echo active_children=$(systemctl list-units '{CHILD}*.service' "
        "--state=active --no-legend | wc -l); "
        f"echo completed=$(grep -h -c '^DEVIL_CLOSURE_CENSUS_OK ' "
        f"{LOG_ROOT}/worker-*.log 2>/dev/null | awk '{{s+=$1}} END{{print s+0}}'); "
        "echo child_memory_bytes=$(for unit in $(systemctl list-units "
        f"'{CHILD}*.service' --state=active --no-legend | awk '{{print $1}}'); "
        "do systemctl show \"$unit\" -p MemoryCurrent --value; done | "
        "awk '{s+=$1} END{print s+0}'); grep -E '^(MemAvailable|MemTotal):' "
        "/proc/meminfo")
    return fleet.send(instance, [command])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--inventory", action="store_true")
    mode.add_argument("--stop-plan", type=Path)
    mode.add_argument("--launch", type=Path)
    mode.add_argument("--status", action="store_true")
    args = parser.parse_args()
    if args.inventory:
        receipts, counts = inventory()
        print(json.dumps({"completed": len(receipts), "counts": counts,
                          "missing": sorted(set(range(fleet.SHARDS)) -
                                            set(receipts))}, indent=2))
    elif args.stop_plan:
        plan = make_plan(args.stop_plan)
        validate_plan(plan)
        print(json.dumps({"plan": str(args.stop_plan),
                          "completed": len(plan["completed"]),
                          "remaining": len(plan["remaining"])}))
    elif args.launch:
        plan = json.loads(args.launch.read_text())
        validate_plan(plan)
        key, version, digest = upload_plan(args.launch)
        result = parallel(lambda instance: launch_host(
            instance, plan, key, version, digest))
        print(json.dumps({"plan_sha256": digest, "key": key,
                          "version_id": version, "hosts": result}, indent=2))
    else:
        print(json.dumps(parallel(status_host), indent=2))


if __name__ == "__main__":
    main()
