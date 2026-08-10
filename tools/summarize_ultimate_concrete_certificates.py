#!/usr/bin/env python3
"""Validate restored concrete-table certificates and report exact sizes/WDL."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Iterable


SCHEMA = "ultimate-concrete-k2-s3-certificate-v2"
PRESERVED = "head-download-full-sha-archive-restore-verified"


@dataclass(frozen=True)
class Artifact:
    filename: str
    states: int
    win: int
    loss: int
    draw: int
    raw_bytes: int
    compressed_bytes: int
    output_sha256: str
    archive_sha256: str
    s3_bucket: str
    s3_key: str
    version_id: str


@dataclass(frozen=True)
class Report:
    schema: str
    generator_model_sha256: str
    inventory_sha256: str
    certificates: int
    artifacts: tuple[Artifact, ...]
    states: int
    win: int
    loss: int
    draw: int
    raw_bytes: int
    compressed_bytes: int


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def integer(document: dict[str, Any], key: str, context: str) -> int:
    value = document.get(key)
    require(type(value) is int and value >= 0, f"{context}: invalid {key}")
    return value


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def certificate_paths(root: Path) -> tuple[Path, ...]:
    paths = tuple(sorted(root.glob("sha256/*/wave-certificate.json")))
    if not paths and root.name == "sha256":
        paths = tuple(sorted(root.glob("*/wave-certificate.json")))
    require(bool(paths), f"{root}: no restored wave certificates found")
    return paths


def load_report(root: Path) -> Report:
    artifacts: list[Artifact] = []
    filenames: set[str] = set()
    models: set[str] = set()
    inventories: set[str] = set()
    paths = certificate_paths(root)
    for path in paths:
        expected = path.parent.name
        require(len(expected) == 64 and digest(path) == expected,
                f"{path}: certificate is not stored under its SHA-256")
        document = json.loads(path.read_text())
        require(document.get("schema") == SCHEMA,
                f"{path}: unsupported certificate schema")
        require(document.get("status") == PRESERVED,
                f"{path}: S3 preservation drill is incomplete")
        require(document.get("local_outputs_retained") is True and
                document.get("local_scratch_retained") is True,
                f"{path}: local recovery material was not retained")
        model = document.get("generator_model_sha256")
        inventory = document.get("inventory_sha256")
        require(isinstance(model, str) and len(model) == 64,
                f"{path}: invalid generator model binding")
        require(isinstance(inventory, str) and len(inventory) == 64,
                f"{path}: invalid inventory binding")
        models.add(model)
        inventories.add(inventory)
        completed = document.get("completed")
        require(isinstance(completed, list) and completed,
                f"{path}: empty completion list")
        for entry in completed:
            require(isinstance(entry, dict) and
                    entry.get("status") == "generated-preserved",
                    f"{path}: artifact was not generated and preserved")
            filename = entry.get("filename")
            require(isinstance(filename, str) and filename.endswith(".uftb") and
                    filename not in filenames,
                    f"{path}: invalid or duplicate output filename")
            filenames.add(filename)
            output = entry.get("output")
            archive = entry.get("archive")
            s3 = entry.get("s3")
            require(isinstance(output, dict) and isinstance(archive, dict) and
                    isinstance(s3, dict), f"{path}: incomplete artifact record")
            states = integer(output, "states", str(path))
            win = integer(output, "win", str(path))
            loss = integer(output, "loss", str(path))
            draw = integer(output, "draw", str(path))
            exceptions = integer(output, "exceptions", str(path))
            require(exceptions == 0 and states == win + loss + draw,
                    f"{path}: W/L/D conservation failed for {filename}")
            require(integer(output, "verification_residual", str(path)) == 0,
                    f"{path}: nonzero Bellman verification residual")
            output_sha = output.get("sha256")
            archive_sha = archive.get("sha256")
            require(isinstance(output_sha, str) and len(output_sha) == 64 and
                    isinstance(archive_sha, str) and len(archive_sha) == 64,
                    f"{path}: invalid artifact digest")
            archive_bytes = integer(archive, "bytes", str(path))
            require(s3.get("sha256") == archive_sha and
                    integer(s3, "bytes", str(path)) == archive_bytes,
                    f"{path}: S3 archive binding mismatch")
            for residual in ("head_full_sha_residual",
                             "download_full_sha_residual",
                             "archive_restore_residual"):
                require(integer(s3, residual, str(path)) == 0,
                        f"{path}: nonzero {residual}")
            bucket = s3.get("bucket")
            key = s3.get("key")
            version = s3.get("version_id")
            require(isinstance(bucket, str) and bucket and
                    isinstance(key, str) and
                    f"/sha256/{archive_sha}/" in "/" + key and
                    isinstance(version, str) and version,
                    f"{path}: S3 object is not versioned/content-addressed")
            artifacts.append(Artifact(
                filename=filename,
                states=states,
                win=win,
                loss=loss,
                draw=draw,
                raw_bytes=integer(output, "bytes", str(path)),
                compressed_bytes=archive_bytes,
                output_sha256=output_sha,
                archive_sha256=archive_sha,
                s3_bucket=bucket,
                s3_key=key,
                version_id=version,
            ))
    require(len(models) == 1, "certificates span multiple generator models")
    require(len(inventories) == 1, "certificates span multiple inventories")
    ordered = tuple(sorted(artifacts, key=lambda artifact: artifact.filename))
    return Report(
        schema="ultimate-concrete-certificate-summary-v1",
        generator_model_sha256=next(iter(models)),
        inventory_sha256=next(iter(inventories)),
        certificates=len(paths),
        artifacts=ordered,
        states=sum(artifact.states for artifact in ordered),
        win=sum(artifact.win for artifact in ordered),
        loss=sum(artifact.loss for artifact in ordered),
        draw=sum(artifact.draw for artifact in ordered),
        raw_bytes=sum(artifact.raw_bytes for artifact in ordered),
        compressed_bytes=sum(artifact.compressed_bytes for artifact in ordered),
    )


def format_bytes(value: int) -> str:
    return f"{value:,}"


def markdown(report: Report) -> str:
    lines = [
        "| Tablebase | States | W | L | D | Raw bytes | Compressed bytes |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for artifact in report.artifacts:
        lines.append(
            f"| `{artifact.filename}` | {artifact.states:,} | {artifact.win:,} | "
            f"{artifact.loss:,} | {artifact.draw:,} | "
            f"{artifact.raw_bytes:,} | {artifact.compressed_bytes:,} |"
        )
    lines.extend((
        f"| **Certified total ({len(report.artifacts)})** | "
        f"**{report.states:,}** | **{report.win:,}** | **{report.loss:,}** | "
        f"**{report.draw:,}** | **{report.raw_bytes:,}** | "
        f"**{report.compressed_bytes:,}** |",
        "",
        f"Generator model: `{report.generator_model_sha256}`  ",
        f"Inventory: `{report.inventory_sha256}`",
    ))
    return "\n".join(lines)


def json_document(report: Report) -> dict[str, Any]:
    document = asdict(report)
    document["artifacts"] = [asdict(artifact) for artifact in report.artifacts]
    return document


def main(argv: Iterable[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("certificates", type=Path,
                        help="directory containing sha256/*/wave-certificate.json")
    parser.add_argument("--format", choices=("json", "markdown"), default="markdown")
    args = parser.parse_args(argv)
    report = load_report(args.certificates)
    if args.format == "json":
        print(json.dumps(json_document(report), indent=2, sort_keys=True))
    else:
        print(markdown(report))


if __name__ == "__main__":
    main()
