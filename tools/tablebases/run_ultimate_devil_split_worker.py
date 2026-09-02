#!/usr/bin/env python3
"""Split one existing Devil worker's campaigns into two disjoint lanes."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import run_ultimate_devil_repartition_worker as base


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--plan-sha256", required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--binary-sha256", required=True)
    parser.add_argument("--source-worker", type=int, required=True)
    parser.add_argument("--lane", type=int, choices=(0, 1), required=True)
    parser.add_argument("--prior-log", type=Path, required=True)
    parser.add_argument("--prior-log-sha256", required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--receipt-prefix", required=True)
    args = parser.parse_args()

    if (sha256_path(args.plan) != args.plan_sha256 or
            sha256_path(args.binary) != args.binary_sha256 or
            sha256_path(args.prior_log) != args.prior_log_sha256):
        raise RuntimeError("Devil split-worker provenance residual")
    plan = json.loads(args.plan.read_text())
    rows = [row for values in plan["assignments"].values()
            for row in values]
    workers = len(rows)
    row = next((value for value in rows
                if int(value["worker"]) == args.source_worker), None)
    if row is None or workers != 133:
        raise RuntimeError("Devil split-worker plan residual")
    campaigns = plan["pair_campaigns"]
    classes = {str(campaign["id"]) for campaign in campaigns}
    if len(classes) != len(campaigns):
        raise RuntimeError("duplicate Devil split campaign")
    completed, worker_complete = base.completed_tasks(
        args.prior_log, args.source_worker, workers, classes)
    if worker_complete:
        return
    args.log.parent.mkdir(parents=True, exist_ok=True)
    lane_completed, _ = base.completed_tasks(
        args.log, args.source_worker, workers, classes)
    completed.update(lane_completed)
    marker = (f"DEVIL_SPLIT_COMPLETE source_worker={args.source_worker} "
              f"lane={args.lane} residual=0")
    if args.log.exists():
        if sum(line == marker for line in
               args.log.read_text(errors="strict").splitlines()) == 1:
            return
        base.archive_log(args.log, args.bucket, args.receipt_prefix,
                         args.plan_sha256, args.source_worker)
    with args.log.open("a", encoding="utf-8") as output:
        if args.lane == 0:
            for shard in map(int, row["shards"]):
                task = ("single", "single:devil", shard)
                if task not in completed:
                    base.run_task(args.binary, output, *task[:2], "", False,
                                  shard)
                    base.archive_log(
                        args.log, args.bucket, args.receipt_prefix,
                        args.plan_sha256, args.source_worker)
        for index, campaign in enumerate(campaigns):
            if index % 2 != args.lane:
                continue
            class_id = str(campaign["id"])
            for shard in range(args.source_worker, base.SHARDS, workers):
                task = ("pair", class_id, shard)
                if task in completed:
                    continue
                base.run_task(args.binary, output, "pair", class_id,
                              str(campaign["piece2"]),
                              bool(campaign["opposing"]), shard)
                base.archive_log(args.log, args.bucket, args.receipt_prefix,
                                 args.plan_sha256, args.source_worker)
        output.write(marker + "\n")
        output.flush()
        base.os.fsync(output.fileno())
        base.archive_log(args.log, args.bucket, args.receipt_prefix,
                         args.plan_sha256, args.source_worker)


if __name__ == "__main__":
    main()
