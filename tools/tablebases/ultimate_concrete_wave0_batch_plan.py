#!/usr/bin/env python3
"""Build the immutable v4 concrete wave-0 batch plan.

This is deliberately a small versioned wrapper around the production planner.
It shares the runner's exact inventory, dependency resolver, resource policy,
and service renderer, while keeping the v1/v2/v3 plans and work roots
untouched.  The wrapper has its own selection and config merge boundary so a
future batch cannot accidentally mark an older batch superseded.

The command only writes local plan/config artifacts.  Upload, host staging,
and service starts are separate authenticated operations performed by the
supervisor after the source bindings have been rehashed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
from typing import Mapping, Sequence


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))

import build_ultimate_concrete_wave0_batch as base  # noqa: E402
import run_ultimate_concrete_tablebase_shard_aws as runner  # noqa: E402


GIB = 1 << 30
BATCH_VERSION = "v4"
SCHEMA = "ultimate-concrete-wave0-batch-plan-v4"
SELECTION_POLICY = "planned-smallest-packed-exact-dependencies-excluding-v3-v1"
MAX_CLASSES = 24
DEFAULT_CLASSES = 24
UNIT_PREFIX = f"ultimatefish-concrete-wave0-batch-{BATCH_VERSION}"
SOURCE_ROOT = f"/mnt/ultimatefish/concrete-wave0-batch/source-{BATCH_VERSION}/ultimatefish"
DEPENDENCY_BASE_ROOT = f"/mnt/ultimatefish/concrete-wave0-batch/dependencies-{BATCH_VERSION}"
MANIFEST = TOOLS / "ultimate_concrete_wave0_batch_v4.json"
JOB_FRAGMENT = TOOLS / "ultimate_concrete_wave0_batch_v4_jobs.json"
WRAPPER_RELATIVE = Path(__file__).relative_to(ROOT).as_posix()
STAGING_HELPER_RELATIVE = "tools/tablebases/stage_ultimate_concrete_wave0_batch_v4.py"
LEGACY_MANIFEST = TOOLS / "ultimate_concrete_wave0_batch.json"
SUPERVISION_CONFIG = TOOLS / "ultimate_aws_supervision.json"

# Version-pinned, parity-checked replacement for the earlier nine-payload
# object whose manifest listed 175 files.  This is provenance only: staging
# still rehashes every extracted dependency and every final binding.
DEPENDENCY_ARCHIVE = {
    "bucket": "ultimatefish-info-20260808-a4e679c6-831688117652",
    "key": (
        "staging/concrete-wave0-batch/dependencies-v4/full/sha256/"
        "6e9e0f83cef26e25fb9ec156c9d19de8832afbdee5dec49eec1895c8fa891f48/"
        "dependencies-v4-full.tar.zst"
    ),
    "version_id": "CO5VzYOBm4Ws12YHMFN8_Ay.1JIG7nCC",
    "bytes": 894129243,
    "sha256": "6e9e0f83cef26e25fb9ec156c9d19de8832afbdee5dec49eec1895c8fa891f48",
    "manifest_sha256": (
        "0117c6892984556c8528a63154164f8f52ebbfae12caac313435133550569bc7"
    ),
    "status": "head-download-full-sha-archive-restore-verified",
}

# The two c8gd hosts were measured idle in the source event and are assigned
# first.  r8gd queues remain explicit but are launchable only after the
# measured scheduler revalidates their mount and memory gates.  i-08 is not
# assigned: its current /mnt/ultimatefish free-space sample is below the
# committed 20 GiB per-unit floor.
HOST_SPECS = {
    "i-0986ed3d272721f02": {
        "name": "giant-ghost-same", "capacity_class": "c8gd",
        "memory_capacity_bytes": 64 * GIB, "cpu_pool": (4, 5, 6, 7, 8, 9),
        "assignment_count": 6, "mount_rotation": ("/mnt/ultimatefish",),
    },
    "i-024a2073283e4336e": {
        "name": "giant-ghost-opposing", "capacity_class": "c8gd",
        "memory_capacity_bytes": 64 * GIB, "cpu_pool": (4, 5, 6, 7, 8, 9),
        "assignment_count": 6, "mount_rotation": ("/mnt/ultimatefish",),
    },
    "i-0b4523116b2f7765c": {
        "name": "concrete-primary", "capacity_class": "r8gd",
        "memory_capacity_bytes": 256 * GIB, "cpu_pool": (8, 9, 10, 11, 12, 13),
        "assignment_count": 6,
        "mount_rotation": ("/mnt/ultimatefish-resume",),
    },
    "i-03c81f90d2c59a2e7": {
        "name": "hidden-primary", "capacity_class": "r8gd",
        "memory_capacity_bytes": 256 * GIB, "cpu_pool": (8, 9, 10, 11, 12, 13),
        "assignment_count": 6, "mount_rotation": ("/mnt/ultimatefish",),
    },
    "i-08c0f44a1776cb34a": {
        "name": "crossed-and-jester-ghost", "capacity_class": "c8gd",
        "memory_capacity_bytes": 64 * GIB, "cpu_pool": (),
        "assignment_count": 0, "mount_rotation": ("/mnt/ultimatefish",),
    },
}
HOST_ASSIGNMENT_ORDER = tuple(HOST_SPECS)


def _sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def _serialized(document: Mapping[str, object]) -> bytes:
    return (json.dumps(document, indent=2, sort_keys=True) + "\n").encode()


def _current_v3_filenames() -> frozenset[str]:
    if not LEGACY_MANIFEST.is_file():
        raise RuntimeError("committed v3 manifest is missing")
    try:
        document = json.loads(LEGACY_MANIFEST.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        raise RuntimeError("committed v3 manifest is malformed") from exc
    if document.get("schema") != "ultimate-concrete-wave0-batch-plan-v1":
        raise RuntimeError("legacy concrete manifest schema residual")
    units = document.get("units")
    if not isinstance(units, list) or not units:
        raise RuntimeError("committed v3 manifest has no units")
    names = {
        str(unit.get("filename")) for unit in units
        if isinstance(unit, Mapping) and unit.get("filename")
    }
    if len(names) != len(units):
        raise RuntimeError("committed v3 manifest has duplicate/missing classes")
    return frozenset(names)


def _configure_base() -> None:
    """Point the production planner's shared implementation at v4 paths."""
    base.BATCH_VERSION = BATCH_VERSION
    base.SCHEMA = SCHEMA
    base.SELECTION_POLICY = SELECTION_POLICY
    base.MAX_CLASSES = MAX_CLASSES
    base.DEFAULT_CLASSES = DEFAULT_CLASSES
    base.UNIT_PREFIX = UNIT_PREFIX
    base.SOURCE_ROOT = SOURCE_ROOT
    base.DEPENDENCY_BASE_ROOT = DEPENDENCY_BASE_ROOT
    base.MANIFEST = MANIFEST
    base.JOB_FRAGMENT = JOB_FRAGMENT
    base.HOST_SPECS = HOST_SPECS
    base.HOST_ASSIGNMENT_ORDER = HOST_ASSIGNMENT_ORDER
    base._selected_rows = _selected_rows
    base.assignment_slots = _assignment_slots


