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
RUN_PLAN_SCHEMA = "ultimate-jester-ghost-measurement-run-plan-v1"


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


def compatibility_inputs(args: argparse.Namespace,
                         manifest: dict[str, Any]) -> tuple[str, str, str]:
    if args.lower_compatibility_certificate is None:
        table = file_record(manifest, "tablebases/kjesterk.uftb")
        overlay = file_record(manifest, "tablebases/kjesterk.ufiw")
        authenticate(args.lower_jester_table, table, "tablebases/kjesterk.uftb")
        authenticate(args.lower_jester_overlay, overlay,
                     "tablebases/kjesterk.ufiw")
        return str(table["sha256"]), str(overlay["sha256"]), ""
    certificate = load_json(args.lower_compatibility_certificate)
    certificate_sha = sha256_file(args.lower_compatibility_certificate)
    old_table = file_record(manifest, "tablebases/kjesterk.uftb")
    if (certificate_sha != args.lower_compatibility_certificate_sha256 or
            certificate.get("schema") !=
            "ultimate-jester-ghost-lower-jester-compatibility-v1" or
            certificate.get("status") !=
            "v4-to-v5-header-only-overlay-rebound" or
            certificate.get("tool_sha256") !=
            args.lower_compatibility_tool_sha256 or
            certificate.get("lower_model_sha256") !=
            args.lower_jester_model_sha256 or
            certificate.get("old_table", {}).get("sha256") !=
            old_table["sha256"] or
            certificate.get("old_overlay", {}).get("sha256") !=
            manifest["lower_jester_overlay_sha256"] or
            certificate.get("residuals") != {
                "table_layout": 0, "table_payload": 0,
                "overlay_layout": 0, "overlay_flags": 0,
                "header_diff": 0, "binding": 0}):
        raise ValueError("lower Jester compatibility certificate mismatch")
    table = certificate.get("new_table")
    overlay = certificate.get("new_overlay")
    if not isinstance(table, dict) or not isinstance(overlay, dict):
        raise ValueError("lower Jester compatibility artifacts missing")
    authenticate(args.lower_jester_table, table, "compatible lower Jester table")
    authenticate(args.lower_jester_overlay, overlay,
                 "compatible lower Jester overlay")
    return str(table["sha256"]), str(overlay["sha256"]), certificate_sha


def authenticate_binary_compatibility(args: argparse.Namespace,
                                      manifest: dict[str, Any]) -> str:
    if args.binary_compatibility_certificate is None:
        return ""
    certificate = load_json(args.binary_compatibility_certificate)
    digest = sha256_file(args.binary_compatibility_certificate)
    if (digest != args.binary_compatibility_certificate_sha256 or
            certificate.get("schema") !=
            "ultimate-jester-ghost-measurement-binary-compatibility-v1" or
            certificate.get("status") !=
            "codec-loader-only-transition-semantics-unchanged" or
            certificate.get("new_binary", {}).get("sha256") !=
            args.binary_sha256 or
            certificate.get("source_sha256") != manifest["source_sha256"] or
            certificate.get("semantic_transition_model_sha256") !=
            manifest["model_sha256"] or
            certificate.get("observation_sha256") !=
            manifest["observation_sha256"] or
            (not isinstance(certificate.get("transition_semantics_sha256"),
                            str) or
             not re.fullmatch(r"[0-9a-f]{64}",
                              certificate["transition_semantics_sha256"])) or
            certificate.get("residuals") != {
                "bundle": 0, "loader_patch": 0, "build": 0,
                "selftest": 0, "transition_semantics": 0}):
        raise ValueError("measurement binary compatibility mismatch")
    return digest


def authenticate_production_preflight(args: argparse.Namespace) -> str:
    digest = sha256_file(args.production_preflight_log)
    lines = args.production_preflight_log.read_text().splitlines()
    if digest != args.production_preflight_log_sha256 or len(lines) != 1:
        raise ValueError("production input preflight log mismatch")
    line = lines[0]
    if not line.startswith("jester_ghost_solve_input_preflight "):
        raise ValueError("production input preflight certificate missing")
    fields = dict(re.findall(r"([a-z_]+) ([0-9]+)", line))
    expected = {
        "canonical": "9612720", "worlds": "37957920", "admitted": "1",
        "source_codec_residual": "0", "lower_jester_residual": "0",
        "lower_ghost_residual": "0", "transition_residual": "0",
    }
    if any(fields.get(name) != value for name, value in expected.items()):
        raise ValueError("production input preflight residual mismatch")
    for required in ("transition_bytes", "peak_disk_bytes",
                     "peak_resident_bytes"):
        if int(fields.get(required, "0")) <= 0:
            raise ValueError("production input preflight estimate missing")
    return digest


