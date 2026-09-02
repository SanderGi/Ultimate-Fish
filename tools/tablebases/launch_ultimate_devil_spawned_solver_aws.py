#!/usr/bin/env python3
"""Stage, launch, and inspect one certifying spawned-only Devil graph."""

from __future__ import annotations

import argparse
import json
import re
import shlex
import subprocess
import time


REGION = "us-west-2"
BUCKET = "ultimatefish-info-20260808-a4e679c6-831688117652"
SOURCE_SHA256 = "b3a358fed09b92d90f43aa23616f3f81eec9589747c70b217681771fc196ed00"
SOURCE_VERSION = "2DWETScmIcUD8S24KxUWZ3vH.02U5SzZ"
SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v6/sha256/{SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v6-source.tar"
)
BINARY_SHA256 = "16f1f86d3ef70fdae3c229bb9102f274a5631999ec694c288ae5bcd682e5cd0f"
BINARY_VERSION = "4x7cBA5XTjJ3vQd1RVQwEo3l5KgxYe2R"
BINARY_KEY = (
    f"sources/binaries/devil-spawned-solver-v6/sha256/{BINARY_SHA256}/"
    "ultimate_tablebase-devil-spawned-v6"
)
SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "max-five-proved-retained-reverse-edges-bellman-parallel-resume-"
    "anonymous-slots-v6"
)
FIXED_SQUARES = frozenset(rank * 8 + file
                          for rank in range(3) for file in range(4))
UNIT_PREFIX = "ultimatefish-devil-spawned-v6-square-"
GENERATION = "v6"
MAX_LIMIT = 2**32 - 1
WORK_ROOT_OVERRIDE: str | None = None
MEMORY_HIGH = 137438953472
MEMORY_MAX = 161061273600


def aws_json(arguments: list[str]) -> dict[str, object]:
    value = json.loads(subprocess.check_output(arguments, text=True))
    if not isinstance(value, dict):
        raise RuntimeError("AWS command did not return an object")
    return value


def send(instance: str, commands: list[str], timeout: int = 300) -> str:
    response = aws_json([
        "aws", "ssm", "send-command", "--region", REGION,
        "--instance-ids", instance, "--document-name", "AWS-RunShellScript",
        "--parameters", json.dumps({"commands": commands}), "--output", "json",
    ])
    command_id = str(response["Command"]["CommandId"])  # type: ignore[index]
    deadline = time.monotonic() + timeout
    while True:
        result = aws_json([
            "aws", "ssm", "get-command-invocation", "--region", REGION,
            "--command-id", command_id, "--instance-id", instance,
            "--output", "json",
        ])
        status = str(result.get("Status"))
        if status == "Success":
            return str(result.get("StandardOutputContent", ""))
        if status not in {"Pending", "InProgress", "Delayed"}:
            raise RuntimeError(
                f"{instance} SSM {status}: "
                f"stdout={result.get('StandardOutputContent', '')!r} "
                f"stderr={result.get('StandardErrorContent', '')!r}"
            )
        if time.monotonic() >= deadline:
            raise RuntimeError(f"{instance} SSM timeout")
        time.sleep(1)


def parse_cpus(value: str) -> tuple[int, ...]:
    cpus: set[int] = set()
    for item in value.split(","):
        match = re.fullmatch(r"([0-9]+)(?:-([0-9]+))?", item)
        if not match:
            raise ValueError(f"invalid CPU set: {value}")
        first = int(match.group(1))
        last = int(match.group(2) or first)
        if first > last or first < 0 or last >= 32:
            raise ValueError(f"invalid CPU set: {value}")
        cpus.update(range(first, last + 1))
    if not cpus:
        raise ValueError("CPU set is empty")
    return tuple(sorted(cpus))


def stage_commands(root: str) -> list[str]:
    binary = f"{root}/ultimate_tablebase-devil-spawned-{GENERATION}"
    source = f"{root}/source.tar"
    return [
        "set -euo pipefail",
        f"install -d -m 0755 {shlex.quote(root)}",
        f"if test ! -f {shlex.quote(source)}; then "
        f"aws s3api get-object --region {REGION} --bucket {BUCKET} "
        f"--key {shlex.quote(SOURCE_KEY)} --version-id "
        f"{shlex.quote(SOURCE_VERSION)} {shlex.quote(source)} >/dev/null; fi",
        f"test \"$(sha256sum {shlex.quote(source)} | cut -d ' ' -f1)\" = "
        f"{SOURCE_SHA256}",
        f"if test ! -f {shlex.quote(binary)}; then "
        f"aws s3api get-object --region {REGION} --bucket {BUCKET} "
        f"--key {shlex.quote(BINARY_KEY)} --version-id "
        f"{shlex.quote(BINARY_VERSION)} {shlex.quote(binary)} >/dev/null; fi",
        f"test \"$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)\" = "
        f"{BINARY_SHA256}",
        f"chmod 0755 {shlex.quote(binary)}",
    ]


