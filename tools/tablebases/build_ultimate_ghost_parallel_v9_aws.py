#!/usr/bin/env python3
"""Build, test, preserve, and restore-check the parallel Berserker/Ghost v9 binary."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base


SOURCE_SHA256 = "56ba686223df241c9762b564dfcfe05f9a99356dee5bec0e15efe8c1f55c760a"
SOURCE_VERSION = "TAlmmqueiOP67bgshs6eVHVETZYNzEn9"
SOURCE_KEY = (
    f"sources/bundles/ghost-parallel-v9/sha256/{SOURCE_SHA256}/"
    "ultimatefish-ghost-parallel-v9-source.tar"
)
SEMANTICS = "ghost-extra-parallel-bellman-and-certification-v9"


def build(instance: str) -> str:
    root = f"/mnt/ultimatefish/ghost-parallel-v9-{SOURCE_SHA256[:8]}"
    source = f"{root}/source.tar"
    tree = f"{root}/source"
    binary = f"{root}/ultimate_ghost_ordinary_information_tablebase-berserker-v9"
    compile_sources = (
        "src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp "
        "src/ultimate/tablebases/ghost_dragon_information_solver.cpp "
        "src/ultimate/tablebases/ghost_public_extra_model.cpp "
        "src/ultimate/tablebases/external_robdd.cpp "
        "src/ultimate/tablebases/ghost_information_probe.cpp "
        "src/ultimate/tablebases/information.cpp src/ultimate/position.cpp "
        "src/ultimate/nnue.cpp"
    )
    commands = [
        "set -euo pipefail",
        f"install -d -m 0755 {shlex.quote(root)} {shlex.quote(tree)}",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        f"--key {shlex.quote(SOURCE_KEY)} --version-id "
        f"{shlex.quote(SOURCE_VERSION)} {shlex.quote(source)} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(source)} | cut -d ' ' -f1)\" = "
        f"{SOURCE_SHA256}",
        f"tar -xf {shlex.quote(source)} -C {shlex.quote(tree)} "
        "--strip-components=1",
        f"cd {shlex.quote(tree)}",
        "g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror "
        "-Isrc/ultimate -Isrc/ultimate/tablebases "
        "tests/tablebases/ultimate_external_robdd.cpp "
        "src/ultimate/tablebases/external_robdd.cpp -pthread "
        f"-o {shlex.quote(root + '/ultimate_external_robdd_test')}",
        shlex.quote(root + "/ultimate_external_robdd_test"),
        "clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic "
        "-Werror -Wno-error=range-loop-construct -include sstream "
        "-Isrc/ultimate -Isrc/ultimate/tablebases "
        "-DULTIMATE_GHOST_ORDINARY_PIECE=Berserker "
        "-DULTIMATE_GHOST_EXTRA_PIECE_TOKEN=Berserker "
        "-DGhostDragonExact=GhostOrdinaryExact "
        "-DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY " + compile_sources +
        f" -pthread -o {shlex.quote(binary)}",
        f"binary_sha=$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)",
        f"binary_size=$(stat -c %s {shlex.quote(binary)})",
        "binary_key=sources/binaries/ghost-parallel-v9/berserker/sha256/"
        "$binary_sha/ultimate_ghost_ordinary_information_tablebase-berserker-v9",
        f"version=$(aws s3api put-object --region {base.REGION} "
        f"--bucket {base.BUCKET} --key \"$binary_key\" "
        f"--body {shlex.quote(binary)} --metadata sha256=\"$binary_sha\","
        f"source-sha256={SOURCE_SHA256},semantics={SEMANTICS} "
        "--query VersionId --output text)",
        "test -n \"$version\" && test \"$version\" != None",
        f"restore=$(mktemp {shlex.quote(root + '/restore.XXXXXX')})",
        "trap 'rm -f \"$restore\"' EXIT",
        f"aws s3api get-object --region {base.REGION} --bucket {base.BUCKET} "
        "--key \"$binary_key\" --version-id \"$version\" "
        "\"$restore\" >/dev/null",
        "test \"$(sha256sum \"$restore\" | cut -d ' ' -f1)\" = "
        "\"$binary_sha\"",
        "printf '%s\\n' GHOST_PARALLEL_V9_BINARY_PRESERVED "
        "sha256=\"$binary_sha\" size=\"$binary_size\" "
        "version_id=\"$version\" key=\"$binary_key\"",
    ]
    return base.send(instance, [
        f"/bin/bash -lc {shlex.quote(chr(10).join(commands))}"
    ], timeout=1200)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    print(build(args.instance))


if __name__ == "__main__":
    main()
