#!/usr/bin/env python3
"""Build a fail-closed, source-pinned batch plan for concrete wave 0.

This tool is intentionally a planner only.  It does not contact AWS, upload
anything, install a unit, or start a service.  The generated manifest contains
one explicit unit/work directory per selected class so a later supervisor can
authenticate each unit independently.

Selection is deterministic: ledger entries must be ``planned`` and belong to
the production runner's filename-sorted wave-0 inventory; the requested smallest
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
JOB_FRAGMENT = TOOLS / "ultimate_concrete_wave0_batch_jobs.json"
SUPERVISION_CONFIG = TOOLS / "ultimate_aws_supervision.json"
sys.path.insert(0, str(TOOLS))

import run_ultimate_concrete_tablebase_shard_aws as runner  # noqa: E402
import supervise_ultimate_aws as supervisor  # noqa: E402
import update_ultimate_tablebase_ledger as ledger  # noqa: E402


SCHEMA = "ultimate-concrete-wave0-batch-plan-v1"
SELECTION_POLICY = "planned-smallest-packed-bytes-filename-index-v1"
CERTIFIED_DEPENDENCY_STATUSES = frozenset({"certified", "preserving"})
MAX_CLASSES = 30
DEFAULT_CLASSES = 24
GIB = 1 << 30
UNIT_PREFIX = "ultimatefish-concrete-wave0-batch"
SOURCE_ROOT = "/mnt/ultimatefish/concrete-wave0-batch/source/ultimatefish"
DEPENDENCY_ROOT = "/mnt/ultimatefish/concrete-wave0-batch/dependencies"
S3_PREFIX = (
    "s3://ultimatefish-info-20260808-a4e679c6-831688117652/"
    "results/concrete/penguin-causal-1083b6f8"
)

# These are the five configured hosts.  Their names, mounts, vCPU counts, and
# disk gates are authenticated against the committed supervision document;
# capacity class and CPU pools are the deterministic scheduler policy for this
# batch (not an instruction to launch every unit concurrently).
HOST_SPECS = {
    "i-03c81f90d2c59a2e7": {
        "name": "hidden-primary", "capacity_class": "r8gd",
        "memory_capacity_bytes": 256 * GIB, "cpu_pool": tuple(range(8, 16)),
        "assignment_count": 8, "mount_rotation": ("/mnt/ultimatefish",),
    },
    "i-0b4523116b2f7765c": {
        "name": "concrete-primary", "capacity_class": "r8gd",
        "memory_capacity_bytes": 256 * GIB, "cpu_pool": tuple(range(8, 15)),
        "assignment_count": 7,
        "mount_rotation": (
            "/mnt/ultimatefish-resume", "/mnt/ultimatefish",
            "/mnt/ultimatefish-overflow",
        ),
    },
    "i-0986ed3d272721f02": {
        "name": "giant-ghost-same", "capacity_class": "c8gd",
        "memory_capacity_bytes": 64 * GIB, "cpu_pool": (4, 5, 6),
        "assignment_count": 3, "mount_rotation": ("/mnt/ultimatefish",),
    },
    "i-024a2073283e4336e": {
        "name": "giant-ghost-opposing", "capacity_class": "c8gd",
        "memory_capacity_bytes": 64 * GIB, "cpu_pool": (4, 5, 6),
        "assignment_count": 3, "mount_rotation": ("/mnt/ultimatefish",),
    },
    "i-08c0f44a1776cb34a": {
        "name": "crossed-and-jester-ghost", "capacity_class": "c8gd",
        "memory_capacity_bytes": 64 * GIB, "cpu_pool": (4, 5, 6),
        "assignment_count": 3, "mount_rotation": ("/mnt/ultimatefish",),
    },
}
HOST_ASSIGNMENT_ORDER = (
    "i-03c81f90d2c59a2e7", "i-0b4523116b2f7765c",
    "i-0986ed3d272721f02", "i-024a2073283e4336e",
    "i-08c0f44a1776cb34a",
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


def serialized_manifest(document: Mapping[str, object]) -> bytes:
    return (json.dumps(document, indent=2, sort_keys=True) + "\n").encode()


def require_committed_sources() -> str:
    """Require every manifest-generating input to be present and clean."""
    paths = tuple(runner.MODEL_SOURCES) + (
        Path(__file__).relative_to(ROOT).as_posix(),
        TEMPLATE.relative_to(ROOT).as_posix(),
        SUPERVISION_CONFIG.relative_to(ROOT).as_posix(),
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


def committed_supervision_document() -> dict[str, object]:
    try:
        result = subprocess.run(
            ["git", "show", f"HEAD:{SUPERVISION_CONFIG.relative_to(ROOT)}"],
            cwd=ROOT, check=True, capture_output=True)
    except (OSError, subprocess.CalledProcessError) as exc:
        raise RuntimeError("cannot read committed AWS supervision config") from exc
    try:
        document = json.loads(result.stdout.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise RuntimeError("committed AWS supervision config is malformed") from exc
    if not isinstance(document, dict):
        raise RuntimeError("committed AWS supervision config must be an object")
    return document


def configured_hosts() -> dict[str, dict[str, object]]:
    """Authenticate the five host records used by deterministic assignment."""
    document = committed_supervision_document()
    configured = {
        str(item.get("instance_id")): item
        for item in document.get("instances", [])
        if isinstance(item, dict) and item.get("instance_id")
    }
    if set(configured) != set(HOST_SPECS):
        raise RuntimeError("configured host inventory does not match five-host batch")
    result: dict[str, dict[str, object]] = {}
    for instance_id, spec in HOST_SPECS.items():
        item = configured[instance_id]
        if item.get("name") != spec["name"] or int(item.get("vcpus", 0)) != 32:
            raise RuntimeError(f"configured host identity residual: {instance_id}")
        mounts = tuple(str(mount) for mount in item.get("mounts", ()))
        for mount in spec["mount_rotation"]:
            if mount not in mounts:
                raise RuntimeError(
                    f"configured host lacks required mount {mount}: {instance_id}")
        minimum_disk = item.get("minimum_disk_free_bytes")
        if not isinstance(minimum_disk, dict):
            raise RuntimeError(f"configured host disk gates missing: {instance_id}")
        if len(spec["cpu_pool"]) < int(spec["assignment_count"]):
            raise RuntimeError(f"host CPU pool cannot assign unique units: {instance_id}")
        result[instance_id] = {
            **spec,
            "configured_name": str(item["name"]),
            "vcpus": int(item["vcpus"]),
            "mounts": mounts,
            "minimum_memory_available_bytes": int(
                item.get("minimum_memory_available_bytes", 0)),
            "minimum_disk_free_bytes": {
                str(mount): int(value) for mount, value in minimum_disk.items()
            },
        }
    return result


def ledger_by_filename() -> dict[str, ledger.Entry]:
    entries = ledger.entries(committed_ledger_text())
    return {entry.filename: entry for entry in entries if entry.filename}


def source_hashes() -> dict[str, str]:
    paths = tuple(runner.MODEL_SOURCES) + (Path(__file__).relative_to(ROOT).as_posix(),)
    if TEMPLATE.exists():
        paths += (TEMPLATE.relative_to(ROOT).as_posix(),)
    paths += (SUPERVISION_CONFIG.relative_to(ROOT).as_posix(),)
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


def resource_policy(record: Mapping[str, object], host: Mapping[str, object],
                    work_mount: str) -> dict[str, object]:
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
    minimum_disk = host.get("minimum_disk_free_bytes")
    if not isinstance(minimum_disk, Mapping) or work_mount not in minimum_disk:
        raise RuntimeError("assigned work mount lacks a committed disk gate")
    disk_peak = scratch_limit + 5 * packed
    return {
        "cpu_count": 1,
        "cpu_quota_percent": 100,
        "cpu_threads": 1,
        "memory_peak_bytes": resident_limit,
        "disk_peak_bytes": {work_mount: disk_peak},
        "states": states,
        "packed_bytes": packed,
        "static_scratch_floor_bytes": static_floor,
        "resident_floor_bytes": resident_floor,
        "scratch_limit_bytes": scratch_limit,
        "resident_limit_bytes": resident_limit,
        "reverse_edge_bytes_limit": reverse_limit,
        "minimum_free_bytes": int(minimum_disk[work_mount]),
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
    inventory_by_filename = {
        str(row["filename"]): row
        for row in (*runner.plan.inventory(0), *runner.supported_inventory())
    }
    records: list[dict[str, object]] = []
    for name in names:
        if Path(name).name != name or not re.fullmatch(r"k[a-z]+k(?:[a-z]+)?\.uftb", name):
            raise RuntimeError(f"malformed class dependency filename: {name!r}")
        entry = by_filename.get(name)
        if entry is None:
            raise RuntimeError(f"class dependency absent from committed ledger: {name}")
        planner_record = inventory_by_filename.get(name)
        if planner_record is None:
            raise RuntimeError(f"class dependency absent from committed planner: {name}")
        records.append({
            "filename": name,
            "ledger_status": entry.status,
            "states": entry.states,
            "bytes": int(planner_record["packed_bytes"]),
            "storage": entry.storage,
            "sha256": entry.digest,
            "authenticated": (
                entry.status in CERTIFIED_DEPENDENCY_STATUSES and
                bool(re.fullmatch(r"[0-9a-f]{64}", entry.digest))),
        })
    return list(names), records


def dependency_manifest_payload(records: Sequence[Mapping[str, object]]) -> dict[str, object]:
    files = [{
        "filename": str(record["filename"]),
        "sha256": str(record["sha256"]),
        "bytes": int(record["bytes"]),
    } for record in records]
    files.sort(key=lambda record: record["filename"])
    return {"schema": runner.DEPENDENCY_SCHEMA, "files": files}


def dependency_manifest_sha256(records: Sequence[Mapping[str, object]]) -> str:
    payload = dependency_manifest_payload(records)
    encoded = (json.dumps(payload, indent=2, sort_keys=True) + "\n").encode()
    return sha256_bytes(encoded)


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


def assignment_slots(count: int, hosts: Mapping[str, Mapping[str, object]]) -> list[dict[str, object]]:
    """Return deterministic host/mount/CPU slots for the selected units."""
    if count != sum(int(HOST_SPECS[instance]["assignment_count"])
                   for instance in HOST_ASSIGNMENT_ORDER):
        raise RuntimeError("batch count must cover the configured assignment plan")
    slots: list[dict[str, object]] = []
    for instance_id in HOST_ASSIGNMENT_ORDER:
        host = hosts[instance_id]
        count_for_host = int(host["assignment_count"])
        cpu_pool = tuple(int(cpu) for cpu in host["cpu_pool"])
        mounts = tuple(str(mount) for mount in host["mount_rotation"])
        for local_index in range(count_for_host):
            if local_index >= len(cpu_pool):
                raise RuntimeError(f"host CPU assignment is not unique: {instance_id}")
            slots.append({
                "instance_id": instance_id,
                "host_name": str(host["name"]),
                "capacity_class": str(host["capacity_class"]),
                "memory_capacity_bytes": int(host["memory_capacity_bytes"]),
                "local_index": local_index,
                "expected_allowed_cpus": str(cpu_pool[local_index]),
                "work_mount": mounts[local_index % len(mounts)],
                "queue_priority": (
                    0 if host["capacity_class"] == "r8gd" else 1),
                "host_assignment_count": count_for_host,
            })
    if len(slots) != count:
        raise RuntimeError("host assignment cardinality residual")
    return slots


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
    hosts = configured_hosts()
    selected = _selected_rows(count)
    slots = assignment_slots(count, hosts)
    model = runner.generator_model_sha256()
    inventory = runner.inventory_sha256()
    template_sha = sha256_path(TEMPLATE)
    source = source_hashes()
    units: list[dict[str, object]] = []
    seen_units: set[str] = set()
    seen_work: set[str] = set()
    all_dependencies: set[str] = set()
    launch_blockers: list[dict[str, object]] = []
    for ordinal, ((index, row), slot) in enumerate(zip(selected, slots), start=1):
        filename = str(row["filename"])
        stem = Path(filename).stem
        unit_name = f"{UNIT_PREFIX}-{ordinal:02d}-class{index:03d}-{stem}.service"
        work_mount = str(slot["work_mount"])
        work = f"{work_mount}/concrete-wave0-batch/batch-{ordinal:02d}-class{index:03d}-{stem}"
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
        runner_limits = resource_policy(
            resource_row, hosts[str(slot["instance_id"])], work_mount)
        scheduler_resources = {
            "cpu_threads": int(runner_limits["cpu_threads"]),
            "memory_peak_bytes": int(runner_limits["memory_peak_bytes"]),
            "disk_peak_bytes": dict(runner_limits["disk_peak_bytes"]),
        }
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
            "HOST_NAME": str(slot["host_name"]),
            "INSTANCE_ID": str(slot["instance_id"]),
            "EXPECTED_ALLOWED_CPUS": str(slot["expected_allowed_cpus"]),
            "WORK_MOUNT": work_mount,
            "SCRATCH_LIMIT": str(runner_limits["scratch_limit_bytes"]),
            "RESIDENT_LIMIT": str(runner_limits["resident_limit_bytes"]),
            "REVERSE_EDGE_LIMIT": str(runner_limits["reverse_edge_bytes_limit"]),
            "MINIMUM_FREE": str(runner_limits["minimum_free_bytes"]),
        }
        service = render_service(substitutions)
        units.append({
            "ordinal": ordinal,
            "wave": 0,
            "inventory_index": index,
            "filename": filename,
            "ledger_result_kind": by_filename[filename].result_kind,
            "record": runner.normalized_record(row),
            "unit": unit_name,
            "instance_id": slot["instance_id"],
            "host_name": slot["host_name"],
            "capacity_class": slot["capacity_class"],
            "memory_capacity_bytes": slot["memory_capacity_bytes"],
            "expected_allowed_cpus": slot["expected_allowed_cpus"],
            "work_mount": work_mount,
            "queue_priority": slot["queue_priority"],
            "scheduler_state": "queued-supervisor-selects-safe-subset",
            "work_directory": work,
            "source_root": SOURCE_ROOT,
            "dependency_root": DEPENDENCY_ROOT,
            "dependencies": dependencies,
            "dependency_records": dependency_records,
            "resource_requirements": scheduler_resources,
            "runner_limits": runner_limits,
            "dependency_manifest_sha256": dependency_manifest_sha256(
                dependency_records),
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
        "supervision_fragment": str(JOB_FRAGMENT.relative_to(ROOT)),
        "supervision_fragment_schema": "ultimate-aws-supervision-job-fragment-v1",
        "s3_prefix": S3_PREFIX,
        "configured_hosts": [
            {
                "instance_id": instance_id,
                "name": host["name"],
                "capacity_class": host["capacity_class"],
                "memory_capacity_bytes": host["memory_capacity_bytes"],
                "vcpus": host["vcpus"],
                "mounts": host["mounts"],
                "cpu_pool": list(host["cpu_pool"]),
                "assignment_count": host["assignment_count"],
            }
            for instance_id in HOST_ASSIGNMENT_ORDER
            for host in (hosts[instance_id],)
        ],
        "scheduler_policy": {
            "state": "queued-supervisor-selects-measured-safe-subset",
            "simultaneous_launch_forbidden": True,
            "priority": "r8gd-before-c8gd",
            "assignment_order": list(HOST_ASSIGNMENT_ORDER),
        },
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


def build_supervision_jobs(document: Mapping[str, object]) -> dict[str, object]:
    """Render an exact, non-mutating supervisor config fragment.

    The fragment is separate from the batch manifest so the manifest's own
    SHA-256 can be bound as a source without creating a circular hash.  The
    caller may merge these records into the canonical supervision config only
    after source staging and remote preflight have authenticated every path.
    """
    jobs: list[dict[str, object]] = []
    raw_manifest_sha256 = sha256_bytes(serialized_manifest(document))
    for unit in document.get("units", []):
        if not isinstance(unit, Mapping):
            raise RuntimeError("batch unit record is malformed")
        runner_source = unit["source_root"]
        source_hashes_for_unit = unit["source_hashes"]
        if not isinstance(source_hashes_for_unit, Mapping):
            raise RuntimeError("batch unit source hashes are malformed")
        bindings = []
        for relative in runner.MODEL_SOURCES:
            digest = source_hashes_for_unit.get(relative)
            if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
                raise RuntimeError(f"missing committed source hash: {relative}")
            bindings.append({
                "path": f"{runner_source}/{relative}", "sha256": digest,
            })
        template_relative = str(TEMPLATE.relative_to(ROOT))
        bindings.append({
            "path": f"{runner_source}/{template_relative}",
            "sha256": str(unit["service_template_sha256"]),
        })
        bindings.append({
            "path": f"/etc/systemd/system/{unit['unit']}",
            "sha256": str(unit["service_sha256"]),
        })
        bindings.append({
            "path": f"{runner_source}/tools/ultimate_concrete_wave0_batch.json",
            "sha256": raw_manifest_sha256,
        })
        bindings.append({
            "path": f"{unit['dependency_root']}/manifest.json",
            "sha256": str(unit["dependency_manifest_sha256"]),
        })
        for dependency in unit["dependency_records"]:
            bindings.append({
                "path": f"{unit['dependency_root']}/{dependency['filename']}",
                "sha256": str(dependency["sha256"]),
            })
        bindings.sort(key=lambda binding: binding["path"])
        job_id = f"concrete-wave0-batch-{int(unit['ordinal']):02d}-class{int(unit['inventory_index']):03d}"
        certifies_public_result = unit["ledger_result_kind"] == "concrete"
        jobs.append({
            "id": job_id,
            "instance_id": unit["instance_id"],
            "unit": unit["unit"],
            "queue_stage": True,
            "expected_allowed_cpus": unit["expected_allowed_cpus"],
            "resource_requirements": unit["resource_requirements"],
            "dependencies": [],
            # A concrete lower table for an information-required class is a
            # dependency, not the public class result.  Keep that ledger row
            # PLANNED until its observation-game solver is certified.
            "ledger_files": ([unit["filename"]]
                             if certifies_public_result else []),
            "ledger_certifies": certifies_public_result,
            "checkpoint_paths": [
                f"{unit['work_directory']}/scratch/*",
                f"{unit['work_directory']}/logs/*",
            ],
            "completion_paths": [
                f"{unit['work_directory']}/certificates/wave-certificate.json",
                f"{unit['work_directory']}/s3-verify/wave-certificate.json",
            ],
            # Queue records must remain AWAITING_STAGE before the unit exists.
            # Publishing future hashes as active bindings would classify every
            # uninstalled unit as SOURCE_MISMATCH.  Staging promotes these
            # intended bindings to source_bindings in a focused commit only
            # after the host has rehashed every installed path.
            "source_bindings": [],
            "staging_source_bindings": bindings,
            "s3_certificates": [],
        })
    if len(jobs) != int(document["requested_classes"]):
        raise RuntimeError("supervision job fragment cardinality residual")
    job_ids = [str(job["id"]) for job in jobs]
    if len(set(job_ids)) != len(job_ids):
        raise RuntimeError("supervision job IDs are not unique")
    return {
        "schema": "ultimate-aws-supervision-job-fragment-v1",
        "status": "plan-only-queue-stage-not-installed",
        "canonical_commit": document["canonical_commit"],
        "batch_manifest": {
            "path": f"{SOURCE_ROOT}/tools/ultimate_concrete_wave0_batch.json",
            "sha256": raw_manifest_sha256,
            "semantic_sha256": document["manifest_sha256"],
        },
        "jobs": jobs,
        "replace_job_ids": ["concrete-wave0-remaining"],
        "retained_placeholder_updates": [
            {"id": "concrete-wave1", "dependencies": job_ids,
             "queue_stage": True},
            {"id": "concrete-wave2", "dependencies": ["concrete-wave1"],
             "queue_stage": True},
        ],
        "no_remote_side_effects": True,
        "never_delete": True,
    }


def write_manifest(document: Mapping[str, object], path: Path = MANIFEST) -> None:
    path.write_bytes(serialized_manifest(document))


def write_job_fragment(document: Mapping[str, object],
                       path: Path = JOB_FRAGMENT) -> None:
    path.write_text(json.dumps(build_supervision_jobs(document), indent=2,
                                sort_keys=True) + "\n")


def merge_supervision_config(document: Mapping[str, object],
                             path: Path = SUPERVISION_CONFIG) -> None:
    """Replace the serial wave-0 placeholder with explicit queued records."""
    fragment = build_supervision_jobs(document)
    if not JOB_FRAGMENT.is_file():
        raise RuntimeError("write the supervision fragment before merging config")
    fragment_sha = sha256_path(JOB_FRAGMENT)
    fragment_label = (str(JOB_FRAGMENT.relative_to(ROOT))
                      if JOB_FRAGMENT.is_relative_to(ROOT)
                      else JOB_FRAGMENT.name)
    current = json.loads(path.read_text())
    jobs = current.get("jobs")
    if not isinstance(jobs, list):
        raise RuntimeError("supervision config jobs are malformed")
    replacements = set(map(str, fragment["replace_job_ids"]))
    batch_ids = {str(item["id"]) for item in fragment["jobs"]}
    updates = {str(item["id"]): item
               for item in fragment["retained_placeholder_updates"]}
    merged: list[dict[str, object]] = []
    seen: set[str] = set()
    for raw in jobs:
        job = dict(raw)
        identifier = str(job["id"])
        if identifier in replacements or identifier in batch_ids:
            continue
        if identifier in updates:
            job["dependencies"] = list(updates[identifier]["dependencies"])
            job["queue_stage"] = True
        merged.append(job)
        seen.add(identifier)
    for raw in fragment["jobs"]:
        job = dict(raw)
        bindings = job.pop("staging_source_bindings")
        if not bindings:
            raise RuntimeError("queued batch job lacks intended source bindings")
        job["staging_plan"] = {
            "path": fragment_label,
            "sha256": fragment_sha,
            "job_id": job["id"],
        }
        if str(job["id"]) in seen:
            raise RuntimeError(f"duplicate queued job {job['id']}")
        merged.append(job)
        seen.add(str(job["id"]))
    current["jobs"] = merged
    supervisor.validate_config(current)
    path.write_text(json.dumps(current, indent=2) + "\n")


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--classes", type=int, default=DEFAULT_CLASSES)
    parser.add_argument("--output", type=Path, default=MANIFEST)
    parser.add_argument("--jobs-output", type=Path, default=JOB_FRAGMENT)
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--merge-supervision-config", action="store_true")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    document = build_document(args.classes)
    if args.write:
        write_manifest(document, args.output)
        write_job_fragment(document, args.jobs_output)
        if args.merge_supervision_config:
            merge_supervision_config(document)
    elif args.merge_supervision_config:
        raise RuntimeError("--merge-supervision-config requires --write")
    else:
        print(json.dumps(document, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