def write_exclusive_json(path: Path, value: object) -> None:
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())


def prepare_measurement_work(work: Path, plan: dict[str, Any]) -> Path:
    """Create or authenticate a resumable measurement directory.

    A retry is admitted only when the exact plan was durably recorded before
    the first native process started.  This prevents an old scratch prefix
    from being interpreted under different code, inputs, or resource gates.
    """
    plan_path = work / "measurement-run-plan.json"
    if work.exists():
        if not work.is_dir() or not plan_path.is_file():
            raise ValueError("measurement work lacks an authenticated run plan")
        if load_json(plan_path) != plan:
            raise ValueError("measurement run plan changed across retry")
    else:
        work.mkdir(parents=True)
        write_exclusive_json(plan_path, plan)
    (work / "scratch").mkdir(exist_ok=True)
    allowed_names = {
        "measurement-run-plan.json", "measurement.log",
        "measurement-run-result.json", "measurement-certificate.json",
    }
    unexpected = []
    for path in work.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(work)
        if (str(relative).startswith("scratch/kjesterghostk") or
                relative.name in allowed_names or
                (relative.parent == Path(".") and
                 re.fullmatch(r"measurement\.(?:attempt|prior)-[0-9]{4}\.log",
                              relative.name))):
            continue
        unexpected.append(relative)
    if unexpected:
        raise ValueError(
            "measurement work contains unexpected files: " +
            ", ".join(map(str, unexpected[:4])))
    return plan_path


def next_measurement_attempt(work: Path) -> Path:
    for attempt in range(1, 10_000):
        path = work / f"measurement.attempt-{attempt:04d}.log"
        if not path.exists():
            return path
    raise RuntimeError("too many retained measurement attempts")


def publish_measurement_log(attempt: Path, canonical: Path) -> None:
    if canonical.exists():
        for number in range(1, 10_000):
            prior = canonical.with_name(
                f"measurement.prior-{number:04d}.log")
            if not prior.exists():
                canonical.replace(prior)
                break
        else:
            raise RuntimeError("too many retained successful measurements")
    attempt.replace(canonical)


def recover_measurement_result(work: Path,
                               plan_sha256: str) -> dict[str, Any] | None:
    result_path = work / "measurement-run-result.json"
    if not result_path.exists():
        return None
    result = load_json(result_path)
    attempt_name = result.get("attempt_log")
    if (result.get("schema") !=
            "ultimate-jester-ghost-measurement-run-result-v1" or
            result.get("run_plan_sha256") != plan_sha256 or
            result.get("exit_code") != 0 or
            not isinstance(result.get("peak_resident_bytes"), int) or
            result["peak_resident_bytes"] < 0 or
            not isinstance(attempt_name, str) or
            not re.fullmatch(r"measurement\.attempt-[0-9]{4}\.log",
                             attempt_name) or
            not isinstance(result.get("log_sha256"), str)):
        raise ValueError("measurement run result is inexact")
    canonical = work / "measurement.log"
    attempt = work / attempt_name
    if canonical.is_file():
        if sha256_file(canonical) != result["log_sha256"]:
            raise ValueError("published measurement log changed")
    elif attempt.is_file() and sha256_file(attempt) == result["log_sha256"]:
        publish_measurement_log(attempt, canonical)
    else:
        raise ValueError("successful measurement log is missing")
    return result


