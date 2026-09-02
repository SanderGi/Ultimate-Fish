#!/usr/bin/env python3
"""Launch authenticated, non-overlapping Devil campaign-resplit workers."""

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


def active_cpu_sets() -> dict[int, str]:
    result: dict[int, str] = {}
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
            begin, end = int(bounds[0]), int(bounds[-1])
            for cpu in range(begin, end + 1):
                if cpu in result:
                    raise RuntimeError("pre-existing CPU overlap residual")
                result[cpu] = unit
    return result


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
        raise RuntimeError("Devil resplit manifest hash residual")
    manifest = json.loads(args.manifest.read_text())
    if (manifest["schema"] != "ultimate-devil-campaign-resplit-v2" or
            sha256_path(args.plan) != manifest["base_plan_sha256"] or
            sha256_path(args.binary) != manifest["binary_sha256"] or
            sha256_path(args.runner) != manifest["runner_sha256"] or
            manifest["source_worker_modulus"] != 133 or
            manifest["campaign_modulus"] != 4 or
            manifest["campaign_residues"] != [1, 3]):
        raise RuntimeError("Devil resplit manifest binding residual")

    occupied = active_cpu_sets()
    targets: set[int] = set()
    for row in manifest["workers"]:
        worker = int(row["source_worker"])
        old_unit = f"ultimatefish-devil-fleet-v6-split-v1-{worker}-1.service"
        old_state = subprocess.run(
            ["systemctl", "is-active", "--quiet", old_unit], check=False)
        if old_state.returncode == 0:
            raise RuntimeError("Devil source split lane is still active")
        base_log = args.root / "fleet-repartition-v2" / f"worker-{worker}.log"
        split_log = args.root / "fleet-split-v1" / f"worker-{worker}-lane-1.log"
        if (sha256_path(base_log) != row["prior_base_log_sha256"] or
                sha256_path(split_log) != row["prior_split_log_sha256"]):
            raise RuntimeError("Devil resplit prior-log residual")
        for raw_residue, raw_cpu in row["residue_cpus"].items():
            residue, cpu = int(raw_residue), int(raw_cpu)
            if residue not in (1, 3) or cpu in targets or cpu in occupied:
                raise RuntimeError("Devil resplit CPU allocation residual")
            targets.add(cpu)

    for row in manifest["workers"]:
        worker = int(row["source_worker"])
        base_log = args.root / "fleet-repartition-v2" / f"worker-{worker}.log"
        split_log = args.root / "fleet-split-v1" / f"worker-{worker}-lane-1.log"
        for raw_residue, raw_cpu in row["residue_cpus"].items():
            residue, cpu = int(raw_residue), int(raw_cpu)
            unit = f"ultimatefish-devil-fleet-v6-resplit-v2-{worker}-{residue}.service"
            log = args.root / "fleet-resplit-v2" / f"worker-{worker}-residue-{residue}.log"
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
                "--prior-split-log", str(split_log),
                "--prior-split-log-sha256", row["prior_split_log_sha256"],
                "--campaign-modulus", "4",
                "--campaign-residue", str(residue),
                "--log", str(log),
                "--bucket", args.bucket,
                "--receipt-prefix", manifest["receipt_prefix"],
            ], check=True)
    print(f"DEVIL_RESPLIT_V2_LAUNCHED workers={len(targets)} residual=0")


if __name__ == "__main__":
    main()
