#!/usr/bin/env python3
"""Build, preserve, and restore-check the restartable Devil v11 binary."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base


# Filled only after the deterministic 16-source bundle is uploaded. Keeping
# the exact VersionId here makes an accidental build from a mutable key fail.
SOURCE_SHA256 = "bb04e578ce251a304bca3eafd329ed49a04aed7656a29f7431fef8a1d7cca400"
SOURCE_VERSION = "vKTPMe4bbaWdgkmQ6ms8CkP9AIK7g0Fi"
SOURCE_KEY = (
    f"sources/bundles/devil-spawned-solver-v11/sha256/{SOURCE_SHA256}/"
    "ultimatefish-devil-spawned-solver-v11-source.tar"
)
SEMANTICS = (
    "first-three-ranks-no-preexisting-minions-fixed-square-disjoint-"
    "max-five-proved-retained-reverse-edges-bellman-parallel-resume-"
    "anonymous-slots-wide-indices-stateless-secondary-square-bitpacked-"
    "restartable-parent-shards-child-locality-buckets-v11"
)


def build(instance: str) -> str:
    if SOURCE_SHA256.startswith("__") or SOURCE_VERSION.startswith("__"):
        raise RuntimeError("Devil v11 source bundle is not version-pinned")
    root = f"/mnt/ultimatefish/devil-spawned-solver-v11-{SOURCE_SHA256[:8]}"
    source = f"{root}/source.tar"
    tree = f"{root}/source"
    binary = f"{root}/ultimate_tablebase-devil-spawned-v11"
    compile_command = (
        "g++ -Isrc/ultimate -Isrc/ultimate/tablebases -std=c++17 -O3 "
        "-DNDEBUG -Wall -Wextra -Wpedantic "
        "src/ultimate/tablebases/tablebase.cpp src/ultimate/position.cpp "
        "src/ultimate/tablebases/tablebase_probe.cpp "
        "src/ultimate/tablebases/information.cpp "
        "src/ultimate/tablebases/information_solver.cpp src/ultimate/nnue.cpp "
        f"-o {shlex.quote(binary)}"
    )
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
        compile_command,
        f"binary_sha=$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)",
        f"binary_size=$(stat -c %s {shlex.quote(binary)})",
        "binary_key=sources/binaries/devil-spawned-solver-v11/sha256/"
        "$binary_sha/ultimate_tablebase-devil-spawned-v11",
        f"version=$(aws s3api put-object --region {base.REGION} "
        f"--bucket {base.BUCKET} --key \"$binary_key\" "
        f"--body {shlex.quote(binary)} --metadata sha256=\"$binary_sha\","
        f"source-sha256={SOURCE_SHA256},semantics={SEMANTICS} "
        "--query VersionId --output text)",
        "test -n \"$version\" && test \"$version\" != None",
        "restore=$(mktemp /mnt/ultimatefish/devil-v11-binary.XXXXXX)",
        "trap 'rm -f \"$restore\"' EXIT",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        "--key \"$binary_key\" --version-id \"$version\" "
        "\"$restore\" >/dev/null",
        "test \"$(sha256sum \"$restore\" | cut -d ' ' -f1)\" = \"$binary_sha\"",
        "printf '%s\\n' DEVIL_V11_BINARY_PRESERVED sha256=\"$binary_sha\" "
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
