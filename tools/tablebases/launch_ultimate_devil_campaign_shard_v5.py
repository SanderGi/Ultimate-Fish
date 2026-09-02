#!/usr/bin/env python3
"""Launch authenticated, disjoint Devil campaign-shard v5 workers."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def occupied_cpus() -> set[int]:
    occupied: set[int] = set()
    units = subprocess.check_output([
        "systemctl", "list-units", "--state=active", "--type=service",
        "--no-legend", "--plain",
    ], text=True)
    for line in units.splitlines():
        unit = line.split()[0]
        raw = subprocess.check_output([
            "systemctl", "show", "--property=AllowedCPUs", "--value", unit,
        ], text=True).strip()
        for group in raw.replace(",", " ").split():
            bounds = group.split("-", 1)
            occupied.update(range(int(bounds[0]), int(bounds[-1]) + 1))
    return occupied


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--manifest-sha256", required=True)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--bucket", required=True)
    args = parser.parse_args()

    if sha256_path(args.manifest) != args.manifest_sha256:
        raise RuntimeError("Devil campaign-shard manifest hash residual")
    manifest = json.loads(args.manifest.read_text())
    if (manifest["schema"] != "ultimate-devil-campaign-shard-v5" or
            sha256_path(args.plan) != manifest["base_plan_sha256"] or
            sha256_path(args.binary) != manifest["binary_sha256"] or
            sha256_path(args.runner) != manifest["runner_sha256"] or
            manifest["source_worker_modulus"] != 133 or
            manifest["campaign_modulus"] != 2 or
            manifest["shard_lane_modulus"] != 2):
        raise RuntimeError("Devil campaign-shard manifest binding residual")

    occupied = occupied_cpus()
    targets: set[int] = set()
    paths: dict[tuple[int, int], tuple[Path, Path]] = {}
    for row in manifest["lanes"]:
        worker, campaign = int(row["source_worker"]), int(row["campaign_lane"])
        old_unit = f"ultimatefish-devil-fleet-v6-campaign-split-v4-{worker}-{campaign}.service"
        if subprocess.run(["systemctl", "is-active", "--quiet", old_unit],
                          check=False).returncode == 0:
            raise RuntimeError("Devil source campaign lane is still active")
        base_log = args.root / "fleet-repartition-v2" / f"worker-{worker}.log"
        campaign_log = (args.root / "fleet-campaign-split-v4" /
                        f"worker-{worker}-lane-{campaign}.log")
        if (sha256_path(base_log) != row["prior_base_log_sha256"] or
                sha256_path(campaign_log) !=
                row["prior_campaign_log_sha256"]):
            raise RuntimeError("Devil campaign-shard prior-log residual")
        paths[(worker, campaign)] = (base_log, campaign_log)
        for raw_shard, raw_cpu in row["shard_cpus"].items():
            shard_lane, cpu = int(raw_shard), int(raw_cpu)
            if (campaign not in (0, 1) or shard_lane not in (0, 1) or
                    cpu in targets or cpu in occupied):
                raise RuntimeError("Devil campaign-shard CPU residual")
            targets.add(cpu)

    for row in manifest["lanes"]:
        worker, campaign = int(row["source_worker"]), int(row["campaign_lane"])
        base_log, campaign_log = paths[(worker, campaign)]
        for raw_shard, raw_cpu in row["shard_cpus"].items():
            shard_lane, cpu = int(raw_shard), int(raw_cpu)
            unit = ("ultimatefish-devil-fleet-v6-campaign-shard-v5-"
                    f"{worker}-{campaign}-{shard_lane}.service")
            log = (args.root / "fleet-campaign-shard-v5" /
                   f"worker-{worker}-campaign-{campaign}-shard-{shard_lane}.log")
            subprocess.run([
                "systemd-run", f"--unit={unit}",
                f"--property=AllowedCPUs={cpu}",
                "--property=CPUAccounting=yes",
                "--property=MemoryAccounting=yes",
                "/usr/bin/python3", str(args.runner),
                "--plan", str(args.plan),
                "--plan-sha256", manifest["base_plan_sha256"],
                "--binary", str(args.binary),
                "--binary-sha256", manifest["binary_sha256"],
                "--source-worker", str(worker),
                "--prior-base-log", str(base_log),
                "--prior-base-log-sha256", row["prior_base_log_sha256"],
                "--prior-campaign-log", str(campaign_log),
                "--prior-campaign-log-sha256",
                row["prior_campaign_log_sha256"],
                "--campaign-lane", str(campaign),
                "--shard-lane", str(shard_lane),
                "--log", str(log), "--bucket", args.bucket,
                "--receipt-prefix", manifest["receipt_prefix"],
            ], check=True)
    print(f"DEVIL_CAMPAIGN_SHARD_V5_LAUNCHED workers={len(targets)} residual=0")


if __name__ == "__main__":
    main()
