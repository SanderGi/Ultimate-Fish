#!/usr/bin/env python3
"""Continue a certified specialized Ghost measurement to its exact solve."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re

import run_ultimate_reciprocal_bishop_ghost_aws as shared


CERTIFICATES = {
    "ultimate-mage-ghost-aws-v1": "mage_ghost_certificate",
    "ultimate-dragon-ghost-aws-v1": "dragon_ghost_certificate",
    "ultimate-parasite-ghost-aws-v1": "parasite_ghost_certificate",
    "ultimate-giant-ghost-aws-v1": "giant_ghost_certificate",
    "ultimate-fisherman-ghost-aws-v1": "fisherman_ghost_certificate",
}


def artifact_records(document: dict[str, object]) -> dict[str, dict[str, object]]:
    records = document.get("artifacts")
    if not isinstance(records, list):
        raise RuntimeError("measurement artifact inventory is not a list")
    result = {}
    for record in records:
        if (not isinstance(record, dict) or
                not isinstance(record.get("path"), str)):
            raise RuntimeError("invalid measurement artifact record")
        result[str(record["path"])] = record
    return result


def authenticate_record(root: Path, records: dict[str, dict[str, object]],
                        relative: str) -> None:
    path = root / relative
    record = records.get(relative)
    if (not isinstance(record, dict) or not path.is_file() or
            record.get("bytes") != path.stat().st_size or
            record.get("sha256") != shared.sha256(path)):
        raise RuntimeError(f"measurement artifact binding residual: {path}")


def validate_completed_measurement(root: Path,
                                   manifest: dict[str, object]) -> str:
    schema = str(manifest.get("schema"))
    certificate = CERTIFICATES.get(schema)
    if certificate is None:
        raise RuntimeError(f"unsupported specialized Ghost schema: {schema}")
    artifact_path = root / "work/artifact-manifest.json"
    artifact_sha256 = shared.sha256(artifact_path)
    artifact = json.loads(artifact_path.read_text())
    if (artifact.get("source_sha256") != manifest.get("source_sha256") or
            artifact.get("model_sha256") != manifest.get("model_sha256")):
        raise RuntimeError("measurement source/model binding residual")
    records = artifact_records(artifact)
    stem = Path(str(manifest["filename"])).stem
    required = [
        "work/logs/build.log", "work/logs/self-test.log",
        "work/logs/merge.log", "work/logs/measure.log",
        *(f"work/transitions/{stem}{suffix}"
          for suffix in shared.TRANSITION_SUFFIXES),
    ]
    for relative in required:
        authenticate_record(root, records, relative)
    executable = root / str(manifest["commands"]["solve"][0])
    measure = root / "work/logs/measure.log"
    if (not executable.is_file() or not os.access(executable, os.X_OK) or
            executable.stat().st_mtime_ns > measure.stat().st_mtime_ns):
        raise RuntimeError("specialized Ghost executable residual")
    text = measure.read_text()
    measurements = re.findall(
        r"^reciprocal_ghost_extra_measurement iterations 1 "
        r"bdd_nodes [1-9][0-9]* peak_rss_bytes [1-9][0-9]* "
        r"proof_complete 0 overlay_written 0$", text, flags=re.MULTILINE)
    certificates = re.findall(
        rf"^{certificate} dual_force_residual 0 structural_residual 0 "
        r"singleton_residual 0 source_remap_residual 0 .*$",
        text, flags=re.MULTILINE)
    if len(measurements) != 1 or len(certificates) != 1:
        raise RuntimeError("specialized Ghost measurement proof residual")
    return artifact_sha256


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path,
                        default=Path("bundle-manifest.json"))
    args = parser.parse_args()
    root = args.manifest.resolve().parent
    manifest = json.loads(args.manifest.read_text())
    shared.verify_inputs(root, manifest)
    shared.prepare_workdirs(root)
    measurement_artifact_sha256 = validate_completed_measurement(
        root, manifest)
    commands = manifest["commands"]
    shared.run(commands["self_test"], root,
               root / "work/logs/resume-self-test.log")
    shared.run(commands["solve"], root, root / "work/logs/solve.log")
    shared.artifact_manifest(root, manifest)
    artifact_path = root / "work/artifact-manifest.json"
    artifact = json.loads(artifact_path.read_text())
    for key, value in manifest.items():
        if isinstance(value, (str, int, bool)):
            artifact[key] = value
    artifact["resume_measurement_artifact_sha256"] = \
        measurement_artifact_sha256
    artifact["resume_runner_sha256"] = shared.sha256(Path(__file__))
    artifact_path.write_text(
        json.dumps(artifact, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
