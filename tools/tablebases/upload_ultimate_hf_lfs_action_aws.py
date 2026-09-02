#!/usr/bin/env python3
"""Upload one preauthorized Hugging Face LFS action without an HF credential.

The local authenticated relay writes a short-lived LFS action to S3. This
worker exact-version downloads and exhaustively validates its certified UFDS
source, uploads the bytes through the action's expiring URLs, and completes the
multipart transfer. It never receives a Hugging Face account token.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
from math import ceil
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Any


TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import publish_ultimate_devil_stateful_hf as publisher  # noqa: E402


def aws_read(uri: str) -> dict[str, Any]:
    completed = subprocess.run(
        ["aws", "s3", "cp", uri, "-"], check=True, capture_output=True)
    return json.loads(completed.stdout)


def response_etag(header_path: Path) -> str:
    values = []
    for line in header_path.read_text(errors="replace").splitlines():
        name, separator, value = line.partition(":")
        if separator and name.lower() == "etag":
            values.append(value.strip())
    if not values or not values[-1]:
        raise RuntimeError("LFS multipart response omitted its ETag")
    return values[-1]


def curl_put(url: str, payload_path: Path, header_path: Path) -> str:
    subprocess.run([
        "curl", "--fail", "--silent", "--show-error", "--retry", "8",
        "--retry-all-errors", "--connect-timeout", "30", "--request", "PUT",
        "--upload-file", str(payload_path),
        "--dump-header", str(header_path), "--output", "/dev/null", url,
    ], check=True)
    return response_etag(header_path)


def upload_multipart(path: Path, action: dict[str, Any], oid: str,
                     workers: int, temporary: Path) -> None:
    upload = action["actions"]["upload"]
    header = upload.get("header", {})
    chunk_size = int(header["chunk_size"])
    urls = [url for _, url in sorted(
        ((int(name), value) for name, value in header.items() if name.isdigit()),
        key=lambda item: item[0])]
    expected_parts = ceil(path.stat().st_size / chunk_size)
    if len(urls) != expected_parts:
        raise RuntimeError(
            f"LFS action has {len(urls)} parts, expected {expected_parts}")

    def upload_part(index: int, url: str) -> tuple[int, str]:
        offset = index * chunk_size
        length = min(chunk_size, path.stat().st_size - offset)
        with path.open("rb") as stream:
            stream.seek(offset)
            payload = stream.read(length)
        if len(payload) != length:
            raise RuntimeError(f"short local read for LFS part {index + 1}")
        response = temporary / f"headers-{index + 1:06d}.txt"
        payload_path = temporary / f"part-{index + 1:06d}.bin"
        payload_path.write_bytes(payload)
        del payload
        try:
            return index, curl_put(url, payload_path, response)
        finally:
            response.unlink(missing_ok=True)
            payload_path.unlink(missing_ok=True)

    etags: list[str | None] = [None] * len(urls)
    with ThreadPoolExecutor(max_workers=min(workers, len(urls))) as executor:
        futures = {executor.submit(upload_part, index, url): index
                   for index, url in enumerate(urls)}
        completed = 0
        for future in as_completed(futures):
            index, etag = future.result()
            etags[index] = etag
            completed += 1
            if completed == len(urls) or completed % 25 == 0:
                print(f"HF_LFS_PARTS completed={completed}/{len(urls)}", flush=True)

    if any(etag is None for etag in etags):
        raise RuntimeError("missing LFS multipart ETag")
    completion = {
        "oid": oid,
        "parts": [{"partNumber": index + 1, "etag": etag}
                  for index, etag in enumerate(etags)],
    }
    subprocess.run([
        "curl", "--fail", "--silent", "--show-error", "--retry", "8",
        "--retry-all-errors", "--connect-timeout", "30", "--request", "POST",
        "--header", "Accept: application/vnd.git-lfs+json",
        "--header", "Content-Type: application/vnd.git-lfs+json",
        "--data-binary", "@-", upload["href"],
    ], input=json.dumps(completion).encode(), check=True)


def upload_single(path: Path, action: dict[str, Any]) -> None:
    upload = action["actions"]["upload"]
    subprocess.run([
        "curl", "--fail", "--silent", "--show-error", "--retry", "8",
        "--retry-all-errors", "--connect-timeout", "30", "--request", "PUT",
        "--upload-file", str(path), "--output", "/dev/null", upload["href"],
    ], check=True)


def upload_validated_file(path: Path, entry: dict[str, Any], workers: int,
                          work_dir: Path) -> None:
    expected_size = int(entry["bytes"])
    expected_sha = str(entry["sha256"])
    action = entry["lfs_batch_action"]
    if (not path.is_file() or path.stat().st_size != expected_size or
            publisher.sha256(path) != expected_sha):
        raise RuntimeError(f"prepared LFS object is not authentic: {path.name}")
    if action.get("oid") != expected_sha or not action.get("actions"):
        raise RuntimeError(f"LFS action does not bind {path.name}")
    with tempfile.TemporaryDirectory(dir=work_dir) as temporary:
        upload = action["actions"]["upload"]
        if "chunk_size" in upload.get("header", {}):
            upload_multipart(path, action, expected_sha, workers,
                             Path(temporary))
        else:
            upload_single(path, action)
    print(f"HF_LFS_OBJECT_UPLOAD_COMPLETE file={path.name} "
          f"bytes={expected_size} sha256={expected_sha}", flush=True)


def run(action_uri: str, work_dir: Path, workers: int, keep_local: bool) -> None:
    request = aws_read(action_uri)
    schema = request.get("schema")
    if schema == "ultimate-fish-hf-lfs-relay-shards-v1":
        table_dir = work_dir / "tablebases"
        entries = request.get("files", ())
        if not entries:
            raise RuntimeError("sharded LFS relay action has no files")
        for entry in entries:
            filename = str(entry["filename"])
            if Path(filename).name != filename:
                raise RuntimeError("unsafe sharded LFS filename")
            upload_validated_file(
                table_dir / filename, entry, workers, work_dir)
        print("HF_LFS_SHARDED_UPLOAD_COMPLETE", flush=True)
        if not keep_local:
            for filename in request.get(
                    "cleanup_filenames",
                    [entry["filename"] for entry in entries]):
                if Path(str(filename)).name != filename:
                    raise RuntimeError("unsafe sharded cleanup filename")
                (table_dir / str(filename)).unlink(missing_ok=True)
            logical = request.get("logical_filename")
            if logical and Path(str(logical)).name == logical:
                (table_dir / str(logical)).unlink(missing_ok=True)
        return
    if schema != "ultimate-fish-hf-lfs-relay-action-v1":
        raise RuntimeError("unexpected LFS relay action schema")
    partition = request["partition"]
    action = request["lfs_batch_action"]
    expected_oid = str(partition["sidecar_sha256"])
    if action.get("oid") != expected_oid:
        raise RuntimeError("LFS action OID does not match the certified sidecar")
    if not action.get("actions"):
        raise RuntimeError("LFS relay action does not require an upload")

    work_dir.mkdir(parents=True, exist_ok=True)
    table_dir = work_dir / "tablebases"
    table_dir.mkdir(exist_ok=True)
    destination = table_dir / request["filename"]
    auditor = publisher.build_auditor(work_dir)
    publisher.download(partition, destination, workers)
    census = publisher.validate_sidecar(destination, partition, auditor, workers)
    print(f"HF_LFS_LOCAL_VERIFIED file={destination.name} "
          f"sha256={expected_oid}", flush=True)

    with tempfile.TemporaryDirectory(dir=work_dir) as temporary:
        upload = action["actions"]["upload"]
        if "chunk_size" in upload.get("header", {}):
            upload_multipart(destination, action, expected_oid, workers,
                             Path(temporary))
        else:
            upload_single(destination, action)
    print("HF_LFS_REMOTE_UPLOAD_COMPLETE " + json.dumps({
        "filename": destination.name,
        "states": census["states"],
        "sha256": expected_oid,
    }, sort_keys=True), flush=True)
    if not keep_local:
        destination.unlink()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--action-s3-uri", required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=32)
    parser.add_argument("--keep-local", action="store_true")
    args = parser.parse_args()
    if args.workers <= 0:
        parser.error("--workers must be positive")
    run(args.action_s3_uri, args.work_dir, args.workers, args.keep_local)


if __name__ == "__main__":
    main()
