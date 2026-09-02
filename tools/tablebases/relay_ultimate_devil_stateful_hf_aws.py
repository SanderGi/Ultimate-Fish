#!/usr/bin/env python3
"""Relay certified Devil UFDS files from versioned S3 to Hugging Face via EC2.

Only short-lived LFS upload actions are shared with the worker. The Hugging
Face account token remains in the local authenticated client.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import publish_ultimate_devil_stateful_hf as publisher  # noqa: E402


BUCKET = publisher.BUCKET
REGION = publisher.REGION
INSTANCE = "i-08c0f44a1776cb34a"
REMOTE_ROOT = Path("/mnt/ultimatefish-devil-fast/final-hf-publish-20260830")
STAGE_PREFIX = "staging/final-hf-lfs-relay-v1"
LFS_MAX_FILE_BYTES = 50_000_000_000


def aws(*arguments: str, input_bytes: bytes | None = None) -> str:
    completed = subprocess.run(
        ["aws", *arguments], input=input_bytes, check=True,
        capture_output=True)
    return completed.stdout.decode().strip()


def aws_bytes(*arguments: str) -> bytes:
    completed = subprocess.run(
        ["aws", *arguments], check=True, capture_output=True)
    return completed.stdout


def put_action(key: str, data: dict[str, Any]) -> str:
    aws("s3", "cp", "-", f"s3://{BUCKET}/{key}", "--region", REGION,
        "--sse", "AES256", input_bytes=json.dumps(data).encode())
    return f"s3://{BUCKET}/{key}"


def send_worker(action_uri: str, bundle_prefix: str, workers: int,
                instance_id: str) -> str:
    commands = [
        "set -euo pipefail",
        f"mkdir -p {REMOTE_ROOT}/tools/tablebases {REMOTE_ROOT}/tablebases",
        (f"aws s3 cp s3://{BUCKET}/{bundle_prefix}/"
         f"publish_ultimate_devil_stateful_hf.py "
         f"{REMOTE_ROOT}/tools/tablebases/publish_ultimate_devil_stateful_hf.py"),
        (f"aws s3 cp s3://{BUCKET}/{bundle_prefix}/"
         f"upload_ultimate_hf_lfs_action_aws.py "
         f"{REMOTE_ROOT}/tools/tablebases/upload_ultimate_hf_lfs_action_aws.py"),
        (f"aws s3 cp s3://{BUCKET}/{bundle_prefix}/"
         f"prepare_ultimate_hf_ufds_shards_aws.py "
         f"{REMOTE_ROOT}/tools/tablebases/prepare_ultimate_hf_ufds_shards_aws.py"),
        (f"aws s3 cp s3://{BUCKET}/{bundle_prefix}/"
         f"audit_ultimate_devil_stateful_sidecar.cpp "
         f"{REMOTE_ROOT}/tools/tablebases/audit_ultimate_devil_stateful_sidecar.cpp"),
        (f"aws s3 cp s3://{BUCKET}/{bundle_prefix}/"
         f"ultimate-devil-stateful-class-certificate.json "
         f"{REMOTE_ROOT}/tablebases/ultimate-devil-stateful-class-certificate.json"),
        (f"python3 {REMOTE_ROOT}/tools/tablebases/upload_ultimate_hf_lfs_action_aws.py "
         f"--action-s3-uri {action_uri} --work-dir {REMOTE_ROOT}/work "
         f"--workers {workers}"),
    ]
    parameters = json.dumps({"commands": commands})
    return aws(
        "ssm", "send-command", "--region", REGION, "--instance-ids", instance_id,
        "--document-name", "AWS-RunShellScript",
        "--comment", "ultimatefish-tokenless-hf-lfs-relay",
        "--parameters", parameters, "--query", "Command.CommandId",
        "--output", "text")


def send_prepare(partition: dict[str, Any], manifest_uri: str,
                 bundle_prefix: str, workers: int, instance_id: str) -> str:
    commands = [
        "set -euo pipefail",
        f"mkdir -p {REMOTE_ROOT}/tools/tablebases {REMOTE_ROOT}/tablebases",
        (f"aws s3 cp s3://{BUCKET}/{bundle_prefix}/"
         f"publish_ultimate_devil_stateful_hf.py "
         f"{REMOTE_ROOT}/tools/tablebases/publish_ultimate_devil_stateful_hf.py"),
        (f"aws s3 cp s3://{BUCKET}/{bundle_prefix}/"
         f"prepare_ultimate_hf_ufds_shards_aws.py "
         f"{REMOTE_ROOT}/tools/tablebases/prepare_ultimate_hf_ufds_shards_aws.py"),
        (f"aws s3 cp s3://{BUCKET}/{bundle_prefix}/"
         f"audit_ultimate_devil_stateful_sidecar.cpp "
         f"{REMOTE_ROOT}/tools/tablebases/audit_ultimate_devil_stateful_sidecar.cpp"),
        (f"aws s3 cp s3://{BUCKET}/{bundle_prefix}/"
         f"ultimate-devil-stateful-class-certificate.json "
         f"{REMOTE_ROOT}/tablebases/ultimate-devil-stateful-class-certificate.json"),
        (f"python3 {REMOTE_ROOT}/tools/tablebases/prepare_ultimate_hf_ufds_shards_aws.py "
         f"--certificate {REMOTE_ROOT}/tablebases/"
         f"ultimate-devil-stateful-class-certificate.json "
         f"--square {partition['square']} --work-dir {REMOTE_ROOT}/work "
         f"--manifest-s3-uri {manifest_uri} --workers {workers}"),
    ]
    return aws(
        "ssm", "send-command", "--region", REGION, "--instance-ids", instance_id,
        "--document-name", "AWS-RunShellScript",
        "--comment", "ultimatefish-prepare-hf-ufds-shards",
        "--parameters", json.dumps({"commands": commands}),
        "--query", "Command.CommandId", "--output", "text")


def wait_worker(command_id: str, instance_id: str) -> dict[str, Any]:
    last_output = ""
    while True:
        try:
            raw = aws(
                "ssm", "get-command-invocation", "--region", REGION,
                "--command-id", command_id, "--instance-id", instance_id,
                "--output", "json")
            result = json.loads(raw)
        except subprocess.CalledProcessError:
            time.sleep(5)
            continue
        output = result.get("StandardOutputContent", "")
        if output != last_output:
            print(output[len(last_output):], end="", flush=True)
            last_output = output
        status = result.get("Status")
        if status in {"Success", "Cancelled", "Failed", "TimedOut",
                      "Cancelling"}:
            if status != "Success":
                raise RuntimeError(
                    f"remote LFS worker {status}: "
                    f"{result.get('StandardErrorContent', '')}")
            return result
        time.sleep(20)


def commit_pointers(api: Any, files: list[dict[str, Any]], repo_id: str,
                    message: str) -> str:
    from huggingface_hub import CommitOperationAdd
    from huggingface_hub.lfs import UploadInfo

    operations = []
    for file in files:
        operation = CommitOperationAdd(
            path_in_repo=f"tablebases/{file['filename']}", path_or_fileobj=b"")
        operation.upload_info = UploadInfo(
            sha256=bytes.fromhex(file["sha256"]),
            size=int(file["bytes"]), sample=b"")
        operation._upload_mode = "lfs"
        operation._is_uploaded = True
        operations.append(operation)
    commit = api.create_commit(
        repo_id=repo_id, repo_type="dataset", operations=operations,
        commit_message=message)
    return commit.oid


def validate_shard_manifest(partition: dict[str, Any],
                            manifest: dict[str, Any]) -> None:
    filename = publisher.remote_name(partition)
    expected_size = (publisher.HEADER.size + int(partition["states"])
                     * publisher.RECORD_BYTES)
    parts = manifest.get("parts", ())
    if (manifest.get("schema") != "ultimate-fish-ufds-shard-manifest-v1" or
            manifest.get("filename") != filename or
            manifest.get("bytes") != expected_size or
            manifest.get("sha256") != partition["sidecar_sha256"] or
            manifest.get("square") != int(partition["square"]) or
            len(parts) < 2 or
            sum(int(part["bytes"]) for part in parts) != expected_size):
        raise RuntimeError(f"invalid UFDS shard manifest for {filename}")
    names = set()
    stem = filename.removesuffix(".ufds")
    for index, part in enumerate(parts):
        expected_name = f"{stem}-part{index:03d}.ufdsp"
        name = str(part.get("filename", ""))
        digest = str(part.get("sha256", ""))
        size = int(part.get("bytes", 0))
        if (name != expected_name or name in names or size <= 0 or
                size > LFS_MAX_FILE_BYTES or
                not re.fullmatch(r"[0-9a-f]{64}", digest)):
            raise RuntimeError(f"invalid UFDS shard entry for {filename}")
        names.add(name)


def remote_shards_match(api: Any, repo_id: str,
                        partition: dict[str, Any]) -> bool:
    from huggingface_hub import hf_hub_download

    filename = publisher.remote_name(partition)
    manifest_name = filename.removesuffix(".ufds") + ".ufdsm"
    remote = publisher.remote_files(api, repo_id)
    if f"tablebases/{manifest_name}" not in remote:
        return False
    try:
        path = hf_hub_download(
            repo_id, f"tablebases/{manifest_name}", repo_type="dataset")
        manifest_bytes = Path(path).read_bytes()
        manifest = json.loads(manifest_bytes)
        validate_shard_manifest(partition, manifest)
    except Exception:
        return False
    manifest_item = remote.get(f"tablebases/{manifest_name}")
    if not publisher.remote_matches(
            manifest_item, len(manifest_bytes), hashlib.sha256(manifest_bytes).hexdigest()):
        return False
    return all(publisher.remote_matches(
        remote.get(f"tablebases/{part['filename']}"),
        int(part["bytes"]), str(part["sha256"])) for part in manifest["parts"])


def request_actions(files: list[dict[str, Any]], token: str,
                    repo_id: str) -> list[dict[str, Any]]:
    from huggingface_hub.lfs import UploadInfo, post_lfs_batch_info

    infos = [UploadInfo(
        sha256=bytes.fromhex(file["sha256"]),
        size=int(file["bytes"]), sample=b"") for file in files]
    actions, errors = post_lfs_batch_info(
        infos, token=token, repo_type="dataset", repo_id=repo_id)
    if errors or len(actions) != len(files):
        raise RuntimeError(f"Hugging Face LFS batch error: {errors!r}")
    by_oid = {action["oid"]: action for action in actions}
    result = []
    for file in files:
        action = by_oid.get(file["sha256"])
        if action is None:
            raise RuntimeError("Hugging Face omitted an LFS object action")
        result.append({**file, "lfs_batch_action": action})
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--certificate", type=Path,
                        default=publisher.DEFAULT_CERTIFICATE)
    parser.add_argument("--repo-id", default=publisher.REPO_ID)
    parser.add_argument("--instance-id", default=INSTANCE)
    parser.add_argument("--bundle-prefix", required=True)
    parser.add_argument("--workers", type=int, default=32)
    parser.add_argument("--square", type=int, action="append")
    args = parser.parse_args()

    from huggingface_hub import HfApi, HfFolder

    token = HfFolder.get_token()
    if not token:
        raise RuntimeError("local Hugging Face authentication is required")
    api = HfApi(token=token)
    certificate = publisher.validate_certificate(args.certificate)
    selected = set(args.square or publisher.EXPECTED_SQUARES)
    for partition in certificate["partitions"]:
        square = int(partition["square"])
        if square not in selected:
            continue
        filename = publisher.remote_name(partition)
        relative = f"tablebases/{filename}"
        expected_size = (publisher.HEADER.size + int(partition["states"])
                         * publisher.RECORD_BYTES)
        if expected_size > LFS_MAX_FILE_BYTES:
            if remote_shards_match(api, args.repo_id, partition):
                print(f"HF_RELAY_SHARDS_ALREADY_VERIFIED square={square} "
                      f"path={relative}", flush=True)
                continue
            manifest_key = (
                f"{STAGE_PREFIX}/manifests/{filename}-"
                f"{partition['sidecar_sha256']}.json")
            manifest_uri = f"s3://{BUCKET}/{manifest_key}"
            command_id = send_prepare(
                partition, manifest_uri, args.bundle_prefix, args.workers,
                args.instance_id)
            print(f"HF_RELAY_SHARDS_PREPARE_STARTED square={square} "
                  f"command={command_id}", flush=True)
            wait_worker(command_id, args.instance_id)
            manifest_bytes = aws_bytes(
                "s3", "cp", manifest_uri, "-", "--region", REGION)
            manifest = json.loads(manifest_bytes)
            validate_shard_manifest(partition, manifest)
            manifest_name = filename.removesuffix(".ufds") + ".ufdsm"
            files = [dict(part) for part in manifest["parts"]]
            files.append({
                "filename": manifest_name,
                "bytes": len(manifest_bytes),
                "sha256": hashlib.sha256(manifest_bytes).hexdigest(),
            })
            requested = request_actions(files, token, args.repo_id)
            uploads = [entry for entry in requested
                       if entry["lfs_batch_action"].get("actions")]
            if uploads:
                nonce = hashlib.sha256(
                    f"{time.time_ns()}:{square}:shards".encode()
                ).hexdigest()[:16]
                key = f"{STAGE_PREFIX}/actions/{filename}-{nonce}.json"
                action_uri = put_action(key, {
                    "schema": "ultimate-fish-hf-lfs-relay-shards-v1",
                    "logical_filename": filename,
                    "cleanup_filenames": [entry["filename"] for entry in files],
                    "files": uploads,
                })
                command_id = send_worker(
                    action_uri, args.bundle_prefix, args.workers,
                    args.instance_id)
                print(f"HF_RELAY_SHARDS_UPLOAD_STARTED square={square} "
                      f"command={command_id}", flush=True)
                wait_worker(command_id, args.instance_id)
            revision = commit_pointers(
                api, files, args.repo_id,
                f"Publish certified stateful Devil partition {partition['label']} shards")
            if not remote_shards_match(api, args.repo_id, partition):
                raise RuntimeError(f"{relative}: committed shard verification failed")
            print(f"HF_RELAY_SHARDS_REMOTE_VERIFIED square={square} "
                  f"revision={revision} bytes={expected_size} "
                  f"sha256={partition['sidecar_sha256']}", flush=True)
            continue

        remote = publisher.remote_files(api, args.repo_id).get(relative)
        if publisher.remote_matches(
                remote, expected_size, partition["sidecar_sha256"]):
            print(f"HF_RELAY_ALREADY_VERIFIED square={square} path={relative}",
                  flush=True)
            continue

        file = {
            "filename": filename,
            "bytes": expected_size,
            "sha256": partition["sidecar_sha256"],
        }
        action = request_actions([file], token, args.repo_id)[0][
            "lfs_batch_action"]
        if action.get("actions"):
            nonce = hashlib.sha256(
                f"{time.time_ns()}:{square}".encode()).hexdigest()[:16]
            key = f"{STAGE_PREFIX}/actions/{filename}-{nonce}.json"
            action_uri = put_action(key, {
                "schema": "ultimate-fish-hf-lfs-relay-action-v1",
                "filename": filename,
                "partition": partition,
                "lfs_batch_action": action,
            })
            command_id = send_worker(
                action_uri, args.bundle_prefix, args.workers, args.instance_id)
            print(f"HF_RELAY_REMOTE_STARTED square={square} command={command_id}",
                  flush=True)
            wait_worker(command_id, args.instance_id)
        revision = commit_pointers(
            api, [file], args.repo_id,
            f"Publish certified stateful Devil partition {partition['label']}")
        remote = publisher.remote_files(api, args.repo_id).get(relative)
        if not publisher.remote_matches(
                remote, expected_size, partition["sidecar_sha256"]):
            raise RuntimeError(f"{relative}: committed LFS verification failed")
        print(f"HF_RELAY_REMOTE_VERIFIED square={square} revision={revision} "
              f"bytes={expected_size} sha256={partition['sidecar_sha256']}",
              flush=True)
    print("HF_RELAY_PUBLISH_OK", flush=True)


if __name__ == "__main__":
    main()
