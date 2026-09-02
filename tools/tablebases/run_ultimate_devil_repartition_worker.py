#!/usr/bin/env python3
"""Run one authenticated, non-overlapping Devil closure worker."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


SHARDS = 992
DEPTH = 12
STATE_LIMIT = 200_000_000
RECEIPT = re.compile(
    r"DEVIL_CLOSURE_CENSUS_OK shard ([0-9]+)/992 visited 200000000 "
    r"boundary ([0-9]+) max_minions ([0-9]+) capped 1")
MARKER = re.compile(
    r"DEVIL_TASK_OK kind=(single|pair) class=([^ ]+) shard=([0-9]+) "
    r"receipt_sha256=([0-9a-f]{64})")


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def completed_tasks(log: Path, worker: int, workers: int,
                    classes: set[str]) -> tuple[set[tuple[str, str, int]], bool]:
    completed: set[tuple[str, str, int]] = set()
    if not log.exists():
        return completed, False
    worker_complete = False
    for line in log.read_text(errors="strict").splitlines():
        if line.startswith("DEVIL_WORKER_COMPLETE "):
            if line != f"DEVIL_WORKER_COMPLETE worker={worker} residual=0" or worker_complete:
                raise RuntimeError("invalid persisted Devil worker completion marker")
            worker_complete = True
            continue
        if not line.startswith("DEVIL_TASK_OK "):
            continue
        match = MARKER.fullmatch(line)
        if match is None:
            raise RuntimeError("invalid persisted Devil completion marker")
        kind, class_id, raw_shard, _ = match.groups()
        shard = int(raw_shard)
        if (not 0 <= shard < SHARDS or
                (kind == "pair" and
                 (class_id not in classes or shard % workers != worker)) or
                (kind == "single" and class_id != "single:devil")):
            raise RuntimeError("persisted Devil marker assignment residual")
        task = (kind, class_id, shard)
        if task in completed:
            raise RuntimeError("duplicate persisted Devil completion marker")
        completed.add(task)
    return completed, worker_complete


def archive_log(log: Path, bucket: str, prefix: str, plan_sha256: str,
                worker: int) -> str:
    digest = sha256_path(log)
    key = f"{prefix}/{plan_sha256}/workers/worker-{worker:03d}.log"
    response = json.loads(subprocess.check_output([
        "aws", "s3api", "put-object", "--bucket", bucket, "--key", key,
        "--body", str(log), "--metadata",
        f"sha256={digest},plan-sha256={plan_sha256},worker={worker}",
        "--output", "json",
    ], text=True))
    version = str(response.get("VersionId", ""))
    if not version:
        raise RuntimeError("versioned Devil worker-log archive residual")
    return version


def run_task(binary: Path, log_stream, kind: str, class_id: str,
             piece2: str, opposing: bool, shard: int) -> None:
    command = [
        str(binary), "--piece", "devil", "--workers", "1",
        "--devil-frontier-depth", str(DEPTH),
        "--devil-frontier-limit", str(STATE_LIMIT),
        "--devil-frontier-shard", str(shard),
        "--devil-frontier-shards", str(SHARDS),
    ]
    if piece2:
        command += ["--piece2", piece2]
    if opposing:
        command.append("--opposing")
    log_stream.write(
        f"DEVIL_TASK_BEGIN kind={kind} class={class_id} shard={shard}\n")
    log_stream.flush()
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, encoding="utf-8", errors="strict")
    receipt = ""
    assert process.stdout is not None
    for line in process.stdout:
        log_stream.write(line)
        candidate = line.rstrip("\n")
        if candidate.startswith("DEVIL_CLOSURE_CENSUS_OK "):
            receipt = candidate
    status = process.wait()
    log_stream.flush()
    if status:
        raise RuntimeError(
            f"Devil task failed ({status}): {class_id} shard {shard}")
    match = RECEIPT.fullmatch(receipt)
    if match is None or int(match.group(1)) != shard:
        raise RuntimeError(
            f"Devil task receipt residual: {class_id} shard {shard}")
    digest = hashlib.sha256(receipt.encode()).hexdigest()
    log_stream.write(
        f"DEVIL_TASK_OK kind={kind} class={class_id} shard={shard} "
        f"receipt_sha256={digest}\n")
    log_stream.flush()
    os.fsync(log_stream.fileno())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--plan-sha256", required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--binary-sha256", required=True)
    parser.add_argument("--worker", type=int, required=True)
    parser.add_argument("--workers", type=int, required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--receipt-prefix", required=True)
    args = parser.parse_args()

    if (sha256_path(args.plan) != args.plan_sha256 or
            sha256_path(args.binary) != args.binary_sha256):
        raise RuntimeError("Devil worker provenance residual")
    plan = json.loads(args.plan.read_text())
    rows = [row for values in plan["assignments"].values()
            for row in values]
    row = next((value for value in rows
                if int(value["worker"]) == args.worker), None)
    if (row is None or args.workers != len(rows) or
            not 0 <= args.worker < args.workers):
        raise RuntimeError("Devil worker plan residual")
    campaigns = plan["pair_campaigns"]
    classes = {str(campaign["id"]) for campaign in campaigns}
    if len(classes) != len(campaigns):
        raise RuntimeError("duplicate Devil pair campaign")
    args.log.parent.mkdir(parents=True, exist_ok=True)
    completed, worker_complete = completed_tasks(
        args.log, args.worker, args.workers, classes)
    if args.log.exists():
        archive_log(args.log, args.bucket, args.receipt_prefix,
                    args.plan_sha256, args.worker)
    if worker_complete:
        return
    with args.log.open("a", encoding="utf-8") as output:
        for shard in map(int, row["shards"]):
            task = ("single", "single:devil", shard)
            if task not in completed:
                run_task(args.binary, output, *task[:2], "", False, shard)
                archive_log(args.log, args.bucket, args.receipt_prefix,
                            args.plan_sha256, args.worker)
        for campaign in campaigns:
            class_id = str(campaign["id"])
            for shard in range(args.worker, SHARDS, args.workers):
                task = ("pair", class_id, shard)
                if task in completed:
                    continue
                run_task(args.binary, output, "pair", class_id,
                         str(campaign["piece2"]),
                         bool(campaign["opposing"]), shard)
                archive_log(args.log, args.bucket, args.receipt_prefix,
                            args.plan_sha256, args.worker)
        output.write(
            f"DEVIL_WORKER_COMPLETE worker={args.worker} residual=0\n")
        output.flush()
        os.fsync(output.fileno())
        archive_log(args.log, args.bucket, args.receipt_prefix,
                    args.plan_sha256, args.worker)


if __name__ == "__main__":
    main()
