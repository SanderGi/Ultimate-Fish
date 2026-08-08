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
    parser.add_argument("--only", action="append", default=[],
                        help="generate only this logical .uftb filename (repeatable)")
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
    if args.only:
        requested = set(args.only)
        known = {str(record["filename"]) for record in selected}
        unknown = sorted(requested - known)
        if unknown:
            raise RuntimeError(f"unknown stateful tablebase class: {', '.join(unknown)}")
        selected = [record for record in selected
                    if str(record["filename"]) in requested]
    if args.max_classes is not None:
        selected = selected[:args.max_classes]

    subprocess.run(["make", "-C", str(ROOT / "src"), "ultimate-tablebase"], check=True)
    args.checkpoint_dir.mkdir(parents=True, exist_ok=True)
    for record in selected:
        output = TABLEBASES / str(record["filename"])
        if output.exists():
            print(f"skip {output.name}", flush=True)
            continue
        checkpoint = args.checkpoint_dir / (output.stem + "-state-v3.checkpoint")
        # A checkpoint stores the full Node and predecessor-count arrays. For
        # the largest stateful graphs, even one atomic replacement would need
        # more temporary disk than a typical clone has available. Those runs
        # trade resumability for bounded disk use; smaller classes retain
        # periodic checkpoints and remove them after verified installation.
        checkpoint_every = "0" if int(record["states"]) >= 300_000_000 else "2000000"
        command = [
            str(GENERATOR), "--piece", str(record["primary"]),
            "--piece2", str(record["secondary"]), "--output", str(output),
            "--checkpoint", str(checkpoint), "--checkpoint-every", checkpoint_every,
        ]
        if record["opposing"]:
            command.append("--opposing")
        if int(record["states"]) >= 300_000_000:
            command.append("--disk-backed")
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
        checkpoint.unlink(missing_ok=True)
        print(f"complete {output.name} {logical_size(output) / 1024**2:.2f} MiB "
              f"in {len(outputs) - 1 or 1} data file(s)", flush=True)
    print("stateful K+K+2 batch complete", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (subprocess.CalledProcessError, RuntimeError, ValueError) as error:
        print(f"stateful tablebase batch error: {error}", file=sys.stderr)
        raise SystemExit(1)
