#!/usr/bin/env python3
"""Launch authenticated, disjoint Devil sixteenth-shard v8 workers."""

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
        raise RuntimeError("Devil sixteenth-shard manifest hash residual")
    manifest = json.loads(args.manifest.read_text())
    if (manifest["schema"] != "ultimate-devil-sixteenth-shard-v8" or
            sha256_path(args.plan) != manifest["base_plan_sha256"] or
            sha256_path(args.binary) != manifest["binary_sha256"] or
            sha256_path(args.runner) != manifest["runner_sha256"] or
            manifest["source_worker_modulus"] != 133 or
            manifest["campaign_modulus"] != 2 or
            manifest["shard_modulus"] != 16):
        raise RuntimeError("Devil sixteenth-shard manifest binding residual")

    occupied = occupied_cpus()
    targets: set[int] = set()
    paths: dict[tuple[int, int, int], tuple[Path, ...]] = {}
    for row in manifest["lanes"]:
        worker = int(row["source_worker"])
        campaign = int(row["campaign_lane"])
        parent = int(row["parent_residue"])
        old_unit = ("ultimatefish-devil-fleet-v6-eighth-shard-v7-"
                    f"{worker}-{campaign}-{parent}.service")
        if subprocess.run(["systemctl", "is-active", "--quiet", old_unit],
                          check=False).returncode == 0:
            raise RuntimeError("Devil source eighth-shard is still active")
        quarter = parent % 4
        prior_shard = quarter % 2
        prior_paths = (
            args.root / "fleet-repartition-v2" / f"worker-{worker}.log",
            args.root / "fleet-campaign-split-v4" /
            f"worker-{worker}-lane-{campaign}.log",
            args.root / "fleet-campaign-shard-v5" /
            f"worker-{worker}-campaign-{campaign}-shard-{prior_shard}.log",
            args.root / "fleet-quarter-shard-v6" /
            f"worker-{worker}-campaign-{campaign}-residue-{quarter}.log",
            args.root / "fleet-eighth-shard-v7" /
            f"worker-{worker}-campaign-{campaign}-residue-{parent}.log",
        )
        paths[(worker, campaign, parent)] = prior_paths
        if [sha256_path(path) for path in prior_paths] != row["prior_hashes"]:
            raise RuntimeError("Devil sixteenth-shard prior-log residual")
        for raw_residue, raw_cpu in row["residue_cpus"].items():
            residue, cpu = int(raw_residue), int(raw_cpu)
            if (campaign not in (0, 1) or parent not in range(8) or
                    residue not in range(16) or residue % 8 != parent or
                    cpu in targets or cpu in occupied):
                raise RuntimeError("Devil sixteenth-shard CPU residual")
            targets.add(cpu)

    for row in manifest["lanes"]:
        worker = int(row["source_worker"])
        campaign = int(row["campaign_lane"])
        parent = int(row["parent_residue"])
        prior_paths = paths[(worker, campaign, parent)]
        for raw_residue, raw_cpu in row["residue_cpus"].items():
            residue, cpu = int(raw_residue), int(raw_cpu)
            unit = ("ultimatefish-devil-fleet-v6-sixteenth-shard-v8-"
                    f"{worker}-{campaign}-{residue}.service")
            log = (args.root / "fleet-sixteenth-shard-v8" /
                   f"worker-{worker}-campaign-{campaign}-residue-{residue}.log")
            command = [
                "systemd-run", f"--unit={unit}",
                f"--property=AllowedCPUs={cpu}",
                "--property=CPUAccounting=yes", "--property=MemoryAccounting=yes",
                "/usr/bin/python3", str(args.runner),
                "--plan", str(args.plan), "--plan-sha256", manifest["base_plan_sha256"],
                "--binary", str(args.binary), "--binary-sha256", manifest["binary_sha256"],
                "--source-worker", str(worker),
            ]
            for prior_path, prior_hash in zip(prior_paths, row["prior_hashes"]):
                command.extend(["--prior-log", str(prior_path),
                                "--prior-log-sha256", prior_hash])
            command.extend([
                "--campaign-lane", str(campaign), "--parent-residue", str(parent),
                "--shard-residue", str(residue), "--log", str(log),
                "--bucket", args.bucket, "--receipt-prefix", manifest["receipt_prefix"],
            ])
            subprocess.run(command, check=True)
    print(f"DEVIL_SIXTEENTH_SHARD_V8_LAUNCHED workers={len(targets)} residual=0")


if __name__ == "__main__":
    main()
