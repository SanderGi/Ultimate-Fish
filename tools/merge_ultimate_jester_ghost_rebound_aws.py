#!/usr/bin/env python3
"""Merge certified rebound Jester/Ghost shards into a fresh third prefix."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
from types import ModuleType
from typing import Any


SCHEMA = "ultimate-jester-ghost-rebound-merge-v1"
EVIDENCE_SCHEMA = "ultimate-jester-ghost-verification-evidence-v1"
EVIDENCE_STATUS = "all-80-rebound-shards-evidence-certified"
SUFFIXES = (".header", ".meta", ".strata", ".index", ".blocks", ".verified")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text())
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected a JSON object")
    return value


def load_verifier(path: Path, expected_sha256: str) -> ModuleType:
    if not path.is_file() or sha256_file(path) != expected_sha256:
        raise ValueError("verifier module SHA-256 mismatch")
    spec = importlib.util.spec_from_file_location("jg_rebound_verifier", path)
    if spec is None or spec.loader is None:
        raise ValueError("cannot import verifier module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_exclusive_json(path: Path, value: object) -> None:
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())


def preflight(args: argparse.Namespace, verifier: ModuleType) -> tuple[
        dict[str, Any], list[dict[str, Any]], dict[str, Any], dict[str, Any]]:
    if (not args.runner_source.is_file() or
            sha256_file(args.runner_source) != args.runner_sha256):
        raise ValueError("merge runner SHA-256 mismatch")
    if (not args.binary.is_file() or
            sha256_file(args.binary) != args.binary_sha256):
        raise ValueError("native binary SHA-256 mismatch")
    bundle = verifier.load_json(args.bundle_manifest)
    rebind = verifier.load_json(args.rebind_manifest)
    shards, merged = verifier.inventory(bundle, rebind)
    evidence = load_json(args.verification_certificate)
    if (sha256_file(args.verification_certificate) !=
            args.verification_certificate_sha256):
        raise ValueError("verification evidence certificate SHA-256 mismatch")
    if (evidence.get("schema") != EVIDENCE_SCHEMA or
            evidence.get("status") != EVIDENCE_STATUS or
            evidence.get("source_sha256") != bundle["source_sha256"] or
            evidence.get("model_sha256") != bundle["model_sha256"] or
            evidence.get("observation_sha256") != bundle["observation_sha256"] or
            evidence.get("binary_sha256") != args.binary_sha256 or
            evidence.get("verifier_sha256") != args.verifier_sha256 or
            evidence.get("verification_summary_sha256") !=
            sha256_file(args.verification_summary) or
            evidence.get("rebind_manifest_sha256") !=
            sha256_file(args.rebind_manifest) or
            evidence.get("shards") != verifier.EXPECTED_SHARDS or
            evidence.get("logs") != verifier.EXPECTED_SHARDS or
            evidence.get("residuals") != {
                "inventory": 0, "binding": 0, "log": 0, "native": 0,
                "summary": 0, "aggregate": 0}):
        raise ValueError("verification evidence provenance/residual mismatch")
    if evidence.get("merged_reference") != {
            key: merged[key] for key in
            ("prefix", "raw_begin", "raw_count", "geometries", "worlds",
             "edges", "payload_sha256", "header_sha256", "marker_sha256")}:
        raise ValueError("verification evidence merged reference mismatch")
    by_name = {record.get("prefix"): record for record in
               evidence.get("records", []) if isinstance(record, dict)}
    if set(by_name) != {record["prefix"] for record in shards}:
        raise ValueError("verification evidence shard inventory mismatch")
    for record in shards:
        name = record["prefix"]
        proof = by_name[name]
        if (any(proof.get(field) != record[field] for field in
                ("raw_begin", "raw_count", "header_sha256",
                 "payload_sha256")) or
                proof.get("native_marker_sha256") !=
                record["marker_sha256"]):
            raise ValueError(f"verification evidence shard mismatch: {name}")
        for suffix, field in ((".header", "header_sha256"),
                              (".verified", "marker_sha256")):
            path = args.transition_tree / f"{name}{suffix}"
            if not path.is_file() or sha256_file(path) != record[field]:
                raise ValueError(f"rebound input changed after verification: {name}")
    if args.output_dir.exists():
        raise ValueError("fresh third-prefix output directory already exists")
    return bundle, shards, merged, evidence


def merge_command(args: argparse.Namespace, bundle: dict[str, Any],
                  output_prefix: Path) -> list[str]:
    command = [str(args.binary), "--merge-transitions",
               "--transition-prefix", str(output_prefix)]
    for name, _, _ in bundle["merge_ranges"]:
        command.extend(("--shard", str(args.transition_tree / name)))
    command.extend((
        "--source-sha256", bundle["source_sha256"],
        "--model-sha256", bundle["model_sha256"],
        "--observation-sha256", bundle["observation_sha256"],
    ))
    return command


def run_merge(args: argparse.Namespace) -> dict[str, Any]:
    verifier = load_verifier(args.verifier, args.verifier_sha256)
    bundle, shards, merged, evidence = preflight(args, verifier)
    args.output_dir.mkdir(parents=False)
    output_prefix = args.output_dir / "kjesterghostk"
    log_path = args.output_dir / "merge.log"
    with log_path.open("xb") as log:
        result = subprocess.run(
            merge_command(args, bundle, output_prefix), stdout=log,
            stderr=subprocess.STDOUT, check=False)
        log.flush()
        os.fsync(log.fileno())
    if result.returncode:
        raise RuntimeError(f"native merge failed; preserved at {args.output_dir}")
    certificate = verifier.parse_native_certificate(
        log_path.read_text(errors="replace"), merged)
    for record in shards:
        name = record["prefix"]
        for suffix, field in ((".header", "header_sha256"),
                              (".verified", "marker_sha256")):
            if sha256_file(args.transition_tree / f"{name}{suffix}") != record[field]:
                raise ValueError(f"native merge modified rebound input: {name}")
    output_paths = [Path(f"{output_prefix}{suffix}") for suffix in SUFFIXES]
    if not all(path.is_file() for path in output_paths):
        raise ValueError("native merge output prefix is incomplete")
    output = [{"path": path.name, "bytes": path.stat().st_size,
               "sha256": sha256_file(path)} for path in output_paths]
    result_certificate = {
        "schema": SCHEMA,
        "status": "fresh-third-prefix-merged-and-authenticated",
        "source_sha256": bundle["source_sha256"],
        "model_sha256": bundle["model_sha256"],
        "observation_sha256": bundle["observation_sha256"],
        "binary_sha256": args.binary_sha256,
        "runner_sha256": args.runner_sha256,
        "verifier_sha256": args.verifier_sha256,
        "verification_certificate_sha256":
            args.verification_certificate_sha256,
        "verification_summary_sha256": evidence[
            "verification_summary_sha256"],
        "rebind_manifest_sha256": sha256_file(args.rebind_manifest),
        "input_shards": len(shards),
        "output_prefix": str(output_prefix),
        "native_certificate": certificate,
        "merge_log_sha256": sha256_file(log_path),
        "artifacts": output,
        "residuals": {"coverage": 0, "binding": 0, "payload": 0,
                      "input_immutability": 0, "output": 0},
    }
    write_exclusive_json(args.output_dir / "merge-certificate.json",
                         result_certificate)
    return {
        "status": result_certificate["status"],
        "certificate_sha256": sha256_file(
            args.output_dir / "merge-certificate.json"),
        "payload_sha256": certificate["payload"],
        "raw": certificate["raw"],
        "canonical": certificate["canonical"],
        "worlds": certificate["worlds"],
        "edges": certificate["edges"],
        "bytes": sum(record["bytes"] for record in output),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner-source", type=Path, required=True)
    parser.add_argument("--runner-sha256", required=True)
    parser.add_argument("--verifier", type=Path, required=True)
    parser.add_argument("--verifier-sha256", required=True)
    parser.add_argument("--bundle-manifest", type=Path, required=True)
    parser.add_argument("--rebind-manifest", type=Path, required=True)
    parser.add_argument("--transition-tree", type=Path, required=True)
    parser.add_argument("--verification-certificate", type=Path, required=True)
    parser.add_argument("--verification-certificate-sha256", required=True)
    parser.add_argument("--verification-summary", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--binary-sha256", required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run_merge(args), sort_keys=True))


if __name__ == "__main__":
    main()
