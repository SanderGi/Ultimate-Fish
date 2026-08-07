#!/usr/bin/env python3
"""Generate the exact budget-admitted stateful K+K+2 tablebases."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

import plan_ultimate_tablebases as plan
import ultimate_tablebase_shards as shards


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "src" / "ultimate_tablebase"
TABLEBASES = ROOT / "tablebases"


def logical_size(path: Path) -> int:
    description = shards.manifest(path)
    return description[0] if description else path.stat().st_size


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--budget", type=int, default=plan.DEFAULT_BUDGET)
    parser.add_argument("--max-classes", type=int)
    parser.add_argument("--checkpoint-dir", type=Path,
                        default=Path("/tmp/ultimatefish-stateful-checkpoints"))
    args = parser.parse_args()

    records = plan.inventory(args.budget)
    missing_stateless = [str(record["filename"]) for record in records
                         if record["phase"] == "kings+2-stateless" and
                         not (TABLEBASES / str(record["filename"])).exists()]
    if missing_stateless:
        raise RuntimeError(
            f"stateful generation requires the stateless closure first; "
            f"{len(missing_stateless)} tables remain")
    selected = [record for record in records
                if record["phase"] == "kings+2-stateful"]
    if args.max_classes is not None:
        selected = selected[:args.max_classes]

    subprocess.run(["make", "-C", str(ROOT / "src"), "ultimate-tablebase"], check=True)
    args.checkpoint_dir.mkdir(parents=True, exist_ok=True)
    for record in selected:
        output = TABLEBASES / str(record["filename"])
        if output.exists():
            print(f"skip {output.name}", flush=True)
            continue
        checkpoint = args.checkpoint_dir / (output.stem + "-state-v1.checkpoint")
        command = [
            str(GENERATOR), "--piece", str(record["primary"]),
            "--piece2", str(record["secondary"]), "--output", str(output),
            "--checkpoint", str(checkpoint), "--checkpoint-every", "2000000",
        ]
        if record["opposing"]:
            command.append("--opposing")
        print(f"generate {output.name}", flush=True)
        subprocess.run(command, cwd=ROOT, check=True)
        outputs = shards.split(output, plan.DEFAULT_SHARD_LIMIT)
        if any(path.stat().st_size >= plan.GITHUB_FILE_LIMIT for path in outputs):
            raise RuntimeError(f"{output.name} has an oversized regular-Git shard")
        actual = sum(path.stat().st_size for path in TABLEBASES.iterdir()
                     if path.is_file() and
                     (path.name.endswith(".uftb") or ".uftb.part" in path.name))
        if actual > args.budget:
            raise RuntimeError("actual tablebase bytes exceeded budget")
        print(f"complete {output.name} {logical_size(output) / 1024**2:.2f} MiB "
              f"in {len(outputs) - 1 or 1} data file(s)", flush=True)
    print("stateful K+K+2 batch complete", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (subprocess.CalledProcessError, RuntimeError, ValueError) as error:
        print(f"stateful tablebase batch error: {error}", file=sys.stderr)
        raise SystemExit(1)

