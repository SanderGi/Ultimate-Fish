#!/usr/bin/env python3
"""Certify a fresh Jester/Ghost merged transition prefix without mutation."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
from types import ModuleType
from typing import Any


SCHEMA = "ultimate-jester-ghost-merge-evidence-v1"
STATUS = "fresh-third-prefix-exhaustively-certified"
SUFFIXES = (".header", ".meta", ".strata", ".index", ".blocks", ".verified")
EXPECTED = {"raw": 38_450_880, "canonical": 9_612_720,
            "worlds": 37_957_920, "edges": 479_456_062}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text())
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected JSON object")
    return value


def load_module(name: str, path: Path, expected_sha256: str) -> ModuleType:
    if not path.is_file() or sha256_file(path) != expected_sha256:
        raise ValueError(f"{name} source SHA-256 mismatch")
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ValueError(f"cannot import {name}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_exclusive_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())


def regular_relative(root: Path, path: Path) -> str:
    root = root.resolve(strict=True)
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"evidence is not a regular file: {path}")
    return path.resolve(strict=True).relative_to(root).as_posix()


def certify(args: argparse.Namespace) -> dict[str, Any]:
    if (not args.audit_source.is_file() or
            sha256_file(args.audit_source) != args.audit_sha256):
        raise ValueError("merge audit source SHA-256 mismatch")
    verifier = load_module("jg_verifier", args.verifier,
                           args.verifier_sha256)
    rebind_module = load_module("jg_rebind", args.rebind_source,
                                args.rebind_source_sha256)
    bundle = verifier.load_json(args.bundle_manifest)
    rebind = verifier.load_json(args.rebind_manifest)
    shards, merged = verifier.inventory(bundle, rebind)
    source = bundle["source_sha256"]
    model = bundle["model_sha256"]
    observation = bundle["observation_sha256"]
    bindings = (source.encode(), model.encode(), observation.encode())

    verification = load_json(args.verification_certificate)
    if (sha256_file(args.verification_certificate) !=
            args.verification_certificate_sha256 or
            verification.get("status") !=
            "all-80-rebound-shards-evidence-certified" or
            verification.get("residuals") != {
                "inventory": 0, "binding": 0, "log": 0, "native": 0,
                "summary": 0, "aggregate": 0}):
        raise ValueError("shard verification certificate mismatch")
    if (not args.merge_runner.is_file() or
            sha256_file(args.merge_runner) != args.merge_runner_sha256):
        raise ValueError("merge runner SHA-256 mismatch")

    merge_certificate = load_json(args.merge_certificate)
    if (merge_certificate.get("schema") !=
            "ultimate-jester-ghost-rebound-merge-v1" or
            merge_certificate.get("status") !=
            "fresh-third-prefix-merged-and-authenticated" or
            merge_certificate.get("source_sha256") != source or
            merge_certificate.get("model_sha256") != model or
            merge_certificate.get("observation_sha256") != observation or
            merge_certificate.get("runner_sha256") !=
            args.merge_runner_sha256 or
            merge_certificate.get("verifier_sha256") !=
            args.verifier_sha256 or
            merge_certificate.get("verification_certificate_sha256") !=
            args.verification_certificate_sha256 or
            merge_certificate.get("input_shards") != 80 or
            merge_certificate.get("residuals") != {
                "coverage": 0, "binding": 0, "payload": 0,
                "input_immutability": 0, "output": 0}):
        raise ValueError("merge certificate provenance/residual mismatch")

    # Re-authenticate every preserved rebound prefix, including all component
    # extents, the combined payload, index monotonicity, and header/marker
    # provenance.  This is deliberately independent of merge completion.
    rebound_records: list[dict[str, Any]] = []
    for record in [*shards, merged]:
        prefix = args.transition_tree / record["prefix"]
        actual = rebind_module.authenticate_prefix(prefix, *bindings)
        if any(actual.get(key) != record.get(key) for key in
               ("prefix", "raw_begin", "raw_count", "complete",
                "geometries", "worlds", "edges", "payload_sha256",
                "header_sha256", "marker_sha256")):
            raise ValueError(f"rebound prefix changed: {record['prefix']}")
        rebound_records.append(actual)

    output_prefix = args.output_dir / "kjesterghostk"
    allowed = {f"kjesterghostk{suffix}" for suffix in SUFFIXES} | {
        "merge.log", "merge-certificate.json"}
    actual_names = {path.name for path in args.output_dir.iterdir()
                    if path.is_file()}
    if actual_names != allowed:
        raise ValueError("fresh merge output inventory mismatch")
    fresh = rebind_module.authenticate_prefix(output_prefix, *bindings)
    if any(fresh.get(key) != merged.get(key) for key in
           ("raw_begin", "raw_count", "complete", "geometries", "worlds",
            "edges", "payload_sha256", "header_sha256", "marker_sha256")):
        raise ValueError("fresh merge differs from preserved merged reference")

    artifact_records = merge_certificate.get("artifacts")
    if not isinstance(artifact_records, list) or len(artifact_records) != 6:
        raise ValueError("merge artifact inventory mismatch")
    by_name = {record.get("path"): record for record in artifact_records
               if isinstance(record, dict)}
    if set(by_name) != {f"kjesterghostk{suffix}" for suffix in SUFFIXES}:
        raise ValueError("merge artifact name inventory mismatch")
    component_hashes: dict[str, dict[str, Any]] = {}
    for name, record in sorted(by_name.items()):
        path = args.output_dir / name
        extent = path.stat().st_size
        digest = sha256_file(path)
        if extent != record.get("bytes") or digest != record.get("sha256"):
            raise ValueError(f"fresh component changed: {name}")
        component_hashes[name] = {"bytes": extent, "sha256": digest}

    native = merge_certificate.get("native_certificate")
    if not isinstance(native, dict) or any(native.get(key) != value
                                           for key, value in EXPECTED.items()):
        raise ValueError("merge aggregate totals mismatch")
    if (native.get("payload") != merged["payload_sha256"] or
            native.get("codec_checks") != 4 *
            (native.get("worlds", 0) + native.get("canonical", 0)) or
            native.get("action_checks") != 4 * native.get("worlds", 0) or
            native.get("transition_checks") != 4 * native.get("edges", 0) or
            native.get("symmetry_checks") != 4 *
            native.get("canonical", 0) or
            any(native.get(field) != 0 for field in
                ("codec_residual", "action_residual", "decision_residual",
                 "transition_residual", "symmetry_residual"))):
        raise ValueError("merge native conservation/residual mismatch")

    evidence = [args.audit_source, args.verifier, args.rebind_source,
                args.bundle_manifest, args.rebind_manifest,
                args.verification_certificate, args.merge_runner,
                args.merge_certificate, args.output_dir / "merge.log"]
    evidence.extend(args.transition_tree /
                    f"{record['prefix']}{suffix}"
                    for record in [*shards, merged]
                    for suffix in (".header", ".verified"))
    certificate = {
        "schema": SCHEMA, "status": STATUS,
        "source_sha256": source, "model_sha256": model,
        "observation_sha256": observation,
        "audit_sha256": args.audit_sha256,
        "verifier_sha256": args.verifier_sha256,
        "rebind_source_sha256": args.rebind_source_sha256,
        "rebind_manifest_sha256": sha256_file(args.rebind_manifest),
        "verification_certificate_sha256":
            args.verification_certificate_sha256,
        "merge_runner_sha256": args.merge_runner_sha256,
        "merge_certificate_sha256": sha256_file(args.merge_certificate),
        "merge_log_sha256": sha256_file(args.output_dir / "merge.log"),
        "rebound_prefixes": len(rebound_records),
        "input_shards": len(shards), "totals": EXPECTED,
        "payload_sha256": fresh["payload_sha256"],
        "fresh_prefix": fresh, "components": component_hashes,
        "residuals": {"inventory": 0, "binding": 0, "rebound": 0,
                      "component": 0, "payload": 0, "aggregate": 0,
                      "conservation": 0},
    }
    write_exclusive_json(args.certificate_output, certificate)
    evidence.append(args.certificate_output)
    artifacts = []
    for path in sorted({path.resolve() for path in evidence}, key=str):
        artifacts.append({"path": regular_relative(args.root, path),
                          "bytes": path.stat().st_size,
                          "sha256": sha256_file(path)})
    manifest = {
        "schema": SCHEMA, "status": STATUS,
        "source_sha256": source, "model_sha256": model,
        "observation_sha256": observation,
        "certificate_sha256": sha256_file(args.certificate_output),
        "payload_sha256": fresh["payload_sha256"],
        "artifacts": artifacts,
    }
    write_exclusive_json(args.artifact_manifest, manifest)
    return {"status": STATUS,
            "certificate_sha256": manifest["certificate_sha256"],
            "payload_sha256": fresh["payload_sha256"],
            "artifacts": len(artifacts), **EXPECTED}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--audit-source", type=Path, required=True)
    parser.add_argument("--audit-sha256", required=True)
    parser.add_argument("--verifier", type=Path, required=True)
    parser.add_argument("--verifier-sha256", required=True)
    parser.add_argument("--rebind-source", type=Path, required=True)
    parser.add_argument("--rebind-source-sha256", required=True)
    parser.add_argument("--bundle-manifest", type=Path, required=True)
    parser.add_argument("--rebind-manifest", type=Path, required=True)
    parser.add_argument("--transition-tree", type=Path, required=True)
    parser.add_argument("--verification-certificate", type=Path, required=True)
    parser.add_argument("--verification-certificate-sha256", required=True)
    parser.add_argument("--merge-runner", type=Path, required=True)
    parser.add_argument("--merge-runner-sha256", required=True)
    parser.add_argument("--merge-certificate", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--certificate-output", type=Path, required=True)
    parser.add_argument("--artifact-manifest", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(certify(args), sort_keys=True))


if __name__ == "__main__":
    main()
