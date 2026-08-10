#!/usr/bin/env python3
"""Plan or run one AWS shard of the supported K+K+2 inventory.

Default execution is plan/preflight-only and never launches a tablebase solve.
``--full`` is Linux/AWS-only, requires explicit disk/RSS/reverse-edge limits and
an S3 prefix, and runs one dependency wave/class range sequentially.  Every
completed UFTB is independently checked, placed in a deterministic
content-addressed archive, locally restored, uploaded with full-SHA metadata,
HEAD-checked, freshly downloaded, rehashed, restored again, and retained.

Pawn promotion creates a strict dependency DAG:

* wave 0: no Pawn (depends only on completed K+1/stateless tables),
* wave 1: one Pawn (depends on complete wave 0),
* wave 2: two Pawns (depends on complete waves 0 and 1).

Ranges are half-open indices into the filename-sorted inventory for one wave.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import subprocess
import sys
import time
from typing import Iterable, Mapping


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import plan_ultimate_tablebases as plan  # noqa: E402
import run_double_jester_information_capture as preservation  # noqa: E402
import ultimate_tablebase_shards as shards  # noqa: E402


SCHEMA = "ultimate-concrete-k2-aws-run-v2"
DEPENDENCY_SCHEMA = "ultimate-concrete-k2-dependencies-v2"
ARCHIVE_SCHEMA = "ultimate-concrete-k2-result-v2"
CERTIFICATE_SCHEMA = "ultimate-concrete-k2-s3-certificate-v2"
GIANT_TAG = 0x32474E4149474655
PIECE_TYPES = (
    "king", "jester", "knight", "pawn", "queen", "rook", "bishop",
    "berserker", "bomb", "ninja", "turtle", "ghost", "mage", "goop",
    "penguin", "parasite", "devil", "minion", "sludge", "sniper",
    "prince", "checker", "checkerking", "giant", "copycat",
    "copycatclone", "angel", "halo", "fisherman", "dragon",
)
PIECE_INDEX = {name: index for index, name in enumerate(PIECE_TYPES)}
MODEL_SOURCES = (
    "src/ultimate/tablebase.cpp",
    "src/ultimate/tablebase_probe.h",
    "src/ultimate/tablebase_probe.cpp",
    "src/ultimate/position.h",
    "src/ultimate/position.cpp",
    "src/ultimate/information.h",
    "src/ultimate/information.cpp",
    "src/ultimate/information_solver.h",
    "src/ultimate/information_solver.cpp",
    "src/ultimate/nnue.h",
    "src/ultimate/nnue.cpp",
    "tools/plan_ultimate_tablebases.py",
    "tools/ultimate_tablebase_shards.py",
    "tools/run_double_jester_information_capture.py",
    "tools/run_ultimate_concrete_tablebase_shard_aws.py",
)
BUILD_SOURCES = (
    "src/ultimate/tablebase.cpp",
    "src/ultimate/position.cpp",
    "src/ultimate/tablebase_probe.cpp",
    "src/ultimate/information.cpp",
    "src/ultimate/information_solver.cpp",
    "src/ultimate/nnue.cpp",
)
PINNED_BUILD = (
    "clang++", "-std=c++17", "-O3", "-DNDEBUG", "-Wall", "-Wextra",
    "-Wpedantic", "-Werror", "-Wno-error=range-loop-construct", "-pthread",
)
DEFERRED_DYNAMIC = {
    "devil": "each spawn adds a persistent Minion board model",
    "sludge": "moves leave one or two persistent Goop board models",
    "angel": (
        "linking creates a Halo plus off-board host/attachment/order state; rescue "
        "relocates the host and nested Angels require a larger closure"
    ),
}
COPYCAT_MIRROR_SEMANTICS = "linked-horizontal-mirror-single-index-v1"


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def normalized_record(record: Mapping[str, object]) -> dict[str, object]:
    normalized = {
        key: record[key] for key in (
            "filename", "primary", "secondary", "opposing", "states",
            "packed_bytes", "shards", "phase",
        )
    }
    if record.get("mirror_simplification"):
        normalized["mirror_simplification"] = COPYCAT_MIRROR_SEMANTICS
        normalized["truncates_native_separation"] = bool(
            record.get("truncates_native_separation"))
    return normalized


def closed_inventory() -> tuple[dict[str, object], ...]:
    rows = sorted(plan.stateful_candidates(), key=lambda row: str(row["filename"]))
    names = [str(row["filename"]) for row in rows]
    if len(rows) != 232 or len(set(names)) != len(names):
        raise RuntimeError("closed stateful K+K+2 inventory cardinality residual")
    if sum(int(row["packed_bytes"]) for row in rows) != 87_872_584_800:
        raise RuntimeError("closed stateful K+K+2 inventory byte residual")
    return tuple(rows)


def mirror_copycat_inventory() -> tuple[dict[str, object], ...]:
    rows = tuple(plan.mirror_copycat_candidates())
    if (len(rows) != 36 or len({str(row["filename"]) for row in rows}) != 36 or
            sum(int(row["packed_bytes"]) for row in rows) != 6_784_978_200):
        raise RuntimeError("mirror Copycat K+K+2 inventory residual")
    return rows


def supported_inventory() -> tuple[dict[str, object], ...]:
    rows = tuple(sorted((*closed_inventory(), *mirror_copycat_inventory()),
                        key=lambda row: str(row["filename"])))
    if (len(rows) != 268 or len({str(row["filename"]) for row in rows}) != 268 or
            sum(int(row["packed_bytes"]) for row in rows) != 94_657_563_000):
        raise RuntimeError("supported K+K+2 inventory residual")
    return rows


def dependency_wave(record: Mapping[str, object]) -> int:
    pawns = int(record["primary"] == "pawn") + int(record["secondary"] == "pawn")
    return min(pawns, 2)


def wave_inventory(wave: int) -> tuple[dict[str, object], ...]:
    if wave not in (0, 1, 2):
        raise ValueError("dependency wave must be 0, 1, or 2")
    return tuple(row for row in supported_inventory() if dependency_wave(row) == wave)


def base_dependency_filenames() -> tuple[str, ...]:
    rows = plan.inventory(0)
    return tuple(sorted(str(row["filename"]) for row in rows
                        if row["phase"] in {"kings+1", "kings+2-stateless"}))


def required_dependency_filenames(wave: int) -> tuple[str, ...]:
    result = set(base_dependency_filenames())
    for earlier in range(wave):
        result.update(str(row["filename"]) for row in wave_inventory(earlier))
    return tuple(sorted(result))


def inventory_sha256() -> str:
    payload = json.dumps(
        [normalized_record(row) for row in supported_inventory()],
        sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(payload).hexdigest()


def generator_model_sha256(root: Path = ROOT) -> str:
    digest = hashlib.sha256()
    contract = json.dumps({
        "schema": SCHEMA, "inventory_sha256": inventory_sha256(),
        "deferred_dynamic": DEFERRED_DYNAMIC,
        "copycat_mirror_semantics": COPYCAT_MIRROR_SEMANTICS,
    }, sort_keys=True, separators=(",", ":")).encode()
    digest.update(len(contract).to_bytes(8, "little"))
    digest.update(contract)
    for relative in MODEL_SOURCES:
        payload = (root / relative).read_bytes()
        encoded = relative.encode()
        digest.update(len(encoded).to_bytes(4, "little"))
        digest.update(encoded)
        digest.update(len(payload).to_bytes(8, "little"))
        digest.update(payload)
    return digest.hexdigest()


def balanced_ranges(rows: tuple[dict[str, object], ...], count: int) -> list[dict[str, int]]:
    if count <= 0 or count > len(rows):
        raise ValueError("shard count is outside the wave inventory")
    result: list[dict[str, int]] = []
    begin = 0
    remaining_weight = sum(int(row["states"]) for row in rows)
    for shard in range(count):
        remaining_shards = count - shard
        if remaining_shards == 1:
            end = len(rows)
        else:
            target = remaining_weight / remaining_shards
            weight = 0
            end = begin
            while end < len(rows) - (remaining_shards - 1):
                next_weight = int(rows[end]["states"])
                if end > begin and weight + next_weight > target:
                    break
                weight += next_weight
                end += 1
        selected = rows[begin:end]
        states = sum(int(row["states"]) for row in selected)
        result.append({
            "shard": shard, "begin": begin, "end": end,
            "classes": end - begin, "states": states,
            "packed_bytes": sum(int(row["packed_bytes"]) for row in selected),
        })
        remaining_weight -= states
        begin = end
    if begin != len(rows) or any(item["begin"] >= item["end"] for item in result):
        raise RuntimeError("balanced class ranges do not cover the wave")
    return result


def wave_costs() -> list[dict[str, object]]:
    costs = []
    present = {path.name for path in (ROOT / "tablebases").glob("*.uftb")}
    for wave in range(3):
        rows = wave_inventory(wave)
        missing = [row for row in rows if str(row["filename"]) not in present]
        costs.append({
            "wave": wave, "classes": len(rows),
            "states": sum(int(row["states"]) for row in rows),
            "packed_bytes": sum(int(row["packed_bytes"]) for row in rows),
            "repository_present": len(rows) - len(missing),
            "repository_missing": len(missing),
            "missing_packed_bytes": sum(int(row["packed_bytes"]) for row in missing),
            "suggested_four_ranges": balanced_ranges(rows, min(4, len(rows))),
        })
    return costs


def deferred_dynamic_inventory() -> tuple[dict[str, object], ...]:
    """Enumerate sufficient K+K+2 classes deferred by user direction."""
    rows: list[dict[str, object]] = []
    for first_index, first in enumerate(plan.PIECES):
        for second in plan.PIECES[first_index:]:
            if not ({first.name, second.name} & set(DEFERRED_DYNAMIC)):
                continue
            for same_team in (True, False):
                if not plan.sufficient_pair(first, second, same_team):
                    continue
                rows.append({
                    "filename": (
                        f"k{first.name}{second.name}k.uftb" if same_team else
                        f"k{first.name}k{second.name}.uftb"),
                    "primary": first.name, "secondary": second.name,
                    "opposing": not same_team,
                })
    rows.sort(key=lambda row: str(row["filename"]))
    if len(rows) != 90 or len({str(row["filename"]) for row in rows}) != 90:
        raise RuntimeError("deferred dynamic K+K+2 inventory residual")
    return tuple(rows)


def deferred_domain_plan() -> dict[str, object]:
    rows = deferred_dynamic_inventory()
    by_family = {
        family: sum(family in {str(row["primary"]), str(row["secondary"])}
                    for row in rows)
        for family in sorted(DEFERRED_DYNAMIC)
    }
    return {
        "schema": "ultimate-deferred-dynamic-k2-v2",
        "status": "explicitly-deferred-no-symbolic-work-authorized",
        "classes": 90, "by_family_overlap": by_family,
        "families": DEFERRED_DYNAMIC,
        "inventory": list(rows),
        "copycat_mirror_classes_in_scope": 36,
        "copycat_mirror_semantics": COPYCAT_MIRROR_SEMANTICS,
        "copycat_native_separation_classes": 0,
        "copycat_separator_classes_deferred": 6,
        "completeness": (
            "The supported inventory is intentionally incomplete for these "
            "90 Devil/Minion, Sludge/Goop, and Angel/Halo classes."
        ),
    }
def load_dependency_manifest(path: Path) -> dict[str, dict[str, object]]:
    document = json.loads(path.read_text())
    if document.get("schema") != DEPENDENCY_SCHEMA:
        raise RuntimeError("concrete dependency manifest schema mismatch")
    records: dict[str, dict[str, object]] = {}
    for record in document.get("files", []):
        name = str(record.get("filename", ""))
        if (Path(name).name != name or name in records or
                not re.fullmatch(r"[0-9a-f]{64}", str(record.get("sha256", ""))) or
                int(record.get("bytes", 0)) <= 0):
            raise RuntimeError("malformed concrete dependency manifest entry")
        records[name] = dict(record)
    return records


def copy_logical(source: Path, target: Path) -> tuple[int, str]:
    description = shards.manifest(source)
    parts: Iterable[tuple[Path, int, str | None]]
    if description is None:
        parts = ((source, source.stat().st_size, None),)
    else:
        _total, records = description
        parts = ((source.parent / item.name, item.size, item.digest.hex())
                 for item in records)
    digest = hashlib.sha256()
    extent = 0
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("xb") as output:
        for path, expected_size, expected_sha in parts:
            part_digest = hashlib.sha256()
            copied = 0
            with path.open("rb") as stream:
                for block in iter(lambda: stream.read(4 << 20), b""):
                    output.write(block)
                    digest.update(block)
                    part_digest.update(block)
                    copied += len(block)
            if copied != expected_size or (expected_sha is not None and
                    part_digest.hexdigest() != expected_sha):
                raise RuntimeError(f"dependency shard hash/extent residual: {path}")
            extent += copied
    target.chmod(0o444)
    return extent, digest.hexdigest()


def stage_dependencies(source: Path, destination: Path,
                       manifest: Mapping[str, Mapping[str, object]],
                       required: Iterable[str]) -> dict[str, object]:
    staged = []
    for name in sorted(required):
        if name not in manifest:
            raise RuntimeError(f"dependency manifest lacks required {name}")
        input_path = source / name
        if not input_path.is_file():
            raise RuntimeError(f"dependency directory lacks {name}")
        output_path = destination / name
        extent, digest = copy_logical(input_path, output_path)
        expected = manifest[name]
        if extent != int(expected["bytes"]) or digest != expected["sha256"]:
            raise RuntimeError(f"logical dependency hash/extent residual: {name}")
        staged.append({"filename": name, "bytes": extent, "sha256": digest})
    result = {
        "schema": DEPENDENCY_SCHEMA, "files": staged,
        "source_manifest_sha256": sha256_path(source / "manifest.json")
            if (source / "manifest.json").is_file() else None,
    }
    return result


def stage_sources(work: Path, model: str) -> Path:
    bundle = work / "bundle"
    for relative in MODEL_SOURCES:
        target = bundle / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / relative, target)
        target.chmod(0o444)
    if generator_model_sha256(bundle) != model:
        raise RuntimeError("staged concrete generator model residual")
    return bundle


def build_binary(work: Path) -> tuple[Path, list[str]]:
    binary = work / "binary/ultimate_tablebase"
    binary.parent.mkdir(parents=True)
    command = [
        *PINNED_BUILD, "-Ibundle/src/ultimate",
        *(str(Path("bundle") / source) for source in BUILD_SOURCES),
        "-o", "binary/ultimate_tablebase",
    ]
    subprocess.run(command, cwd=work, check=True)
    binary.chmod(0o555)
    return binary, command


def class_command(record: Mapping[str, object], *, dry_run: int = 0,
                  dry_run_begin: int = 0) -> list[str]:
    name = str(record["filename"])
    command = [
        "binary/ultimate_tablebase", "--piece", str(record["primary"]),
        "--piece2", str(record["secondary"]),
    ]
    if record["opposing"]:
        command.append("--opposing")
    if dry_run:
        command.extend(["--dry-run", str(dry_run),
                        "--dry-run-begin", str(dry_run_begin)])
    else:
        command.extend([
            "--output", f"outputs/{name}",
            "--checkpoint", f"scratch/{Path(name).stem}",
            "--checkpoint-every", "0", "--disk-backed",
        ])
    return command


def parse_uftb(path: Path, record: Mapping[str, object],
               proof_log: Path) -> dict[str, object]:
    base = struct.Struct("<8sIIIIIIII")
    with path.open("rb") as stream:
        header = stream.read(base.size)
        if len(header) != base.size:
            raise RuntimeError("truncated generated UFTB header")
        (magic, version, primary, states, legacy_edges, substates,
         wdl_bytes, dtw_bytes, exceptions) = base.unpack(header)
        if magic != b"UFTB1\0\0\0" or version not in (5, 6, 7):
            raise RuntimeError("generated UFTB magic/version residual")
        secondary_data = stream.read(8)
        if len(secondary_data) != 8:
            raise RuntimeError("truncated generated UFTB secondary header")
        secondary, secondary_color = struct.unpack("<II", secondary_data)
        exact_edges = legacy_edges
        if version >= 6:
            payload = stream.read(8)
            if len(payload) != 8:
                raise RuntimeError("truncated generated UFTB exact edge header")
            exact_edges = struct.unpack("<Q", payload)[0]
        codec_tag = 0
        if version >= 7:
            payload = stream.read(8)
            if len(payload) != 8:
                raise RuntimeError("truncated generated UFTB codec tag")
            codec_tag = struct.unpack("<Q", payload)[0]
        expected_extent = (40 + 8 + (8 if version >= 6 else 0) +
                           (8 if version >= 7 else 0) + wdl_bytes +
                           dtw_bytes + exceptions * 6)
        if path.stat().st_size != expected_extent:
            raise RuntimeError("generated UFTB extent residual")
        expected_states = int(record["states"])
        primary_piece = PIECE_INDEX[str(record["primary"])]
        secondary_piece = PIECE_INDEX[str(record["secondary"])]
        giant = "giant" in {record["primary"], record["secondary"]}
        primary_spec = next(
            piece for piece in plan.PIECES
            if piece.name == record["primary"])
        secondary_spec = next(
            piece for piece in plan.PIECES
            if piece.name == record["secondary"])
        expected_substates = plan.pair_state_factor(
            primary_spec, secondary_spec)
        if (primary != primary_piece or secondary != secondary_piece or
                secondary_color != int(bool(record["opposing"])) or
                states != expected_states or substates != expected_substates or
                wdl_bytes != (states + 3) // 4 or dtw_bytes != states or
                (version == 7) != giant or (giant and codec_tag != GIANT_TAG) or
                (version == 6 and exact_edges <= 0xFFFFFFFF)):
            raise RuntimeError("generated UFTB material/codec header residual")

        valid_bytes = bytes(0 if all((value >> shift) & 3 in (1, 2, 3)
                                     for shift in (0, 2, 4, 6)) else 1
                            for value in range(256))
        translation = bytes.maketrans(bytes(range(256)), valid_bytes)
        full_bytes = states // 4
        remaining = full_bytes
        while remaining:
            chunk = stream.read(min(4 << 20, remaining))
            if not chunk or b"\1" in chunk.translate(translation):
                raise RuntimeError("generated UFTB WDL plane residual")
            remaining -= len(chunk)
        if states % 4:
            tail = stream.read(1)
            if len(tail) != 1:
                raise RuntimeError("truncated generated UFTB WDL tail")
            value = tail[0]
            for ordinal in range(states % 4):
                if ((value >> (2 * ordinal)) & 3) not in (1, 2, 3):
                    raise RuntimeError("generated UFTB WDL tail residual")
            if value >> (2 * (states % 4)):
                raise RuntimeError("generated UFTB nonzero WDL padding")
        stream.seek(dtw_bytes, 1)
        prior = -1
        for _ in range(exceptions):
            payload = stream.read(6)
            if len(payload) != 6:
                raise RuntimeError("truncated generated UFTB DTW exception")
            index, distance = struct.unpack("<IH", payload)
            if index <= prior or index >= states or distance < 255:
                raise RuntimeError("generated UFTB DTW exception residual")
            prior = index
        if stream.read(1):
            raise RuntimeError("generated UFTB has trailing bytes")

    text = proof_log.read_text(errors="replace")
    output = re.search(
        rf"^output outputs/{re.escape(path.name)} edges ([0-9]+) "
        r"win ([0-9]+) loss ([0-9]+) draw ([0-9]+)$", text, re.MULTILINE)
    if (output is None or
            re.search(rf"^verifyok states {states}$", text, re.MULTILINE) is None or
            re.search(rf"^complete states {states}/{states}"
                      r"(?: elapsed [0-9eE+.-]+s)?$",
                      text, re.MULTILINE) is None):
        raise RuntimeError("generated UFTB proof-log certificate residual")
    edges, wins, losses, draws = map(int, output.groups())
    if edges != exact_edges or wins + losses + draws != states:
        raise RuntimeError("generated UFTB proof-log conservation residual")
    return {
        "bytes": path.stat().st_size, "sha256": sha256_path(path),
        "version": version, "states": states, "substates": substates,
        "edges": edges, "win": wins, "loss": losses, "draw": draws,
        "exceptions": exceptions, "verification_residual": 0,
    }


def run_logged(command: list[str], log: Path, work: Path,
               environment: Mapping[str, str]) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("xb") as output:
        result = subprocess.run(command, cwd=work, env=dict(environment),
                                stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"concrete generator failed; inspect {log}")


def _live_work_files(work: Path, checkpoint_stem: str,
                     output_name: str, pid: int) -> dict[tuple[int, int], tuple[int, str]]:
    """Return each active solve inode once, including unlinked open files."""
    paths = [work / "outputs" / output_name]
    paths.extend((work / "scratch").glob(f"{checkpoint_stem}*"))
    proc_fds = Path(f"/proc/{pid}/fd")
    if proc_fds.is_dir():
        paths.extend(proc_fds.iterdir())
    files: dict[tuple[int, int], tuple[int, str]] = {}
    for path in paths:
        try:
            metadata = path.stat()
        except (FileNotFoundError, PermissionError):
            continue
        if not stat.S_ISREG(metadata.st_mode):
            continue
        try:
            label = os.readlink(path) if path.parent == proc_fds else str(path)
        except OSError:
            label = str(path)
        if path.parent == proc_fds:
            active_path = label.removesuffix(" (deleted)")
            roots = (str(work / "scratch"), str(work / "outputs"))
            if not any(active_path == root or active_path.startswith(root + os.sep)
                       for root in roots):
                continue
        files[(metadata.st_dev, metadata.st_ino)] = (metadata.st_size, label)
    return files


def _linux_rss_bytes(pid: int) -> int:
    try:
        for line in Path(f"/proc/{pid}/status").read_text().splitlines():
            if line.startswith("VmRSS:"):
                return int(line.split()[1]) * 1024
    except (FileNotFoundError, PermissionError):
        pass
    return 0


def resource_snapshot(work: Path, checkpoint_stem: str,
                      output_name: str, pid: int) -> dict[str, int]:
    files = _live_work_files(work, checkpoint_stem, output_name, pid)
    return {
        "active_file_bytes": sum(size for size, _label in files.values()),
        "reverse_edge_bytes": sum(
            size for size, label in files.values()
            if label.removesuffix(" (deleted)").endswith(".predecessors")),
        "rss_bytes": _linux_rss_bytes(pid),
        "filesystem_free_bytes": shutil.disk_usage(work).free,
    }


def resource_limit_violation(snapshot: Mapping[str, int], *,
                             scratch_limit: int, resident_limit: int,
                             reverse_edge_bytes_limit: int,
                             minimum_free_bytes: int) -> str | None:
    limits = (
        ("active_file_bytes", scratch_limit, "scratch"),
        ("rss_bytes", resident_limit, "resident"),
        ("reverse_edge_bytes", reverse_edge_bytes_limit, "reverse-edge"),
    )
    for key, limit, label in limits:
        if int(snapshot[key]) > limit:
            return f"{label} resource limit exceeded: {snapshot[key]} > {limit}"
    if int(snapshot["filesystem_free_bytes"]) < minimum_free_bytes:
        return ("minimum-free resource limit crossed: "
                f"{snapshot['filesystem_free_bytes']} < {minimum_free_bytes}")
    return None


def run_logged_monitored(command: list[str], log: Path, work: Path,
                         environment: Mapping[str, str], *,
                         checkpoint_stem: str, output_name: str,
                         scratch_limit: int, resident_limit: int,
                         reverse_edge_bytes_limit: int,
                         minimum_free_bytes: int,
                         monitor_interval: float) -> dict[str, object]:
    """Run one owned child with fail-closed Linux resource guards.

    A violation terminates only this runner's child and deliberately leaves all
    opt-in named scratch behind. No cleanup operation exists.
    """
    if not sys.platform.startswith("linux"):
        raise RuntimeError("monitored full concrete generation requires Linux /proc")
    log.parent.mkdir(parents=True, exist_ok=True)
    peaks = {"active_file_bytes": 0, "reverse_edge_bytes": 0, "rss_bytes": 0}
    samples = 0
    violation: str | None = None
    with log.open("xb") as output:
        process = subprocess.Popen(command, cwd=work, env=dict(environment),
                                   stdout=output, stderr=subprocess.STDOUT)
        while process.poll() is None:
            snapshot = resource_snapshot(
                work, checkpoint_stem, output_name, process.pid)
            samples += 1
            for key in peaks:
                peaks[key] = max(peaks[key], snapshot[key])
            violation = resource_limit_violation(
                snapshot, scratch_limit=scratch_limit,
                resident_limit=resident_limit,
                reverse_edge_bytes_limit=reverse_edge_bytes_limit,
                minimum_free_bytes=minimum_free_bytes)
            if violation:
                process.terminate()
                try:
                    process.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                break
            time.sleep(monitor_interval)
        returncode = process.wait()
    certificate: dict[str, object] = {
        "samples": samples, "monitor_interval_seconds": monitor_interval,
        "peaks": peaks, "violation": violation,
        "returncode": returncode, "scratch_retained": True,
    }
    preservation.write_json(log.with_suffix(".resources.json"), certificate)
    if violation:
        raise RuntimeError(f"{violation}; child stopped and scratch retained: {log}")
    if returncode:
        raise RuntimeError(f"concrete generator failed; inspect {log}")
    return certificate


def selection_plan(wave: int, begin: int, end: int) -> tuple[
        tuple[dict[str, object], ...], dict[str, object]]:
    rows = wave_inventory(wave)
    if begin < 0 or end <= begin or end > len(rows):
        raise RuntimeError("class range is outside the selected dependency wave")
    selected = rows[begin:end]
    packed = sum(int(row["packed_bytes"]) for row in selected)
    max_states = max(int(row["states"]) for row in selected)
    static_floor = max(
        int(row["states"]) * (8 + 4 + 8) + int(row["packed_bytes"])
        for row in selected)
    resident_floor = max_states * 4 + (2 << 30)
    return selected, {
        "wave": wave, "begin": begin, "end": end,
        "classes": len(selected), "states": sum(int(row["states"]) for row in selected),
        "packed_bytes": packed, "max_states": max_states,
        "static_scratch_floor_bytes": static_floor,
        "resident_floor_bytes": resident_floor,
    }


def require_aws_full(args: argparse.Namespace, measurement: Mapping[str, object],
                     work: Path) -> None:
    if sys.platform == "darwin":
        raise RuntimeError("full concrete K+K+2 generation is forbidden on macOS")
    if args.aws_execution_ack != "EC2" or not args.s3_prefix:
        raise RuntimeError("--full requires --aws-execution-ack EC2 and --s3-prefix")
    if args.scratch_limit <= 0 or args.resident_limit <= 0 or \
            args.reverse_edge_bytes_limit <= 0 or args.minimum_free_bytes <= 0:
        raise RuntimeError(
            "--full requires explicit scratch/RSS/reverse-edge/minimum-free limits")
    minimum = int(measurement["static_scratch_floor_bytes"]) + \
        args.reverse_edge_bytes_limit
    if minimum > args.scratch_limit:
        raise RuntimeError("static+reverse working estimate exceeds scratch limit")
    if int(measurement["resident_floor_bytes"]) > args.resident_limit:
        raise RuntimeError("resident working estimate exceeds RSS limit")
    available = shutil.disk_usage(work).free
    if available < args.minimum_free_bytes:
        raise RuntimeError("initial free disk is below minimum-free gate")
    durable = (args.scratch_limit * int(measurement["classes"]) +
               5 * int(measurement["packed_bytes"]))
    if durable > available * 9 // 10:
        raise RuntimeError("solve/archive/S3 restore plan exceeds free-disk gate")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-directory", type=Path, required=True)
    parser.add_argument("--dependencies", type=Path, required=True)
    parser.add_argument("--dependency-manifest", type=Path, required=True)
    parser.add_argument("--wave", type=int, choices=(0, 1, 2), required=True)
    parser.add_argument("--range-begin", type=int, required=True)
    parser.add_argument("--range-end", type=int, required=True)
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--aws-execution-ack")
    parser.add_argument("--scratch-limit", type=int, default=0)
    parser.add_argument("--resident-limit", type=int, default=0)
    parser.add_argument("--reverse-edge-bytes-limit", type=int, default=0)
    parser.add_argument("--minimum-free-bytes", type=int, default=0)
    parser.add_argument("--monitor-interval", type=float, default=5.0)
    parser.add_argument("--dry-run-samples", type=int, default=20_000)
    parser.add_argument("--s3-prefix")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    if args.monitor_interval <= 0:
        raise RuntimeError("--monitor-interval must be positive")
    selected, measurement = selection_plan(
        args.wave, args.range_begin, args.range_end)
    model = generator_model_sha256()
    costs = wave_costs()
    dependency_records = load_dependency_manifest(args.dependency_manifest)
    required = required_dependency_filenames(args.wave)
    missing_dependencies = sorted(set(required) - set(dependency_records))
    plan_document = {
        "schema": SCHEMA, "status": "plan-only-full-not-launched",
        "generator_model_sha256": model,
        "inventory_sha256": inventory_sha256(),
        "closed_classes": 232, "closed_packed_bytes": 65_951_886_000,
        "copycat_mirror_classes": 36,
        "copycat_mirror_packed_bytes": 6_784_978_200,
        "supported_classes": 268,
        "supported_packed_bytes": 72_736_864_200,
        "stateless_classes_complete": 160,
        "deferred_dynamic": deferred_domain_plan(),
        "wave_costs": costs,
        "selection": measurement,
        "selected": [normalized_record(row) for row in selected],
        "required_dependencies": len(required),
        "missing_dependencies": missing_dependencies,
        "full_explicitly_requested": bool(args.full), "never_delete": True,
    }
    if missing_dependencies:
        raise RuntimeError(
            f"dependency manifest is incomplete for wave {args.wave}: "
            f"{len(missing_dependencies)} missing")
    if not args.full:
        print(json.dumps(plan_document, indent=2, sort_keys=True))
        return 0

    work = args.work_directory.resolve()
    work.mkdir(parents=True, exist_ok=True)
    if any(work.iterdir()):
        raise RuntimeError(f"AWS concrete work directory must be empty: {work}")
    require_aws_full(args, measurement, work)
    bundle = stage_sources(work, model)
    staged_manifest = stage_dependencies(
        args.dependencies.resolve(), work / "dependencies", dependency_records, required)
    preservation.write_json(work / "dependencies/manifest.json", staged_manifest)
    binary, build_command = build_binary(work)
    binary_sha = sha256_path(binary)
    if generator_model_sha256(bundle) != model:
        raise RuntimeError("concrete generator source changed during build")
    (work / "outputs").mkdir()
    (work / "scratch").mkdir()
    preservation.write_json(work / "run-plan.json", {
        **plan_document, "status": "full-preflight", "build": build_command,
        "binary_sha256": binary_sha,
        "dependency_manifest_sha256": sha256_path(
            work / "dependencies/manifest.json"),
    })
    environment = dict(os.environ)
    environment["ULTIMATE_TABLEBASE_PRESERVE_SCRATCH"] = "1"
    environment["ULTIMATE_TABLEBASE_PATH"] = os.pathsep.join(
        (str(work / "dependencies"), str(work / "outputs")))

    # Four deterministic strata catch codec/dependency defects without
    # allocating any state plane.  This is preflight, not a solve.
    for record in selected:
        for sample in range(4):
            count = min(args.dry_run_samples, int(record["states"]))
            begin = (int(record["states"]) - count) * sample // 3
            log = work / "logs/preflight" / (
                f"{Path(str(record['filename'])).stem}-{sample}.log")
            run_logged(class_command(record, dry_run=count, dry_run_begin=begin),
                       log, work, environment)

    completed = []
    for record in selected:
        filename = str(record["filename"])
        log = work / "logs/generate" / f"{Path(filename).stem}.log"
        resources = run_logged_monitored(
            class_command(record), log, work, environment,
            checkpoint_stem=Path(filename).stem, output_name=filename,
            scratch_limit=args.scratch_limit,
            resident_limit=args.resident_limit,
            reverse_edge_bytes_limit=args.reverse_edge_bytes_limit,
            minimum_free_bytes=args.minimum_free_bytes,
            monitor_interval=args.monitor_interval)
        output = work / "outputs" / filename
        verification = parse_uftb(output, record, log)
        result_manifest = {
            "schema": ARCHIVE_SCHEMA, "generator_model_sha256": model,
            "inventory_sha256": inventory_sha256(), "record": normalized_record(record),
            "output": verification, "proof_log_sha256": sha256_path(log),
            "binary_sha256": binary_sha,
            "dependency_manifest_sha256": sha256_path(
                work / "dependencies/manifest.json"),
            "resource_certificate": resources,
            "bellman_verification_residual": 0, "never_delete": True,
        }
        manifest_path = work / "results" / f"{Path(filename).stem}.json"
        preservation.write_json(manifest_path, result_manifest)
        files = {
            f"tablebases/{filename}": output,
            f"proof/{manifest_path.name}": manifest_path,
            f"proof/{log.name}": log,
            "proof/dependency-manifest.json": work / "dependencies/manifest.json",
            "binary/ultimate_tablebase": binary,
        }
        for relative in MODEL_SOURCES:
            files[f"sources/{relative}"] = work / "bundle" / relative
        archive_path, archive_sha = preservation.content_address_archive(
            work / "archives", Path(filename).stem, files, ARCHIVE_SCHEMA)
        restored = preservation.restore_zstd_archive(
            archive_path, work / "restore-local" / Path(filename).stem,
            ARCHIVE_SCHEMA)
        restored_verification = parse_uftb(
            restored[f"tablebases/{filename}"], record,
            restored[f"proof/{log.name}"])
        if restored_verification["sha256"] != verification["sha256"]:
            raise RuntimeError("locally restored UFTB full-SHA residual")
        key = (f"concrete/v2/model/{model}/wave-{args.wave}/sha256/"
               f"{archive_sha}/{archive_path.name}")
        remote = preservation.upload_head_download_verify(
            source=archive_path, digest=archive_sha,
            extent=archive_path.stat().st_size, prefix=args.s3_prefix,
            key=key, download=work / "s3-verify" / archive_path.name,
            archive_schema=ARCHIVE_SCHEMA)
        completed.append({
            "filename": filename, "status": "generated-preserved",
            "output": verification, "archive": {
                "bytes": archive_path.stat().st_size, "sha256": archive_sha,
                "key": key,
            }, "s3": remote,
        })

    certificate = {
        "schema": CERTIFICATE_SCHEMA,
        "status": "head-download-full-sha-archive-restore-verified",
        "generator_model_sha256": model, "inventory_sha256": inventory_sha256(),
        "selection": measurement, "completed": completed,
        "local_outputs_retained": True, "local_scratch_retained": True,
        "safe_to_delete_gate": False,
    }
    certificate_path = work / "certificates/wave-certificate.json"
    preservation.write_json(certificate_path, certificate)
    certificate_sha = sha256_path(certificate_path)
    certificate_remote = preservation.upload_head_download_verify(
        source=certificate_path, digest=certificate_sha,
        extent=certificate_path.stat().st_size, prefix=args.s3_prefix,
        key=(f"concrete/v2/certificates/sha256/{certificate_sha}/"
             f"{certificate_path.name}"),
        download=work / "s3-verify" / certificate_path.name,
        archive_schema=None)
    print(json.dumps({
        "status": "wave-range-generated-and-s3-restored",
        "generator_model_sha256": model, "selection": measurement,
        "certificate_sha256": certificate_sha,
        "certificate_s3": certificate_remote,
        "safe_to_delete_gate": False,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