def completed_measurement(work: Path, plan_sha256: str) -> dict[str, Any] | None:
    certificate_path = work / "measurement-certificate.json"
    if not certificate_path.exists():
        return None
    certificate = load_json(certificate_path)
    log = work / "measurement.log"
    run_result = recover_measurement_result(work, plan_sha256)
    if (run_result is None or
            certificate.get("schema") != SCHEMA or
            certificate.get("status") != STATUS or
            certificate.get("run_plan_sha256") != plan_sha256 or
            certificate.get("peak_resident_bytes") !=
            run_result["peak_resident_bytes"] or
            not log.is_file() or
            certificate.get("measurement") != parse_log(log) or
            certificate.get("residuals") != {
                "binding": 0, "transition": 0, "measurement": 0,
                "proof_output": 0, "resource": 0}):
        raise ValueError("existing measurement certificate is inexact")
    return certificate


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


def validate_upper_bdd_gates(max_nodes: int, unique_slots: int) -> None:
    if (max_nodes <= 0 or unique_slots <= 0 or
            (unique_slots & (unique_slots - 1))):
        raise ValueError("invalid upper ProductRobdd measurement gates")


def run(args: argparse.Namespace) -> dict[str, Any]:
    validate_upper_bdd_gates(
        args.bdd_max_upper_nodes, args.bdd_upper_unique_slots)
    if (not args.runner_source.is_file() or
            sha256_file(args.runner_source) != args.runner_sha256):
        raise ValueError("measurement runner SHA-256 mismatch")
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
    binary_compatibility_sha = authenticate_binary_compatibility(
        args, manifest)
    production_preflight_sha = authenticate_production_preflight(args)
    for suffix, record in merge_evidence["components"].items():
        path = args.transition_prefix.parent / suffix
        if (not path.is_file() or path.stat().st_size != record["bytes"] or
                sha256_file(path) != record["sha256"]):
            raise ValueError(f"certified transition component changed: {suffix}")
    inputs = {
        "tablebases/kjesterghostk.uftb": args.source_table,
        "tablebases/kghostk.ufgm": args.lower_ghost_sidecar,
    }
    for relative, path in inputs.items():
        authenticate(path, file_record(manifest, relative), relative)
    lower_table_sha, lower_overlay_sha, compatibility_sha = \
        compatibility_inputs(args, manifest)

    scratch = args.work / "scratch/kjesterghostk"
    log = args.work / "measurement.log"
    command = [
        str(args.binary), "--measure", "1",
        "--transition-prefix", str(args.transition_prefix),
        "--input", str(args.source_table),
        "--lower-jester-table", str(args.lower_jester_table),
        "--lower-jester-overlay", str(args.lower_jester_overlay),
        "--lower-jester-model-sha256", args.lower_jester_model_sha256,
        "--lower-jester-overlay-sha256",
        lower_overlay_sha,
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
        "--bdd-max-upper-nodes", str(args.bdd_max_upper_nodes),
        "--bdd-upper-unique-slots", str(args.bdd_upper_unique_slots),
    ]
    plan = {
        "schema": RUN_PLAN_SCHEMA,
        "command": command,
        "runner_sha256": args.runner_sha256,
        "binary_sha256": args.binary_sha256,
        "binary_compatibility_certificate_sha256":
            binary_compatibility_sha,
        "production_preflight_log_sha256": production_preflight_sha,
        "merge_evidence_sha256": args.merge_evidence_sha256,
        "lower_compatibility_certificate_sha256": compatibility_sha,
        "lower_jester_table_sha256": lower_table_sha,
        "lower_jester_overlay_sha256": lower_overlay_sha,
        "transition_payload_sha256": merge_evidence["payload_sha256"],
        "gates": {
            "maximum_disk_bytes": args.maximum_disk_bytes,
            "maximum_resident_bytes": args.maximum_resident_bytes,
            "minimum_free_bytes": args.minimum_free_bytes,
            "bdd_max_upper_nodes": args.bdd_max_upper_nodes,
            "bdd_upper_unique_slots": args.bdd_upper_unique_slots,
        },
    }
    plan_path = prepare_measurement_work(args.work, plan)
    plan_sha = sha256_file(plan_path)
    completed = completed_measurement(args.work, plan_sha)
    if completed is not None:
        return completed

    run_result = recover_measurement_result(args.work, plan_sha)
    if run_result is None:
        peak_rss = 0
        attempt_log = next_measurement_attempt(args.work)
        with attempt_log.open("xb") as output:
            process = subprocess.Popen(command, stdout=output,
                                       stderr=subprocess.STDOUT)
            while process.poll() is None:
                try:
                    status = Path(f"/proc/{process.pid}/status").read_text()
                    match = re.search(
                        r"^VmRSS:\s+([0-9]+) kB$", status, re.M)
                    if match:
                        peak_rss = max(
                            peak_rss, int(match.group(1)) * 1024)
                        if peak_rss > args.maximum_resident_bytes:
                            process.terminate()
                            process.wait()
                            raise RuntimeError(
                                "measurement exceeded resident gate")
                except FileNotFoundError:
                    pass
                time.sleep(1)
            if process.returncode:
                raise RuntimeError(
                    f"measurement failed ({process.returncode}); "
                    f"inspect {attempt_log}")
            output.flush()
            os.fsync(output.fileno())
        run_result = {
            "schema": "ultimate-jester-ghost-measurement-run-result-v1",
            "run_plan_sha256": plan_sha,
            "attempt_log": attempt_log.name,
            "log_sha256": sha256_file(attempt_log),
            "exit_code": 0,
            "peak_resident_bytes": peak_rss,
        }
        write_exclusive_json(
            args.work / "measurement-run-result.json", run_result)
        publish_measurement_log(attempt_log, log)
    peak_rss = run_result["peak_resident_bytes"]
    certificate = parse_log(log)
    stats = os.statvfs(args.work)
    result = {
        "schema": SCHEMA, "status": STATUS,
        "source_sha256": manifest["source_sha256"],
        "model_sha256": manifest["model_sha256"],
        "observation_sha256": manifest["observation_sha256"],
        "runner_sha256": args.runner_sha256,
        "run_plan_sha256": plan_sha,
        "binary_sha256": args.binary_sha256,
        "binary_compatibility_certificate_sha256":
            binary_compatibility_sha,
        "production_preflight_log_sha256": production_preflight_sha,
        "merge_evidence_sha256": args.merge_evidence_sha256,
        "lower_compatibility_certificate_sha256": compatibility_sha,
        "lower_jester_table_sha256": lower_table_sha,
        "lower_jester_overlay_sha256": lower_overlay_sha,
        "transition_payload_sha256": merge_evidence["payload_sha256"],
        "measurement": certificate,
        "peak_resident_bytes": peak_rss,
        "scratch_bytes": scratch_bytes(scratch),
        "free_bytes_after": stats.f_bavail * stats.f_frsize,
        "gates": {"maximum_disk_bytes": args.maximum_disk_bytes,
                  "maximum_resident_bytes": args.maximum_resident_bytes,
                  "minimum_free_bytes": args.minimum_free_bytes,
                  "bdd_max_upper_nodes": args.bdd_max_upper_nodes,
                  "bdd_upper_unique_slots": args.bdd_upper_unique_slots},
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
    parser.add_argument("--binary-compatibility-certificate", type=Path)
    parser.add_argument("--binary-compatibility-certificate-sha256",
                        default="")
    parser.add_argument("--production-preflight-log", type=Path,
                        required=True)
    parser.add_argument("--production-preflight-log-sha256", required=True)
    parser.add_argument("--transition-prefix", type=Path, required=True)
    parser.add_argument("--source-table", type=Path, required=True)
    parser.add_argument("--lower-jester-table", type=Path, required=True)
    parser.add_argument("--lower-jester-overlay", type=Path, required=True)
    parser.add_argument("--lower-jester-model-sha256", required=True)
    parser.add_argument("--lower-compatibility-certificate", type=Path)
    parser.add_argument("--lower-compatibility-certificate-sha256",
                        default="")
    parser.add_argument("--lower-compatibility-tool-sha256", default="")
    parser.add_argument("--lower-ghost-sidecar", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--maximum-disk-bytes", type=int, required=True)
    parser.add_argument("--maximum-resident-bytes", type=int, required=True)
    parser.add_argument("--minimum-free-bytes", type=int, required=True)
    parser.add_argument("--bdd-max-upper-nodes", type=int,
                        default=500_000_000)
    parser.add_argument("--bdd-upper-unique-slots", type=int,
                        default=1 << 30)
    args = parser.parse_args()
    print(json.dumps(run(args), sort_keys=True))


if __name__ == "__main__":
    main()
