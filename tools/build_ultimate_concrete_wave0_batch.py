#!/usr/bin/env python3
"""Build a fail-closed, source-pinned batch plan for concrete wave 0.

This tool is intentionally a planner only.  It does not contact AWS, upload
anything, install a unit, or start a service.  The generated manifest contains
one explicit unit/work directory per selected class so a later supervisor can
authenticate each unit independently.

Selection is deterministic: ledger entries must be ``planned`` and belong to
the production runner's filename-sorted wave-0 inventory; the ten smallest
packed classes are selected (ties are resolved by filename and inventory
index).  Dependencies are obtained from the runner's exact per-class
``class_dependency_filenames`` function.  A missing or non-certified
dependency is a launch blocker, never a reason to silently broaden the
dependency set.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Mapping, Sequence


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
TEMPLATE = TOOLS / "ultimatefish-concrete-wave0-batch.service.template"
MANIFEST = TOOLS / "ultimate_concrete_wave0_batch.json"
sys.path.insert(0, str(TOOLS))

import run_ultimate_concrete_tablebase_shard_aws as runner  # noqa: E402
import update_ultimate_tablebase_ledger as ledger  # noqa: E402


SCHEMA = "ultimate-concrete-wave0-batch-plan-v1"
SELECTION_POLICY = "planned-smallest-packed-bytes-filename-index-v1"
CERTIFIED_DEPENDENCY_STATUSES = frozenset({"certified", "preserving"})
MAX_CLASSES = 30
DEFAULT_CLASSES = 10
GIB = 1 << 30
MINIMUM_FREE_BYTES = 320 * GIB
UNIT_PREFIX = "ultimatefish-concrete-wave0-batch"
WORK_ROOT = "/mnt/ultimatefish/concrete-wave0-batch"
SOURCE_ROOT = "/mnt/ultimatefish/concrete-wave0-batch/source/ultimatefish"
DEPENDENCY_ROOT = "/mnt/ultimatefish/concrete-wave0-batch/dependencies"
S3_PREFIX = (
    "s3://ultimatefish-info-20260808-a4e679c6-831688117652/"
    "results/concrete/penguin-causal-1083b6f8"
)


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_commit() -> str:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, check=True,
            capture_output=True, text=True)
    except (OSError, subprocess.CalledProcessError) as exc:
        raise RuntimeError("cannot resolve canonical git commit") from exc
    commit = result.stdout.strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise RuntimeError("canonical git commit is not a full SHA-1")
    return commit


def require_committed_sources() -> str:
    """Require every manifest-generating input to be present and clean."""
    paths = tuple(runner.MODEL_SOURCES) + (
        Path(__file__).relative_to(ROOT).as_posix(),
        TEMPLATE.relative_to(ROOT).as_posix(),
    )
    try:
        tracked = subprocess.run(
            ["git", "ls-files", "--error-unmatch", "--", *paths], cwd=ROOT,
            check=False, capture_output=True, text=True)
        result = subprocess.run(
            ["git", "diff", "--quiet", "HEAD", "--", *paths], cwd=ROOT,
            check=False)
    except OSError as exc:
        raise RuntimeError("cannot inspect canonical source cleanliness") from exc
    if tracked.returncode or result.returncode:
        raise RuntimeError("production model/planner sources are not committed")
    return canonical_commit()


def committed_ledger_text() -> str:
    try:
        result = subprocess.run(
            ["git", "show", f"HEAD:{Path('tablebases/README.md')}"], cwd=ROOT,
            check=True, capture_output=True)
    except (OSError, subprocess.CalledProcessError) as exc:
        raise RuntimeError("cannot read committed tablebase ledger") from exc
    return result.stdout.decode("utf-8")


def ledger_by_filename() -> dict[str, ledger.Entry]:
    entries = ledger.entries(committed_ledger_text())
    return {entry.filename: entry for entry in entries if entry.filename}


def source_hashes() -> dict[str, str]:
    paths = tuple(runner.MODEL_SOURCES) + (Path(__file__).relative_to(ROOT).as_posix(),)
    if TEMPLATE.exists():
        paths += (TEMPLATE.relative_to(ROOT).as_posix(),)
    result: dict[str, str] = {}
    for relative in paths:
        path = ROOT / relative
        if not path.is_file():
            raise RuntimeError(f"missing source/template path: {relative}")
        result[relative] = sha256_path(path)
    return dict(sorted(result.items()))


def _safe_atom(value: str, *, label: str) -> None:
    if not re.fullmatch(r"[a-z0-9._-]+", value):
        raise RuntimeError(f"unsafe {label}: {value!r}")


def resource_policy(record: Mapping[str, object]) -> dict[str, int]:
    states = int(record["states"])
    packed = int(record["packed_bytes"])
    # Keep the estimate coupled to the production runner instead of silently
    # reimplementing its state-plane accounting here.
    index = int(record["_batch_inventory_index"])
    selected, measurement = runner.selection_plan(0, index, index + 1)
    if len(selected) != 1 or selected[0]["filename"] != record["filename"]:
        raise RuntimeError("runner selection/resource measurement residual")
    static_floor = int(measurement["static_scratch_floor_bytes"])
    resident_floor = int(measurement["resident_floor_bytes"])
    reverse_limit = max(16 * GIB, static_floor)
    scratch_limit = max(20 * GIB, static_floor + reverse_limit)
    resident_limit = max(16 * GIB, 2 * resident_floor)
    return {
        "cpu_count": 1,
        "cpu_quota_percent": 100,
        "states": states,
        "packed_bytes": packed,
        "static_scratch_floor_bytes": static_floor,
        "resident_floor_bytes": resident_floor,
        "scratch_limit_bytes": scratch_limit,
        "resident_limit_bytes": resident_limit,
        "reverse_edge_bytes_limit": reverse_limit,
        "minimum_free_bytes": MINIMUM_FREE_BYTES,
    }


def _dependency_records(
    record: Mapping[str, object], by_filename: Mapping[str, ledger.Entry]
) -> tuple[list[str], list[dict[str, object]]]:
    resolver = getattr(runner, "class_dependency_filenames", None)
    if not callable(resolver):
        raise RuntimeError("runner lacks exact class_dependency_filenames API")
    names = tuple(str(name) for name in resolver(record))
    if tuple(sorted(set(names))) != names:
        raise RuntimeError("class dependency resolver returned duplicate/unsorted names")
    records: list[dict[str, object]] = []
    for name in names:
        if Path(name).name != name or not re.fullmatch(r"k[a-z]+k(?:[a-z]+)?\.uftb", name):
            raise RuntimeError(f"malformed class dependency filename: {name!r}")
        entry = by_filename.get(name)
        if entry is None:
            raise RuntimeError(f"class dependency absent from committed ledger: {name}")
        records.append({
            "filename": name,
            "ledger_status": entry.status,
            "states": entry.states,
            "storage": entry.storage,
            "sha256": entry.digest,
            "authenticated": (
                entry.status in CERTIFIED_DEPENDENCY_STATUSES and
                bool(re.fullmatch(r"[0-9a-f]{64}", entry.digest))),
        })
    return list(names), records


def _selected_rows(count: int) -> list[tuple[int, dict[str, object]]]:
    if not 1 <= count <= MAX_CLASSES:
        raise RuntimeError(f"class count must be in [1, {MAX_CLASSES}]")
    rows = runner.wave_inventory(0)
    by_filename = ledger_by_filename()
    eligible: list[tuple[int, dict[str, object]]] = []
    excluded_statuses = {
        "certified", "computing", "blocked", "preserving", "deferred", "draw",
    }
    for index, row in enumerate(rows):
        filename = str(row["filename"])
        entry = by_filename.get(filename)
        if entry is None or entry.status != "planned":
            continue
        if entry.status in excluded_statuses:
            continue
        if row.get("primary") in runner.DEFERRED_DYNAMIC or row.get("secondary") in runner.DEFERRED_DYNAMIC:
            raise RuntimeError(f"deferred dynamic class reached wave-0 inventory: {filename}")
        if row.get("mirror_simplification") and row.get("secondary") in {"penguin", "mage", "fisherman"}:
            raise RuntimeError(f"Copycat separator class reached inventory: {filename}")
        eligible.append((index, row))
    if len(eligible) < count:
        raise RuntimeError(f"only {len(eligible)} planned wave-0 classes are eligible")
    eligible.sort(key=lambda item: (int(item[1]["packed_bytes"]), str(item[1]["filename"]), item[0]))
    return eligible[:count]


def render_service(substitutions: Mapping[str, str]) -> str:
    template = TEMPLATE.read_text()
    rendered = template
    for key, value in substitutions.items():
        rendered = rendered.replace(f"@{key}@", value)
    if "@" in rendered:
        raise RuntimeError("service template has unresolved placeholders")
    return rendered


def build_document(count: int = DEFAULT_CLASSES) -> dict[str, object]:
    commit = require_committed_sources()
    by_filename = ledger_by_filename()
    selected = _selected_rows(count)
    model = runner.generator_model_sha256()
    inventory = runner.inventory_sha256()
    template_sha = sha256_path(TEMPLATE)
    source = source_hashes()
    units: list[dict[str, object]] = []
    seen_units: set[str] = set()
    seen_work: set[str] = set()
    all_dependencies: set[str] = set()
    launch_blockers: list[dict[str, object]] = []
    for ordinal, (index, row) in enumerate(selected, start=1):
        filename = str(row["filename"])
        stem = Path(filename).stem
        unit_name = f"{UNIT_PREFIX}-{ordinal:02d}-class{index:03d}-{stem}.service"
        work = f"{WORK_ROOT}/batch-{ordinal:02d}-class{index:03d}-{stem}"
        _safe_atom(unit_name.removesuffix(".service"), label="unit")
        _safe_atom(stem, label="class stem")
        if unit_name in seen_units or work in seen_work:
            raise RuntimeError("batch unit/work directory collision")
        seen_units.add(unit_name)
        seen_work.add(work)
        dependencies, dependency_records = _dependency_records(row, by_filename)
        all_dependencies.update(dependencies)
        unauthenticated = [record for record in dependency_records if not record["authenticated"]]
        if unauthenticated:
            launch_blockers.append({
                "unit": unit_name,
                "reason": "dependency-ledger-status-not-certified-or-preserving",
                "dependencies": [record["filename"] for record in unauthenticated],
            })
        resource_row = dict(row)
        resource_row["_batch_inventory_index"] = index
        resources = resource_policy(resource_row)
        substitutions = {
            "UNIT": unit_name,
            "CLASS_INDEX": str(index),
            "CLASS_INDEX_END": str(index + 1),
            "CLASS_FILENAME": filename,
            "CLASS_STEM": stem,
            "PRIMARY": str(row["primary"]),
            "SECONDARY": str(row["secondary"]),
            "OPPOSING_FLAG": "--opposing" if row["opposing"] else "",
            "MODEL_SHA256": model,
            "INVENTORY_SHA256": inventory,
            "CANONICAL_COMMIT": commit,
            "SOURCE_ROOT": SOURCE_ROOT,
            "WORK_DIRECTORY": work,
            "DEPENDENCY_ROOT": DEPENDENCY_ROOT,
            "DEPENDENCY_MANIFEST": f"{DEPENDENCY_ROOT}/manifest.json",
            "S3_PREFIX": S3_PREFIX,
            "SCRATCH_LIMIT": str(resources["scratch_limit_bytes"]),
            "RESIDENT_LIMIT": str(resources["resident_limit_bytes"]),
            "REVERSE_EDGE_LIMIT": str(resources["reverse_edge_bytes_limit"]),
            "MINIMUM_FREE": str(resources["minimum_free_bytes"]),
        }
        service = render_service(substitutions)
        units.append({
            "ordinal": ordinal,
            "wave": 0,
            "inventory_index": index,
            "filename": filename,
            "record": runner.normalized_record(row),
            "unit": unit_name,
            "work_directory": work,
            "source_root": SOURCE_ROOT,
            "dependency_root": DEPENDENCY_ROOT,
            "dependencies": dependencies,
            "dependency_records": dependency_records,
            "resource_requirements": resources,
            "service_template": str(TEMPLATE.relative_to(ROOT)),
            "service_template_sha256": template_sha,
            "service_sha256": sha256_bytes(service.encode()),
            "service_text": service,
            "source_hashes": source,
        })
    document: dict[str, object] = {
        "schema": SCHEMA,
        "status": "plan-only-not-uploaded-not-installed-not-launched",
        "canonical_commit": commit,
        "selection_policy": SELECTION_POLICY,
        "requested_classes": count,
        "max_classes": MAX_CLASSES,
        "wave": 0,
        "inventory_sha256": inventory,
        "generator_model_sha256": model,
        "source_hashes": source,
        "service_template": str(TEMPLATE.relative_to(ROOT)),
        "service_template_sha256": template_sha,
        "s3_prefix": S3_PREFIX,
        "required_dependency_statuses": sorted(CERTIFIED_DEPENDENCY_STATUSES),
        "all_dependencies": sorted(all_dependencies),
        "all_dependency_count": len(all_dependencies),
        "launch_ready": not launch_blockers,
        "launch_blockers": launch_blockers,
        "units": units,
        "no_remote_side_effects": True,
        "never_delete": True,
    }
    encoded = json.dumps(document, sort_keys=True, separators=(",", ":")).encode()
    document["manifest_sha256"] = sha256_bytes(encoded)
    return document


def write_manifest(document: Mapping[str, object], path: Path = MANIFEST) -> None:
    path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--classes", type=int, default=DEFAULT_CLASSES)
    parser.add_argument("--output", type=Path, default=MANIFEST)
    parser.add_argument("--write", action="store_true")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    document = build_document(args.classes)
    if args.write:
        write_manifest(document, args.output)
    else:
        print(json.dumps(document, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
