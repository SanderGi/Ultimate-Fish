#!/usr/bin/env python3
"""Stage an authenticated v4 concrete wave-0 batch without overwrites.

The v4 dependency archive is a *candidate* source, not a destination.  Each
archive is extracted into its own fresh directory, and a dependency is chosen
only when its basename, complete file extent, and SHA-256 match the committed
v4 plan.  Identical replicas are harmless; a missing or divergent replica is
fatal.  Existing v4 roots are validated idempotently and are never replaced.

This tool does not start a service.  ``--install-services`` installs only
missing, exact unit files, reloads systemd, and verifies that every selected
unit remains inactive.  It is intentionally separate from queue promotion and
the scheduler.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tarfile
import tempfile
from typing import Iterable, Iterator, Mapping, Sequence


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "ultimate-concrete-wave0-v4-stage-v1"
DEPENDENCY_SCHEMA = "ultimate-concrete-k2-dependencies-v2"
V4_MARKER = "/concrete-wave0-batch/"


class StageError(RuntimeError):
    """A fail-closed staging or authentication error."""


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def file_identity(path: Path) -> tuple[int, str]:
    if not path.is_file():
        raise StageError(f"not a regular file: {path}")
    return path.stat().st_size, sha256_path(path)


def _safe_member_name(name: str) -> PurePosixPath:
    relative = PurePosixPath(name)
    if relative.is_absolute() or ".." in relative.parts:
        raise StageError(f"unsafe archive member: {name!r}")
    if not relative.parts or relative == PurePosixPath("."):
        raise StageError(f"empty archive member: {name!r}")
    return relative


def _tar_stream(archive: Path) -> tuple[tarfile.TarFile, subprocess.Popen[bytes] | None]:
    """Open a regular tar or zstd-compressed tar as a streaming reader."""
    if archive.suffix == ".zst":
        process: subprocess.Popen[bytes] = subprocess.Popen(
            ["zstd", "-dc", str(archive)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        assert process.stdout is not None
        try:
            return tarfile.open(fileobj=process.stdout, mode="r|"), process
        except Exception:
            process.kill()
            process.wait()
            raise
    return tarfile.open(archive, mode="r:"), None


def extract_archive(archive: Path, candidate: Path) -> None:
    """Extract one archive into a new, path-safe candidate directory."""
    archive = archive.resolve()
    if not archive.is_file():
        raise StageError(f"archive is missing: {archive}")
    if candidate.exists():
        raise StageError(f"candidate must be fresh: {candidate}")
    candidate.mkdir(parents=True)
    tar, process = _tar_stream(archive)
    try:
        for member in tar:
            relative = _safe_member_name(member.name)
            target = candidate.joinpath(*relative.parts)
            if member.issym() or member.islnk() or not (member.isdir() or member.isfile()):
                raise StageError(f"archive member is not a regular file/dir: {member.name}")
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                raise StageError(f"duplicate archive member: {member.name}")
            source = tar.extractfile(member)
            if source is None:
                raise StageError(f"archive member cannot be read: {member.name}")
            with target.open("xb") as output:
                shutil.copyfileobj(source, output, length=4 << 20)
    finally:
        tar.close()
        if process is not None:
            stderr = process.stderr.read().decode(errors="replace") if process.stderr else ""
            status = process.wait()
            if status != 0:
                raise StageError(f"zstd extraction failed for {archive}: {stderr.strip()}")


def candidate_files(roots: Iterable[Path], basename: str) -> list[Path]:
    """Return deterministic regular-file candidates for one basename."""
    found: list[Path] = []
    for root in roots:
        if not root.is_dir():
            raise StageError(f"candidate root is missing: {root}")
        found.extend(path for path in root.rglob(basename) if path.is_file())
    return sorted(found, key=lambda path: str(path))


def validate_dependency_archive(root: Path) -> dict[str, dict[str, object]]:
    """Require dependency manifest/payload parity and return its records.

    The old S3 object that prompted this helper carried a 175-file manifest
    beside only nine payloads.  Treating that object as complete is unsafe, so
    parity is checked before any dependency can be copied.  Multiple identical
    payload replicas are allowed; every replica must still authenticate to the
    one manifest record.
    """
    manifests = candidate_files([root], "manifest.json")
    if len(manifests) != 1:
        raise StageError(f"dependency archive must contain one manifest: {root}")
    try:
        document = json.loads(manifests[0].read_text())
    except (OSError, json.JSONDecodeError) as exc:
        raise StageError(f"malformed dependency archive manifest: {manifests[0]}") from exc
    if document.get("schema") != DEPENDENCY_SCHEMA:
        raise StageError(f"unexpected dependency archive schema: {manifests[0]}")
    records = document.get("files")
    if not isinstance(records, list) or not records:
        raise StageError("dependency archive manifest has no files")
    by_name: dict[str, dict[str, object]] = {}
    for raw in records:
        if not isinstance(raw, Mapping):
            raise StageError("malformed dependency archive record")
        name = str(raw.get("filename", ""))
        if not name or Path(name).name != name or name in by_name:
            raise StageError(f"duplicate/malformed dependency manifest name: {name}")
        by_name[name] = {
            "filename": name,
            "bytes": int(raw.get("bytes", -1)),
            "sha256": str(raw.get("sha256", "")),
        }
    payloads = sorted(path for path in root.rglob("*.uftb") if path.is_file())
    payload_names = {path.name for path in payloads}
    if payload_names != set(by_name):
        raise StageError(
            "dependency archive manifest/payload mismatch: "
            f"missing={sorted(set(by_name) - payload_names)} "
            f"extra={sorted(payload_names - set(by_name))}")
    for name, record in sorted(by_name.items()):
        matches = candidate_files([root], name)
        if not matches:
            raise StageError(f"manifest payload missing: {name}")
        for path in matches:
            if file_identity(path) != (int(record["bytes"]), str(record["sha256"])):
                raise StageError(f"manifest payload hash/extent mismatch: {path}")
    return by_name


def resolve_exact_candidate(roots: Iterable[Path], record: Mapping[str, object]) -> Path:
    """Resolve a dependency by basename + exact bytes/SHA, fail closed."""
    name = str(record.get("filename", ""))
    expected_bytes = int(record.get("bytes", -1))
    expected_sha = str(record.get("sha256", ""))
    if not name or expected_bytes < 0 or len(expected_sha) != 64:
        raise StageError(f"malformed dependency record: {record!r}")
    candidates = candidate_files(roots, name)
    if not candidates:
        raise StageError(f"missing dependency candidate: {name}")
    matching: list[Path] = []
    divergent: list[str] = []
    for path in candidates:
        size, digest = file_identity(path)
        if size == expected_bytes and digest == expected_sha:
            matching.append(path)
        else:
            divergent.append(f"{path} ({size},{digest})")
    if divergent:
        # A valid duplicate does not mask a divergent replica.  This catches
        # mixed-generation archives and accidental basename collisions.
        raise StageError(
            f"divergent dependency candidate {name}: " + "; ".join(divergent))
    if not matching:
        raise StageError(f"dependency proof mismatch: {name}")
    return min(matching, key=lambda path: str(path))


def _write_new(path: Path, payload: bytes, mode: int = 0o444) -> bool:
    """Create one file without replacing a concurrent/pre-existing path."""
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open("xb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
    except FileExistsError:
        return False
    path.chmod(mode)
    return True


def copy_exact(source: Path, destination: Path, *, mode: int = 0o444) -> str:
    """Copy a file only if the destination is absent or exactly identical."""
    expected = file_identity(source)
    if destination.exists():
        if file_identity(destination) != expected:
            raise StageError(f"pre-existing destination mismatch: {destination}")
        return "preserved"
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(
        f".{destination.name}.stage-{os.getpid()}-{next(tempfile._get_candidate_names())}")
    try:
        with source.open("rb") as input_stream, temporary.open("xb") as output:
            shutil.copyfileobj(input_stream, output, length=4 << 20)
            output.flush()
            os.fsync(output.fileno())
        temporary.chmod(mode)
        try:
            os.link(temporary, destination)
        except FileExistsError:
            # Another actor won the race.  It must still authenticate exactly.
            if file_identity(destination) != expected:
                raise StageError(f"concurrent destination mismatch: {destination}")
            return "preserved"
        return "copied"
    finally:
        temporary.unlink(missing_ok=True)


def _manifest_payload(records: Sequence[Mapping[str, object]]) -> bytes:
    payload = {
        "schema": DEPENDENCY_SCHEMA,
        "files": sorted(
            [{"filename": str(record["filename"]),
              "sha256": str(record["sha256"]),
              "bytes": int(record["bytes"])} for record in records],
            key=lambda item: item["filename"],
        ),
    }
    return (json.dumps(payload, indent=2, sort_keys=True) + "\n").encode()


def materialize_dependency_root(
    root: Path,
    records: Sequence[Mapping[str, object]],
    candidates: Mapping[str, Path],
    expected_manifest_sha: str,
) -> None:
    """Fill/validate one distinct v4 dependency root without replacement."""
    if "/dependencies-v4/" not in str(root):
        raise StageError(f"dependency root is not a v4 root: {root}")
    root.mkdir(parents=True, exist_ok=True)
    expected_names = {str(record["filename"]) for record in records}
    allowed = expected_names | {"manifest.json"}
    for child in root.iterdir():
        if child.name not in allowed:
            raise StageError(f"unexpected file in dependency root: {child}")
    for record in records:
        name = str(record["filename"])
        source = candidates[name]
        destination = root / name
        if file_identity(source) != (int(record["bytes"]), str(record["sha256"])):
            raise StageError(f"candidate changed before copy: {name}")
        copy_exact(source, destination)
    payload = _manifest_payload(records)
    digest = hashlib.sha256(payload).hexdigest()
    if digest != expected_manifest_sha:
        raise StageError(f"dependency subset manifest mismatch: {root}")
    manifest = root / "manifest.json"
    if manifest.exists():
        if file_identity(manifest) != (len(payload), digest):
            raise StageError(f"pre-existing dependency manifest mismatch: {manifest}")
    elif not _write_new(manifest, payload):
        if file_identity(manifest) != (len(payload), digest):
            raise StageError(f"concurrent dependency manifest mismatch: {manifest}")


def _find_unique_relative(root: Path, relative: str) -> Path:
    matches = sorted(
        path for path in root.rglob(Path(relative).name)
        if path.is_file() and str(path).endswith(relative))
    if not matches:
        raise StageError(f"archive lacks {relative}")
    identities = {file_identity(path) for path in matches}
    if len(identities) != 1:
        raise StageError(f"divergent archive replicas for {relative}")
    return matches[0]


def source_candidate_root(candidate: Path) -> Path:
    marker = _find_unique_relative(candidate, "tools/ultimate_concrete_wave0_batch_v4.json")
    return marker.parent.parent


def materialize_source_root(target: Path, candidate: Path,
                            expected_hashes: Mapping[str, object]) -> None:
    """Validate an existing source-v4 root or copy a fresh one exactly."""
    if "/source-v4/" not in str(target):
        raise StageError(f"source root is not a v4 root: {target}")
    source = source_candidate_root(candidate)
    expected = {str(path): str(digest) for path, digest in expected_hashes.items()}
    for relative, digest in expected.items():
        path = source / relative
        if not path.is_file() or sha256_path(path) != digest:
            raise StageError(f"source candidate mismatch: {relative}")
    if target.exists():
        for relative, digest in expected.items():
            path = target / relative
            if not path.is_file() or sha256_path(path) != digest:
                raise StageError(f"pre-existing source mismatch: {path}")
        return
    target.mkdir(parents=True)
    # Copy only the authenticated source tree; do not carry archive metadata
    # or unexpected files into the executable root.
    for relative in expected:
        copy_exact(source / relative, target / relative, mode=0o444)
    for directory in sorted({(target / rel).parent for rel in expected}, key=str):
        directory.mkdir(parents=True, exist_ok=True)
    # copy_exact created parent directories as needed; source files are enough
    # for the runner bindings, while unrelated archive files stay isolated.


def materialize_units(units_target: Path, candidate: Path,
                      units: Sequence[Mapping[str, object]]) -> None:
    if units_target != Path("/etc/systemd/system"):
        # Test callers may use a temporary root, but it must remain a literal
        # unit directory rather than an arbitrary work root.
        if units_target.name != "system":
            raise StageError(f"unit destination is not a system directory: {units_target}")
    units_target.mkdir(parents=True, exist_ok=True)
    for unit in units:
        name = str(unit["unit"])
        if not name.startswith("ultimatefish-concrete-wave0-batch-v4-"):
            raise StageError(f"non-v4 unit in plan: {name}")
        source = _find_unique_relative(candidate, name)
        expected = (int(unit["service_text"].encode().__len__()),
                    str(unit["service_sha256"]))
        if file_identity(source) != expected:
            raise StageError(f"unit archive mismatch: {name}")
        copy_exact(source, units_target / name)


def verify_bindings(jobs: Sequence[Mapping[str, object]], units_by_name: Mapping[str, Mapping[str, object]]) -> int:
    """Rehash every final binding and require every selected work root empty."""
    checked = 0
    for job in jobs:
        for binding in job.get("staging_source_bindings", ()):
            path = Path(str(binding["path"]))
            if not path.is_file() or sha256_path(path) != str(binding["sha256"]):
                raise StageError(f"source binding mismatch: {path}")
            checked += 1
        unit = units_by_name[str(job["unit"])]
        work = Path(str(unit["work_directory"]))
        work.mkdir(parents=True, exist_ok=True)
        if any(work.iterdir()):
            raise StageError(f"work root is not empty: {work}")
    return checked


def _systemd_state(unit: str) -> dict[str, str]:
    output = subprocess.check_output(
        ["systemctl", "show", unit, "-p", "LoadState", "-p", "ActiveState",
         "-p", "SubState", "-p", "Result", "-p", "ExecMainStatus", "--no-pager"],
        text=True,
    )
    state: dict[str, str] = {}
    for line in output.splitlines():
        key, separator, value = line.partition("=")
        if separator:
            state[key] = value
    return state


def verify_inactive(units: Sequence[Mapping[str, object]]) -> None:
    for unit in units:
        name = str(unit["unit"])
        state = _systemd_state(name)
        if state.get("LoadState") != "loaded" or state.get("ActiveState") != "inactive":
            raise StageError(f"unit is not loaded/inactive: {name}: {state}")


def load_plan(path: Path) -> dict[str, object]:
    try:
        document = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        raise StageError(f"malformed v4 plan: {path}") from exc
    if document.get("schema") != "ultimate-concrete-wave0-batch-plan-v4":
        raise StageError("unexpected v4 plan schema")
    if not isinstance(document.get("units"), list) or not document["units"]:
        raise StageError("v4 plan has no units")
    return document


def stage_host(*, plan: Mapping[str, object], jobs_document: Mapping[str, object],
               host_id: str, source_archive: Path, units_archive: Path,
               dependency_archives: Sequence[Path], candidate_parent: Path,
               source_target: Path | None = None,
               units_target: Path = Path("/etc/systemd/system"),
               install_services: bool = False) -> dict[str, object]:
    all_units = [unit for unit in plan["units"]
                 if str(unit["instance_id"]) == host_id]
    if not all_units:
        raise StageError(f"host has no v4 units: {host_id}")
    source_target = source_target or Path(str(all_units[0]["source_root"]))
    candidate_parent = candidate_parent.resolve()
    candidate_parent.mkdir(parents=True, exist_ok=True)
    source_candidate = candidate_parent / "source-candidate"
    unit_candidate = candidate_parent / "units-candidate"
    extract_archive(source_archive, source_candidate)
    extract_archive(units_archive, unit_candidate)
    dependency_candidates: list[Path] = []
    for index, archive in enumerate(dependency_archives):
        candidate = candidate_parent / f"dependency-candidate-{index:02d}"
        extract_archive(archive, candidate)
        validate_dependency_archive(candidate)
        dependency_candidates.append(candidate)
    expected_hashes = dict(plan.get("source_hashes", {}))
    materialize_source_root(source_target, source_candidate, expected_hashes)
    unit_records = list(all_units)
    materialize_units(units_target, unit_candidate, unit_records)
    by_name: dict[str, Path] = {}
    records_by_name: dict[str, Mapping[str, object]] = {}
    for unit in unit_records:
        for record in unit["dependency_records"]:
            name = str(record["filename"])
            records_by_name[name] = record
    for name, record in sorted(records_by_name.items()):
        by_name[name] = resolve_exact_candidate(dependency_candidates, record)
    for unit in unit_records:
        materialize_dependency_root(
            Path(str(unit["dependency_root"])),
            list(unit["dependency_records"]),
            by_name,
            str(unit["dependency_manifest_sha256"]),
        )
    units_by_name = {str(unit["unit"]): unit for unit in unit_records}
    jobs = [job for job in jobs_document["jobs"]
            if str(job["instance_id"]) == host_id]
    checked = verify_bindings(jobs, units_by_name)
    if install_services:
        subprocess.run(["systemctl", "daemon-reload"], check=True)
        verify_inactive(unit_records)
    return {
        "schema": SCHEMA,
        "host_id": host_id,
        "units": len(unit_records),
        "jobs": len(jobs),
        "bindings": checked,
        "services_installed_inactive": bool(install_services),
        "dependencies": sorted(by_name),
    }


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--jobs", type=Path, required=True)
    parser.add_argument("--host-id", required=True)
    parser.add_argument("--source-archive", type=Path, required=True)
    parser.add_argument("--units-archive", type=Path, required=True)
    parser.add_argument("--dependency-archive", type=Path, action="append", required=True)
    parser.add_argument("--candidate-parent", type=Path, required=True)
    parser.add_argument("--source-target", type=Path)
    parser.add_argument("--units-target", type=Path, default=Path("/etc/systemd/system"))
    parser.add_argument("--install-services", action="store_true")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    plan = load_plan(args.plan)
    jobs = json.loads(args.jobs.read_text())
    if not isinstance(jobs.get("jobs"), list):
        raise StageError("malformed v4 jobs document")
    result = stage_host(
        plan=plan,
        jobs_document=jobs,
        host_id=args.host_id,
        source_archive=args.source_archive,
        units_archive=args.units_archive,
        dependency_archives=args.dependency_archive,
        candidate_parent=args.candidate_parent,
        source_target=args.source_target,
        units_target=args.units_target,
        install_services=args.install_services,
    )
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except StageError as exc:
        print(f"v4 staging failed closed: {exc}", file=sys.stderr)
        raise SystemExit(1)
