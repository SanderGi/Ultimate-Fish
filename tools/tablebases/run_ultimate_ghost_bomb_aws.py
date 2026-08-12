#!/usr/bin/env python3
"""Run one authenticated exact Bomb/Ghost AWS bundle."""

from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path
import re

import run_ultimate_reciprocal_bishop_ghost_aws as shared


RESUME_PREFIXES = {
    "kbombghostk.uftb": Path(
        "/mnt/ultimatefish/hidden-kbombghostk-7af5dec2/resume-v2/"
        "work/transitions/kbombghostk"),
    "kbombkghost.uftb": Path(
        "/mnt/ultimatefish/hidden-kbombkghost-7af5dec2/resume-v2/"
        "work/transitions/kbombkghost"),
}


def merged_transition_is_complete(root: Path, command: list[str]) -> bool:
    return shared.merged_transition_is_complete(root, command)


def validate_manifest(manifest: dict[str, object]) -> None:
    expected_orientation = {
        "kbombghostk.uftb": "same",
        "kbombkghost.uftb": "opposing",
    }.get(manifest.get("filename"))
    implementation = manifest.get("implementation_sha256")
    commit = manifest.get("canonical_commit")
    if (manifest.get("schema") != "ultimate-bomb-ghost-aws-v3" or
            expected_orientation is None or
            manifest.get("orientation") != expected_orientation or
            not isinstance(commit, str) or len(commit) != 40 or
            any(character not in "0123456789abcdef" for character in commit) or
            not isinstance(implementation, str) or
            len(implementation) != 64 or
            any(character not in "0123456789abcdef"
                for character in implementation) or
            manifest.get("geometries") != 492_960 or
            manifest.get("shards") != 64 or
            manifest.get("shard_count_distribution") != {
                "7703": 32, "7702": 32} or
            manifest.get("parallelism") != 29):
        raise RuntimeError("invalid Bomb/Ghost manifest")
    shards = manifest.get("commands", {}).get("shards")
    if not isinstance(shards, list) or len(shards) != 64:
        raise RuntimeError("invalid Bomb/Ghost shard inventory")


def validate_transition_resume(resume: dict[str, object],
                               manifest: dict[str, object]) -> Path:
    prefix = Path(str(resume.get("source_prefix", "")))
    expected_prefix = RESUME_PREFIXES.get(str(manifest.get("filename")))
    expected_names = [f"{prefix.name}{suffix}"
                      for suffix in shared.TRANSITION_SUFFIXES]
    records = resume.get("files")
    if (resume.get("schema") != "ultimate-bomb-ghost-transition-resume-v1" or
            resume.get("filename") != manifest.get("filename") or
            resume.get("orientation") != manifest.get("orientation") or
            resume.get("model_sha256") != manifest.get("model_sha256") or
            resume.get("observation_sha256") !=
            manifest.get("observation_sha256") or
            prefix != expected_prefix or
            not isinstance(records, list) or len(records) != 7):
        raise RuntimeError("invalid Bomb/Ghost transition resume manifest")
    expected = set(expected_names + ["merge.log"])
    seen = set()
    for record in records:
        if not isinstance(record, dict) or set(record) != {
                "name", "bytes", "sha256"}:
            raise RuntimeError("invalid Bomb/Ghost transition record")
        name = str(record["name"])
        digest = str(record["sha256"])
        if (name not in expected or name in seen or int(record["bytes"]) <= 0 or
                len(digest) != 64 or any(character not in
                    "0123456789abcdef" for character in digest)):
            raise RuntimeError("invalid Bomb/Ghost transition record")
        path = ((prefix.parent / name) if name != "merge.log" else
                prefix.parent.parent / "logs" / name)
        if (not path.is_file() or path.stat().st_size != int(record["bytes"]) or
                shared.sha256(path) != digest):
            raise RuntimeError(f"Bomb/Ghost transition authentication failed: {path}")
        seen.add(name)
    if seen != expected:
        raise RuntimeError("Bomb/Ghost transition inventory residual")
    return prefix


def resumed_measure_command(command: list[str], prefix: Path) -> list[str]:
    result = copy.copy(command)
    try:
        index = result.index("--transition-prefix") + 1
    except (ValueError, IndexError) as error:
        raise RuntimeError("Bomb/Ghost measure lacks transition prefix") from error
    result[index] = str(prefix)
    return result


