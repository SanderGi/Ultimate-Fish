#!/usr/bin/env python3
"""Stage and launch the exact Devil-spawned-Minions-only closure census."""

from __future__ import annotations

import argparse
import json
import shlex
import subprocess
import time


REGION = "us-west-2"
BUCKET = "ultimatefish-info-20260808-a4e679c6-831688117652"
INSTANCE = "i-08c0f44a1776cb34a"
SOURCE_SHA256 = "2e7fecc251cff69ddb2bfd956e2dfd40092d495cf45fb0ac652cc4f5a096e05d"
SOURCE_VERSION = "mVPh8Cl0zQN.ULjSnSATgkpjcCamR8IN"
SOURCE_KEY = (f"sources/bundles/devil-closure-v1/sha256/{SOURCE_SHA256}/"
              "ultimatefish-devil-closure-v1-source.tar")
SEMANTICS = "no-preexisting-minions-devil-spawned-only-v1"
ROOT = f"/mnt/ultimatefish/devil-closure-v1-{SOURCE_SHA256[:8]}"
UNIT = "ultimatefish-devil-closure-queen-same-census-v1"
CPUS = tuple((*range(0, 14), *range(15, 32)))
SHARDS = 992
HIGH_CAP_CPUS = tuple((*range(0, 8), *range(15, 19)))
HIGH_CAP_LIMIT = 10_000_000
HIGH_CAP_UNIT = "ultimatefish-devil-closure-queen-depth2-highcap-v1"
DEPTH3_LIMIT = 20_000_000
DEPTH3_UNIT = "ultimatefish-devil-closure-queen-depth3-sample-v1"


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
    binary = f"{ROOT}/ultimate_tablebase-devil-v1"
    marker = f"{ROOT}/CENSUS.COMPLETE"
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
    launch_lines = []
    for slot, cpu in enumerate(CPUS):
        child = f"ultimatefish-devil-closure-queen-census-{slot:02d}"
        log = f"{ROOT}/shard-slot-{slot:02d}.log"
        loop = (
            "set -euo pipefail; "
            f"for ((shard={slot}; shard<{SHARDS}; shard+=31)); do "
            f"{shlex.quote(binary)} --piece devil --piece2 queen --workers 1 "
            "--devil-frontier-depth 2 --devil-frontier-limit 2000000 "
            f"--devil-frontier-shard $shard --devil-frontier-shards {SHARDS}; "
            f"done >>{shlex.quote(log)} 2>&1"
        )
        launch_lines.append(
            f"systemctl is-active --quiet {child}.service || "
            f"systemd-run --quiet --collect --unit={child} "
            f"--property=AllowedCPUs={cpu} --property=Nice=10 "
            f"--property=OOMPolicy=stop /bin/bash -lc {shlex.quote(loop)}")
    parent = "\n".join((
        "set -euo pipefail", *launch_lines,
        "while test \"$(systemctl list-units "
        "'ultimatefish-devil-closure-queen-census-*.service' "
        "--state=active --no-legend | wc -l)\" -ne 0; do sleep 5; done",
        f"test \"$(grep -h -c '^DEVIL_CLOSURE_CENSUS_OK ' {ROOT}/shard-slot-*.log | awk '{{s+=$1}} END{{print s+0}}')\" = {SHARDS}",
        f"printf '%s\\n' source_sha256={SOURCE_SHA256} semantics={SEMANTICS} shards={SHARDS} >{marker}.tmp",
        f"mv {marker}.tmp {marker}",
    ))
    stage = "\n".join((
        "set -euo pipefail", f"mkdir -p {ROOT} {source}",
        f"aws s3api get-object --region {REGION} --bucket {BUCKET} --key {SOURCE_KEY} "
        f"--version-id {SOURCE_VERSION} {archive} >/dev/null",
        f"test \"$(sha256sum {archive} | cut -d' ' -f1)\" = {SOURCE_SHA256}",
        f"tar -xf {archive} -C {source}",
        f"test \"$(python3 -c 'import json; print(len(json.load(open(\"{source}/bundle-manifest.json\"))[\"files\"]))')\" = 16",
        f"cd {source}", compile_command,
        f"binary_sha=$(sha256sum {binary} | cut -d' ' -f1)",
        f"binary_key=sources/binaries/devil-closure-v1/sha256/$binary_sha/{binary.rsplit('/', 1)[-1]}",
        f"binary_version=$(aws s3api put-object --region {REGION} --bucket {BUCKET} "
        f"--key $binary_key --body {binary} --metadata sha256=$binary_sha,source-sha256={SOURCE_SHA256},semantics={SEMANTICS} --query VersionId --output text)",
        f"if test -s {marker}; then echo COMPLETE; "
        f"elif systemctl is-active --quiet {UNIT}.service; then echo RUNNING; "
        f"else systemd-run --quiet --collect --unit={UNIT} "
        f"--property=AllowedCPUs=0-13,15-31 --property=Nice=10 "
        f"--property=OOMPolicy=stop /bin/bash -lc {shlex.quote(parent)}; echo STARTED; fi",
        "echo DEVIL_SOURCE_SHA256=" + SOURCE_SHA256,
        "echo DEVIL_BINARY_SHA256=$binary_sha",
        "echo DEVIL_BINARY_VERSION_ID=$binary_version",
        "echo DEVIL_BINARY_KEY=$binary_key",
    ))
    return send([stage], timeout=300)


