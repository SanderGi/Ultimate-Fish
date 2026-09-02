#!/usr/bin/env python3
"""Resume one stopped Devil campaign residue as disjoint shard lanes."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path

import run_ultimate_devil_repartition_worker as base


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--plan-sha256", required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--binary-sha256", required=True)
    parser.add_argument("--source-worker", type=int, required=True)
    parser.add_argument("--prior-log", action="append", type=Path,
                        required=True)
    parser.add_argument("--prior-log-sha256", action="append", required=True)
    parser.add_argument("--shard-lane", type=int, choices=(0, 1), required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--receipt-prefix", required=True)
    args = parser.parse_args()

    if (base.sha256_path(args.plan) != args.plan_sha256 or
            base.sha256_path(args.binary) != args.binary_sha256 or
            len(args.prior_log) != 3 or
            len(args.prior_log_sha256) != 3 or
            any(base.sha256_path(path) != expected for path, expected in
                zip(args.prior_log, args.prior_log_sha256))):
        raise RuntimeError("Devil shard-split provenance residual")
    plan = json.loads(args.plan.read_text())
    rows = [row for values in plan["assignments"].values()
            for row in values]
    workers = len(rows)
    if (workers != 133 or not any(
            int(row["worker"]) == args.source_worker for row in rows)):
        raise RuntimeError("Devil shard-split plan residual")
    campaigns = plan["pair_campaigns"]
    classes = {str(campaign["id"]) for campaign in campaigns}
    if len(classes) != len(campaigns):
        raise RuntimeError("duplicate Devil shard-split campaign")

    completed: set[tuple[str, str, int]] = set()
    for prior in args.prior_log:
        prior_completed, worker_complete = base.completed_tasks(
            prior, args.source_worker, workers, classes)
        if worker_complete:
            return
        completed.update(prior_completed)
    args.log.parent.mkdir(parents=True, exist_ok=True)
    lane_completed, _ = base.completed_tasks(
        args.log, args.source_worker, workers, classes)
    completed.update(lane_completed)
    marker = (f"DEVIL_SHARD_SPLIT_COMPLETE source_worker={args.source_worker} "
              f"campaign_residue=1 shard_lane={args.shard_lane} residual=0")
    if args.log.exists():
        lines = args.log.read_text(errors="strict").splitlines()
        if sum(line == marker for line in lines) == 1:
            return
        base.archive_log(args.log, args.bucket, args.receipt_prefix,
                         args.plan_sha256, args.source_worker)

    with args.log.open("a", encoding="utf-8") as output:
        for index, campaign in enumerate(campaigns):
            if index % 4 != 1:
                continue
            class_id = str(campaign["id"])
            for ordinal, shard in enumerate(range(
                    args.source_worker, base.SHARDS, workers)):
                if ordinal % 2 != args.shard_lane:
                    continue
                task = ("pair", class_id, shard)
                if task in completed:
                    continue
                base.run_task(args.binary, output, "pair", class_id,
                              str(campaign["piece2"]),
                              bool(campaign["opposing"]), shard)
                base.archive_log(
                    args.log, args.bucket, args.receipt_prefix,
                    args.plan_sha256, args.source_worker)
        output.write(marker + "\n")
        output.flush()
        os.fsync(output.fileno())
        base.archive_log(args.log, args.bucket, args.receipt_prefix,
                         args.plan_sha256, args.source_worker)


if __name__ == "__main__":
    main()
