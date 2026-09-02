#!/usr/bin/env python3
"""Publish certified stateful Devil partitions to Hugging Face, fail closed."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CERTIFICATE = ROOT / "tablebases/ultimate-devil-stateful-class-certificate.json"
AUDITOR_SOURCE = ROOT / "tools/tablebases/audit_ultimate_devil_stateful_sidecar.cpp"
BUCKET = "ultimatefish-info-20260808-a4e679c6-831688117652"
REGION = "us-west-2"
REPO_ID = "SanderGi/Ultimate-Fish-Tablebases"
MAGIC = b"UFDSV1\0\0"
HEADER = struct.Struct("<8sIIQII")
RECORD_BYTES = 10
EXPECTED_SQUARES = (0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def remote_name(partition: dict[str, Any]) -> str:
    return f"kdevilk-{str(partition['label']).lower()}.ufds"


def validate_certificate(path: Path) -> dict[str, Any]:
    certificate = json.loads(path.read_text())
    if certificate.get("schema") != "ultimate-devil-stateful-class-certificate-v1":
        raise RuntimeError("unexpected Devil class-certificate schema")
    if certificate.get("material") != "King+Devil+causally-spawned-Minions vs King":
        raise RuntimeError("unexpected Devil class-certificate material")
    if tuple(certificate.get("fixed_squares", ())) != EXPECTED_SQUARES:
        raise RuntimeError("Devil class certificate does not cover exactly twelve squares")
    partitions = certificate.get("partitions", ())
    if tuple(int(partition["square"]) for partition in partitions) != EXPECTED_SQUARES:
        raise RuntimeError("Devil partition order/coverage mismatch")
    totals = {
        "states": sum(int(partition["states"]) for partition in partitions),
        "wins": sum(int(partition["outcomes"]["win"]) for partition in partitions),
        "losses": sum(int(partition["outcomes"]["loss"]) for partition in partitions),
        "draws": sum(int(partition["outcomes"]["draw"]) for partition in partitions),
        "max_dtw": max(int(partition["max_dtw"]) for partition in partitions),
        "conservation_residual": sum(int(partition["conservation_residual"])
                                     for partition in partitions),
    }
    if totals != certificate.get("aggregate"):
        raise RuntimeError(f"Devil class aggregate mismatch: {totals!r}")
    return certificate


def build_auditor(work_dir: Path) -> Path:
    binary = work_dir / "audit_ultimate_devil_stateful_sidecar"
    source_hash = sha256(AUDITOR_SOURCE)
    receipt = work_dir / "auditor.sha256"
    if binary.is_file() and receipt.is_file() and receipt.read_text().strip() == source_hash:
        return binary
    temporary = binary.with_suffix(".tmp")
    subprocess.run([
        "g++", "-std=c++17", "-O3", "-DNDEBUG", "-pthread", "-Wall",
        "-Wextra", "-Wpedantic", "-Werror", str(AUDITOR_SOURCE), "-o", str(temporary),
    ], check=True)
    temporary.replace(binary)
    receipt.write_text(source_hash + "\n")
    return binary


def download(partition: dict[str, Any], destination: Path, workers: int) -> None:
    square = int(partition["square"])
    key = (f"results/devil-stateful-v1/sidecars/square-{square}/sha256/"
           f"{partition['sidecar_sha256']}/devil-{square}.ufds")
    temporary = destination.with_suffix(destination.suffix + ".part")
    assembled = destination.with_suffix(destination.suffix + ".assembled")
    expected_size = HEADER.size + int(partition["states"]) * RECORD_BYTES
    if destination.is_file() and destination.stat().st_size == expected_size:
        return
    destination.unlink(missing_ok=True)
    if (temporary.is_file() and temporary.stat().st_size == expected_size and
            assembled.is_file()):
        temporary.replace(destination)
        assembled.unlink()
        return
    head = json.loads(subprocess.check_output([
        "aws", "s3api", "head-object", "--region", REGION, "--bucket", BUCKET,
        "--key", key, "--version-id", str(partition["sidecar_version_id"]),
        "--output", "json",
    ], text=True))
    if (int(head.get("ContentLength", -1)) != expected_size or
            head.get("VersionId") != partition["sidecar_version_id"]):
        raise RuntimeError(f"square {square}: version-pinned S3 HEAD mismatch")

    chunk_bytes = 512 << 20
    ranges = [(start, min(expected_size, start + chunk_bytes) - 1)
              for start in range(0, expected_size, chunk_bytes)]
    range_dir = destination.parent / f".{destination.name}.ranges"
    range_dir.mkdir(exist_ok=True)
    if not temporary.is_file() or temporary.stat().st_size != expected_size:
        temporary.unlink(missing_ok=True)
        assembled.unlink(missing_ok=True)
        for marker in range_dir.glob("*.done"):
            marker.unlink()
        with temporary.open("wb") as stream:
            stream.truncate(expected_size)

    def fetch(index: int, start: int, end: int) -> Path:
        part = range_dir / f"{index:06d}.part"
        if part.is_file() and part.stat().st_size == end - start + 1:
            return part
        part.unlink(missing_ok=True)
        subprocess.run([
            "aws", "s3api", "get-object", "--region", REGION,
            "--bucket", BUCKET, "--key", key,
            "--version-id", str(partition["sidecar_version_id"]),
            "--range", f"bytes={start}-{end}", str(part),
        ], check=True, stdout=subprocess.DEVNULL)
        if part.stat().st_size != end - start + 1:
            raise RuntimeError(f"square {square}: truncated S3 range {index}")
        return part

    with ThreadPoolExecutor(max_workers=min(workers, len(ranges))) as executor:
        futures = {
            executor.submit(fetch, index, start, end): index
            for index, (start, end) in enumerate(ranges)
            if not (range_dir / f"{index:06d}.done").is_file()
        }
        completed = len(ranges) - len(futures)
        for future in as_completed(futures):
            index = futures[future]
            part = future.result()
            start, end = ranges[index]
            with temporary.open("r+b", buffering=0) as output, part.open("rb") as stream:
                output.seek(start)
                shutil.copyfileobj(stream, output, 8 << 20)
                output.flush()
            part.unlink()
            (range_dir / f"{index:06d}.done").touch()
            completed += 1
            print(f"DEVIL_HF_RANGE square={square} completed={completed}/{len(ranges)}",
                  flush=True)

    with temporary.open("rb") as stream:
        os.fsync(stream.fileno())
    assembled.touch()
    for index in range(len(ranges)):
        (range_dir / f"{index:06d}.done").unlink()
    range_dir.rmdir()
    if temporary.stat().st_size != expected_size:
        raise RuntimeError(f"square {square}: assembled extent mismatch")
    temporary.replace(destination)
    assembled.unlink()


def validate_sidecar(path: Path, partition: dict[str, Any], auditor: Path,
                     workers: int) -> dict[str, Any]:
    expected_count = int(partition["states"])
    expected_size = HEADER.size + expected_count * RECORD_BYTES
    if path.stat().st_size != expected_size:
        raise RuntimeError(f"{path.name}: extent mismatch")
    with path.open("rb") as stream:
        values = HEADER.unpack(stream.read(HEADER.size))
    expected_header = (MAGIC, 1, int(partition["square"]), expected_count,
                       RECORD_BYTES, 0)
    if values != expected_header:
        raise RuntimeError(f"{path.name}: header mismatch: {values!r}")
    actual_sha = sha256(path)
    if actual_sha != partition["sidecar_sha256"]:
        raise RuntimeError(f"{path.name}: SHA-256 mismatch: {actual_sha}")
    completed = subprocess.run([
        str(auditor), "--input", str(path), "--square", str(partition["square"]),
        "--workers", str(workers),
    ], check=True, capture_output=True, text=True)
    census = json.loads(completed.stdout)
    expected_outcomes = {name: int(partition["outcomes"][name])
                         for name in ("win", "loss", "draw")}
    if census.get("states") != expected_count or census.get("outcomes") != expected_outcomes:
        raise RuntimeError(f"{path.name}: exhaustive outcome census mismatch")
    if census.get("conservation_residual") != 0 or census.get("sorted_key_residual") != 0:
        raise RuntimeError(f"{path.name}: exhaustive sidecar audit residual")
    if max(int(value) for value in census["max_dtw"].values()) != int(partition["max_dtw"]):
        raise RuntimeError(f"{path.name}: max-DTW mismatch")
    return census


def remote_files(api: Any, repo_id: str) -> dict[str, Any]:
    return {item.rfilename: item for item in api.repo_info(
        repo_id, repo_type="dataset", files_metadata=True).siblings}


def remote_matches(item: Any, expected_size: int, expected_sha: str) -> bool:
    return bool(item and item.lfs and item.size == expected_size and
                item.lfs.get("sha256") == expected_sha)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--certificate", type=Path, default=DEFAULT_CERTIFICATE)
    parser.add_argument("--work-dir", type=Path,
                        default=Path("/tmp/ultimatefish-devil-hf-publish-20260829"))
    parser.add_argument("--repo-id", default=REPO_ID)
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--square", type=int, action="append")
    parser.add_argument("--keep-local", action="store_true")
    args = parser.parse_args()
    if args.workers <= 0:
        parser.error("--workers must be positive")

    try:
        from huggingface_hub import HfApi
    except ImportError as error:
        raise RuntimeError("huggingface_hub is required") from error

    certificate = validate_certificate(args.certificate)
    selected = set(args.square or EXPECTED_SQUARES)
    if not selected.issubset(EXPECTED_SQUARES):
        parser.error("--square must name a certified fixed Devil square")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    (args.work_dir / "tablebases").mkdir(exist_ok=True)
    auditor = build_auditor(args.work_dir)
    api = HfApi()

    for partition in certificate["partitions"]:
        square = int(partition["square"])
        if square not in selected:
            continue
        name = remote_name(partition)
        relative = f"tablebases/{name}"
        expected_size = HEADER.size + int(partition["states"]) * RECORD_BYTES
        if expected_size > 50_000_000_000:
            raise RuntimeError(
                f"{relative} is {expected_size} bytes and exceeds Hugging Face's "
                "50 GB per-file limit; use relay_ultimate_devil_stateful_hf_aws.py "
                "to publish authenticated UFDS shards")
        remote = remote_files(api, args.repo_id).get(relative)
        if remote_matches(remote, expected_size, partition["sidecar_sha256"]):
            print(f"DEVIL_HF_ALREADY_VERIFIED square={square} path={relative}", flush=True)
            continue

        destination = args.work_dir / relative
        print(f"DEVIL_HF_DOWNLOAD square={square} bytes={expected_size} path={name}",
              flush=True)
        download(partition, destination, args.workers)
        census = validate_sidecar(destination, partition, auditor, args.workers)
        print(f"DEVIL_HF_LOCAL_VERIFIED square={square} sha256="
              f"{partition['sidecar_sha256']} outcomes={json.dumps(census['outcomes'], sort_keys=True)}",
              flush=True)
        api.upload_large_folder(
            repo_id=args.repo_id, repo_type="dataset", folder_path=str(args.work_dir),
            allow_patterns=relative, num_workers=min(args.workers, 8),
        )
        remote = remote_files(api, args.repo_id).get(relative)
        if not remote_matches(remote, expected_size, partition["sidecar_sha256"]):
            raise RuntimeError(f"{relative}: remote LFS verification failed")
        print(f"DEVIL_HF_REMOTE_VERIFIED square={square} bytes={expected_size} "
              f"sha256={partition['sidecar_sha256']}", flush=True)
        if not args.keep_local:
            destination.unlink()

    print("DEVIL_HF_PUBLISH_OK", flush=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"Devil Hugging Face publication error: {error}", file=sys.stderr)
        raise