def _selected_rows(count: int = DEFAULT_CLASSES) -> list[tuple[int, dict[str, object]]]:
    if not 1 <= count <= MAX_CLASSES:
        raise RuntimeError(f"class count must be in [1, {MAX_CLASSES}]")
    legacy = _current_v3_filenames()
    rows = runner.wave_inventory(0)
    by_filename = base.ledger_by_filename()
    artifact_names = frozenset(base.dependency_artifact_records())
    excluded_statuses = frozenset(
        {"certified", "computing", "draw", "preserving", "blocked", "deferred"})
    eligible: list[tuple[int, dict[str, object]]] = []
    for index, row in enumerate(rows):
        filename = str(row["filename"])
        entry = by_filename.get(filename)
        if entry is None or entry.status != "planned":
            continue
        if filename in legacy or filename in base.information.AFFECTED_FILENAMES:
            continue
        if entry.status in excluded_statuses:
            continue
        if row.get("primary") in runner.DEFERRED_DYNAMIC or row.get("secondary") in runner.DEFERRED_DYNAMIC:
            raise RuntimeError(f"deferred dynamic class reached inventory: {filename}")
        if row.get("mirror_simplification") and row.get("secondary") in {"penguin", "mage", "fisherman"}:
            raise RuntimeError(f"Copycat separator class reached inventory: {filename}")
        dependencies = tuple(runner.class_dependency_filenames(row))
        if any(name not in artifact_names for name in dependencies):
            continue
        eligible.append((index, row))
    eligible.sort(key=lambda item: (int(item[1]["packed_bytes"]), str(item[1]["filename"]), item[0]))
    if len(eligible) < count:
        raise RuntimeError(f"only {len(eligible)} v4 classes are eligible")
    chosen = eligible[:count]
    if _current_v3_filenames().intersection(str(row["filename"]) for _, row in chosen):
        raise RuntimeError("v4 selection duplicates a current v3 class")
    return chosen