def validate_completed_setup(root: Path,
                             manifest: dict[str, object]) -> None:
    """Authenticate the built binary and finished sizing pass before resuming."""
    logs = {
        "work/logs/build.log": root / "work/logs/build.log",
        "work/logs/self-test.log": root / "work/logs/self-test.log",
        "work/logs/measure.log": root / "work/logs/measure.log",
    }
    artifact_path = root / "work/artifact-manifest.json"
    artifact = json.loads(artifact_path.read_text())
    records = artifact.get("artifacts")
    matches = {
        name: next((record for record in records
                    if isinstance(record, dict) and
                    record.get("path") == name), None)
        for name in logs
    } if isinstance(records, list) else {}
    if (artifact.get("filename") != manifest.get("filename") or
            artifact.get("model_sha256") != manifest.get("model_sha256") or
            any(not isinstance(matches.get(name), dict) or
                matches[name].get("bytes") != path.stat().st_size or
                matches[name].get("sha256") != shared.sha256(path)
                for name, path in logs.items())):
        raise RuntimeError("completed Bomb/Ghost setup binding residual")
    executable = root / "ultimate_ghost_bomb_information_tablebase"
    if (not executable.is_file() or
            not os.access(executable, os.X_OK) or
            executable.stat().st_mtime_ns > logs["work/logs/measure.log"].stat().st_mtime_ns):
        raise RuntimeError("completed Bomb/Ghost executable residual")
    self_test = logs["work/logs/self-test.log"].read_text()
    if ("ghost_bomb_exact_self_test codec_states 151831680 remap_residual 0 "
            "belief_cap none" not in self_test or
            "bomb_ghost_resource geometries 492960 concrete_worlds 37957920 "
            not in self_test):
        raise RuntimeError("completed Bomb/Ghost self-test proof residual")
    text = logs["work/logs/measure.log"].read_text()
    measurements = re.findall(
        r"^reciprocal_ghost_extra_measurement iterations 1 bdd_nodes [1-9][0-9]* "
        r"peak_rss_bytes [1-9][0-9]* proof_complete 0 overlay_written 0$",
        text, flags=re.MULTILINE)
    certificates = re.findall(
        r"^bomb_ghost_certificate dual_force_residual 0 structural_residual 0 "
        r"singleton_residual 0 source_remap_residual 0 .*$",
        text, flags=re.MULTILINE)
    if len(measurements) != 1 or len(certificates) != 1:
        raise RuntimeError("completed Bomb/Ghost measurement proof residual")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path,
                        default=Path("bundle-manifest.json"))
    parser.add_argument("--full", action="store_true",
                        help="continue after measurement to the exact solve")
    parser.add_argument(
        "--resume-completed-measurement", action="store_true",
        help="authenticate and skip an already completed one-iteration sizing pass")
    parser.add_argument("--transition-resume-manifest", type=Path,
                        required=True,
                        help="authenticate a preserved read-only merged graph")
    args = parser.parse_args()
    if args.resume_completed_measurement and not args.full:
        parser.error("--resume-completed-measurement requires --full")
    root = args.manifest.resolve().parent
    manifest = json.loads(args.manifest.read_text())
    validate_manifest(manifest)
    shared.verify_inputs(root, manifest)
    shared.prepare_workdirs(root)
    work = root / "work"
    commands = manifest["commands"]
    if args.resume_completed_measurement:
        validate_completed_setup(root, manifest)
        shared.run(commands["self_test"], root,
                   work / "logs" / "resume-self-test.log")
    else:
        shared.run(commands["build"], root, work / "logs" / "build.log")
        shared.run(commands["self_test"], root,
                   work / "logs" / "self-test.log")
    resume_path = args.transition_resume_manifest.resolve()
    resume = json.loads(resume_path.read_text())
    prefix = validate_transition_resume(resume, manifest)
    if not args.resume_completed_measurement:
        shared.run(resumed_measure_command(commands["measure"], prefix), root,
                   work / "logs" / "measure.log")
    # Reauthenticate after the expensive read to prove the preserved graph was
    # neither regenerated nor mutated by the corrected measurement pass.
    validate_transition_resume(resume, manifest)
    if args.full:
        shared.run(resumed_measure_command(commands["solve"], prefix), root,
                   work / "logs" / "solve.log")
        validate_transition_resume(resume, manifest)
    shared.artifact_manifest(root, manifest)
    artifact_path = work / "artifact-manifest.json"
    artifact = json.loads(artifact_path.read_text())
    for key in ("schema", "filename", "orientation",
                "implementation_sha256",
                "normalized_source_sha256", "lower_bomb_full_sha256",
                "lower_bomb_source_sha256",
                "lower_bomb_model_sha256"):
        artifact[key] = manifest[key]
    artifact["transition_resume_manifest_sha256"] = shared.sha256(resume_path)
    artifact["transition_resume"] = resume
    artifact_path.write_text(
        json.dumps(artifact, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
