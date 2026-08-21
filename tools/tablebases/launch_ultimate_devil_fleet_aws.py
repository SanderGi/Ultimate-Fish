#!/usr/bin/env python3
"""Stage and run the authenticated Devil v5 closure census fleet-wide."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import shlex
import subprocess
import time


REGION = "us-west-2"
BUCKET = "ultimatefish-info-20260808-a4e679c6-831688117652"
SOURCE_SHA256 = "9429b8bbd1001acf49f26b6085d709796ab2388b320ed9ae46947479858634c9"
SOURCE_VERSION = "5Xxzz52cxOKFol0Hiok8UvYyOEIjyD2l"
SOURCE_KEY = (f"sources/bundles/devil-closure-v5/sha256/{SOURCE_SHA256}/"
              "ultimatefish-devil-closure-v5-source.tar")
BINARY_SHA256 = "1965bc7e828672b9dcc1d9247546271d67e735bd48ab8018cd9498b917325db6"
BINARY_VERSION = "Z8qMKzKA6vMZyMYFzDY0pexmzdwV0vpW"
BINARY_KEY = (f"sources/binaries/devil-closure-v5/sha256/{BINARY_SHA256}/"
              "ultimate_tablebase-devil-v5")
SEMANTICS = (
    "no-preexisting-minions-devil-first-three-ranks-cooldown-"
    "exact-bitboard-codec-v5"
)
ROOT = f"/mnt/ultimatefish/devil-closure-v5-{SOURCE_SHA256[:8]}"
PARENT = "ultimatefish-devil-fleet-v5"
CHILD_PREFIX = PARENT + "-"
SHARDS = 992
DEPTH = 12
STATE_LIMIT = 50_000_000

# These allocations avoid every currently configured tablebase CPU.  A live
# deep-frontier sample established a roughly 2.6-GiB peak per worker, so the
# 64-GiB hosts are capped at 16 workers while the 256-GiB hosts retain their
# CPU-target allocations.  The partition stride remains 116 so restarting a
# reduced host never overlaps work already in progress elsewhere.
HOSTS = (
    ("i-03c81f90d2c59a2e7",
     (0, *range(3, 14), *range(15, 19), *range(20, 23), *range(24, 29)), 0,
     16 * 1024**3),
    ("i-0986ed3d272721f02", tuple(range(3, 19)), 24, 8 * 1024**3),
    ("i-024a2073283e4336e", tuple(range(4, 20)), 50, 8 * 1024**3),
    ("i-08c0f44a1776cb34a", (*range(0, 14), 15, 16), 75,
     8 * 1024**3),
    ("i-0b4523116b2f7765c",
     (*range(13, 21), 22, 23, *range(25, 28)), 103, 16 * 1024**3),
)
TOTAL_WORKERS = 116
ACTIVE_WORKERS = sum(len(cpus) for _, cpus, _, _ in HOSTS)
assert ACTIVE_WORKERS == 85

# The 64-GiB hosts were removed after their deep-frontier memory plus retained
# Ghost solves proved unsafe.  The continuation waves cover every slot
# assigned to those hosts and every deliberate gap, using only the measured
# free CPU sets on the 256-GiB hosts.  Slots 24-39 are repeated from zero rather
# than trusting partial logs stranded on i098.  Continuation 3 relocates the
# final i0b wave to i03 because i0b's assigned CPUs remain I/O-bound.
CONTINUATION_HOSTS = {
    "continuation-1": (
        ("i-03c81f90d2c59a2e7",
         (0, *range(3, 14), *range(15, 19), *range(20, 23), *range(24, 29)),
         24, 16 * 1024**3),
        ("i-0b4523116b2f7765c",
         (*range(13, 21), 22, 23, *range(25, 28)),
         48, 16 * 1024**3),
    ),
    "continuation-2": (
        ("i-03c81f90d2c59a2e7",
         (0, *range(3, 14), 15, 16), 61, 16 * 1024**3),
        ("i-0b4523116b2f7765c",
         (*range(13, 21), 22, 23, 25, 26), 91, 16 * 1024**3),
    ),
    "continuation-3": (
        ("i-03c81f90d2c59a2e7",
         (0, *range(3, 14)), 91, 16 * 1024**3),
    ),
    # Every one of the 992 exact 50-million-state samples reached its cap.
    # Re-audit the same disjoint starting-shard partition at 100 million
    # states, sharing the work across all presently idle CPUs.  Per-worker
    # cgroups make this a fail-closed census: a host cannot trade a Ghost
    # solver's memory for deeper Devil evidence.
    "highcap-100m": (
        ("i-03c81f90d2c59a2e7",
         (0, *range(3, 14), *range(15, 19), *range(20, 32)), 0,
         64 * 1024**3),
        ("i-08c0f44a1776cb34a", tuple(range(0, 6)), 28,
         16 * 1024**3),
        ("i-0986ed3d272721f02", tuple(range(3, 9)), 34,
         16 * 1024**3),
        ("i-0b4523116b2f7765c",
         (*range(13, 21), 22, 23, *range(25, 29), 30, 31), 40,
         64 * 1024**3),
    ),
    # Four i03 slots are paused to keep the 256-GiB host below its aggregate
    # memory gate before any of their samples completed. Continue those exact
    # global slots on otherwise-idle i024 CPUs without overlapping coverage.
    "highcap-100m-tail": (
        ("i-024a2073283e4336e", tuple(range(4, 8)), 20,
         32 * 1024**3),
    ),
}
assert sum(len(host[1]) for host in CONTINUATION_HOSTS["continuation-1"]) == 37
assert sum(len(host[1]) for host in CONTINUATION_HOSTS["continuation-2"]) == 26
assert sum(len(host[1]) for host in CONTINUATION_HOSTS["continuation-3"]) == 12
assert sum(len(host[1]) for host in CONTINUATION_HOSTS["highcap-100m"]) == 56
assert sum(len(host[1]) for host in CONTINUATION_HOSTS["highcap-100m-tail"]) == 4


def campaign_stride(campaign: str) -> int:
    return 56 if campaign.startswith("highcap-100m") else TOTAL_WORKERS


def campaign_state_limit(campaign: str) -> int:
    return 100_000_000 if campaign.startswith("highcap-100m") else STATE_LIMIT


def campaign_units(campaign: str) -> tuple[str, str, str]:
    if campaign == "primary":
        return PARENT, CHILD_PREFIX, "fleet-116"
    if campaign == "highcap-100m-tail":
        return (
            "ultimatefish-devil-orchestrator-v5-highcap-100m-tail",
            "ultimatefish-devil-fleet-v5-highcap-100m-tail-",
            "fleet-116-highcap-100m",
        )
    return (f"ultimatefish-devil-orchestrator-v5-{campaign}",
            f"ultimatefish-devil-fleet-v5-{campaign}-",
            f"fleet-116-{campaign}")


def aws_json(arguments: list[str]) -> dict[str, object]:
    value = json.loads(subprocess.check_output(arguments, text=True))
    if not isinstance(value, dict):
        raise RuntimeError("AWS command did not return an object")
    return value


def send(instance: str, commands: list[str], timeout: int = 300) -> str:
    response = aws_json([
        "aws", "ssm", "send-command", "--region", REGION,
        "--instance-ids", instance, "--document-name", "AWS-RunShellScript",
        "--parameters", json.dumps({"commands": commands}), "--output", "json",
    ])
    command_id = str(response["Command"]["CommandId"])  # type: ignore[index]
    deadline = time.monotonic() + timeout
    while True:
        result = aws_json([
            "aws", "ssm", "get-command-invocation", "--region", REGION,
            "--command-id", command_id, "--instance-id", instance,
            "--output", "json",
        ])
        status = str(result.get("Status"))
        if status == "Success":
            return str(result.get("StandardOutputContent", ""))
        if status not in {"Pending", "InProgress", "Delayed"}:
            raise RuntimeError(
                f"{instance} SSM {status}: "
                f"{result.get('StandardErrorContent', '')}")
        if time.monotonic() >= deadline:
            raise RuntimeError(f"{instance} SSM timeout")
        time.sleep(1)


def launch_host(campaign_host: tuple[
        str, tuple[str, tuple[int, ...], int, int]]) -> str:
    campaign, host = campaign_host
    instance, cpus, base, reserve = host
    parent_unit, child_prefix, log_name = campaign_units(campaign)
    source = f"{ROOT}/source.tar"
    binary = f"{ROOT}/ultimate_tablebase-devil-v5"
    logs = f"{ROOT}/{log_name}"
    marker = f"{logs}/COMPLETE-{instance}"
    children: list[str] = []
    expected = 0
    stride = campaign_stride(campaign)
    state_limit = campaign_state_limit(campaign)
    for slot, cpu in enumerate(cpus):
        global_slot = base + slot
        expected += (SHARDS - 1 - global_slot) // stride + 1
        child = f"{child_prefix}{global_slot:02d}"
        log = f"{logs}/slot-{global_slot:02d}.log"
        loop = (
            "set -euo pipefail; "
            f"completed=$(grep -c '^DEVIL_CLOSURE_CENSUS_OK ' "
            f"{shlex.quote(log)} 2>/dev/null || true); "
            f"start=$(({global_slot}+completed*{stride})); "
            f"for ((shard=start; shard<{SHARDS}; "
            f"shard+={stride})); do "
            f"{shlex.quote(binary)} --piece devil --workers 1 "
            f"--devil-frontier-depth {DEPTH} "
            f"--devil-frontier-limit {state_limit} "
            f"--devil-frontier-shard $shard --devil-frontier-shards {SHARDS}; "
            f"done >>{shlex.quote(log)} 2>&1"
        )
        children.append(
            f"systemctl is-active --quiet {child}.service || "
            f"systemd-run --quiet --collect --unit={child} "
            f"--property=AllowedCPUs={cpu} --property=Nice=10 "
            + ("--property=MemoryHigh=7516192768 "
               "--property=MemoryMax=8589934592 "
               if campaign == "highcap-100m-tail" else
               "--property=MemoryHigh=5368709120 "
               "--property=MemoryMax=7516192768 "
               if campaign == "highcap-100m" else "") +
            f"--property=OOMPolicy=stop /bin/bash -lc {shlex.quote(loop)}")
    cpu_set = ",".join(map(str, cpus))
    # Keep the transient command on one shell line.  Some systemd versions
    # serialize literal newlines in `systemd-run ... /bin/bash -lc ARG` as
    # stray unit directives, which can silently omit a child launch.
    parent = "; ".join((
        "set -euo pipefail", *children,
        f"while test \"$(systemctl list-units '{child_prefix}*.service' "
        "--state=active --no-legend | wc -l)\" -ne 0; do sleep 5; done",
        f"test \"$(grep -h -c '^DEVIL_CLOSURE_CENSUS_OK ' "
        f"{logs}/slot-*.log | awk '{{s+=$1}} END{{print s+0}}')\" "
        f"-ge {expected}",
        f"printf '%s\\n' source_sha256={SOURCE_SHA256} "
        f"binary_sha256={BINARY_SHA256} semantics={SEMANTICS} "
        f"workers={len(cpus)} expected_samples={expected} "
        f"state_limit={state_limit} stride={stride} >{marker}.tmp",
        f"mv {marker}.tmp {marker}",
    ))
    stage = "\n".join((
        "set -euo pipefail",
        f"available=$(awk '/^MemAvailable:/{{print $2*1024}}' /proc/meminfo)",
        f"test \"$available\" -ge {reserve}",
        f"mkdir -p {ROOT} {logs}",
        f"if test -f {source} && test \"$(sha256sum {source} | cut -d' ' -f1)\" "
        f"= {SOURCE_SHA256}; then :; else aws s3api get-object --region {REGION} "
        f"--bucket {BUCKET} --key {SOURCE_KEY} --version-id {SOURCE_VERSION} "
        f"{source}.new >/dev/null; test \"$(sha256sum {source}.new | cut -d' ' -f1)\" "
        f"= {SOURCE_SHA256}; mv {source}.new {source}; fi",
        f"if test -f {binary} && test \"$(sha256sum {binary} | cut -d' ' -f1)\" "
        f"= {BINARY_SHA256}; then :; else aws s3api get-object --region {REGION} "
        f"--bucket {BUCKET} --key {BINARY_KEY} --version-id {BINARY_VERSION} "
        f"{binary}.new >/dev/null; test \"$(sha256sum {binary}.new | cut -d' ' -f1)\" "
        f"= {BINARY_SHA256}; chmod 755 {binary}.new; mv {binary}.new {binary}; fi",
        f"chmod 755 {binary}",
        f"if test -s {marker}; then echo COMPLETE; "
        f"elif systemctl is-active --quiet {parent_unit}.service; then echo RUNNING; "
        f"else systemd-run --quiet --collect --unit={parent_unit} "
        f"--property=AllowedCPUs={cpu_set} --property=Nice=10 "
        f"--property=OOMPolicy=stop /bin/bash -lc {shlex.quote(parent)}; "
        "echo STARTED; fi",
        f"echo instance={instance} workers={len(cpus)} "
        f"source_sha256={SOURCE_SHA256} binary_sha256={BINARY_SHA256}",
    ))
    return send(instance, [stage], timeout=300)


def status_host(campaign_host: tuple[
        str, tuple[str, tuple[int, ...], int, int]]) -> str:
    campaign, host = campaign_host
    instance, cpus, base, _ = host
    parent_unit, child_prefix, log_name = campaign_units(campaign)
    logs = f"{ROOT}/{log_name}"
    stride = campaign_stride(campaign)
    expected = sum((SHARDS - 1 - (base + slot)) // stride + 1
                   for slot in range(len(cpus)))
    mismatch_checks = []
    for slot in range(len(cpus)):
        global_slot = base + slot
        slot_expected = (SHARDS - 1 - global_slot) // stride + 1
        log = f"{logs}/slot-{global_slot:02d}.log"
        mismatch_checks.append(
            f"actual=$(grep -c '^DEVIL_CLOSURE_CENSUS_OK ' {log} "
            f"2>/dev/null || true); test \"$actual\" -eq {slot_expected} || "
            f"echo slot={global_slot} samples=$actual/{slot_expected}")
    mismatch_command = "; ".join(mismatch_checks)
    command = (
        f"echo instance={instance} expected_samples={expected}; "
        f"systemctl show {parent_unit}.service --property=ActiveState,SubState,Result,"
        "ExecMainStatus,AllowedCPUs,CPUUsageNSec,MemoryCurrent --no-pager; "
        f"echo active_children=$(systemctl list-units '{child_prefix}*.service' "
        "--state=active --no-legend | wc -l); "
        f"echo completed_samples=$(grep -h -c '^DEVIL_CLOSURE_CENSUS_OK ' "
        f"{logs}/slot-*.log 2>/dev/null | awk '{{s+=$1}} END{{print s+0}}'); "
        "echo child_memory_bytes=$(for unit in $(systemctl list-units "
        f"'{child_prefix}*.service' --state=active --no-legend | awk "
        "'{print $1}'); do systemctl show \"$unit\" -p MemoryCurrent --value; "
        "done | awk '{s+=$1} END{print s+0}'); "
        "grep -E '^(MemAvailable|MemTotal):' /proc/meminfo; "
        f"ls -l {logs}/COMPLETE-{instance} 2>/dev/null || true; "
        f"echo incomplete_slots; {mismatch_command}; "
        f"if test ! -s {logs}/COMPLETE-{instance}; then "
        f"echo parent_journal_tail; journalctl -u {parent_unit}.service -n 20 "
        "--no-pager -o cat; fi"
    )
    return send(instance, [command])


def stop_host(campaign_host: tuple[
        str, tuple[str, tuple[int, ...], int, int]]) -> str:
    campaign, host = campaign_host
    instance, _, _, _ = host
    parent_unit, child_prefix, _ = campaign_units(campaign)
    return send(instance, [
        f"systemctl stop {parent_unit}.service || true; "
        f"systemctl stop '{child_prefix}*.service' || true; echo STOPPED"
    ])


def run_parallel(action, campaign: str,
                 instances: set[str] | None = None) -> None:
    hosts = HOSTS if campaign == "primary" else CONTINUATION_HOSTS[campaign]
    selected = [host for host in hosts
                if instances is None or host[0] in instances]
    if not selected:
        raise RuntimeError("no configured host matched --instance")
    with ThreadPoolExecutor(max_workers=3) as executor:
        futures = {executor.submit(action, (campaign, host)): host[0]
                   for host in selected}
        for future in as_completed(futures):
            instance = futures[future]
            print(f"[{instance}]\n{future.result().rstrip()}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--stop", action="store_true")
    parser.add_argument("--campaign", choices=(
        "primary", *CONTINUATION_HOSTS), default="primary")
    parser.add_argument("--instance", action="append", default=[])
    args = parser.parse_args()
    run_parallel(launch_host if args.launch else
                 status_host if args.status else stop_host,
                 args.campaign,
                 set(args.instance) if args.instance else None)


if __name__ == "__main__":
    main()
