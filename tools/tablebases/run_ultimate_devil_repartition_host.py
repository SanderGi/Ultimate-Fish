#!/usr/bin/env python3
"""Launch and supervise one host's authenticated Devil worker partition."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


CHILD_PREFIX = "ultimatefish-devil-fleet-v6-repartition-v2-"


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def active(unit: str) -> bool:
    result = subprocess.run(
        ["systemctl", "is-active", "--quiet", f"{unit}.service"],
        check=False)
    return result.returncode == 0


def complete(log: Path, worker: int) -> bool:
    if not log.is_file():
        return False
    marker = f"DEVIL_WORKER_COMPLETE worker={worker} residual=0"
    return sum(line == marker for line in
               log.read_text(errors="strict").splitlines()) == 1


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--plan-sha256", required=True)
    parser.add_argument("--plan-version", required=True)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--binary-sha256", required=True)
    parser.add_argument("--worker-runner", type=Path, required=True)
    parser.add_argument("--worker-runner-sha256", required=True)
    parser.add_argument("--log-root", type=Path, required=True)
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--receipt-prefix", required=True)
    args = parser.parse_args()

    if (sha256_path(args.plan) != args.plan_sha256 or
            sha256_path(args.binary) != args.binary_sha256 or
            sha256_path(args.worker_runner) != args.worker_runner_sha256):
        raise RuntimeError("Devil host-launch provenance residual")
    plan = json.loads(args.plan.read_text())
    rows = plan["assignments"].get(args.instance)
    all_rows = [row for values in plan["assignments"].values()
                for row in values]
    if not isinstance(rows, list) or not rows or len(all_rows) != 133:
        raise RuntimeError("Devil host assignment residual")
    worker_count = len(all_rows)
    args.log_root.mkdir(parents=True, exist_ok=True)
    for row in rows:
        worker = int(row["worker"])
        cpu = int(row["cpu"])
        unit = f"{CHILD_PREFIX}{worker:02d}"
        log = args.log_root / f"worker-{worker:02d}.log"
        if complete(log, worker) or active(unit):
            continue
        subprocess.run([
            "systemd-run", "--quiet", "--collect", f"--unit={unit}",
            f"--property=AllowedCPUs={cpu}", "--property=Nice=10",
            "--property=MemoryHigh=6442450944",
            "--property=MemoryMax=7516192768",
            "--property=OOMPolicy=stop", "/usr/bin/python3",
            str(args.worker_runner), "--plan", str(args.plan),
            "--plan-sha256", args.plan_sha256,
            "--binary", str(args.binary),
            "--binary-sha256", args.binary_sha256,
            "--worker", str(worker), "--workers", str(worker_count),
            "--log", str(log),
            "--bucket", args.bucket,
            "--receipt-prefix", args.receipt_prefix,
        ], check=True)

    while any(active(f"{CHILD_PREFIX}{int(row['worker']):02d}")
              for row in rows):
        time.sleep(10)
    incomplete = [int(row["worker"]) for row in rows
                  if not complete(
                      args.log_root / f"worker-{int(row['worker']):02d}.log",
                      int(row["worker"]))]
    if incomplete:
        raise RuntimeError(f"incomplete Devil workers: {incomplete}")
    marker = args.log_root / f"COMPLETE-{args.instance}"
    temporary = marker.with_name(f"{marker.name}.tmp-{os.getpid()}")
    expected = sum(len(row["shards"]) for row in rows) + sum(
        len(range(int(row["worker"]), 992, worker_count))
        for row in rows) * len(plan["pair_campaigns"])
    with temporary.open("x", encoding="utf-8") as output:
        output.write(
            f"plan_sha256={args.plan_sha256} "
            f"plan_version={args.plan_version} expected={expected}\n")
        output.flush()
        os.fsync(output.fileno())
    temporary.replace(marker)


if __name__ == "__main__":
    main()
