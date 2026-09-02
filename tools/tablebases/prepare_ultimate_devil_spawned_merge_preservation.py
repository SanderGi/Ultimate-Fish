#!/usr/bin/env python3
"""Prepare a verified Devil-root merge for standard concrete preservation.

This never solves or modifies a tablebase.  It authenticates the twelve root
fragments and merged UFTB, writes the standard retained-result manifest, and
builds/restores the deterministic archive consumed by
``preserve_ultimate_concrete_existing_aws.py``.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil

import finalize_ultimate_aws_concrete_class as finalizer
import merge_ultimate_devil_spawned_roots as merger
import run_double_jester_information_capture as preservation
import run_ultimate_concrete_tablebase_shard_aws as concrete


ROOT = Path(__file__).resolve().parents[2]
FILENAME = "kdevilk.uftb"
ARCHIVE_SCHEMA = "ultimate-concrete-k2-result-v2"
RESULT_SCHEMA = "ultimate-concrete-k2-result-v2"


def sha256_json(value: object) -> str:
    payload = json.dumps(value, sort_keys=True,
                         separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def prepare(output: Path, receipt_path: Path, fragments_directory: Path,
            work: Path) -> dict[str, object]:
    preservation.require_empty_work_directory(work)
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    if receipt.get("schema") != "ultimate-devil-spawned-root-merge-v1":
        raise RuntimeError("unexpected Devil merge receipt schema")
    merged = receipt.get("output")
    if not isinstance(merged, dict):
        raise RuntimeError("Devil merge receipt lacks output")
    if (output.stat().st_size != int(merged.get("bytes", -1)) or
            preservation.sha256_path(output) != merged.get("sha256")):
        raise RuntimeError("Devil merge output extent/full-SHA residual")

    fragment_paths = sorted(fragments_directory.glob("devil-*.roots"))
    if len(fragment_paths) != len(merger.EXPECTED_SQUARES):
        raise RuntimeError("exactly twelve retained Devil fragments required")
    authenticated = {
        int(item["square"]): (str(item["sha256"]), int(item["roots"]))
        for item in receipt.get("fragments", [])
    }
    if set(authenticated) != set(merger.EXPECTED_SQUARES):
        raise RuntimeError("Devil receipt fixed-square coverage residual")
    for path in fragment_paths:
        summary, _ = merger.read_fragment(path)
        square = int(summary["square"])
        if (str(summary["sha256"]), int(summary["roots"])) != authenticated[square]:
            raise RuntimeError(f"Devil fragment/receipt residual: {path}")

    output_target = work / "outputs" / FILENAME
    output_target.parent.mkdir(parents=True)
    shutil.copy2(output, output_target)
    proof = work / "logs" / "devil-spawned-root-merge.log"
    proof.parent.mkdir(parents=True)
    proof.write_text(
        f"verifyok states {merged['states']}\n"
        f"output outputs/{FILENAME} edges {merged['edges']} "
        f"win {merged['packed_win']} loss {merged['packed_loss']} "
        f"draw {merged['packed_draw']}\n"
        f"complete states {merged['states']}/{merged['states']} elapsed 0s\n",
        encoding="utf-8")
    record = dict(finalizer.record_for(FILENAME))
    verification = concrete.parse_uftb(output_target, record, proof)
    if verification["sha256"] != merged["sha256"]:
        raise RuntimeError("Devil merge parser full-SHA residual")

    model = sha256_json({
        "schema": receipt["schema"],
        "semantics": receipt.get("semantics"),
        "merge_source_sha256": preservation.sha256_path(
            ROOT / "tools/tablebases/merge_ultimate_devil_spawned_roots.py"),
        "fragments": receipt["fragments"],
    })
    inventory = sha256_json({
        "filename": FILENAME,
        "record": record,
        "fixed_squares": sorted(merger.EXPECTED_SQUARES),
    })
    result = {
        "schema": RESULT_SCHEMA,
        "record": record,
        "output": verification,
        "generator_model_sha256": model,
        "inventory_sha256": inventory,
        "worker_threads": 1,
        "merge_receipt_sha256": preservation.sha256_path(receipt_path),
        "bellman_verification_residual": 0,
        "fixed_square_residual": 0,
        "duplicate_root_residual": 0,
        "never_delete": True,
    }
    result_path = work / "results" / "kdevilk.json"
    preservation.write_json(result_path, result)
    run_plan = {
        "generator_model_sha256": model,
        "inventory_sha256": inventory,
        "worker_threads": 1,
        "selection": {"wave": "devil-spawned-root-merge-v1",
                      "begin": 0, "end": 1, "classes": 1},
    }
    preservation.write_json(work / "run-plan.json", run_plan)

    files = {
        f"tablebases/{FILENAME}": output_target,
        f"proof/{proof.name}": proof,
        "proof/devil-root-merge-receipt.json": receipt_path,
        "sources/tools/tablebases/merge_ultimate_devil_spawned_roots.py":
            ROOT / "tools/tablebases/merge_ultimate_devil_spawned_roots.py",
    }
    for path in fragment_paths:
        files[f"proof/fragments/{path.name}"] = path
    archive, archive_sha = preservation.content_address_archive(
        work / "archives", "kdevilk", files, ARCHIVE_SCHEMA)
    restored = preservation.restore_zstd_archive(
        archive, work / "restore-local" / "kdevilk", ARCHIVE_SCHEMA)
    restored_verification = concrete.parse_uftb(
        restored[f"tablebases/{FILENAME}"], record,
        restored[f"proof/{proof.name}"])
    if restored_verification["sha256"] != verification["sha256"]:
        raise RuntimeError("restored Devil merge full-SHA residual")
    return {
        "schema": "ultimate-devil-spawned-merge-preservation-preparation-v1",
        "status": "merged-result-verified-archived-restored",
        "filename": FILENAME,
        "output": verification,
        "archive": {"path": str(archive), "bytes": archive.stat().st_size,
                    "sha256": archive_sha},
        "generator_model_sha256": model,
        "inventory_sha256": inventory,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--receipt", required=True, type=Path)
    parser.add_argument("--fragments-directory", required=True, type=Path)
    parser.add_argument("--work-directory", required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(prepare(args.output.resolve(), args.receipt.resolve(),
                             args.fragments_directory.resolve(),
                             args.work_directory.resolve()),
                     indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
