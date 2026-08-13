#!/usr/bin/env python3
"""Bind one stopped concrete frontier for local or cross-host resume.

Only the immutable node and out-degree planes are checkpoint inputs.  Partial
offset/predecessor files are recorded as phase evidence but are deliberately
excluded from transport because the resumed generator rebuilds them.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tablebases"))
import resume_ultimate_concrete_frontier_aws as resume  # noqa: E402
import run_ultimate_concrete_tablebase_shard_aws as concrete  # noqa: E402
import plan_ultimate_tablebases as plan  # noqa: E402


def file_record(source: Path, relative: str) -> dict[str, Any]:
    path = source / relative
    if not path.is_file():
        raise RuntimeError(f"missing retained file: {path}")
    return {
        "relative_path": relative,
        "bytes": path.stat().st_size,
        "sha256": resume.sha256_path(path),
    }


def plane_record(source: Path, stem: str, name: str,
                 *, digest: bool) -> dict[str, Any]:
    relative = f"scratch/{stem}.{name}"
    path = source / relative
    if not path.is_file():
        raise RuntimeError(f"missing retained plane: {path}")
    result: dict[str, Any] = {
        "relative_path": relative,
        "bytes": path.stat().st_size,
        "allocated_bytes": path.stat().st_blocks * 512,
    }
    if digest:
        result["sha256"] = resume.sha256_path(path)
    return result


def build(source: Path, wave: int, index: int,
          transport_source: Path | None = None) -> dict[str, Any]:
    rows = concrete.wave_inventory(wave)
    if index < 0 or index >= len(rows):
        raise RuntimeError("inventory index is out of range")
    record = concrete.normalized_record(rows[index])
    stem = Path(str(record["filename"])).stem
    run_plan_path = source / "run-plan.json"
    run_plan = json.loads(run_plan_path.read_text(encoding="utf-8"))
    if run_plan.get("selected") != [record] or run_plan.get("status") != \
            "full-preflight":
        raise RuntimeError("retained run plan does not bind the requested class")

    log_relative = f"logs/generate/{stem}.log"
    resource_relative = f"logs/generate/{stem}.resources.json"
    log_text = (source / log_relative).read_text(encoding="utf-8")
    if re.search(r"^(propagate queue|verifyok states|complete states)",
                 log_text, re.MULTILINE):
        raise RuntimeError("retrograde propagation already began")
    frontier = re.findall(r"^frontier states [^\n]+", log_text, re.MULTILINE)
    reverse = re.findall(r"^reverse states [^\n]+", log_text, re.MULTILINE)
    if not frontier or not reverse:
        raise RuntimeError("retained log lacks complete-frontier/reverse evidence")
    resource = json.loads((source / resource_relative).read_text())
    if (resource.get("returncode") != -15 or
            not resource.get("scratch_retained") or
            not resource.get("violation")):
        raise RuntimeError("retained resource certificate is not resumable")

    pieces = {piece.name: piece for piece in plan.PIECES}
    primary = pieces[record["primary"]]
    secondary = pieces[record["secondary"]]
    substates = plan.pair_state_factor(primary, secondary)
    document: dict[str, Any] = {
        "schema": resume.SCHEMA,
        "status": "resource-gated-frontier-transportable",
        "source_work_directory": str(source.resolve()),
        "generator_model_sha256": run_plan["generator_model_sha256"],
        "inventory_sha256": run_plan["inventory_sha256"],
        "binary_sha256": run_plan["binary_sha256"],
        "dependency_manifest_sha256": run_plan[
            "dependency_manifest_sha256"],
        "states": record["states"],
        "substates": substates,
        "last_frontier_marker": frontier[-1],
        "last_reverse_marker": reverse[-1],
        "resource_violation": resource["violation"],
        "selection": {"wave": wave, "begin": index, "end": index + 1,
                      "classes": 1},
        "record": record,
        "run_plan": file_record(source, "run-plan.json"),
        "generator_log": file_record(source, log_relative),
        "resource_certificate": file_record(source, resource_relative),
        "binary": file_record(source, "binary/ultimate_tablebase"),
        "dependency_manifest": file_record(
            source, "dependencies/manifest.json"),
        "planes": {
            name: plane_record(source, stem, name, digest=True)
            for name in ("nodes", "degrees")
        },
        "discarded_reverse_graph": {
            name: plane_record(source, stem, name, digest=False)
            for name in ("offsets", "predecessors")
        },
    }
    resume.authenticate_manifest(document)
    if transport_source is not None:
        document["source_work_directory"] = str(transport_source.resolve())
    return document


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--wave", type=int, required=True)
    parser.add_argument("--index", type=int, required=True)
    parser.add_argument("--transport-source", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    document = build(args.source.resolve(), args.wave, args.index,
                     args.transport_source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, indent=2) + "\n",
                           encoding="utf-8")
    print(json.dumps({
        "status": "authenticated-transportable-frontier",
        "output": str(args.output),
        "manifest_sha256": resume.sha256_path(args.output),
        "filename": document["record"]["filename"],
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
