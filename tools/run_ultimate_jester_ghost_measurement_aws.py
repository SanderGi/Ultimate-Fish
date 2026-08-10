#!/usr/bin/env python3
"""Run one authenticated measurement-only Jester/Ghost sweep on AWS."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
from typing import Any


SCHEMA = "ultimate-jester-ghost-measurement-v1"
STATUS = "measurement-complete-full-not-launched"


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


def file_record(manifest: dict[str, Any], relative: str) -> dict[str, Any]:
    records = {record.get("path"): record for record in
               manifest.get("files", []) if isinstance(record, dict)}
    record = records.get(relative)
    if not isinstance(record, dict):
        raise ValueError(f"bundle lacks {relative}")
    return record


def authenticate(path: Path, record: dict[str, Any], label: str) -> None:
    if (not path.is_file() or path.stat().st_size != record.get("bytes") or
            sha256_file(path) != record.get("sha256")):
        raise ValueError(f"{label} extent/SHA-256 mismatch")


def write_exclusive_json(path: Path, value: object) -> None:
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())


def scratch_bytes(prefix: Path) -> int:
    return sum(path.stat().st_size for path in prefix.parent.glob(
        f"{prefix.name}*") if path.is_file())


def parse_log(path: Path) -> dict[str, Any]:
    lines = path.read_text(errors="replace").splitlines()
    summaries = [line for line in lines
                 if line.startswith("information_summary side ")]
    symbolic = [line for line in lines
                if line.startswith("information_symbolic_certificate ")]
    artifacts = [line for line in lines
                 if line.startswith("jester_ghost_artifacts ")]
    if len(summaries) != 2 or len(symbolic) != 1 or len(artifacts) != 1:
        raise ValueError("measurement certificate lines are missing")
    if (not all(" belief_cap none exhaustive 1" in line
                for line in summaries) or
            " belief_cap none powerset_exact 1" not in symbolic[0]):
        raise ValueError("measurement exact-belief semantics mismatch")
    fields = dict(re.findall(r"([a-z_]+) ([0-9]+)", symbolic[0]))
    if (fields.get("iterations") != "1" or
            any(fields.get(name) != "0" for name in
                ("bellman_residual", "monotonicity_residual",
                 "singleton_residual"))):
        raise ValueError("measurement iteration/residual mismatch")
    return {"iterations": 1,
            "upper_nodes": int(fields["upper_nodes"]),
            "lower_nodes": int(fields["lower_nodes"]),
            "log_sha256": sha256_file(path)}


def run(args: argparse.Namespace) -> dict[str, Any]:
    if (not args.runner_source.is_file() or
            sha256_file(args.runner_source) != args.runner_sha256):
        raise ValueError("measurement runner SHA-256 mismatch")
    if args.work.exists():
        raise ValueError("fresh measurement work directory already exists")
    manifest = load_json(args.bundle_manifest)
    merge_evidence = load_json(args.merge_evidence)
    if (sha256_file(args.merge_evidence) != args.merge_evidence_sha256 or
            merge_evidence.get("status") !=
            "fresh-third-prefix-exhaustively-certified" or
            merge_evidence.get("source_sha256") !=
            manifest.get("source_sha256") or
            merge_evidence.get("model_sha256") !=
            manifest.get("model_sha256") or
            merge_evidence.get("observation_sha256") !=
            manifest.get("observation_sha256") or
            merge_evidence.get("residuals") != {
                "inventory": 0, "binding": 0, "rebound": 0,
                "component": 0, "payload": 0, "aggregate": 0,
                "conservation": 0}):
        raise ValueError("merge evidence provenance/residual mismatch")
    if (not args.binary.is_file() or
            sha256_file(args.binary) != args.binary_sha256):
        raise ValueError("measurement binary SHA-256 mismatch")
    for suffix, record in merge_evidence["components"].items():
        path = args.transition_prefix.parent / suffix
        if (not path.is_file() or path.stat().st_size != record["bytes"] or
                sha256_file(path) != record["sha256"]):
            raise ValueError(f"certified transition component changed: {suffix}")
    inputs = {
        "tablebases/kjesterghostk.uftb": args.source_table,
        "tablebases/kjesterk.uftb": args.lower_jester_table,
        "tablebases/kjesterk.ufiw": args.lower_jester_overlay,
        "tablebases/kghostk.ufgm": args.lower_ghost_sidecar,
    }
    for relative, path in inputs.items():
        authenticate(path, file_record(manifest, relative), relative)

    args.work.mkdir(parents=True)
    scratch = args.work / "scratch/kjesterghostk"
    scratch.parent.mkdir()
    log = args.work / "measurement.log"
    command = [
        str(args.binary), "--measure", "1",
        "--transition-prefix", str(args.transition_prefix),
        "--input", str(args.source_table),
        "--lower-jester-table", str(args.lower_jester_table),
        "--lower-jester-overlay", str(args.lower_jester_overlay),
        "--lower-jester-model-sha256", args.lower_jester_model_sha256,
        "--lower-jester-overlay-sha256",
        manifest["lower_jester_overlay_sha256"],
        "--lower-ghost-sidecar", str(args.lower_ghost_sidecar),
        "--lower-ghost-sidecar-sha256",
        manifest["lower_ghost_sidecar_sha256"],
        "--scratch", str(scratch),
        "--source-sha256", manifest["source_sha256"],
        "--model-sha256", manifest["model_sha256"],
        "--observation-sha256", manifest["observation_sha256"],
        "--max-disk-bytes", str(args.maximum_disk_bytes),
        "--max-resident-bytes", str(args.maximum_resident_bytes),
        "--min-free-disk-bytes", str(args.minimum_free_bytes),
    ]
    peak_rss = 0
    with log.open("xb") as output:
        process = subprocess.Popen(command, stdout=output,
                                   stderr=subprocess.STDOUT)
        while process.poll() is None:
            try:
                status = Path(f"/proc/{process.pid}/status").read_text()
                match = re.search(r"^VmRSS:\s+([0-9]+) kB$", status, re.M)
                if match:
                    peak_rss = max(peak_rss, int(match.group(1)) * 1024)
                    if peak_rss > args.maximum_resident_bytes:
                        process.terminate()
                        raise RuntimeError("measurement exceeded resident gate")
            except FileNotFoundError:
                pass
            time.sleep(1)
        if process.returncode:
            raise RuntimeError(f"measurement failed ({process.returncode})")
        output.flush()
        os.fsync(output.fileno())
    certificate = parse_log(log)
    unexpected = [path for path in args.work.rglob("*") if path.is_file() and
                  path != log and not str(path).startswith(str(scratch))]
    if unexpected:
        raise ValueError("measurement wrote unexpected proof output")
    stats = os.statvfs(args.work)
    result = {
        "schema": SCHEMA, "status": STATUS,
        "source_sha256": manifest["source_sha256"],
        "model_sha256": manifest["model_sha256"],
        "observation_sha256": manifest["observation_sha256"],
        "runner_sha256": args.runner_sha256,
        "binary_sha256": args.binary_sha256,
        "merge_evidence_sha256": args.merge_evidence_sha256,
        "transition_payload_sha256": merge_evidence["payload_sha256"],
        "measurement": certificate,
        "peak_resident_bytes": peak_rss,
        "scratch_bytes": scratch_bytes(scratch),
        "free_bytes_after": stats.f_bavail * stats.f_frsize,
        "gates": {"maximum_disk_bytes": args.maximum_disk_bytes,
                  "maximum_resident_bytes": args.maximum_resident_bytes,
                  "minimum_free_bytes": args.minimum_free_bytes},
        "full_solve_launched": False,
        "residuals": {"binding": 0, "transition": 0, "measurement": 0,
                      "proof_output": 0, "resource": 0},
    }
    write_exclusive_json(args.work / "measurement-certificate.json", result)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner-source", type=Path, required=True)
    parser.add_argument("--runner-sha256", required=True)
    parser.add_argument("--bundle-manifest", type=Path, required=True)
    parser.add_argument("--merge-evidence", type=Path, required=True)
    parser.add_argument("--merge-evidence-sha256", required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--binary-sha256", required=True)
    parser.add_argument("--transition-prefix", type=Path, required=True)
    parser.add_argument("--source-table", type=Path, required=True)
    parser.add_argument("--lower-jester-table", type=Path, required=True)
    parser.add_argument("--lower-jester-overlay", type=Path, required=True)
    parser.add_argument("--lower-jester-model-sha256", required=True)
    parser.add_argument("--lower-ghost-sidecar", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--maximum-disk-bytes", type=int, required=True)
    parser.add_argument("--maximum-resident-bytes", type=int, required=True)
    parser.add_argument("--minimum-free-bytes", type=int, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args), sort_keys=True))


if __name__ == "__main__":
    main()
