#!/usr/bin/env python3
"""Build and launch the corrected opposed Ghost/Penguin concrete source."""

from __future__ import annotations

import argparse
import shlex

import launch_ultimate_devil_spawned_solver_aws as aws


INSTANCE = "i-024a2073283e4336e"
SOURCE_SHA256 = "a66ab20828a2f981737f68efcb41f8b08323319136f8273737cf215e1ed25ec0"
SOURCE_VERSION = "IJy9WzKY87DKDCpTcE.mo1SvOs1c.Zwi"
SOURCE_KEY = (
    f"sources/bundles/penguin-terminal-fix-v1/sha256/{SOURCE_SHA256}/"
    "ultimatefish-penguin-terminal-fix-v1-source.tar"
)
ROOT = "/mnt/ultimatefish/penguin-terminal-fix-v1-a66ab208"
UNIT = "ultimatefish-concrete-kghostkpenguin-terminal-fix-v1.service"
WITNESS = 318855161


def commands() -> list[str]:
    source = f"{ROOT}/source.tar"
    tree = f"{ROOT}/source"
    binary = f"{ROOT}/ultimate_tablebase"
    work = f"{ROOT}/work"
    output = f"{work}/kghostkpenguin.uftb"
    log = f"{work}/solve.log"
    dependency = "/mnt/ultimatefish/concrete-penguin-current-v1/dependencies"
    run = (
        f"env ULTIMATE_TABLEBASE_PATH={shlex.quote(dependency)} "
        f"{shlex.quote(binary)} --piece ghost --piece2 penguin --opposing "
        f"--workers 8 --output {shlex.quote(output)} "
        f"--checkpoint {shlex.quote(work + '/checkpoint')} "
        f"--checkpoint-every 0 --disk-backed >>{shlex.quote(log)} 2>&1; "
        f"byte=$(od -An -tu1 -j $((48+{WITNESS}/4)) -N1 {shlex.quote(output)}); "
        f"test $(( (byte >> (({WITNESS}%4)*2)) & 3 )) = 1; "
        f"grep -Fq 'verifyok states 607326720' {shlex.quote(log)}; "
        f"table_sha=$(sha256sum {shlex.quote(output)} | cut -d ' ' -f1); "
        "table_key=results/concrete/penguin-terminal-fix-v1/sha256/"
        "$table_sha/kghostkpenguin.uftb; "
        f"version=$(aws s3api put-object --region {aws.REGION} "
        f"--bucket {aws.BUCKET} --key \"$table_key\" "
        f"--body {shlex.quote(output)} --metadata sha256=\"$table_sha\","
        f"source-sha256={SOURCE_SHA256},witness-index={WITNESS},witness-wdl=1 "
        "--query VersionId --output text); "
        "printf '%s\\n' PENGUIN_TERMINAL_FIX_COMPLETE "
        "sha256=\"$table_sha\" version_id=\"$version\" key=\"$table_key\""
    )
    return [
        "set -euo pipefail",
        f"install -d -m 0755 {shlex.quote(tree)} {shlex.quote(work)}",
        f"if test ! -f {shlex.quote(source)}; then aws s3api get-object --region {aws.REGION} --bucket {aws.BUCKET} "
        f"--key {shlex.quote(SOURCE_KEY)} --version-id "
        f"{shlex.quote(SOURCE_VERSION)} {shlex.quote(source)} >/dev/null; fi",
        f"test \"$(sha256sum {shlex.quote(source)} | cut -d ' ' -f1)\" = "
        f"{SOURCE_SHA256}",
        f"if test ! -f {shlex.quote(tree + '/src/ultimate/tablebases/tablebase.cpp')}; then tar -xf {shlex.quote(source)} -C {shlex.quote(tree)}; fi",
        f"cd {shlex.quote(tree)}",
        "taskset -c 24-31 clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra "
        "-Wpedantic -Isrc/ultimate src/ultimate/tablebases/tablebase.cpp "
        "src/ultimate/position.cpp src/ultimate/tablebases/tablebase_probe.cpp "
        "src/ultimate/tablebases/information.cpp "
        "src/ultimate/tablebases/information_solver.cpp src/ultimate/nnue.cpp "
        f"-o {shlex.quote(binary)}",
        f"binary_sha=$(sha256sum {shlex.quote(binary)} | cut -d ' ' -f1)",
        "binary_key=sources/binaries/penguin-terminal-fix-v1/sha256/"
        "$binary_sha/ultimate_tablebase",
        f"binary_version=$(aws s3api put-object --region {aws.REGION} "
        f"--bucket {aws.BUCKET} --key \"$binary_key\" "
        f"--body {shlex.quote(binary)} --metadata sha256=\"$binary_sha\"," 
        f"source-sha256={SOURCE_SHA256} --query VersionId --output text)",
        "systemctl set-property --runtime "
        "ultimatefish-devil-companion-v23-same-bishop-square-1.service "
        "AllowedCPUs=4-23",
        f"systemd-run --unit={UNIT.removesuffix('.service')} --property=AllowedCPUs=24-31 "
        "--property=MemoryHigh=30064771072 --property=MemoryMax=34359738368 "
        f"/bin/bash -lc {shlex.quote(run)}",
        f"printf '%s\\n' PENGUIN_TERMINAL_FIX_LAUNCHED unit={UNIT} "
        "source_sha256=" + SOURCE_SHA256 + " binary_sha256=$binary_sha "
        "binary_version=$binary_version cpus=24-31",
    ]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", default=INSTANCE)
    parser.add_argument("--inspect", action="store_true")
    parser.add_argument("--preserve", action="store_true")
    args = parser.parse_args()
    if args.preserve:
        output = f"{ROOT}/work/kghostkpenguin.uftb"
        print(aws.send(args.instance, [
            "set -euo pipefail",
            f"byte=$(od -An -tu1 -j $((48+{WITNESS}/4)) -N1 {output})",
            f"test $(( (byte >> (({WITNESS}%4)*2)) & 3 )) = 1",
            f"grep -Fq 'verifyok states 607326720' {ROOT}/work/solve.log",
            f"table_sha=$(sha256sum {output} | cut -d ' ' -f1)",
            "table_key=results/concrete/penguin-terminal-fix-v1/sha256/$table_sha/kghostkpenguin.uftb",
            f"version=$(aws s3api put-object --region {aws.REGION} --bucket {aws.BUCKET} --key \"$table_key\" --body {output} --metadata sha256=\"$table_sha\",source-sha256={SOURCE_SHA256},witness-index={WITNESS},witness-wdl=1 --query VersionId --output text)",
            f"restore=$(mktemp {ROOT}/restore.XXXXXX)",
            f"aws s3api get-object --region {aws.REGION} --bucket {aws.BUCKET} --key \"$table_key\" --version-id \"$version\" \"$restore\" >/dev/null",
            "test \"$(sha256sum \"$restore\" | cut -d ' ' -f1)\" = \"$table_sha\"",
            "printf '%s\\n' PENGUIN_TERMINAL_FIX_PRESERVED sha256=\"$table_sha\" version_id=\"$version\" key=\"$table_key\" witness_wdl=1",
        ], timeout=1200))
    elif args.inspect:
        print(aws.send(args.instance, [
            f"systemctl show {UNIT} -p ActiveState -p Result -p ExecMainStatus -p AllowedCPUs -p MemoryCurrent -p CPUUsageNSec --no-pager",
            f"systemctl show {UNIT} -p MainPID --value | xargs -r ps -o pid,etimes,pcpu,pmem,rss,stat,wchan:24 -p",
            f"stat -c 'log_mtime=%Y log_bytes=%s' {ROOT}/work/solve.log 2>/dev/null || true",
            f"tail -n 30 {ROOT}/work/solve.log 2>/dev/null || true",
            f"journalctl -u {UNIT} -n 20 --no-pager 2>/dev/null || true",
            f"if test -f {ROOT}/work/kghostkpenguin.uftb; then stat -c 'bytes=%s' {ROOT}/work/kghostkpenguin.uftb; fi",
            f"if test -f {ROOT}/work/kghostkpenguin.uftb; then byte=$(od -An -tu1 -j $((48+{WITNESS}/4)) -N1 {ROOT}/work/kghostkpenguin.uftb); echo witness_wdl=$(( (byte >> (({WITNESS}%4)*2)) & 3 )); fi",
            f"if test -f {ROOT}/work/kghostkpenguin.uftb; then table_sha=$(sha256sum {ROOT}/work/kghostkpenguin.uftb | cut -d ' ' -f1); echo table_sha=$table_sha; fi",
        ]))
    else:
        print(aws.send(args.instance, commands(), timeout=1200))


if __name__ == "__main__":
    main()
