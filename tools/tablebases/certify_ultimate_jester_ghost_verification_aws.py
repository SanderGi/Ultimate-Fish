#!/usr/bin/env python3
"""Certify and inventory completed Jester/Ghost rebound shard evidence.

All verification inputs are read-only.  The certificate and artifact manifest
are created only after all 80 native logs, resume markers, rebound bindings,
and aggregate totals exactly reproduce the completed verification summary.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
from types import ModuleType
from typing import Any


SCHEMA = "ultimate-jester-ghost-verification-evidence-v1"
STATUS = "all-80-rebound-shards-evidence-certified"


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


def load_verifier(path: Path, expected_sha256: str) -> ModuleType:
    if not path.is_file() or sha256_file(path) != expected_sha256:
        raise ValueError("verifier module SHA-256 mismatch")
    spec = importlib.util.spec_from_file_location("jg_rebound_verifier", path)
    if spec is None or spec.loader is None:
        raise ValueError("cannot import verifier module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def exact_relative(root: Path, path: Path) -> str:
    root = root.resolve(strict=True)
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"evidence artifact is not a regular file: {path}")
    try:
        return path.resolve(strict=True).relative_to(root).as_posix()
    except ValueError as error:
        raise ValueError(f"evidence artifact escapes root: {path}") from error


def write_exclusive_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())


def certify(args: argparse.Namespace) -> dict[str, Any]:
    if (not args.audit_source.is_file() or
            sha256_file(args.audit_source) != args.audit_sha256):
        raise ValueError("audit source SHA-256 mismatch")
    verifier = load_verifier(args.verifier, args.verifier_sha256)
    bundle = verifier.load_json(args.bundle_manifest)
    rebind = verifier.load_json(args.rebind_manifest)
    shards, merged = verifier.inventory(bundle, rebind)
    source = bundle["source_sha256"]
    model = bundle["model_sha256"]
    observation = bundle["observation_sha256"]
    if (not args.binary.is_file() or
            sha256_file(args.binary) != args.binary_sha256):
        raise ValueError("native binary SHA-256 mismatch")
    if args.transition_tree.resolve() != Path(
            rebind["destination_tree"]).resolve():
        raise ValueError("transition tree/rebind destination mismatch")

    summary = load_json(args.state_dir / "verification-summary.json")
    expected_marker_names = {f"{record['prefix']}.json" for record in shards}
    actual_marker_names = {path.name for path in
                           (args.state_dir / "completed").glob("*.json")}
    if actual_marker_names != expected_marker_names:
        raise ValueError("completion marker inventory is not exactly 80 shards")

    marker_digests: dict[str, str] = {}
    referenced_logs: set[Path] = set()
    results: list[dict[str, Any]] = []
    evidence: list[Path] = []
    for record in shards:
        name = record["prefix"]
        header = args.transition_tree / f"{name}.header"
        native_marker = args.transition_tree / f"{name}.verified"
        if (not header.is_file() or not native_marker.is_file() or
                sha256_file(header) != record["header_sha256"] or
                sha256_file(native_marker) != record["marker_sha256"]):
            raise ValueError(f"rebound input authentication failed: {name}")
        marker_path = args.state_dir / "completed" / f"{name}.json"
        marker = load_json(marker_path)
        binding = verifier.marker_binding(
            record, args.binary_sha256, source, model, observation)
        if any(marker.get(key) != value for key, value in binding.items()):
            raise ValueError(f"completion marker binding mismatch: {name}")
        if marker.get("status") != "exhaustive-native-regeneration-certified":
            raise ValueError(f"completion marker status mismatch: {name}")
        log_relative = Path(str(marker.get("log", "")))
        if (log_relative.is_absolute() or ".." in log_relative.parts or
                log_relative.parts[:1] != ("logs",)):
            raise ValueError(f"unsafe completion log path: {name}")
        log_path = args.state_dir / log_relative
        if (not log_path.is_file() or
                sha256_file(log_path) != marker.get("log_sha256")):
            raise ValueError(f"completion log SHA-256 mismatch: {name}")
        certificate = verifier.parse_native_certificate(
            log_path.read_text(errors="replace"), record)
        if marker.get("certificate") != certificate:
            raise ValueError(f"completion log/certificate mismatch: {name}")
        marker_digest = sha256_file(marker_path)
        marker_digests[name] = marker_digest
        referenced_logs.add(log_path.resolve())
        results.append({
            "prefix": name,
            "raw_begin": record["raw_begin"],
            "raw_count": record["raw_count"],
            "header_sha256": record["header_sha256"],
            "native_marker_sha256": record["marker_sha256"],
            "payload_sha256": record["payload_sha256"],
            "completion_marker_sha256": marker_digest,
            "log": log_relative.as_posix(),
            "log_sha256": marker["log_sha256"],
            "certificate": certificate,
        })
        evidence.extend((header, native_marker, marker_path, log_path))

    actual_logs = {path.resolve() for path in
                   (args.state_dir / "logs").glob("*.log")}
    if actual_logs != referenced_logs:
        raise ValueError("verification log inventory is not exactly 80 shards")
    results.sort(key=lambda result: result["raw_begin"])
    totals = {field: sum(result["certificate"][field] for result in results)
              for field in ("raw", "canonical", "worlds", "live", "actions",
                            "observations", "edges", "codec_checks",
                            "action_checks", "decision_checks",
                            "transition_checks", "symmetry_checks")}
    expected_summary = {
        "schema": verifier.SCHEMA,
        "status": "all-80-rebound-shards-exhaustively-regenerated",
        "shards": len(results),
        "parallelism": verifier.EXPECTED_PARALLELISM,
        "binary_sha256": args.binary_sha256,
        "source_sha256": source,
        "model_sha256": model,
        "observation_sha256": observation,
        "rebind_manifest_sha256": sha256_file(args.rebind_manifest),
        "totals": totals,
        "completion_marker_sha256": marker_digests,
    }
    if summary != expected_summary:
        raise ValueError("verification summary does not exactly reproduce evidence")
    if (totals["raw"] != verifier.RAW_DOMAIN or
            totals["canonical"] != merged["geometries"] or
            totals["worlds"] != merged["worlds"] or
            totals["edges"] != merged["edges"]):
        raise ValueError("verification aggregate/merged reference mismatch")

    service_log = args.state_dir / "service.log"
    if not service_log.is_file():
        raise ValueError("verification service log is missing")
    evidence.extend((args.state_dir / "verification-summary.json",
                     args.rebind_manifest, args.bundle_manifest,
                     args.verifier, args.binary, service_log,
                     args.audit_source))
    certificate = {
        "schema": SCHEMA,
        "status": STATUS,
        "source_sha256": source,
        "model_sha256": model,
        "observation_sha256": observation,
        "binary_sha256": args.binary_sha256,
        "verifier_sha256": args.verifier_sha256,
        "audit_sha256": args.audit_sha256,
        "rebind_manifest_sha256": sha256_file(args.rebind_manifest),
        "verification_summary_sha256": sha256_file(
            args.state_dir / "verification-summary.json"),
        "shards": len(results),
        "logs": len(referenced_logs),
        "totals": totals,
        "merged_reference": {
            key: merged[key] for key in
            ("prefix", "raw_begin", "raw_count", "geometries", "worlds",
             "edges", "payload_sha256", "header_sha256", "marker_sha256")
        },
        "records": results,
        "residuals": {
            "inventory": 0, "binding": 0, "log": 0, "native": 0,
            "summary": 0, "aggregate": 0,
        },
    }
    write_exclusive_json(args.certificate_output, certificate)
    evidence.append(args.certificate_output)
    unique_evidence = sorted({path.resolve() for path in evidence}, key=str)
    artifacts = [{
        "path": exact_relative(args.root, path),
        "bytes": path.stat().st_size,
        "sha256": sha256_file(path),
    } for path in unique_evidence]
    artifact_manifest = {
        "schema": SCHEMA,
        "status": STATUS,
        "source_sha256": source,
        "model_sha256": model,
        "observation_sha256": observation,
        "certificate_sha256": sha256_file(args.certificate_output),
        "verification_summary_sha256": certificate[
            "verification_summary_sha256"],
        "artifacts": artifacts,
    }
    write_exclusive_json(args.artifact_manifest, artifact_manifest)
    return {
        "status": STATUS,
        "certificate_sha256": artifact_manifest["certificate_sha256"],
        "verification_summary_sha256": certificate[
            "verification_summary_sha256"],
        "shards": len(results),
        "artifacts": len(artifacts),
        "totals": totals,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--audit-source", type=Path, required=True)
    parser.add_argument("--audit-sha256", required=True)
    parser.add_argument("--verifier", type=Path, required=True)
    parser.add_argument("--verifier-sha256", required=True)
    parser.add_argument("--bundle-manifest", type=Path, required=True)
    parser.add_argument("--rebind-manifest", type=Path, required=True)
    parser.add_argument("--transition-tree", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--binary-sha256", required=True)
    parser.add_argument("--state-dir", type=Path, required=True)
    parser.add_argument("--certificate-output", type=Path, required=True)
    parser.add_argument("--artifact-manifest", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(certify(args), sort_keys=True))


if __name__ == "__main__":
    main()
