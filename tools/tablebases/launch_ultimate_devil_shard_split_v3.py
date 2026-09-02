#!/usr/bin/env python3
"""Launch authenticated, non-overlapping Devil shard-split workers."""

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
        raise RuntimeError("Devil shard-split manifest hash residual")
    manifest = json.loads(args.manifest.read_text())
    if (manifest["schema"] != "ultimate-devil-shard-split-v3" or
            sha256_path(args.plan) != manifest["base_plan_sha256"] or
            sha256_path(args.binary) != manifest["binary_sha256"] or
            sha256_path(args.runner) != manifest["runner_sha256"] or
            manifest["source_worker_modulus"] != 133 or
            manifest["campaign_modulus"] != 4 or
            manifest["campaign_residue"] != 1 or
            manifest["shard_lane_modulus"] != 2):
        raise RuntimeError("Devil shard-split manifest binding residual")

    occupied = occupied_cpus()
    targets: set[int] = set()
    checked: dict[int, list[Path]] = {}
    for row in manifest["workers"]:
        worker = int(row["source_worker"])
        old_unit = f"ultimatefish-devil-fleet-v6-resplit-v2-{worker}-1.service"
        if subprocess.run(["systemctl", "is-active", "--quiet", old_unit],
                          check=False).returncode == 0:
            raise RuntimeError("Devil source residue lane is still active")
        logs = [
            args.root / "fleet-repartition-v2" / f"worker-{worker}.log",
            args.root / "fleet-split-v1" / f"worker-{worker}-lane-1.log",
            args.root / "fleet-resplit-v2" / f"worker-{worker}-residue-1.log",
        ]
        if any(sha256_path(path) != expected for path, expected in
               zip(logs, row["prior_hashes"])):
            raise RuntimeError("Devil shard-split prior-log residual")
        checked[worker] = logs
        for raw_lane, raw_cpu in row["lane_cpus"].items():
            lane, cpu = int(raw_lane), int(raw_cpu)
            if lane not in (0, 1) or cpu in targets or cpu in occupied:
                raise RuntimeError("Devil shard-split CPU allocation residual")
            targets.add(cpu)

    for row in manifest["workers"]:
        worker = int(row["source_worker"])
        logs = checked[worker]
        for raw_lane, raw_cpu in row["lane_cpus"].items():
            lane, cpu = int(raw_lane), int(raw_cpu)
            unit = f"ultimatefish-devil-fleet-v6-shard-split-v3-{worker}-{lane}.service"
            log = args.root / "fleet-shard-split-v3" / f"worker-{worker}-lane-{lane}.log"
            command = [
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
            ]
            for prior, digest in zip(logs, row["prior_hashes"]):
                command += ["--prior-log", str(prior),
                            "--prior-log-sha256", digest]
            command += [
                "--shard-lane", str(lane), "--log", str(log),
                "--bucket", args.bucket,
                "--receipt-prefix", manifest["receipt_prefix"],
            ]
            subprocess.run(command, check=True)
    print(f"DEVIL_SHARD_SPLIT_V3_LAUNCHED workers={len(targets)} residual=0")


if __name__ == "__main__":
    main()
