#!/usr/bin/env python3
"""Stage and launch the corrected first-three-ranks K+Devil-v-K closure."""

from __future__ import annotations

import argparse
import json
import shlex
import subprocess
import time


REGION = "us-west-2"
BUCKET = "ultimatefish-info-20260808-a4e679c6-831688117652"
INSTANCE = "i-08c0f44a1776cb34a"
SOURCE_SHA256 = "9429b8bbd1001acf49f26b6085d709796ab2388b320ed9ae46947479858634c9"
SOURCE_VERSION = "5Xxzz52cxOKFol0Hiok8UvYyOEIjyD2l"
SOURCE_KEY = (f"sources/bundles/devil-closure-v5/sha256/{SOURCE_SHA256}/"
              "ultimatefish-devil-closure-v5-source.tar")
SEMANTICS = (
    "no-preexisting-minions-devil-first-three-ranks-cooldown-"
    "exact-bitboard-codec-v5"
)
ROOT = f"/mnt/ultimatefish/devil-closure-v5-{SOURCE_SHA256[:8]}"
UNIT = "ultimatefish-devil-single-frontier-v5"
CPUS = tuple(range(0, 12))
SHARDS = 992
DEPTH = 12
STATE_LIMIT = 50_000_000
HIGH_CAP_UNIT = "ultimatefish-devil-single-highcap-v5"
HIGH_CAP_CPUS = (0, 1, 2, 3)
HIGH_CAP_DEPTH = 12
HIGH_CAP_LIMIT = 50_000_000


def aws_json(arguments: list[str]) -> dict[str, object]:
    value = json.loads(subprocess.check_output(arguments, text=True))
    if not isinstance(value, dict):
        raise RuntimeError("AWS command did not return an object")
    return value


def send(commands: list[str], timeout: int = 300) -> str:
    response = aws_json([
        "aws", "ssm", "send-command", "--region", REGION,
        "--instance-ids", INSTANCE, "--document-name", "AWS-RunShellScript",
        "--parameters", json.dumps({"commands": commands}), "--output", "json",
    ])
    command_id = str(response["Command"]["CommandId"])  # type: ignore[index]
    deadline = time.monotonic() + timeout
    while True:
        result = aws_json([
            "aws", "ssm", "get-command-invocation", "--region", REGION,
            "--command-id", command_id, "--instance-id", INSTANCE,
            "--output", "json",
        ])
        status = str(result.get("Status"))
        if status == "Success":
            return str(result.get("StandardOutputContent", ""))
        if status not in {"Pending", "InProgress", "Delayed"}:
            raise RuntimeError(f"SSM {status}: {result.get('StandardErrorContent', '')}")
        if time.monotonic() >= deadline:
            raise RuntimeError("SSM timeout")
        time.sleep(1)


