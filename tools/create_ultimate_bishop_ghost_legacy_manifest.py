#!/usr/bin/env python3
"""Authenticate the legacy Bishop/Ghost result into the generic archive schema."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import tarfile


SCHEMA = "ultimate-bishop-ghost-legacy-artifact-manifest-v1"
OUTPUT = "kbishopghostk.ufiw"
PROOF = "kbishopghostk-proof.tar.gz"
OUTPUT_SHA = "kbishopghostk.ufiw.sha256"
INPUTS = "inputs.sha256"
EXPECTED_PROOF_MEMBERS = {
    "solve.log", "measure.log", "merge.log", "shards.sha256",
    "transitions.sha256", OUTPUT_SHA, "solve-started-at.txt",
    "solve-completed-at.txt", "SHARDS_COMPLETE", "MERGE_COMPLETE",
    "MEASUREMENT_COMPLETE", "SOLVE_COMPLETE",
}
SHA_LINE = re.compile(r"([0-9a-f]{64})  (.+)\Z")


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(8 * 1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def read_sha_lines(path: Path) -> list[tuple[str, str]]:
    records: list[tuple[str, str]] = []
    for line in path.read_text(encoding="ascii").splitlines():
        match = SHA_LINE.fullmatch(line)
        if not match:
            raise RuntimeError(f"malformed SHA inventory line in {path.name}")
        records.append((match.group(1), match.group(2)))
    if not records:
        raise RuntimeError(f"empty SHA inventory: {path.name}")
    return records


def require_certificate(log: str, pattern: str, label: str) -> str:
    matches = [line for line in log.splitlines() if re.search(pattern, line)]
    if len(matches) != 1:
        raise RuntimeError(f"missing or ambiguous {label} certificate")
    return matches[0]


def build_manifest(root: Path, output: Path) -> dict[str, object]:
    root = root.resolve(strict=True)
    paths = {name: root / name for name in (OUTPUT, PROOF, OUTPUT_SHA, INPUTS)}
    if any(path.is_symlink() or not path.is_file() for path in paths.values()):
        raise RuntimeError("legacy Bishop/Ghost artifact is missing or not regular")

    output_records = read_sha_lines(paths[OUTPUT_SHA])
    if len(output_records) != 1 or Path(output_records[0][1]).name != OUTPUT:
        raise RuntimeError("output SHA declaration does not bind kbishopghostk.ufiw")
    output_digest = sha256_path(paths[OUTPUT])
    if output_records[0][0] != output_digest:
        raise RuntimeError("Bishop/Ghost output SHA residual")

    inputs: list[dict[str, object]] = []
    for digest, name in read_sha_lines(paths[INPUTS]):
        source = Path(name)
        if source.is_symlink() or not source.is_file() or sha256_path(source) != digest:
            raise RuntimeError(f"input SHA residual: {name}")
        inputs.append({"path": name, "bytes": source.stat().st_size,
                       "sha256": digest})

    with tarfile.open(paths[PROOF], "r:gz") as archive:
        members = archive.getmembers()
        names = [member.name for member in members]
        if (set(names) != EXPECTED_PROOF_MEMBERS or len(names) != len(set(names)) or
                any(not member.isfile() or PurePosixPath(member.name).is_absolute() or
                    ".." in PurePosixPath(member.name).parts for member in members)):
            raise RuntimeError("legacy proof archive inventory residual")
        embedded_sha = archive.extractfile(OUTPUT_SHA)
        solve = archive.extractfile("solve.log")
        if embedded_sha is None or solve is None:
            raise RuntimeError("legacy proof archive is unreadable")
        if embedded_sha.read() != paths[OUTPUT_SHA].read_bytes():
            raise RuntimeError("embedded output SHA declaration residual")
        solve_log = solve.read().decode("utf-8")

    symbolic = require_certificate(
        solve_log,
        r"information_symbolic_certificate .*bellman_residual 0 .*"
        r"monotonicity_residual 0 .*singleton_residual 0 .*"
        r"compaction_root_residual 0 .*belief_cap none powerset_exact 1$",
        "symbolic",
    )
    summaries = [line for line in solve_log.splitlines()
                 if line.startswith("information_summary ")]
    if (len(summaries) != 2 or
            any("bellman_residual 0 rank_residual 0 belief_cap none exhaustive 1"
                not in line for line in summaries)):
        raise RuntimeError("information summary certificate residual")
    conservation = require_certificate(
        solve_log,
        r"ghost_extra_external_root_conservation .*"
        r"independent_grouping_residual 0 realization_residual 0 "
        r"conservation_residual 0$",
        "conservation",
    )
    overlay = require_certificate(
        solve_log,
        rf"information_overlay .* bytes {paths[OUTPUT].stat().st_size} .*"
        r"root_grouping_residual 0 conservation_residual 0$",
        "overlay",
    )

    artifacts = []
    for name in (OUTPUT, PROOF, OUTPUT_SHA, INPUTS):
        path = paths[name]
        artifacts.append({"path": name, "bytes": path.stat().st_size,
                          "sha256": sha256_path(path)})
    manifest: dict[str, object] = {
        "schema": SCHEMA,
        "model": "same-side-bishop-ghost-arbitrary-belief",
        "output_sha256": output_digest,
        "inputs": inputs,
        "certificates": {
            "symbolic": symbolic, "summaries": summaries,
            "conservation": conservation, "overlay": overlay,
        },
        "artifacts": artifacts,
    }
    payload = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        stream.write(payload)
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = build_manifest(args.root, args.output)
    print(json.dumps({"schema": SCHEMA, "artifacts": len(manifest["artifacts"]),
                      "manifest_sha256": sha256_path(args.output)}, sort_keys=True))


if __name__ == "__main__":
    main()