def status() -> str:
    return send([
        f"systemctl show {UNIT}.service --property=ActiveState,SubState,Result,ExecMainStatus,AllowedCPUs,CPUUsageNSec,MemoryCurrent --no-pager; "
        f"echo active_children=$(systemctl list-units 'ultimatefish-devil-closure-queen-census-*.service' --state=active --no-legend | wc -l); "
        f"echo completed_shards=$(grep -h -c '^DEVIL_CLOSURE_CENSUS_OK ' {ROOT}/shard-slot-*.log 2>/dev/null | awk '{{s+=$1}} END{{print s+0}}'); "
        f"tail -n 12 {ROOT}/shard-slot-00.log 2>/dev/null || true; "
        f"ls -l {ROOT}/CENSUS.COMPLETE 2>/dev/null || true"])


def launch_probe(depth: int, limit: int, label: str, unit: str) -> str:
    """Launch a bounded sample that sizes one Devil closure BFS layer."""
    binary = f"{ROOT}/ultimate_tablebase-devil-v1"
    logs = f"{ROOT}/{label}"
    expected_binary = "d2f4202810dd8006d3674e7e383c48c2015bc036dd84e081b59fec4fd27a2965"
    children = []
    for shard, cpu in enumerate(HIGH_CAP_CPUS):
        child = f"ultimatefish-devil-queen-{label}-{shard:02d}"
        log = f"{logs}/shard-{shard:02d}.log"
        command = (
            "set -euo pipefail; "
            f"{shlex.quote(binary)} --piece devil --piece2 queen --workers 1 "
            f"--devil-frontier-depth {depth} "
            f"--devil-frontier-limit {limit} "
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
        f"'ultimatefish-devil-queen-{label}-*.service' "
        "--state=active --no-legend | wc -l)\" -ne 0; do sleep 5; done",
        f"test \"$(grep -h -c '^DEVIL_CLOSURE_CENSUS_OK ' {logs}/shard-*.log "
        f"| awk '{{s+=$1}} END{{print s+0}}')\" = {len(HIGH_CAP_CPUS)}",
        f"printf '%s\\n' source_sha256={SOURCE_SHA256} "
        f"semantics={SEMANTICS} depth={depth} state_limit={limit} "
        f"sample_shards={len(HIGH_CAP_CPUS)} >{logs}/COMPLETE.tmp",
        f"mv {logs}/COMPLETE.tmp {logs}/COMPLETE",
    ))
    return send([
        "set -euo pipefail",
        f"test \"$(sha256sum {binary} | cut -d' ' -f1)\" = {expected_binary}",
        f"mkdir -p {logs}",
        f"if test -s {logs}/COMPLETE; then echo COMPLETE; "
        f"elif systemctl is-active --quiet {unit}.service; then "
        f"echo RUNNING; else systemd-run --quiet --collect "
        f"--unit={unit} --property=AllowedCPUs=0-7,15-18 "
        f"--property=Nice=10 --property=OOMPolicy=stop /bin/bash -lc "
        f"{shlex.quote(parent)}; echo STARTED; fi",
        f"echo DEVIL_BINARY_SHA256={expected_binary}",
        f"echo DEVIL_PROBE_DEPTH={depth}",
        f"echo DEVIL_STATE_LIMIT={limit}",
        f"echo DEVIL_SAMPLE_SHARDS={len(HIGH_CAP_CPUS)}",
    ])


def status_probe(label: str, unit: str) -> str:
    logs = f"{ROOT}/{label}"
    return send([
        f"systemctl show {unit}.service "
        "--property=ActiveState,SubState,Result,ExecMainStatus,AllowedCPUs,"
        "CPUUsageNSec,MemoryCurrent --no-pager; "
        "echo active_children=$(systemctl list-units "
        f"'ultimatefish-devil-queen-{label}-*.service' "
        "--state=active --no-legend | wc -l); "
        f"echo completed_samples=$(grep -h -c '^DEVIL_CLOSURE_CENSUS_OK ' "
        f"{logs}/shard-*.log 2>/dev/null | awk '{{s+=$1}} END{{print s+0}}'); "
        f"tail -n 4 {logs}/shard-00.log 2>/dev/null || true; "
        f"ls -l {logs}/COMPLETE 2>/dev/null || true"])


def launch_high_cap() -> str:
    return launch_probe(2, HIGH_CAP_LIMIT, "highcap-depth2", HIGH_CAP_UNIT)


def status_high_cap() -> str:
    return status_probe("highcap-depth2", HIGH_CAP_UNIT)


def launch_depth3() -> str:
    return launch_probe(3, DEPTH3_LIMIT, "depth3-sample", DEPTH3_UNIT)


def status_depth3() -> str:
    return status_probe("depth3-sample", DEPTH3_UNIT)


def stop_depth3() -> str:
    return send([
        f"systemctl stop {DEPTH3_UNIT}.service || true; "
        "systemctl stop 'ultimatefish-devil-queen-depth3-sample-*.service' || true; "
        "echo STOPPED_INVALID_DOMAIN"
    ])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    mode.add_argument("--launch-high-cap", action="store_true")
    mode.add_argument("--status-high-cap", action="store_true")
    mode.add_argument("--launch-depth3", action="store_true")
    mode.add_argument("--status-depth3", action="store_true")
    mode.add_argument("--stop-depth3", action="store_true")
    args = parser.parse_args()
    if args.launch:
        output = launch()
    elif args.status:
        output = status()
    elif args.launch_high_cap:
        output = launch_high_cap()
    elif args.status_high_cap:
        output = status_high_cap()
    elif args.launch_depth3:
        output = launch_depth3()
    elif args.stop_depth3:
        output = stop_depth3()
    else:
        output = status_depth3()
    print(output)


if __name__ == "__main__":
    main()
