#!/usr/bin/env python3
"""Build, test, preserve, and restore-check a Ghost parallel-v8 binary."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as base


SOURCE_SHA256 = "2d1ff830eb622de7880af5961cf1c3e4b6f14817a657da4103b9227a88400be7"
SOURCE_VERSION = "1.mg.armfxwA91tewK8YZvLeUoXonqvj"
SOURCE_KEY = (
    f"sources/bundles/ghost-parallel-v8/sha256/{SOURCE_SHA256}/"
    "ultimatefish-ghost-parallel-v8-source.tar"
)
SEMANTICS = (
    "ghost-public-extra-exact-unit-geometry-dynamic-bellman-"
    "level-ordered-parallel-"
    "robdd-mark-direct-topological-copy-parallel-unique-rebuild-v8"
)
PIECES = {"berserker": "Berserker", "ninja": "Ninja", "queen": "Queen"}


def build(instance: str, piece: str) -> str:
    macro = PIECES[piece]
    root = f"/mnt/ultimatefish/ghost-parallel-v8-{SOURCE_SHA256[:8]}"
    source = f"{root}/source.tar"
    tree = f"{root}/source"
    binary = f"{root}/ultimate_ghost_ordinary_information_tablebase-{piece}-v8"
    compile_sources = (
        "src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp "
        "src/ultimate/tablebases/ghost_ordinary_information_solver.cpp "
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
        f"-DULTIMATE_GHOST_ORDINARY_PIECE={macro} "
        "-DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY " + compile_sources +
        f" -pthread -o {shlex.quote(binary)}",
        f"binary_sha=$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)",
        f"binary_size=$(stat -c %s {shlex.quote(binary)})",
        f"binary_key=sources/binaries/ghost-parallel-v8/{piece}/sha256/"
        f"$binary_sha/ultimate_ghost_ordinary_information_tablebase-{piece}-v8",
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
        "printf '%s\\n' GHOST_PARALLEL_V8_BINARY_PRESERVED "
        "piece=" + piece + " sha256=\"$binary_sha\" size=\"$binary_size\" "
        "version_id=\"$version\" key=\"$binary_key\"",
    ]
    return base.send(instance, [
        f"/bin/bash -lc {shlex.quote(chr(10).join(commands))}"
    ], timeout=1200)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--piece", choices=sorted(PIECES), required=True)
    args = parser.parse_args()
    print(build(args.instance, args.piece))


if __name__ == "__main__":
    main()