def launch() -> str:
    source = f"{ROOT}/source"
    archive = f"{ROOT}/source.tar"
    binary = f"{ROOT}/ultimate_tablebase-devil-v5"
    logs = f"{ROOT}/single-frontier"
    marker = f"{logs}/COMPLETE"
    build_sources = (
        "src/ultimate/tablebases/tablebase.cpp", "src/ultimate/position.cpp",
        "src/ultimate/tablebases/tablebase_probe.cpp",
        "src/ultimate/tablebases/information.cpp",
        "src/ultimate/tablebases/information_solver.cpp", "src/ultimate/nnue.cpp",
    )
    compile_command = " ".join(map(shlex.quote, (
        "clang++", "-std=c++17", "-O3", "-DNDEBUG", "-Wall", "-Wextra",
        "-Wpedantic", "-Werror", "-Wno-error=range-loop-construct", "-pthread",
        "-Isrc/ultimate", *build_sources, "-o", binary)))
    children = []
    for shard, cpu in enumerate(CPUS):
        child = f"ultimatefish-devil-single-frontier-v5-{shard:02d}"
        log = f"{logs}/shard-{shard:02d}.log"
        command = (
            "set -euo pipefail; "
            f"{shlex.quote(binary)} --piece devil --workers 1 "
            f"--devil-frontier-depth {DEPTH} --devil-frontier-limit {STATE_LIMIT} "
            f"--devil-frontier-shard {shard} --devil-frontier-shards {SHARDS} "
            f">{shlex.quote(log)} 2>&1"
        )
        children.append(
            f"systemctl is-active --quiet {child}.service || "
            f"systemd-run --quiet --collect --unit={child} "
            f"--property=AllowedCPUs={cpu} --property=Nice=10 "
            f"--property=OOMPolicy=stop /bin/bash -lc {shlex.quote(command)}")
    parent = "\n".join((
        "set -euo pipefail", *children,
        "while test \"$(systemctl list-units "
        "'ultimatefish-devil-single-frontier-v5-*.service' "
        "--state=active --no-legend | wc -l)\" -ne 0; do sleep 5; done",
        f"test \"$(grep -h -c '^DEVIL_CLOSURE_CENSUS_OK ' {logs}/shard-*.log "
        f"| awk '{{s+=$1}} END{{print s+0}}')\" = {len(CPUS)}",
        f"printf '%s\\n' source_sha256={SOURCE_SHA256} semantics={SEMANTICS} "
        f"depth={DEPTH} state_limit={STATE_LIMIT} sample_shards={len(CPUS)} "
        f">{marker}.tmp",
        f"mv {marker}.tmp {marker}",
    ))
    stage = "\n".join((
        "set -euo pipefail", f"mkdir -p {ROOT} {source} {logs}",
        f"aws s3api get-object --region {REGION} --bucket {BUCKET} --key {SOURCE_KEY} "
        f"--version-id {SOURCE_VERSION} {archive} >/dev/null",
        f"test \"$(sha256sum {archive} | cut -d' ' -f1)\" = {SOURCE_SHA256}",
        f"tar -xf {archive} -C {source}",
        f"test \"$(python3 -c 'import json; print(len(json.load(open(\"{source}/bundle-manifest.json\"))[\"files\"]))')\" = 16",
        f"cd {source}", compile_command,
        f"binary_sha=$(sha256sum {binary} | cut -d' ' -f1)",
        f"binary_key=sources/binaries/devil-closure-v5/sha256/$binary_sha/{binary.rsplit('/', 1)[-1]}",
        f"binary_version=$(aws s3api put-object --region {REGION} --bucket {BUCKET} "
        f"--key $binary_key --body {binary} --metadata sha256=$binary_sha,source-sha256={SOURCE_SHA256},semantics={SEMANTICS} --query VersionId --output text)",
        f"if test -s {marker}; then echo COMPLETE; "
        f"elif systemctl is-active --quiet {UNIT}.service; then echo RUNNING; "
        f"else systemd-run --quiet --collect --unit={UNIT} "
        f"--property=AllowedCPUs=0-11 --property=Nice=10 "
        f"--property=OOMPolicy=stop /bin/bash -lc {shlex.quote(parent)}; echo STARTED; fi",
        "echo DEVIL_SOURCE_SHA256=" + SOURCE_SHA256,
        "echo DEVIL_BINARY_SHA256=$binary_sha",
        "echo DEVIL_BINARY_VERSION_ID=$binary_version",
        "echo DEVIL_BINARY_KEY=$binary_key",
    ))
    return send([stage], timeout=300)


def status() -> str:
    logs = f"{ROOT}/single-frontier"
    return send([
        f"systemctl show {UNIT}.service --property=ActiveState,SubState,Result,"
        "ExecMainStatus,AllowedCPUs,CPUUsageNSec,MemoryCurrent --no-pager; "
        "echo active_children=$(systemctl list-units "
        "'ultimatefish-devil-single-frontier-v5-*.service' "
        "--state=active --no-legend | wc -l); "
        f"echo completed_samples=$(grep -h -c '^DEVIL_CLOSURE_CENSUS_OK ' "
        f"{logs}/shard-*.log 2>/dev/null | awk '{{s+=$1}} END{{print s+0}}'); "
        "for unit in $(systemctl list-units "
        "'ultimatefish-devil-single-frontier-v5-*.service' "
        "--state=active --no-legend | awk '{print $1}'); do "
        "printf '%s ' \"$unit\"; systemctl show \"$unit\" "
        "--property=AllowedCPUs,CPUUsageNSec,MemoryCurrent --value | "
        "tr '\\n' ' '; echo; done; "
        "grep -E '^(MemAvailable|MemTotal):' /proc/meminfo; "
        f"tail -n 12 {logs}/shard-00.log 2>/dev/null || true; "
        f"ls -l {logs}/COMPLETE 2>/dev/null || true"])


def stop() -> str:
    return send([
        f"systemctl stop {UNIT}.service || true; "
        "systemctl stop 'ultimatefish-devil-single-frontier-v5-*.service' || true; "
        "echo STOPPED_INCOMPLETE_COOLDOWN_DOMAIN"
    ])


