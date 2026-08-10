#!/usr/bin/env python3
"""Exhaustively verify a rebound Jester/Ghost transition shard inventory.

The migration tree is immutable input.  Successful native regenerations are
recorded in a separate state directory, so an interrupted run resumes without
rechecking certified shards and without modifying transition payloads.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
from typing import Any


SCHEMA = "ultimate-jester-ghost-rebound-verification-v1"
REBIND_SCHEMA = "ultimate-jester-ghost-transition-rebind-v1"
BUNDLE_SCHEMA = "ultimate-jester-ghost-aws-v2"
REBIND_STATUS = "copied-authenticated-model-rebound-payload-unchanged"
RAW_DOMAIN = 38_450_880
EXPECTED_SHARDS = 80
EXPECTED_PARALLELISM = 29
EXPECTED_CANONICAL = 9_612_720
EXPECTED_WORLDS = 37_957_920
EXPECTED_EDGES = 479_456_062
SHA_RE = re.compile(r"^[0-9a-f]{64}$")
CERTIFICATE_RE = re.compile(
    r"^jester_ghost_transition raw (?P<raw>\d+) canonical (?P<canonical>\d+)"
    r" worlds (?P<worlds>\d+) live (?P<live>\d+) actions (?P<actions>\d+)"
    r" observations (?P<observations>\d+) edges (?P<edges>\d+)"
    r" codec_checks (?P<codec_checks>\d+) action_checks (?P<action_checks>\d+)"
    r" decision_checks (?P<decision_checks>\d+)"
    r" transition_checks (?P<transition_checks>\d+)"
    r" symmetry_checks (?P<symmetry_checks>\d+)"
    r" codec_residual (?P<codec_residual>\d+)"
    r" action_residual (?P<action_residual>\d+)"
    r" decision_residual (?P<decision_residual>\d+)"
    r" transition_residual (?P<transition_residual>\d+)"
    r" symmetry_residual (?P<symmetry_residual>\d+)"
    r" payload_sha256 (?P<payload>[0-9a-f]{64})$"
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text())
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected a JSON object")
    return value


def require_sha(value: Any, label: str) -> str:
    if not isinstance(value, str) or not SHA_RE.fullmatch(value):
        raise ValueError(f"invalid {label}")
    return value


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    temporary_path = Path(temporary)
    try:
        with os.fdopen(descriptor, "w") as stream:
            json.dump(value, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, path)
    finally:
        if temporary_path.exists():
            temporary_path.unlink()


def append_only_log_path(log_dir: Path, name: str) -> Path:
    candidate = log_dir / f"{name}.log"
    attempt = 1
    while candidate.exists():
        candidate = log_dir / f"{name}.attempt-{attempt:03d}.log"
        attempt += 1
    return candidate


def inventory(bundle: dict[str, Any], rebind: dict[str, Any]) -> tuple[
        list[dict[str, Any]], dict[str, Any]]:
    if bundle.get("schema") != BUNDLE_SCHEMA:
        raise ValueError("incompatible Jester/Ghost bundle manifest")
    if (bundle.get("raw_geometries") != RAW_DOMAIN or
            bundle.get("merge_inputs") != EXPECTED_SHARDS or
            bundle.get("parallelism") != EXPECTED_PARALLELISM):
        raise ValueError("bundle range inventory or scheduling drift")
    if (rebind.get("schema") != REBIND_SCHEMA or
            rebind.get("status") != REBIND_STATUS):
        raise ValueError("rebound transition manifest is not certified")
    source = require_sha(bundle.get("source_sha256"), "bundle source SHA-256")
    model = require_sha(bundle.get("model_sha256"), "bundle model SHA-256")
    observation = require_sha(
        bundle.get("observation_sha256"), "bundle observation SHA-256")
    if (rebind.get("source_sha256"), rebind.get("new_model_sha256"),
            rebind.get("observation_sha256")) != (source, model, observation):
        raise ValueError("bundle/rebind provenance mismatch")

    ranges = bundle.get("merge_ranges")
    records = rebind.get("records")
    if (not isinstance(ranges, list) or len(ranges) != EXPECTED_SHARDS or
            not isinstance(records, list)):
        raise ValueError("missing 80-shard migration inventory")
    by_name = {record.get("prefix"): record for record in records
               if isinstance(record, dict)}
    shards: list[dict[str, Any]] = []
    cursor = 0
    for entry in ranges:
        if (not isinstance(entry, list) or len(entry) != 3 or
                not isinstance(entry[0], str)):
            raise ValueError("invalid bundle merge range")
        name, begin, count = entry
        if (not isinstance(begin, int) or not isinstance(count, int) or
                begin != cursor or count <= 0):
            raise ValueError("shard ranges are not exact and contiguous")
        record = by_name.get(name)
        if (record is None or record.get("raw_begin") != begin or
                record.get("raw_count") != count or record.get("complete")):
            raise ValueError(f"rebound record mismatch for {name}")
        for field in ("payload_sha256", "header_sha256", "marker_sha256"):
            require_sha(record.get(field), f"{name} {field}")
        shards.append(record)
        cursor += count
    if cursor != RAW_DOMAIN or len({record["prefix"] for record in shards}) != EXPECTED_SHARDS:
        raise ValueError("shards do not partition the full raw domain")

    complete = [record for record in records
                if isinstance(record, dict) and record.get("complete")]
    if len(complete) != 1:
        raise ValueError("expected exactly one complete rebound record")
    merged = complete[0]
    if (merged.get("raw_begin") != 0 or merged.get("raw_count") != RAW_DOMAIN or
            merged.get("geometries") != EXPECTED_CANONICAL or
            merged.get("worlds") != EXPECTED_WORLDS or
            merged.get("edges") != EXPECTED_EDGES):
        raise ValueError("complete rebound certificate totals drifted")
    return shards, merged


def parse_native_certificate(output: str, record: dict[str, Any]) -> dict[str, Any]:
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    matches = [CERTIFICATE_RE.fullmatch(line) for line in lines]
    matches = [match for match in matches if match is not None]
    if len(matches) != 1:
        raise ValueError("native verifier did not emit exactly one certificate")
    values: dict[str, Any] = {
        key: (value if key == "payload" else int(value))
        for key, value in matches[0].groupdict().items()
    }
    if (values["raw"] != record["raw_count"] or
            values["canonical"] != record["geometries"] or
            values["worlds"] != record["worlds"] or
            values["edges"] != record["edges"] or
            values["payload"] != record["payload_sha256"]):
        raise ValueError("native/rebind shard certificate mismatch")
    for field in ("codec_residual", "action_residual", "decision_residual",
                  "transition_residual", "symmetry_residual"):
        if values[field] != 0:
            raise ValueError(f"nonzero {field}")
    if (values["codec_checks"] != 4 *
            (values["worlds"] + values["canonical"]) or
            values["action_checks"] != 4 * values["worlds"] or
            values["decision_checks"] < 4 *
            (record.get("strata", 0) + values["canonical"]) or
            values["transition_checks"] != 4 * values["edges"] or
            values["symmetry_checks"] != 4 * values["canonical"]):
        raise ValueError("native shard conservation certificate mismatch")
    return values


def marker_binding(record: dict[str, Any], binary_sha: str,
                   source: str, model: str, observation: str) -> dict[str, Any]:
    return {
        "schema": SCHEMA,
        "prefix": record["prefix"],
        "raw_begin": record["raw_begin"],
        "raw_count": record["raw_count"],
        "header_sha256": record["header_sha256"],
        "marker_sha256": record["marker_sha256"],
        "payload_sha256": record["payload_sha256"],
        "binary_sha256": binary_sha,
        "source_sha256": source,
        "model_sha256": model,
        "observation_sha256": observation,
    }


def verify_one(binary: Path, transition_tree: Path, state_dir: Path,
               record: dict[str, Any], binary_sha: str, source: str,
               model: str, observation: str) -> dict[str, Any]:
    name = str(record["prefix"])
    header_path = transition_tree / f"{name}.header"
    native_marker_path = transition_tree / f"{name}.verified"
    if (not header_path.is_file() or not native_marker_path.is_file() or
            sha256_file(header_path) != record["header_sha256"] or
            sha256_file(native_marker_path) != record["marker_sha256"]):
        raise ValueError(f"rebound header/marker authentication failed: {name}")
    marker_path = state_dir / "completed" / f"{name}.json"
    binding = marker_binding(record, binary_sha, source, model, observation)
    if marker_path.exists():
        marker = load_json(marker_path)
        if any(marker.get(key) != value for key, value in binding.items()):
            raise ValueError(f"stale or corrupt completion marker: {name}")
        log_path = state_dir / str(marker.get("log", ""))
        if (not log_path.is_file() or
                sha256_file(log_path) != marker.get("log_sha256")):
            raise ValueError(f"completion log authentication failed: {name}")
        certificate = parse_native_certificate(
            log_path.read_text(errors="replace"), record)
        if marker.get("certificate") != certificate:
            raise ValueError(f"completion certificate authentication failed: {name}")
        return marker

    log_dir = state_dir / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    log_path = append_only_log_path(log_dir, name)
    command = [str(binary), "--verify-transitions", "--transition-prefix",
               str(transition_tree / name), "--source-sha256", source,
               "--model-sha256", model, "--observation-sha256", observation,
               "--allow-partial-merge"]
    with log_path.open("xb") as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                check=False)
        log.flush()
        os.fsync(log.fileno())
    output = log_path.read_text(errors="replace")
    if result.returncode:
        raise RuntimeError(f"native verification failed for {name}: {log_path}")
    certificate = parse_native_certificate(output, record)
    if (sha256_file(header_path) != record["header_sha256"] or
            sha256_file(native_marker_path) != record["marker_sha256"]):
        raise ValueError(f"native verifier modified rebound input: {name}")
    marker = {
        **binding,
        "status": "exhaustive-native-regeneration-certified",
        "certificate": certificate,
        "log": str(log_path.relative_to(state_dir)),
        "log_sha256": sha256_file(log_path),
    }
    write_json_atomic(marker_path, marker)
    return marker


def verify(args: argparse.Namespace) -> dict[str, Any]:
    bundle = load_json(args.bundle_manifest)
    rebind = load_json(args.rebind_manifest)
    shards, merged = inventory(bundle, rebind)
    source = require_sha(bundle["source_sha256"], "source SHA-256")
    model = require_sha(bundle["model_sha256"], "model SHA-256")
    observation = require_sha(bundle["observation_sha256"],
                              "observation SHA-256")
    if not args.binary.is_file() or not os.access(args.binary, os.X_OK):
        raise ValueError("native verifier binary is absent or not executable")
    binary_sha = sha256_file(args.binary)
    if binary_sha != require_sha(args.binary_sha256, "expected binary SHA-256"):
        raise ValueError("native verifier binary SHA-256 mismatch")
    if args.jobs != EXPECTED_PARALLELISM:
        raise ValueError("production verification requires exactly 29 workers")
    if args.transition_tree.resolve() != Path(
            rebind.get("destination_tree", "")).resolve():
        raise ValueError("transition tree does not match rebound manifest")
    if (args.state_dir.resolve() == args.transition_tree.resolve() or
            args.transition_tree.resolve() in args.state_dir.resolve().parents):
        raise ValueError("verification state must be outside the rebound tree")
    args.state_dir.mkdir(parents=True, exist_ok=True)

    results = []
    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        futures = [executor.submit(
            verify_one, args.binary, args.transition_tree, args.state_dir,
            record, binary_sha, source, model, observation)
            for record in shards]
        for future in as_completed(futures):
            results.append(future.result())
    results.sort(key=lambda marker: marker["raw_begin"])
    totals = {field: sum(marker["certificate"][field] for marker in results)
              for field in ("raw", "canonical", "worlds", "live", "actions",
                            "observations", "edges", "codec_checks",
                            "action_checks", "decision_checks",
                            "transition_checks", "symmetry_checks")}
    if (totals["raw"] != RAW_DOMAIN or
            totals["canonical"] != merged["geometries"] or
            totals["worlds"] != merged["worlds"] or
            totals["edges"] != merged["edges"]):
        raise ValueError("80-shard aggregate does not match merged certificate")
    summary = {
        "schema": SCHEMA,
        "status": "all-80-rebound-shards-exhaustively-regenerated",
        "shards": len(results),
        "parallelism": args.jobs,
        "binary_sha256": binary_sha,
        "source_sha256": source,
        "model_sha256": model,
        "observation_sha256": observation,
        "rebind_manifest_sha256": sha256_file(args.rebind_manifest),
        "totals": totals,
        "completion_marker_sha256": {
            marker["prefix"]: sha256_file(
                args.state_dir / "completed" / f"{marker['prefix']}.json")
            for marker in results},
    }
    write_json_atomic(args.state_dir / "verification-summary.json", summary)
    return summary


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle-manifest", type=Path, required=True)
    parser.add_argument("--rebind-manifest", type=Path, required=True)
    parser.add_argument("--transition-tree", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--binary-sha256", required=True)
    parser.add_argument("--state-dir", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=EXPECTED_PARALLELISM)
    args = parser.parse_args()
    print(json.dumps(verify(args), sort_keys=True))


if __name__ == "__main__":
    main()
