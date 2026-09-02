#!/usr/bin/env python3
"""Build and preserve the exact Berserker/Ghost edge-witness probe."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base


SOURCE_SHA256 = "9dd55ab62de368e48529320022b99e0f61ea48e3ec1c0627800d341140285161"
SOURCE_VERSION = "KaDWBB1Xrzpa69Ux5.yUEWpb8n7TWQcC"
SOURCE_KEY = (
    "sources/bundles/ghost-terminal-metadata-rebind-v12-diagnostic/sha256/"
    f"{SOURCE_SHA256}/ultimatefish-ghost-terminal-rebind-v12-diagnostic-source.tar"
)
SEMANTICS = "ghost-berserker-lower-edge-witness-diagnostic-v12"


def build(instance: str) -> str:
    root = f"/mnt/ultimatefish/ghost-terminal-rebind-v12-{SOURCE_SHA256[:8]}"
    source = f"{root}/source.tar"
    tree = f"{root}/source-diagnostic"
    binary = f"{root}/ultimate_ghost_berserker_information_tablebase-v12-diagnostic"
    installed = (
        "/mnt/ultimatefish/berserker-ghost-opposed-restore-v12/bin/"
        "ultimate_ghost_berserker_information_tablebase-v12-diagnostic"
    )
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
        f"rm -rf {shlex.quote(tree)}",
        f"install -d -m 0755 {shlex.quote(tree)}",
        f"tar -xf {shlex.quote(source)} -C {shlex.quote(tree)} --strip-components=1",
        f"cd {shlex.quote(tree)}",
        "clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic "
        "-Werror -Wno-error=range-loop-construct -include sstream "
        "-Isrc/ultimate -Isrc/ultimate/tablebases "
        "-DULTIMATE_GHOST_ORDINARY_PIECE=Berserker "
        "-DULTIMATE_GHOST_EXTRA_PIECE_TOKEN=Berserker "
        "-DULTIMATE_GHOST_EXTRA_SUBSTATES=10 "
        "-DGhostDragonExact=GhostOrdinaryExact "
        "-DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY " + compile_sources
        + f" -pthread -o {shlex.quote(binary)}",
        f"binary_sha=$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)",
        f"binary_size=$(stat -c %s {shlex.quote(binary)})",
        "binary_key=sources/binaries/ghost-terminal-metadata-rebind-v12-diagnostic/"
        "sha256/$binary_sha/ultimate_ghost_berserker_information_tablebase-v12-diagnostic",
        f"version=$(aws s3api put-object --region {base.REGION} "
        f"--bucket {base.BUCKET} --key \"$binary_key\" "
        f"--body {shlex.quote(binary)} --metadata sha256=\"$binary_sha\","
        f"source-sha256={SOURCE_SHA256},semantics={SEMANTICS} "
        "--query VersionId --output text)",
        "test -n \"$version\" && test \"$version\" != None",
        f"install -m 0755 {shlex.quote(binary)} {shlex.quote(installed)}",
        f"printf '%s\\n' GHOST_BERSERKER_DIAGNOSTIC_V12_PRESERVED "
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
