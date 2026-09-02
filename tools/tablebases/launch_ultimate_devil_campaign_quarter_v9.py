#!/usr/bin/env python3
"""Launch authenticated, disjoint Devil campaign-quarter v9 workers."""

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
        raw = subprocess.check_output([
            "systemctl", "show", "--property=AllowedCPUs", "--value",
            line.split()[0],
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
        raise RuntimeError("Devil campaign-quarter manifest hash residual")
    manifest = json.loads(args.manifest.read_text())
    if (manifest["schema"] != "ultimate-devil-campaign-quarter-v9" or
            sha256_path(args.plan) != manifest["base_plan_sha256"] or
            sha256_path(args.binary) != manifest["binary_sha256"] or
            sha256_path(args.runner) != manifest["runner_sha256"] or
            manifest["source_worker_modulus"] != 133 or
            manifest["campaign_modulus"] != 4 or
            manifest["shard_modulus"] != 16):
        raise RuntimeError("Devil campaign-quarter binding residual")

    occupied = occupied_cpus()
    targets: set[int] = set()
    paths: dict[tuple[int, int], tuple[Path, ...]] = {}
    for row in manifest["lanes"]:
        worker = int(row["source_worker"])
        parent_campaign = int(row["parent_campaign_lane"])
        shard = int(row["shard_residue"])
        old_unit = ("ultimatefish-devil-fleet-v6-sixteenth-shard-v8-"
                    f"{worker}-{parent_campaign}-{shard}.service")
        if subprocess.run(["systemctl", "is-active", "--quiet", old_unit],
                          check=False).returncode == 0:
            raise RuntimeError("Devil source sixteenth-shard is still active")
        quarter, eighth = shard % 4, shard % 8
        prior_shard = quarter % 2
        prior_paths = (
            args.root / "fleet-repartition-v2" / f"worker-{worker}.log",
            args.root / "fleet-campaign-split-v4" /
            f"worker-{worker}-lane-{parent_campaign}.log",
            args.root / "fleet-campaign-shard-v5" /
            f"worker-{worker}-campaign-{parent_campaign}-shard-{prior_shard}.log",
            args.root / "fleet-quarter-shard-v6" /
            f"worker-{worker}-campaign-{parent_campaign}-residue-{quarter}.log",
            args.root / "fleet-eighth-shard-v7" /
            f"worker-{worker}-campaign-{parent_campaign}-residue-{eighth}.log",
            args.root / "fleet-sixteenth-shard-v8" /
            f"worker-{worker}-campaign-{parent_campaign}-residue-{shard}.log",
        )
        paths[(worker, shard)] = prior_paths
        if [sha256_path(path) for path in prior_paths] != row["prior_hashes"]:
            raise RuntimeError("Devil campaign-quarter prior-log residual")
        for raw_campaign, raw_cpu in row["campaign_cpus"].items():
            campaign, cpu = int(raw_campaign), int(raw_cpu)
            if (parent_campaign not in (0, 1) or campaign not in range(4) or
                    campaign % 2 != parent_campaign or shard not in range(16) or
                    cpu in targets or cpu in occupied):
                raise RuntimeError("Devil campaign-quarter CPU residual")
            targets.add(cpu)

    for row in manifest["lanes"]:
        worker = int(row["source_worker"])
        parent_campaign = int(row["parent_campaign_lane"])
        shard = int(row["shard_residue"])
        prior_paths = paths[(worker, shard)]
        for raw_campaign, raw_cpu in row["campaign_cpus"].items():
            campaign, cpu = int(raw_campaign), int(raw_cpu)
            unit = ("ultimatefish-devil-fleet-v6-campaign-quarter-v9-"
                    f"{worker}-{campaign}-{shard}.service")
            log = (args.root / "fleet-campaign-quarter-v9" /
                   f"worker-{worker}-campaign-{campaign}-shard-{shard}.log")
            command = [
                "systemd-run", f"--unit={unit}", f"--property=AllowedCPUs={cpu}",
                "--property=CPUAccounting=yes", "--property=MemoryAccounting=yes",
                "/usr/bin/python3", str(args.runner), "--plan", str(args.plan),
                "--plan-sha256", manifest["base_plan_sha256"],
                "--binary", str(args.binary), "--binary-sha256", manifest["binary_sha256"],
                "--source-worker", str(worker),
            ]
            for prior_path, prior_hash in zip(prior_paths, row["prior_hashes"]):
                command.extend(["--prior-log", str(prior_path),
                                "--prior-log-sha256", prior_hash])
            command.extend([
                "--parent-campaign-lane", str(parent_campaign),
                "--campaign-residue", str(campaign), "--shard-residue", str(shard),
                "--log", str(log), "--bucket", args.bucket,
                "--receipt-prefix", manifest["receipt_prefix"],
            ])
            subprocess.run(command, check=True)
    print(f"DEVIL_CAMPAIGN_QUARTER_V9_LAUNCHED workers={len(targets)} residual=0")


if __name__ == "__main__":
    main()
