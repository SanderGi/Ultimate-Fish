#!/usr/bin/env python3
"""Build the add-only v4r2 concrete wave-0 staging namespace.

The original v4 roots were partially staged before the authenticated helper
and complete dependency archive were published.  v4r2 is intentionally a
new namespace: no v4/v3 path is replaced, and the old v4 queue records remain
in the supervisor config as superseded.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
from typing import Mapping, Sequence


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

import build_ultimate_concrete_wave0_batch_v4 as v4  # noqa: E402


GIB = 1 << 30
BATCH_VERSION = "v4r2"
SCHEMA = "ultimate-concrete-wave0-batch-plan-v4r2"
UNIT_PREFIX = f"ultimatefish-concrete-wave0-batch-{BATCH_VERSION}"
SOURCE_ROOT = f"/mnt/ultimatefish/concrete-wave0-batch/source-{BATCH_VERSION}/ultimatefish"
DEPENDENCY_BASE_ROOT = f"/mnt/ultimatefish/concrete-wave0-batch/dependencies-{BATCH_VERSION}"
MANIFEST = TOOLS / "ultimate_concrete_wave0_batch_v4r2.json"
JOB_FRAGMENT = TOOLS / "ultimate_concrete_wave0_batch_v4r2_jobs.json"
WRAPPER_RELATIVE = Path(__file__).relative_to(ROOT).as_posix()
STAGING_HELPER_RELATIVE = v4.STAGING_HELPER_RELATIVE
SUPERVISION_CONFIG = v4.SUPERVISION_CONFIG
DEPENDENCY_ARCHIVE = v4.DEPENDENCY_ARCHIVE

_V4_DEFAULT_NAMESPACE = {
    "BATCH_VERSION": v4.BATCH_VERSION,
    "SCHEMA": v4.SCHEMA,
    "UNIT_PREFIX": v4.UNIT_PREFIX,
    "SOURCE_ROOT": v4.SOURCE_ROOT,
    "DEPENDENCY_BASE_ROOT": v4.DEPENDENCY_BASE_ROOT,
    "MANIFEST": v4.MANIFEST,
    "JOB_FRAGMENT": v4.JOB_FRAGMENT,
}


def _sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def _serialized(document: Mapping[str, object]) -> bytes:
    return (json.dumps(document, indent=2, sort_keys=True) + "\n").encode()


def _configure_base() -> None:
    # Reuse the exact v4 selector/host policy and production dependency
    # resolver, changing only the immutable namespace and generated outputs.
    v4.BATCH_VERSION = BATCH_VERSION
    v4.SCHEMA = SCHEMA
    v4.UNIT_PREFIX = UNIT_PREFIX
    v4.SOURCE_ROOT = SOURCE_ROOT
    v4.DEPENDENCY_BASE_ROOT = DEPENDENCY_BASE_ROOT
    v4.MANIFEST = MANIFEST
    v4.JOB_FRAGMENT = JOB_FRAGMENT
    v4._configure_base()


def _restore_v4_namespace() -> None:
    for name, value in _V4_DEFAULT_NAMESPACE.items():
        setattr(v4, name, value)


def build_document(count: int = v4.DEFAULT_CLASSES) -> dict[str, object]:
    _configure_base()
    try:
        document = v4.base.build_document(count)
        source = dict(document["source_hashes"])
        source.pop(SUPERVISION_CONFIG.relative_to(ROOT).as_posix(), None)
        source[WRAPPER_RELATIVE] = _sha256_path(Path(__file__))
        helper = ROOT / STAGING_HELPER_RELATIVE
        if not helper.is_file():
            raise RuntimeError("committed v4 staging helper is missing")
        source[STAGING_HELPER_RELATIVE] = _sha256_path(helper)
        source = dict(sorted(source.items()))
        document["schema"] = SCHEMA
        document["batch_version"] = BATCH_VERSION
        document["selection_policy"] = v4.SELECTION_POLICY
        document["source_hashes"] = source
        document["version_namespace"] = {
            "unit_prefix": UNIT_PREFIX,
            "source_root": SOURCE_ROOT,
            "dependency_root": DEPENDENCY_BASE_ROOT,
            "supersedes_namespace": "v4",
            "legacy_work_roots_immutable": True,
        }
        document["scheduler_policy"] = {
            "state": "queued-supervisor-selects-measured-safe-subset",
            "simultaneous_launch_forbidden": True,
            "priority": "measured-idle-c8gd-before-r8gd",
            "assignment_order": list(v4.HOST_ASSIGNMENT_ORDER),
            "unassigned_host": "i-08c0f44a1776cb34a",
        }
        document["dependency_archive"] = dict(v4.DEPENDENCY_ARCHIVE)
        document["dependency_archive"]["required_dependencies"] = sorted(
            document["all_dependencies"])
        for unit in document["units"]:
            unit["source_hashes"] = source
        document.pop("manifest_sha256", None)
        encoded = json.dumps(document, sort_keys=True, separators=(",", ":")).encode()
        document["manifest_sha256"] = hashlib.sha256(encoded).hexdigest()
        return document
    finally:
        _restore_v4_namespace()


def build_supervision_jobs(document: Mapping[str, object]) -> dict[str, object]:
    _configure_base()
    try:
        fragment = v4.base.build_supervision_jobs(document)
        old_name = "ultimate_concrete_wave0_batch.json"
        new_name = MANIFEST.name
        helper_digest = str(document["source_hashes"][STAGING_HELPER_RELATIVE])
        wrapper_digest = str(document["source_hashes"][WRAPPER_RELATIVE])
        archive = dict(document["dependency_archive"])
        for job, unit in zip(fragment["jobs"], document["units"]):
            bindings = []
            for binding in job["staging_source_bindings"]:
                item = dict(binding)
                if str(item["path"]).endswith("/" + old_name):
                    item["path"] = str(item["path"])[:-len(old_name)] + new_name
                bindings.append(item)
            bindings.append({
                "path": f"{unit['source_root']}/{WRAPPER_RELATIVE}",
                "sha256": wrapper_digest,
            })
            bindings.append({
                "path": f"{unit['source_root']}/{STAGING_HELPER_RELATIVE}",
                "sha256": helper_digest,
            })
            bindings.sort(key=lambda item: str(item["path"]))
            job["staging_source_bindings"] = bindings
            job["dependency_archive"] = dict(archive)
            job["supersedes_job_id"] = (
                f"concrete-wave0-batch-v4-{int(unit['ordinal']):02d}-"
                f"class{int(unit['inventory_index']):03d}")
        fragment["batch_manifest"] = {
            "path": f"{SOURCE_ROOT}/tools/{new_name}",
            "sha256": hashlib.sha256(_serialized(document)).hexdigest(),
            "semantic_sha256": document["manifest_sha256"],
        }
        fragment["replace_job_ids"] = []
        fragment["retained_placeholder_updates"] = [
            {"id": "concrete-wave1",
             "dependencies": [job["id"] for job in fragment["jobs"]],
             "queue_stage": True},
            {"id": "concrete-wave2", "dependencies": ["concrete-wave1"],
             "queue_stage": True},
        ]
        fragment["version_namespace"] = BATCH_VERSION
        fragment["dependency_archive"] = dict(archive)
        return fragment
    finally:
        _restore_v4_namespace()


def write_manifest(document: Mapping[str, object], path: Path = MANIFEST) -> None:
    path.write_bytes(_serialized(document))


def write_job_fragment(document: Mapping[str, object], path: Path = JOB_FRAGMENT) -> None:
    path.write_text(json.dumps(build_supervision_jobs(document), indent=2, sort_keys=True) + "\n")


def merge_supervision_config(document: Mapping[str, object], path: Path = SUPERVISION_CONFIG) -> None:
    fragment = build_supervision_jobs(document)
    current = json.loads(path.read_text())
    jobs = current.get("jobs")
    if not isinstance(jobs, list):
        raise RuntimeError("supervision config jobs are malformed")
    old_v4 = {
        str(job["id"]): job for job in fragment["jobs"]
    }
    merged: list[dict[str, object]] = []
    for raw in jobs:
        job = dict(raw)
        identifier = str(job.get("id", ""))
        # Regeneration is idempotent: the previous r2 records are replaced by
        # the freshly bound fragment below, rather than duplicated in the
        # explicit queue.  v1-v3 and the superseded v4 records are retained.
        if identifier.startswith("concrete-wave0-batch-v4r2-"):
            continue
        if identifier.startswith("concrete-wave0-batch-v4-"):
            suffix = identifier.removeprefix("concrete-wave0-batch-v4-")
            replacement = f"concrete-wave0-batch-v4r2-{suffix}"
            if replacement in old_v4:
                job["advanceable"] = False
                job["queue_stage"] = True
                job["source_bindings"] = []
                job["superseded_by"] = replacement
            merged.append(job)
            continue
        if identifier == "concrete-wave1":
            job["dependencies"] = [str(item["id"]) for item in fragment["jobs"]]
            job["queue_stage"] = True
        elif identifier == "concrete-wave2":
            job["dependencies"] = ["concrete-wave1"]
            job["queue_stage"] = True
        merged.append(job)
    for raw in fragment["jobs"]:
        job = dict(raw)
        job.pop("supersedes_job_id", None)
        job["staging_plan"] = {
            "path": (str(JOB_FRAGMENT.relative_to(ROOT))
                     if JOB_FRAGMENT.is_relative_to(ROOT) else JOB_FRAGMENT.name),
            "sha256": _sha256_path(JOB_FRAGMENT),
            "job_id": job["id"],
        }
        merged.append(job)
    current["jobs"] = merged
    v4.base.supervisor.validate_config(current)
    path.write_text(json.dumps(current, indent=2) + "\n")


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--classes", type=int, default=v4.DEFAULT_CLASSES)
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
