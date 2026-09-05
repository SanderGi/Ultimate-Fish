#!/usr/bin/env python3
"""Recompute and publish the two primary-Jester/Angel information overlays.

The EC2 worker receives short-lived, read-only Hugging Face download URLs, not
the user's Hugging Face token.  Completed overlays are staged through a private
S3 bucket, downloaded locally, hash checked, and committed to Hugging Face by
the authenticated local client.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import time
from typing import Any


REGION = "us-west-2"
REPO_ID = "SanderGi/Ultimate-Fish-Tablebases"
SOURCE_COMMIT = "30e7b3e91e4728277273a689e89128071881c907"
SOURCE_ARCHIVE_BYTES = 16_527_360
SOURCE_ARCHIVE_SHA256 = "06a3f16af76db19b3973fe4d06932bf311b35eab9f04c9732abeabdaeb5d1930"
SOURCE_REVISION = "c574ae2347e2b324f0e90888478fb498776cf372"
REMOTE_ROOT = Path("/opt/ultimatefish-jester-angel-recompute-20260904-v2")
SAME_UNIT = "ultimatefish-jester-angel-same-20260904.service"
OPPOSED_UNIT = "ultimatefish-jester-angel-opposed-20260904.service"

MODEL_SHA256 = "5582d014c827845df5eb658078ad0be8f367b9fde087bf22509e673fdfd23c66"
LOWER_MODEL_SHA256 = "af8d6187577e6c0fedc0b9643020f843e9b61cab03af63ef2cd2138e0ca41c7b"

INPUTS = {
    "kjesterangelk.uftb": (
        142_342_264,
        "0c52518058082de282fe2e3dbaec3b507dd091ba4dea7076740ddaaf853d2da9",
    ),
    "kjesterkangel.uftb": (
        94_894_864,
        "eb4d6890acfaa8f3dc1429f94868206e0405c92f3dd4d36a91549d932f310718",
    ),
    "kjesterk.uftb": (
        1_232_440,
        "3d896b07c0f7ee97da5aabefee6551c90732bbc200343a4af51a08b678e236aa",
    ),
    "kjesterk.ufiw": (
        986_080,
        "b8aeceb739780ae9ab74c82cc476478c53d2e4296dd91010b073b9d6c46ee7f9",
    ),
}

OUTPUTS = {
    "kjesterangelk.ufiw": {
        "bytes": 113_873_920,
        "sha256": "69041ef5b9727975045fe69edf51b76e960022f7ada23f8bd1d219b5a4296f6f",
        "source": INPUTS["kjesterangelk.uftb"][1],
        "unit": SAME_UNIT,
        "directory": "same",
    },
    "kjesterkangel.ufiw": {
        "bytes": 75_916_000,
        "sha256": "9b24d964480077be987a9dc6c05f6b63e3be3bac11d050d2fab56053cd92d0da",
        "source": INPUTS["kjesterkangel.uftb"][1],
        "unit": OPPOSED_UNIT,
        "directory": "opposed",
    },
}


def aws(*arguments: str, input_bytes: bytes | None = None) -> str:
    completed = subprocess.run(
        ["aws", *arguments], input=input_bytes, check=True,
        capture_output=True)
    return completed.stdout.decode().strip()


def hf_token() -> str:
    from huggingface_hub import get_token

    token = get_token()
    if not token:
        raise RuntimeError("local Hugging Face authentication is required")
    return token


def signed_download_url(filename: str, token: str) -> str:
    import requests
    from huggingface_hub import hf_hub_url

    url = hf_hub_url(
        REPO_ID, f"tablebases/{filename}", repo_type="dataset",
        revision=SOURCE_REVISION)
    response = requests.get(
        url, headers={"Authorization": f"Bearer {token}"}, stream=True,
        allow_redirects=False, timeout=60)
    try:
        if response.status_code not in {301, 302, 303, 307, 308}:
            raise RuntimeError(
                f"Hugging Face did not issue a signed URL for {filename}: "
                f"HTTP {response.status_code}")
        location = response.headers.get("Location")
        if not location:
            raise RuntimeError(f"Hugging Face omitted the URL for {filename}")
        return location
    finally:
        response.close()


def send_script(instance: str, script: str, comment: str) -> str:
    return aws(
        "ssm", "send-command", "--region", REGION,
        "--instance-ids", instance, "--document-name", "AWS-RunShellScript",
        "--comment", comment,
        "--parameters", json.dumps({"commands": [script]}),
        "--query", "Command.CommandId", "--output", "text")


def wait_command(instance: str, command: str, timeout: int = 1_800) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            result = json.loads(aws(
                "ssm", "get-command-invocation", "--region", REGION,
                "--instance-id", instance, "--command-id", command,
                "--output", "json"))
        except subprocess.CalledProcessError:
            time.sleep(3)
            continue
        status = result.get("Status")
        if status == "Success":
            return result
        if status in {"Cancelled", "Cancelling", "Failed", "TimedOut"}:
            raise RuntimeError(
                f"SSM command {command} {status}: "
                f"{result.get('StandardErrorContent', '')}")
        time.sleep(5)
    raise TimeoutError(f"SSM command {command} did not finish")


def q(value: str | Path) -> str:
    return shlex.quote(str(value))


def compute_script(urls: dict[str, str], bucket: str) -> str:
    root = str(REMOTE_ROOT)
    inputs = str(REMOTE_ROOT / "inputs")
    binary = str(REMOTE_ROOT / "ultimate_tablebase")
    source = str(REMOTE_ROOT / "source")
    lines = [
        "set -euo pipefail",
        "dnf install -y gcc-c++",
        f"test ! -e {q(REMOTE_ROOT)}",
        f"install -d -m 0755 {q(inputs)} {q(REMOTE_ROOT / 'work/same/scratch')} "
        f"{q(REMOTE_ROOT / 'work/opposed/scratch')} {q(source)}",
        f"aws s3 cp s3://{bucket}/inputs/ultimatefish-30e7b3e9-source.tar "
        f"{q(REMOTE_ROOT / 'source.tar')} --region {REGION} --only-show-errors",
        f"test \"$(stat -c %s {q(REMOTE_ROOT / 'source.tar')})\" = {SOURCE_ARCHIVE_BYTES}",
        f"test \"$(sha256sum {q(REMOTE_ROOT / 'source.tar')} | cut -d' ' -f1)\" = "
        f"{SOURCE_ARCHIVE_SHA256}",
        f"tar -xf {q(REMOTE_ROOT / 'source.tar')} -C {q(source)}",
        f"test \"$(sha256sum {q(Path(source) / 'src/ultimate/tablebases/tablebase.cpp')} | cut -d' ' -f1)\" = "
        "2fcaa42fb4616fc5e17c9c1bcbaa81d1b003bd86d08ce80444b1cbeaa2bdaf81",
        f"test \"$(sha256sum {q(Path(source) / 'src/ultimate/tablebases/tablebase_probe.cpp')} | cut -d' ' -f1)\" = "
        "e17573a1b0aa01bbb232a2144903b300c8945c8efb64d862da919dd14dc5449c",
    ]
    for filename, url in urls.items():
        destination = Path(inputs) / filename
        size, digest = INPUTS[filename]
        lines.extend([
            f"curl --fail --silent --show-error --location --retry 8 "
            f"--retry-all-errors --output {q(destination)} {q(url)}",
            f"test \"$(stat -c %s {q(destination)})\" = {size}",
            f"test \"$(sha256sum {q(destination)} | cut -d' ' -f1)\" = {digest}",
        ])
    lines.extend([
        f"test \"$(awk '/MemAvailable:/ {{print $2 * 1024}}' /proc/meminfo)\" -ge 236223201280",
        f"test \"$(df --output=avail -B1 {q(root)} | tail -n 1)\" -ge 268435456000",
        f"cd {q(source)}",
        "g++ -Isrc/ultimate -Isrc/ultimate/tablebases -std=c++17 -O3 -DNDEBUG "
        "-Wall -Wextra -Wpedantic src/ultimate/tablebases/tablebase.cpp "
        "src/ultimate/position.cpp src/ultimate/tablebases/tablebase_probe.cpp "
        "src/ultimate/tablebases/information.cpp "
        f"src/ultimate/tablebases/information_solver.cpp src/ultimate/nnue.cpp -o {q(binary)}",
        f"{q(binary)} --piece jester --piece2 angel --workers 8 --self-test "
        f"--dry-run 20000 >{q(REMOTE_ROOT / 'self-test-same.log')}",
        f"{q(binary)} --piece jester --piece2 angel --opposing --workers 8 "
        f"--self-test --dry-run 20000 >{q(REMOTE_ROOT / 'self-test-opposed.log')}",
        f"grep -Fqx 'angeljestertransitiondecisionok index 492966 residual 0' "
        f"{q(REMOTE_ROOT / 'self-test-same.log')}",
        f"grep -Fqx 'parallelresumereversescanok workers 8' "
        f"{q(REMOTE_ROOT / 'self-test-opposed.log')}",
        "systemd-run --unit=ultimatefish-jester-angel-same-20260904 "
        "--description='Ultimate Fish same-team Jester Angel information solve' "
        f"--property=Type=exec --property=WorkingDirectory={q(root)} "
        "--property=MemoryMax=130G "
        f"--property=StandardOutput=append:{q(REMOTE_ROOT / 'work/same/solve.log')} "
        f"--property=StandardError=append:{q(REMOTE_ROOT / 'work/same/solve.log')} "
        f"--setenv=ULTIMATE_TABLEBASE_PATH={q(inputs)} {q(binary)} "
        "--piece jester --piece2 angel --workers 8 "
        f"--solve-jester-information {q(Path(inputs) / 'kjesterangelk.uftb')} "
        f"--information-overlay {q(REMOTE_ROOT / 'work/same/kjesterangelk.ufiw')} "
        f"--information-scratch {q(REMOTE_ROOT / 'work/same/scratch')} "
        f"--information-source-sha256 {INPUTS['kjesterangelk.uftb'][1]} "
        f"--information-model-sha256 {MODEL_SHA256} "
        f"--lower-information-overlay {q(Path(inputs) / 'kjesterk.ufiw')} "
        f"--lower-information-source-sha256 {INPUTS['kjesterk.uftb'][1]} "
        f"--lower-information-model-sha256 {LOWER_MODEL_SHA256}",
        "systemd-run --unit=ultimatefish-jester-angel-opposed-20260904 "
        "--description='Ultimate Fish opposed Jester Angel information solve' "
        f"--property=Type=exec --property=WorkingDirectory={q(root)} "
        "--property=MemoryMax=112G "
        f"--property=StandardOutput=append:{q(REMOTE_ROOT / 'work/opposed/solve.log')} "
        f"--property=StandardError=append:{q(REMOTE_ROOT / 'work/opposed/solve.log')} "
        f"--setenv=ULTIMATE_TABLEBASE_PATH={q(inputs)} {q(binary)} "
        "--piece jester --piece2 angel --opposing --workers 8 "
        f"--solve-jester-information {q(Path(inputs) / 'kjesterkangel.uftb')} "
        f"--information-overlay {q(REMOTE_ROOT / 'work/opposed/kjesterkangel.ufiw')} "
        f"--information-scratch {q(REMOTE_ROOT / 'work/opposed/scratch')} "
        f"--information-source-sha256 {INPUTS['kjesterkangel.uftb'][1]} "
        f"--information-model-sha256 {MODEL_SHA256} "
        f"--lower-information-overlay {q(Path(inputs) / 'kjesterk.ufiw')} "
        f"--lower-information-source-sha256 {INPUTS['kjesterk.uftb'][1]} "
        f"--lower-information-model-sha256 {LOWER_MODEL_SHA256}",
        "echo JESTER_ANGEL_RECOMPUTE_STARTED",
    ])
    return "\n".join(lines)


def status_script() -> str:
    lines = ["set -euo pipefail"]
    for filename, output in OUTPUTS.items():
        unit = output["unit"]
        log = REMOTE_ROOT / "work" / str(output["directory"]) / "solve.log"
        overlay = REMOTE_ROOT / "work" / str(output["directory"]) / filename
        lines.extend([
            f"systemctl show {q(unit)} -p Id -p ActiveState -p SubState -p Result "
            "-p ExecMainStatus -p MemoryCurrent -p MemoryPeak -p CPUUsageNSec "
            "-p ExecMainPID -p NRestarts",
            f"pid=$(systemctl show {q(unit)} -p ExecMainPID --value); "
            "if test \"$pid\" -gt 0; then "
            "ps -p \"$pid\" -o pid,etimes,time,%cpu,%mem,rss,vsz,stat,comm; fi",
            f"tail -n 4 {q(log)} 2>/dev/null || true",
            f"if test -f {q(overlay)}; then stat -c '%n %s' {q(overlay)}; "
            f"sha256sum {q(overlay)}; fi",
        ])
    lines.append(f"df -h {q(REMOTE_ROOT)}")
    return "\n".join(lines)


def verify_and_stage_script(bucket: str) -> str:
    lines = ["set -euo pipefail"]
    inspect_header = (
        "import struct,sys; h=open(sys.argv[1], 'rb').read(160); "
        "m,v,p,s,c,n,u=struct.unpack_from('<8s6I',h); "
        "assert m==b'UFIW2\\0\\0\\0' and v==2; "
        "print(h[32:96].decode(),h[96:160].decode())"
    )
    for filename, expected in OUTPUTS.items():
        path = REMOTE_ROOT / "work" / str(expected["directory"]) / filename
        lines.extend([
            f"test \"$(systemctl show {q(expected['unit'])} -p Result --value)\" = success",
            f"test \"$(stat -c %s {q(path)})\" = {expected['bytes']}",
            f"test \"$(sha256sum {q(path)} | cut -d' ' -f1)\" = {expected['sha256']}",
            f"python3 -c {q(inspect_header)} {q(path)}",
            f"aws s3 cp {q(path)} s3://{q(bucket)}/outputs/{q(filename)} "
            f"--region {REGION} --only-show-errors --sse AES256 "
            f"--metadata sha256={expected['sha256']},source-sha256={expected['source']},model-sha256={MODEL_SHA256}",
            f"echo STAGED {filename} {expected['bytes']} {expected['sha256']}",
        ])
    return "\n".join(lines)


def command_compute(args: argparse.Namespace) -> None:
    token = hf_token()
    urls = {name: signed_download_url(name, token) for name in INPUTS}
    command = send_script(
        args.instance, compute_script(urls, args.bucket),
        "ultimatefish-jester-angel-recompute-setup")
    print(f"AWS_COMPUTE_SETUP_STARTED command={command}", flush=True)
    result = wait_command(args.instance, command, args.timeout)
    print(result.get("StandardOutputContent", ""), end="")


def command_status(args: argparse.Namespace) -> None:
    command = send_script(
        args.instance, status_script(), "ultimatefish-jester-angel-status")
    result = wait_command(args.instance, command, args.timeout)
    print(result.get("StandardOutputContent", ""), end="")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_overlay(path: Path, expected: dict[str, Any]) -> None:
    if path.stat().st_size != expected["bytes"] or sha256(path) != expected["sha256"]:
        raise RuntimeError(f"regenerated overlay mismatch: {path.name}")
    with path.open("rb") as stream:
        header = stream.read(160)
    magic, version, _primary, _secondary, _color, count, _substates = \
        struct.unpack_from("<8s6I", header)
    if (magic != b"UFIW2\0\0\0" or version != 2 or
            count + 160 != expected["bytes"] or
            header[32:96].decode() != expected["source"] or
            header[96:160].decode() != MODEL_SHA256):
        raise RuntimeError(f"invalid regenerated UFIW2 binding: {path.name}")


def command_publish(args: argparse.Namespace) -> None:
    command = send_script(
        args.instance, verify_and_stage_script(args.bucket),
        "ultimatefish-jester-angel-verify-stage")
    result = wait_command(args.instance, command, args.timeout)
    print(result.get("StandardOutputContent", ""), end="", flush=True)

    from huggingface_hub import CommitOperationAdd, HfApi

    token = hf_token()
    api = HfApi(token=token)
    with tempfile.TemporaryDirectory(
            prefix="ultimatefish-jester-angel-publish-") as temporary:
        directory = Path(temporary)
        operations = []
        for filename, expected in OUTPUTS.items():
            destination = directory / filename
            subprocess.run([
                "aws", "s3", "cp",
                f"s3://{args.bucket}/outputs/{filename}", str(destination),
                "--region", REGION, "--only-show-errors",
            ], check=True)
            validate_overlay(destination, expected)
            operations.append(CommitOperationAdd(
                path_in_repo=f"tablebases/{filename}",
                path_or_fileobj=destination))
        commit = api.create_commit(
            repo_id=REPO_ID, repo_type="dataset", operations=operations,
            commit_message="Publish missing certified Jester/Angel information overlays")
    info = api.repo_info(
        REPO_ID, repo_type="dataset", revision=commit.oid,
        files_metadata=True)
    remote = {item.rfilename: item for item in info.siblings}
    for filename, expected in OUTPUTS.items():
        item = remote.get(f"tablebases/{filename}")
        digest = getattr(item.lfs, "sha256", None) if item and item.lfs else None
        if not item or item.size != expected["bytes"] or digest != expected["sha256"]:
            raise RuntimeError(f"Hugging Face verification failed: {filename}")
    print(f"HF_JESTER_ANGEL_PUBLISHED revision={commit.oid}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--timeout", type=int, default=1_800)
    subparsers = parser.add_subparsers(dest="command", required=True)
    compute = subparsers.add_parser("compute")
    compute.add_argument("--bucket", required=True)
    subparsers.add_parser("status")
    publish = subparsers.add_parser("publish")
    publish.add_argument("--bucket", required=True)
    args = parser.parse_args()
    {"compute": command_compute, "status": command_status,
     "publish": command_publish}[args.command](args)


if __name__ == "__main__":
    main()