def _assignment_slots(count: int, hosts: Mapping[str, Mapping[str, object]]) -> list[dict[str, object]]:
    expected = sum(int(HOST_SPECS[item]["assignment_count"]) for item in HOST_ASSIGNMENT_ORDER)
    if count != expected:
        raise RuntimeError(f"v4 count must cover deterministic slots ({expected})")
    slots: list[dict[str, object]] = []
    for instance_id in HOST_ASSIGNMENT_ORDER:
        host = hosts[instance_id]
        cpus = tuple(int(cpu) for cpu in host["cpu_pool"])
        mounts = tuple(str(mount) for mount in host["mount_rotation"])
        assigned = int(host["assignment_count"])
        if assigned > len(cpus):
            raise RuntimeError(f"v4 host CPU slots are not unique: {instance_id}")
        for local_index in range(assigned):
            slots.append({
                "instance_id": instance_id,
                "host_name": str(host["name"]),
                "capacity_class": str(host["capacity_class"]),
                "memory_capacity_bytes": int(host["memory_capacity_bytes"]),
                "local_index": local_index,
                "expected_allowed_cpus": str(cpus[local_index]),
                "work_mount": mounts[local_index % len(mounts)],
                # The measured-idle c8gd hosts are scheduler priority 0;
                # r8gd slots remain explicit fallback capacity.
                "queue_priority": 0 if host["capacity_class"] == "c8gd" else 1,
                "host_assignment_count": assigned,
            })
    if len(slots) != count:
        raise RuntimeError("v4 host assignment cardinality residual")
    for instance_id in HOST_SPECS:
        assigned_cpus = [slot["expected_allowed_cpus"] for slot in slots
                         if slot["instance_id"] == instance_id]
        if len(assigned_cpus) != len(set(assigned_cpus)):
            raise RuntimeError(f"v4 CPU overlap in batch: {instance_id}")
    return slots


def build_document(count: int = DEFAULT_CLASSES) -> dict[str, object]:
    _configure_base()
    document = base.build_document(count)
    # The shared builder has already performed all runner/dependency/resource
    # checks.  Add this wrapper's own source and immutable-namespace proofs.
    source = dict(document["source_hashes"])
    # The supervisor config is the queue consumer that this planner updates;
    # binding its digest into the queue it contains would create a circular
    # self-hash on every promotion commit.  Its canonical Git commit is still
    # recorded below, while installed runner inputs remain content-addressed.
    source.pop(SUPERVISION_CONFIG.relative_to(ROOT).as_posix(), None)
    source[WRAPPER_RELATIVE] = _sha256_path(Path(__file__))
    helper_path = ROOT / STAGING_HELPER_RELATIVE
    if not helper_path.is_file():
        raise RuntimeError("committed v4 staging helper is missing")
    source[STAGING_HELPER_RELATIVE] = _sha256_path(helper_path)
    source = dict(sorted(source.items()))
    document["schema"] = SCHEMA
    document["selection_policy"] = SELECTION_POLICY
    document["batch_version"] = BATCH_VERSION
    document["version_namespace"] = {
        "unit_prefix": UNIT_PREFIX,
        "source_root": SOURCE_ROOT,
        "dependency_root": DEPENDENCY_BASE_ROOT,
        "legacy_manifest": str(LEGACY_MANIFEST.relative_to(ROOT)),
        "legacy_work_roots_immutable": True,
    }
    document["source_hashes"] = source
    for unit in document["units"]:
        unit["source_hashes"] = source
    document["scheduler_policy"] = {
        "state": "queued-supervisor-selects-measured-safe-subset",
        "simultaneous_launch_forbidden": True,
        "priority": "measured-idle-c8gd-before-r8gd",
        "assignment_order": list(HOST_ASSIGNMENT_ORDER),
        "unassigned_host": "i-08c0f44a1776cb34a",
    }
    document["dependency_archive"] = dict(DEPENDENCY_ARCHIVE)
    document["dependency_archive"]["required_dependencies"] = sorted(
        document["all_dependencies"])
    document.pop("manifest_sha256", None)
    encoded = json.dumps(document, sort_keys=True, separators=(",", ":")).encode()
    document["manifest_sha256"] = hashlib.sha256(encoded).hexdigest()
    return document