def launch_high_cap() -> str:
    binary = f"{ROOT}/ultimate_tablebase-devil-v5"
    logs = f"{ROOT}/single-highcap"
    expected_binary = "962e634cc3903d915cfe90bc68ac5f487184f622b4cd8d855ce0dfb11f81bf5b"
    children = []
    for shard, cpu in enumerate(HIGH_CAP_CPUS):
        child = f"ultimatefish-devil-single-highcap-v5-{shard:02d}"
        log = f"{logs}/shard-{shard:02d}.log"
        command = (
            "set -euo pipefail; "
            f"{shlex.quote(binary)} --piece devil --workers 1 "
            f"--devil-frontier-depth {HIGH_CAP_DEPTH} "
            f"--devil-frontier-limit {HIGH_CAP_LIMIT} "
            f"--devil-frontier-shard {shard} --devil-frontier-shards {SHARDS} "
            f">{shlex.quote(log)} 2>&1"
        )
        children.append(
            f"systemctl is-active --quiet {child}.service || "
            f"systemd-run --quiet --collect --unit={child} "
            f"--property=AllowedCPUs={cpu} --property=Nice=10 "
            f"--property=OOMPolicy=stop /bin/bash -lc {shlex.quote(command)}")
    parent = "\n".join((
        "set -euo pipefail", *children,
        "while test \"$(systemctl list-units "
        "'ultimatefish-devil-single-highcap-v5-*.service' "
        "--state=active --no-legend | wc -l)\" -ne 0; do sleep 5; done",
        f"test \"$(grep -h -c '^DEVIL_CLOSURE_CENSUS_OK ' {logs}/shard-*.log "
        f"| awk '{{s+=$1}} END{{print s+0}}')\" = {len(HIGH_CAP_CPUS)}",
        f"printf '%s\\n' source_sha256={SOURCE_SHA256} semantics={SEMANTICS} "
        f"depth={HIGH_CAP_DEPTH} state_limit={HIGH_CAP_LIMIT} "
        f"sample_shards={len(HIGH_CAP_CPUS)} >{logs}/COMPLETE.tmp",
        f"mv {logs}/COMPLETE.tmp {logs}/COMPLETE",
    ))
    return send([
        "set -euo pipefail",
        f"test \"$(sha256sum {binary} | cut -d' ' -f1)\" = {expected_binary}",
        f"mkdir -p {logs}",
        f"if test -s {logs}/COMPLETE; then echo COMPLETE; "
        f"elif systemctl is-active --quiet {HIGH_CAP_UNIT}.service; then "
        f"echo RUNNING; else systemd-run --quiet --collect "
        f"--unit={HIGH_CAP_UNIT} --property=AllowedCPUs=0-3 "
        f"--property=Nice=10 --property=OOMPolicy=stop /bin/bash -lc "
        f"{shlex.quote(parent)}; echo STARTED; fi",
        f"echo DEVIL_BINARY_SHA256={expected_binary}",
        f"echo DEVIL_HIGH_CAP_LIMIT={HIGH_CAP_LIMIT}",
    ])


def status_high_cap() -> str:
    logs = f"{ROOT}/single-highcap"
    return send([
        f"systemctl show {HIGH_CAP_UNIT}.service --property=ActiveState,SubState,"
        "Result,ExecMainStatus,AllowedCPUs,CPUUsageNSec,MemoryCurrent --no-pager; "
        "echo active_children=$(systemctl list-units "
        "'ultimatefish-devil-single-highcap-v5-*.service' "
        "--state=active --no-legend | wc -l); "
        f"echo completed_samples=$(grep -h -c '^DEVIL_CLOSURE_CENSUS_OK ' "
        f"{logs}/shard-*.log 2>/dev/null | awk '{{s+=$1}} END{{print s+0}}'); "
        f"tail -n 12 {logs}/shard-00.log 2>/dev/null || true; "
        f"ls -l {logs}/COMPLETE 2>/dev/null || true"])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--stop", action="store_true")
    mode.add_argument("--launch-high-cap", action="store_true")
    mode.add_argument("--status-high-cap", action="store_true")
    args = parser.parse_args()
    if args.launch:
        output = launch()
    elif args.stop:
        output = stop()
    elif args.launch_high_cap:
        output = launch_high_cap()
    elif args.status_high_cap:
        output = status_high_cap()
    else:
        output = status()
    print(output)


if __name__ == "__main__":
    main()
