#!/usr/bin/env python3
"""Resumable, size-capped batch generation for stateless K+K+2 tables."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import subprocess
import sys

import plan_ultimate_tablebases as plan


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "src" / "ultimate_tablebase"
TABLEBASES = ROOT / "tablebases"


def generate(record: dict[str, object], checkpoint_dir: Path) -> Path:
    output = TABLEBASES / str(record["filename"])
    if output.exists():
        print(f"skip {output.name}", flush=True)
        return output
    checkpoint = checkpoint_dir / (output.stem + "-fast-v2.checkpoint")
    command = [
        str(GENERATOR), "--piece", str(record["primary"]),
        "--piece2", str(record["secondary"]),
        "--output", str(output), "--checkpoint", str(checkpoint),
        "--checkpoint-every", "2000000",
    ]
    if record["opposing"]:
        command.append("--opposing")
    print(f"generate {output.name}", flush=True)
    subprocess.run(command, cwd=ROOT, check=True)
    size = output.stat().st_size
    if size >= plan.GITHUB_FILE_LIMIT:
        raise RuntimeError(f"{output.name} exceeds GitHub's regular file limit")
    print(f"complete {output.name} {size / 1024**2:.2f} MiB", flush=True)
    return output


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--workers", type=int, default=1, choices=(1, 2))
    parser.add_argument("--max-classes", type=int)
    parser.add_argument("--budget", type=int, default=plan.DEFAULT_BUDGET)
    parser.add_argument("--checkpoint-dir", type=Path,
                        default=Path("/tmp/ultimatefish-k2-checkpoints"))
    args = parser.parse_args()
    subprocess.run(["make", "-C", str(ROOT / "src"), "ultimate-tablebase"], check=True)
    args.checkpoint_dir.mkdir(parents=True, exist_ok=True)
    records = [record for record in plan.inventory()
               if record["phase"] == "kings+2-stateless"]
    if args.max_classes is not None:
        records = records[:args.max_classes]

    existing = sum(path.stat().st_size for path in TABLEBASES.glob("*.uftb"))
    reserved = sum(int(record["packed_bytes"]) for record in records
                   if not (TABLEBASES / str(record["filename"])).exists())
    if existing + reserved > args.budget:
        raise RuntimeError(
            f"planned total {(existing + reserved) / 1024**3:.3f} GiB exceeds "
            f"budget {args.budget / 1024**3:.3f} GiB")

    if args.workers == 1:
        for record in records:
            generate(record, args.checkpoint_dir)
            actual = sum(path.stat().st_size for path in TABLEBASES.glob("*.uftb"))
            if actual > args.budget:
                raise RuntimeError("actual tablebase bytes exceeded budget")
    else:
        # Submit only one bounded pair at a time so a failed verifier cannot
        # leave the rest of a 160-class batch queued behind it.
        with ThreadPoolExecutor(max_workers=2) as pool:
            for begin in range(0, len(records), 2):
                futures = [pool.submit(generate, record, args.checkpoint_dir)
                           for record in records[begin:begin + 2]]
                for future in as_completed(futures):
                    future.result()
                actual = sum(path.stat().st_size for path in TABLEBASES.glob("*.uftb"))
                if actual > args.budget:
                    raise RuntimeError("actual tablebase bytes exceeded budget")
    print("stateless K+K+2 batch complete", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (subprocess.CalledProcessError, RuntimeError) as error:
        print(f"tablebase batch error: {error}", file=sys.stderr)
        raise SystemExit(1)