def build_supervision_jobs(document: Mapping[str, object]) -> dict[str, object]:
    _configure_base()
    fragment = base.build_supervision_jobs(document)
    old_name = "ultimate_concrete_wave0_batch.json"
    new_name = MANIFEST.name
    wrapper_digest = str(document["source_hashes"][WRAPPER_RELATIVE])
    helper_digest = str(document["source_hashes"][STAGING_HELPER_RELATIVE])
    for job, unit in zip(fragment["jobs"], document["units"]):
        bindings = []
        for binding in job["staging_source_bindings"]:
            item = dict(binding)
            if str(item["path"]).endswith("/" + old_name):
                item["path"] = str(item["path"])[:-len(old_name)] + new_name
            bindings.append(item)
        wrapper_path = f"{unit['source_root']}/{WRAPPER_RELATIVE}"
        bindings.append({"path": wrapper_path, "sha256": wrapper_digest})
        helper_path = f"{unit['source_root']}/{STAGING_HELPER_RELATIVE}"
        bindings.append({"path": helper_path, "sha256": helper_digest})
        # Keep the exact input archive and VersionId attached to every queue
        # record so a later promotion cannot silently use the superseded
        # nine-payload object.
        job_archive = dict(DEPENDENCY_ARCHIVE)
        job_archive["required_dependencies"] = list(document["all_dependencies"])
        bindings.sort(key=lambda item: str(item["path"]))
        job["staging_source_bindings"] = bindings
        job["dependency_archive"] = job_archive
    fragment["batch_manifest"]["path"] = f"{SOURCE_ROOT}/tools/{new_name}"
    fragment["replace_job_ids"] = ["concrete-wave0-remaining"]
    fragment["retained_placeholder_updates"] = [
        {"id": "concrete-wave1", "dependencies": [job["id"] for job in fragment["jobs"]],
         "queue_stage": True},
        {"id": "concrete-wave2", "dependencies": ["concrete-wave1"],
         "queue_stage": True},
    ]
    fragment["version_namespace"] = BATCH_VERSION
    fragment["dependency_archive"] = dict(document["dependency_archive"])
    return fragment


def write_manifest(document: Mapping[str, object], path: Path = MANIFEST) -> None:
    path.write_bytes(_serialized(document))


def write_job_fragment(document: Mapping[str, object], path: Path = JOB_FRAGMENT) -> None:
    path.write_text(json.dumps(build_supervision_jobs(document), indent=2, sort_keys=True) + "\n")


def merge_supervision_config(document: Mapping[str, object], path: Path = SUPERVISION_CONFIG) -> None:
    """Add only v4 queue records; preserve every older batch record verbatim."""
    fragment = build_supervision_jobs(document)
    if not JOB_FRAGMENT.is_file():
        raise RuntimeError("write v4 supervision fragment before merging config")
    current = json.loads(path.read_text())
    jobs = current.get("jobs")
    if not isinstance(jobs, list):
        raise RuntimeError("supervision config jobs are malformed")
    replacements = set(map(str, fragment["replace_job_ids"]))
    ids = {str(job["id"]) for job in fragment["jobs"]}
    updates = {str(item["id"]): item for item in fragment["retained_placeholder_updates"]}
    merged: list[dict[str, object]] = []
    seen: set[str] = set()
    for raw in jobs:
        job = dict(raw)
        identifier = str(job.get("id", ""))
        if identifier in replacements or identifier in ids:
            continue
        if identifier in updates:
            job["dependencies"] = list(updates[identifier]["dependencies"])
            job["queue_stage"] = True
        merged.append(job)
        seen.add(identifier)
    fragment_sha = _sha256_path(JOB_FRAGMENT)
    fragment_label = (str(JOB_FRAGMENT.relative_to(ROOT))
                      if JOB_FRAGMENT.is_relative_to(ROOT)
                      else JOB_FRAGMENT.name)
    for raw in fragment["jobs"]:
        job = dict(raw)
        bindings = job.pop("staging_source_bindings")
        if not bindings:
            raise RuntimeError(f"v4 queued job lacks staging bindings: {job['id']}")
        job["staging_plan"] = {"path": fragment_label, "sha256": fragment_sha, "job_id": job["id"]}
        if str(job["id"]) in seen:
            raise RuntimeError(f"duplicate v4 queued job: {job['id']}")
        merged.append(job)
        seen.add(str(job["id"]))
    current["jobs"] = merged
    base.supervisor.validate_config(current)
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