def launch(instance: str, square: int, cpus: tuple[int, ...], volume: str,
           limit: int, work_root_override: str | None = None) -> str:
    if square not in FIXED_SQUARES:
        raise ValueError("fixed Devil square must be on files a-d of ranks 1-3")
    if not volume.startswith("/mnt/ultimatefish"):
        raise ValueError("Devil work volume must be an Ultimate Fish mount")
    if (work_root_override is not None and
            not work_root_override.startswith("/mnt/ultimatefish")):
        raise ValueError("Devil retained work root must be an Ultimate Fish mount")
    if limit <= 0 or limit >= MAX_LIMIT:
        raise ValueError(f"Devil graph limit must be in 1..{MAX_LIMIT - 1}")
    root = (f"{volume.rstrip('/')}/devil-spawned-solver-{GENERATION}-"
            f"{SOURCE_SHA256[:8]}")
    work_root = work_root_override or WORK_ROOT_OVERRIDE or root
    work = f"{work_root}/graphs/square-{square}"
    log = f"{work_root}/logs/square-{square}.log"
    binary = f"{root}/ultimate_tablebase-devil-spawned-{GENERATION}"
    unit = f"{UNIT_PREFIX}{square}"
    cpu_set = ",".join(map(str, cpus))
    command = (
        f"exec {shlex.quote(binary)} --piece devil --workers {len(cpus)} "
        f"--solve-devil-spawned-square {square} --devil-spawned-limit {limit} "
        f"--devil-spawned-work {shlex.quote(work)} >>{shlex.quote(log)} 2>&1"
    )
    commands = stage_commands(root) + [
        f"install -d -m 0755 {shlex.quote(work)} "
        f"{shlex.quote(work_root + '/logs')}",
        f"test ! -e {shlex.quote(work + f'/devil-{square}.roots')}",
        f"systemctl is-active --quiet {unit}.service || "
        f"systemd-run --quiet --collect --unit={unit} "
        f"--property=AllowedCPUs={cpu_set} --property=Nice=5 "
        f"--property=MemoryHigh={MEMORY_HIGH} --property=MemoryMax={MEMORY_MAX} "
        "--property=OOMPolicy=stop "
        "--setenv=ULTIMATE_TABLEBASE_PRESERVE_SCRATCH=1 "
        f"/bin/bash -lc {shlex.quote(command)}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "-p AllowedCPUs -p MemoryHigh -p MemoryMax --no-pager",
        f"printf '%s\n' source_sha256={SOURCE_SHA256} "
        f"source_version={SOURCE_VERSION} binary_sha256={BINARY_SHA256} "
        f"binary_version={BINARY_VERSION} semantics={SEMANTICS}",
    ]
    return send(instance, commands, timeout=300)


def status(instance: str, square: int, volume: str,
           work_root_override: str | None = None,
           log_path_override: str | None = None) -> str:
    if (work_root_override is not None and
            not work_root_override.startswith("/mnt/ultimatefish")):
        raise ValueError("Devil retained work root must be an Ultimate Fish mount")
    if (log_path_override is not None and
            not log_path_override.startswith("/mnt/ultimatefish")):
        raise ValueError("Devil log must be on an Ultimate Fish mount")
    unit = f"{UNIT_PREFIX}{square}"
    root = (f"{volume.rstrip('/')}/devil-spawned-solver-{GENERATION}-"
            f"{SOURCE_SHA256[:8]}")
    work_root = work_root_override or WORK_ROOT_OVERRIDE or root
    log = log_path_override or work_root + f"/logs/square-{square}.log"
    return send(instance, [
        f"systemctl status {unit}.service --no-pager -l || true",
        f"tail -n 40 {shlex.quote(log)} || true",
        f"du -sh {shlex.quote(work_root + f'/graphs/square-{square}')} || true",
    ], timeout=300)


