#!/usr/bin/env python3
"""Build, preserve, and restore-check the denser Devil v18 binary."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base


SOURCE_SHA256 = "ab8f3616b1687be61402f6ebb8cabb6dd0175da9e59e894f9f35b5231f6544c8"
SOURCE_VERSION = "rRA2WfMgxGhDwx8jVk3cB9gZBVh1gXgg"
SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v18/sha256/{SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v18-source.tar"
)
SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "retained-reverse-edges-bellman-parallel-resume-seven-byte-key-"
    "stripe-aligned-packed-hash-four-bit-exact-fingerprint-filter-"
    "90-percent-load-v18"
)


def build(instance: str) -> str:
    root = f"/mnt/ultimatefish/devil-spawned-solver-v18-{SOURCE_SHA256[:8]}"
    source = f"{root}/source.tar"
    tree = f"{root}/source"
    binary = f"{root}/ultimate_tablebase-devil-spawned-v18"
    commands = [
        "set -euo pipefail",
        f"install -d -m 0755 {shlex.quote(root)} {shlex.quote(tree)}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(SOURCE_KEY)} --version-id "
        f"{shlex.quote(SOURCE_VERSION)} {shlex.quote(source)} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(source)} | cut -d ' ' -f1)\" = "
        f"{SOURCE_SHA256}",
        f"tar -xf {shlex.quote(source)} -C {shlex.quote(tree)}",
        f"cd {shlex.quote(tree)}",
        "taskset -c 22-30 g++ -Isrc/ultimate -Isrc/ultimate/tablebases "
        "-std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic "
        "src/ultimate/tablebases/tablebase.cpp src/ultimate/position.cpp "
        "src/ultimate/tablebases/tablebase_probe.cpp "
        "src/ultimate/tablebases/information.cpp "
        "src/ultimate/tablebases/information_solver.cpp src/ultimate/nnue.cpp "
        f"-o {shlex.quote(binary)}",
        "reverse_test=$(mktemp -d /mnt/ultimatefish/devil-v18-reverse.XXXXXX)",
        "migration_test=$(mktemp -d /mnt/ultimatefish/devil-v18-migrate.XXXXXX)",
        f"taskset -c 22-30 {shlex.quote(binary)} "
        '--devil-reverse-self-test "$reverse_test"',
        f"taskset -c 22-30 {shlex.quote(binary)} "
        '--devil-checkpoint-migration-self-test "$migration_test"',
        f"binary_sha=$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)",
        f"binary_size=$(stat -c %s {shlex.quote(binary)})",
        "binary_key=sources/binaries/devil-spawned-solver-v18/sha256/"
        "$binary_sha/ultimate_tablebase-devil-spawned-v18",
        f"version=$(aws s3api put-object --region {base.REGION} "
        f"--bucket {base.BUCKET} --key \"$binary_key\" "
        f"--body {shlex.quote(binary)} --metadata sha256=\"$binary_sha\"," 
        f"source-sha256={SOURCE_SHA256},semantics={SEMANTICS} "
        "--query VersionId --output text)",
        "test -n \"$version\" && test \"$version\" != None",
        "restore=$(mktemp /mnt/ultimatefish/devil-v18-binary.XXXXXX)",
        "trap 'rm -f \"$restore\"; rm -rf \"$reverse_test\" "
        "\"$migration_test\"' EXIT",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        "--key \"$binary_key\" --version-id \"$version\" "
        "\"$restore\" >/dev/null",
        "test \"$(sha256sum \"$restore\" | cut -d ' ' -f1)\" = "
        "\"$binary_sha\"",
        "printf '%s\\n' DEVIL_V18_BINARY_PRESERVED sha256=\"$binary_sha\" "
        "size=\"$binary_size\" version_id=\"$version\" key=\"$binary_key\"",
    ]
    return base.send(instance, commands, timeout=1200)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build(args.instance))


if __name__ == "__main__":
    main()
