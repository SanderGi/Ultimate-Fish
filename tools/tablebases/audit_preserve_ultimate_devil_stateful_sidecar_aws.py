#!/usr/bin/env python3
"""Exhaustively census, preserve, and restore-authenticate a Devil UFDS file."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

import launch_ultimate_devil_spawned_solver_aws as base


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tools/tablebases/audit_ultimate_devil_stateful_sidecar.cpp"
SCHEMA = "ultimate-devil-stateful-census-v1"
PREFIX = "sources/devil-stateful-census-v1"
UNIT_PREFIX = "ultimatefish-audit-preserve-devil-stateful-census-v1-square-"


def cpu_count(specification: str) -> int:
    values: set[int] = set()
    for part in specification.split(","):
        bounds = part.split("-", 1)
        first = int(bounds[0])
        last = int(bounds[-1])
        if first < 0 or last < first:
            raise ValueError("invalid CPU set")
        values.update(range(first, last + 1))
    if not values:
        raise ValueError("empty CPU set")
    return len(values)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def upload_source() -> tuple[str, str, str]:
    source_sha = sha256(SOURCE)
    key = f"{PREFIX}/source/sha256/{source_sha}/{SOURCE.name}"
    response = json.loads(subprocess.check_output([
        "aws", "s3api", "put-object", "--region", base.REGION,
        "--bucket", base.BUCKET, "--key", key, "--body", str(SOURCE),
        "--metadata", f"schema={SCHEMA},sha256={source_sha}", "--output", "json",
    ], text=True))
    version = str(response.get("VersionId", ""))
    if not version:
        raise RuntimeError("source upload lacks VersionId")
    return source_sha, key, version


def launch(instance: str, square: int, root: str, cpus: str,
           sidecar_sha256: str, sidecar_version_id: str,
           memory_high: int, memory_max: int) -> str:
    if square not in base.FIXED_SQUARES:
        raise ValueError("invalid fixed Devil square")
    if not root.startswith("/mnt/ultimatefish"):
        raise ValueError("retained Devil root must be on an Ultimate Fish mount")
    if len(sidecar_sha256) != 64 or any(c not in "0123456789abcdef" for c in sidecar_sha256):
        raise ValueError("invalid sidecar SHA-256")
    if not sidecar_version_id:
        raise ValueError("sidecar VersionId is required")
    if memory_high <= 0 or memory_max <= 0 or memory_high > memory_max:
        raise ValueError("invalid memory gates")
    source_sha, source_key, source_version = upload_source()
    stage = f"{root.rstrip('/')}/stateful-census-v1-{source_sha[:8]}"
    source = f"{stage}/{SOURCE.name}"
    binary = f"{stage}/audit_ultimate_devil_stateful_sidecar"
    sidecar = f"{root.rstrip('/')}/graphs/square-{square}/devil-{square}.ufds"
    census = f"{root.rstrip('/')}/graphs/square-{square}/devil-{square}.ufds.census.json"
    log = f"{root.rstrip('/')}/graphs/square-{square}/devil-{square}.ufds.census.log"
    unit = f"{UNIT_PREFIX}{square}"
    enrich_filter = (
        ". + {sidecar_sha256:$sidecar_sha256,"
        "sidecar_version_id:$sidecar_version_id,source_sha256:$source_sha256,"
        "binary_sha256:$binary_sha256,binary_version_id:$binary_version_id,"
        "restore_residual:0,never_delete:true}"
    )
    build = " && ".join([
        "set -euo pipefail",
        f"install -d -m 0755 {shlex.quote(stage)}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(source_key)} --version-id {shlex.quote(source_version)} "
        f"{shlex.quote(source)} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(source)} | cut -d ' ' -f1)\" = {source_sha}",
        f"g++ -std=c++17 -O3 -DNDEBUG -pthread -Wall -Wextra -Wpedantic -Werror "
        f"{shlex.quote(source)} -o {shlex.quote(binary + '.tmp')}",
        f"mv {shlex.quote(binary + '.tmp')} {shlex.quote(binary)}",
        f"binary_sha=$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)",
        f"binary_key={PREFIX}/binary/sha256/$binary_sha/audit_ultimate_devil_stateful_sidecar",
        f"binary_version=$(aws s3api put-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key \"$binary_key\" --body {shlex.quote(binary)} --metadata "
        f"schema={SCHEMA},sha256=$binary_sha,source-sha256={source_sha} --query VersionId --output text)",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key \"$binary_key\" --version-id \"$binary_version\" {shlex.quote(binary + '.restore')} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(binary + '.restore')} | cut -d ' ' -f1)\" = \"$binary_sha\"",
        f"rm -f {shlex.quote(binary + '.restore')}",
        f"printf '%s\\n' binary_sha256=\"$binary_sha\" binary_version_id=\"$binary_version\" "
        f">{shlex.quote(stage + '/binary.receipt')}",
    ])
    base.send(instance, [build], timeout=900)
    command = " && ".join([
        "set -euo pipefail",
        f"test -s {shlex.quote(sidecar)}",
        f"test \"$(sha256sum {shlex.quote(sidecar)} | cut -d ' ' -f1)\" = {sidecar_sha256}",
        f"{shlex.quote(binary)} --input {shlex.quote(sidecar)} --square {square} "
        f"--workers {cpu_count(cpus)} >{shlex.quote(census + '.tmp')}",
        f"jq -e '.schema == \"{SCHEMA}\" and .square == {square} and "
        ".conservation_residual == 0 and .sorted_key_residual == 0' "
        f"{shlex.quote(census + '.tmp')} >/dev/null",
        f"binary_sha=$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)",
        f"binary_version=$(cut -d= -f2 {shlex.quote(stage + '/binary.receipt')} | tail -n1)",
        f"jq --sort-keys --arg sidecar_sha256 {sidecar_sha256} "
        f"--arg sidecar_version_id {shlex.quote(sidecar_version_id)} "
        f"--arg source_sha256 {source_sha} --arg binary_sha256 \"$binary_sha\" "
        f"--arg binary_version_id \"$binary_version\" {shlex.quote(enrich_filter)} "
        f"{shlex.quote(census + '.tmp')} >{shlex.quote(census)}",
        f"rm -f {shlex.quote(census + '.tmp')}",
        f"receipt_sha=$(sha256sum {shlex.quote(census)} | cut -d ' ' -f1)",
        f"receipt_key=results/devil-stateful-v1/censuses/square-{square}/sha256/$receipt_sha/devil-{square}.json",
        f"receipt_version=$(aws s3api put-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key \"$receipt_key\" --body {shlex.quote(census)} --metadata "
        f"schema={SCHEMA},sha256=$receipt_sha,sidecar-sha256={sidecar_sha256} --query VersionId --output text)",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key \"$receipt_key\" --version-id \"$receipt_version\" {shlex.quote(census + '.restore')} >/dev/null",
        f"cmp -s {shlex.quote(census)} {shlex.quote(census + '.restore')}",
        f"rm -f {shlex.quote(census + '.restore')}",
        f"printf '%s\\n' DEVIL_STATEFUL_CENSUS_PRESERVED square={square} "
        f"sha256=\"$receipt_sha\" version_id=\"$receipt_version\" >>{shlex.quote(log)}",
    ])
    return base.send(instance, [
        f"systemctl is-active --quiet {unit}.service || systemd-run --quiet --collect "
        f"--unit={unit} --property=AllowedCPUs={shlex.quote(cpus)} "
        f"--property=MemoryHigh={memory_high} --property=MemoryMax={memory_max} "
        f"--property=Nice=10 /bin/bash -lc {shlex.quote(command + ' >>' + shlex.quote(log) + ' 2>&1')}",
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result --no-pager",
        f"printf '%s\\n' census={shlex.quote(census)} log={shlex.quote(log)}",
    ])


def status(instance: str, square: int, root: str) -> str:
    work = f"{root.rstrip('/')}/graphs/square-{square}"
    unit = f"{UNIT_PREFIX}{square}"
    census = f"{work}/devil-{square}.ufds.census.json"
    log = f"{work}/devil-{square}.ufds.census.log"
    return base.send(instance, [
        f"systemctl show {unit}.service -p ActiveState -p SubState -p Result "
        "-p MainPID -p CPUUsageNSec -p MemoryCurrent --no-pager || true",
        f"tail -n 20 {shlex.quote(log)} 2>/dev/null || true",
        f"test ! -s {shlex.quote(census)} || cat {shlex.quote(census)}",
        f"find {shlex.quote(root.rstrip('/'))} -maxdepth 2 -path "
        "'*/stateful-census-v1-*/binary.receipt' -type f -exec cat {} \\; 2>/dev/null || true",
    ])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--launch", action="store_true")
    mode.add_argument("--status", action="store_true")
    parser.add_argument("--instance", required=True)
    parser.add_argument("--square", type=int, required=True)
    parser.add_argument("--root", required=True)
    parser.add_argument("--cpus", default="0-15")
    parser.add_argument("--sidecar-sha256")
    parser.add_argument("--sidecar-version-id")
    parser.add_argument("--memory-high", type=int, default=34359738368)
    parser.add_argument("--memory-max", type=int, default=51539607552)
    args = parser.parse_args()
    if args.launch:
        if not args.sidecar_sha256 or not args.sidecar_version_id:
            parser.error("--launch requires sidecar SHA-256 and VersionId")
        print(launch(args.instance, args.square, args.root, args.cpus,
                     args.sidecar_sha256, args.sidecar_version_id,
                     args.memory_high, args.memory_max))
    else:
        print(status(args.instance, args.square, args.root))


if __name__ == "__main__":
    main()