def preserve(instance: str, square: int, volume: str,
             work_root_override: str | None = None,
             log_path_override: str | None = None) -> str:
    """Version-preserve and restore-authenticate one completed root fragment."""
    if square not in FIXED_SQUARES:
        raise ValueError("fixed Devil square must be on files a-d of ranks 1-3")
    if not volume.startswith("/mnt/ultimatefish"):
        raise ValueError("Devil work volume must be an Ultimate Fish mount")
    if (work_root_override is not None and
            not work_root_override.startswith("/mnt/ultimatefish")):
        raise ValueError("Devil retained work root must be an Ultimate Fish mount")
    if (log_path_override is not None and
            not log_path_override.startswith("/mnt/ultimatefish")):
        raise ValueError("Devil log must be on an Ultimate Fish mount")
    root = (f"{volume.rstrip('/')}/devil-spawned-solver-{GENERATION}-"
            f"{SOURCE_SHA256[:8]}")
    work_root = work_root_override or WORK_ROOT_OVERRIDE or root
    work = f"{work_root}/graphs/square-{square}"
    fragment = f"{work}/devil-{square}.roots"
    log = log_path_override or f"{work_root}/logs/square-{square}.log"
    prefix = f"results/devil-spawned-solver-{GENERATION}/fragments/square-{square}"
    commands = [
        "set -euo pipefail",
        f"test -s {shlex.quote(fragment)}",
        f"test \"$(stat -c %s {shlex.quote(fragment)})\" -gt 1024",
        f"grep -Fq {shlex.quote(f'DEVIL_SPAWNED_SQUARE_OK square {square} ')} "
        f"{shlex.quote(log)}",
        f"fragment_sha=$(sha256sum {shlex.quote(fragment)} | cut -d ' ' -f1)",
        f"fragment_size=$(stat -c %s {shlex.quote(fragment)})",
        f"fragment_key={shlex.quote(prefix)}/sha256/$fragment_sha/"
        f"{shlex.quote(f'devil-{square}.roots')}",
        f"version=$(aws s3api put-object --region {REGION} --bucket {BUCKET} "
        f"--key \"$fragment_key\" --body {shlex.quote(fragment)} "
        f"--metadata sha256=\"$fragment_sha\",source-sha256={SOURCE_SHA256},"
        f"binary-sha256={BINARY_SHA256},semantics={SEMANTICS} "
        "--query VersionId --output text)",
        "test -n \"$version\" && test \"$version\" != None",
        f"head_sha=$(aws s3api head-object --region {REGION} --bucket {BUCKET} "
        "--key \"$fragment_key\" --version-id \"$version\" "
        "--query 'Metadata.sha256' --output text)",
        f"head_size=$(aws s3api head-object --region {REGION} --bucket {BUCKET} "
        "--key \"$fragment_key\" --version-id \"$version\" "
        "--query ContentLength --output text)",
        "test \"$head_sha\" = \"$fragment_sha\"",
        "test \"$head_size\" = \"$fragment_size\"",
        "restore=$(mktemp /mnt/ultimatefish/devil-fragment-restore.XXXXXX)",
        "trap 'rm -f \"$restore\"' EXIT",
        f"aws s3api get-object --region {REGION} --bucket {BUCKET} "
        "--key \"$fragment_key\" --version-id \"$version\" \"$restore\" >/dev/null",
        "test \"$(sha256sum \"$restore\" | cut -d ' ' -f1)\" = \"$fragment_sha\"",
        f"printf '%s\\n' DEVIL_SPAWNED_FRAGMENT_PRESERVED "
        f"square={square} sha256=\"$fragment_sha\" size=\"$fragment_size\" "
        "version_id=\"$version\" key=\"$fragment_key\"",
    ]
    return send(instance, commands, timeout=900)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--preserve", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--square", required=True, type=int)
    parser.add_argument("--cpus", default="0-31")
    parser.add_argument("--volume", default="/mnt/ultimatefish")
    parser.add_argument(
        "--work-root",
        help=("retained graph/log root when it differs from the immutable "
              "v6 binary staging root"),
    )
    parser.add_argument(
        "--log-path",
        help="authenticated completion log when it is outside the retained graph root",
    )
    parser.add_argument("--limit", type=int, default=4_200_000_000)
    args = parser.parse_args()
    if args.launch:
        print(launch(args.instance, args.square, parse_cpus(args.cpus),
                     args.volume, args.limit, args.work_root))
    elif args.preserve:
        print(preserve(args.instance, args.square, args.volume,
                       args.work_root, args.log_path))
    else:
        print(status(args.instance, args.square, args.volume,
                     args.work_root, args.log_path))


if __name__ == "__main__":
    main()
