#!/usr/bin/env python3
"""Inspect the failed authenticated opposed Penguin/Ghost v8 solve."""

from __future__ import annotations

import argparse

import launch_ultimate_devil_spawned_solver_aws as remote


INSTANCE = "i-03c81f90d2c59a2e7"
UNIT = "ultimatefish-info-kghostkpenguin-concrete-current-v8.service"
ROOT = "/mnt/ultimatefish-penguin/opposed-kghostkpenguin-fresh-v5/work"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--geometry", type=int)
    parser.add_argument("--actual", type=int)
    parser.add_argument("--concrete", action="store_true")
    parser.add_argument("--launch-v9", action="store_true")
    parser.add_argument("--v9", action="store_true")
    parser.add_argument("--launch-v10", action="store_true")
    parser.add_argument("--v10", action="store_true")
    args = parser.parse_args()
    commands = [
        f"systemctl show {UNIT} -p ActiveState -p SubState -p Result "
        "-p ExecMainCode -p ExecMainStatus -p MainPID --no-pager",
        f"journalctl -u {UNIT} --since '-2 hours' --no-pager -n 120 || true",
        f"find {ROOT}/logs -maxdepth 1 -type f -printf '%TY-%Tm-%TdT%TH:%TM:%TS %s %p\\n' "
        "| sort | tail -20",
        f"tail -n 40 {ROOT}/logs/concrete-current-v7.log 2>/dev/null "
        "| cut -c1-1000 || true",
        f"find {ROOT}/solve-concrete-current-v1 -maxdepth 2 -type f "
        "-printf '%s %p\\n' | sort -nr | head -30",
        f"find {ROOT}/results -maxdepth 1 -type f -printf '%s %p\\n' | sort -nr",
        "df -B1 /mnt/ultimatefish-penguin",
        "free -b",
    ] if args.geometry is None else []
    if args.v10:
        commands = [
            "systemctl show ultimatefish-info-kghostkpenguin-terminal-fix-v10.service -p MainPID -p Result -p ExecMainStatus -p ActiveState -p SubState -p CPUUsageNSec -p MemoryCurrent -p AllowedCPUs --no-pager",
            f"tail -n 80 {ROOT}/logs/terminal-fix-v10.log 2>/dev/null || true",
            "journalctl -u ultimatefish-info-kghostkpenguin-terminal-fix-v10.service -n 20 --no-pager",
        ]
    elif args.launch_v10:
        commands = [
            "set -euo pipefail",
            "aws s3api get-object --region us-west-2 --bucket ultimatefish-info-20260808-a4e679c6-831688117652 --key sources/wrappers/ghost-penguin-terminal-fix-v10/sha256/56ee885dbee3a41475cd45792899fa3b1073a28cedc509dcd466798944e3a001/ultimatefish-resume-kghostkpenguin-terminal-fix-v10.sh --version-id nwLeIIG5jyAkZz2vBkng5wvUySQLzjub /usr/local/bin/ultimatefish-resume-kghostkpenguin-terminal-fix-v10.sh >/dev/null",
            "aws s3api get-object --region us-west-2 --bucket ultimatefish-info-20260808-a4e679c6-831688117652 --key sources/units/ghost-penguin-terminal-fix-v10/sha256/0c6a39542f8d433c8703d34b49e0f7d9ffdd9bb7d30c198da9417f689806339f/ultimatefish-info-kghostkpenguin-terminal-fix-v10.service --version-id ZlFhBWI8SfEdQM6DDXKhH_qxIA8_tHnn /etc/systemd/system/ultimatefish-info-kghostkpenguin-terminal-fix-v10.service >/dev/null",
            "test \"$(sha256sum /usr/local/bin/ultimatefish-resume-kghostkpenguin-terminal-fix-v10.sh | cut -d ' ' -f1)\" = 56ee885dbee3a41475cd45792899fa3b1073a28cedc509dcd466798944e3a001",
            "test \"$(sha256sum /etc/systemd/system/ultimatefish-info-kghostkpenguin-terminal-fix-v10.service | cut -d ' ' -f1)\" = 0c6a39542f8d433c8703d34b49e0f7d9ffdd9bb7d30c198da9417f689806339f",
            "chmod 0755 /usr/local/bin/ultimatefish-resume-kghostkpenguin-terminal-fix-v10.sh",
            "systemctl daemon-reload",
            "systemctl start --no-block ultimatefish-info-kghostkpenguin-terminal-fix-v10.service",
            "systemctl show ultimatefish-info-kghostkpenguin-terminal-fix-v10.service -p ActiveState -p AllowedCPUs --no-pager",
        ]
    elif args.v9:
        commands = [
            "systemctl show ultimatefish-info-kghostkpenguin-terminal-fix-v9.service -p MainPID -p Result -p ExecMainStatus -p ActiveState -p SubState -p CPUUsageNSec -p MemoryCurrent -p AllowedCPUs --no-pager",
            f"tail -n 80 {ROOT}/logs/terminal-fix-v9.log 2>/dev/null || true",
            "journalctl -u ultimatefish-info-kghostkpenguin-terminal-fix-v9.service -n 20 --no-pager",
        ]
    elif args.launch_v9:
        commands = [
            "set -euo pipefail",
            "aws s3api get-object --region us-west-2 --bucket ultimatefish-info-20260808-a4e679c6-831688117652 --key sources/wrappers/ghost-penguin-terminal-fix-v9/sha256/d88a1ee1b3ab214c7f2c4d79c2fadd50a79dfc8697f8fdb806af7babfd12fdd4/ultimatefish-resume-kghostkpenguin-terminal-fix-v9.sh --version-id dtmU7nBLzRqC619mf7uVAQAxOBBjWAis /usr/local/bin/ultimatefish-resume-kghostkpenguin-terminal-fix-v9.sh >/dev/null",
            "aws s3api get-object --region us-west-2 --bucket ultimatefish-info-20260808-a4e679c6-831688117652 --key sources/units/ghost-penguin-terminal-fix-v9/sha256/d855a6382da56b0e6d6b9e8499c444703f57b20c34a610ebcbea906cbfe938fb/ultimatefish-info-kghostkpenguin-terminal-fix-v9.service --version-id rhDy49lM_K5eQOOI2HMXbdi1zmmTkiKT /etc/systemd/system/ultimatefish-info-kghostkpenguin-terminal-fix-v9.service >/dev/null",
            "test \"$(sha256sum /usr/local/bin/ultimatefish-resume-kghostkpenguin-terminal-fix-v9.sh | cut -d ' ' -f1)\" = d88a1ee1b3ab214c7f2c4d79c2fadd50a79dfc8697f8fdb806af7babfd12fdd4",
            "test \"$(sha256sum /etc/systemd/system/ultimatefish-info-kghostkpenguin-terminal-fix-v9.service | cut -d ' ' -f1)\" = d855a6382da56b0e6d6b9e8499c444703f57b20c34a610ebcbea906cbfe938fb",
            "chmod 0755 /usr/local/bin/ultimatefish-resume-kghostkpenguin-terminal-fix-v9.sh",
            "systemctl daemon-reload",
            "systemctl start --no-block ultimatefish-info-kghostkpenguin-terminal-fix-v9.service",
            "systemctl show ultimatefish-info-kghostkpenguin-terminal-fix-v9.service -p ActiveState -p AllowedCPUs --no-pager",
        ]
    elif args.concrete:
        commands = [
            "grep -nE 'kghostkpenguin|checkpoint|resume|command|piece2|opposing' /mnt/ultimatefish/concrete-penguin-current-v1/work/run-plan.json | head -n 100",
            "grep -nE 'checkpoint|resume|verify|states|edges|WDL|output|elapsed|error|fatal' /mnt/ultimatefish/concrete-penguin-current-v1/work/logs/generate/kghostkpenguin.log | tail -n 120",
            "sha256sum /mnt/ultimatefish/concrete-penguin-current-v1/work/binary/ultimate_tablebase",
        ]
    if args.geometry is not None:
        if args.actual is None:
            parser.error("--geometry requires --actual")
        inspector = "/tmp/inspect_ultimate_ghost_extra_transition-55a8bd06.py"
        commands.extend([
            "aws s3api get-object --region us-west-2 --bucket "
            "ultimatefish-info-20260808-a4e679c6-831688117652 --key "
            "sources/tools/ghost-extra-transition-inspector/sha256/"
            "55a8bd06b8eda6e71b74dd99df4d3d822fe88dda394215d7c33a01893e57f66a/"
            "inspect_ultimate_ghost_extra_transition.py --version-id "
            f"0rVWHeNmtG_ej98181U6uMrMzew52QWG {inspector} >/dev/null",
            f"test $(sha256sum {inspector} | cut -d' ' -f1) = "
            "55a8bd06b8eda6e71b74dd99df4d3d822fe88dda394215d7c33a01893e57f66a",
        ])
        commands.append(
            f"cd {ROOT.rsplit('/work', 1)[0]} && python3 "
            f"{inspector} "
            f"work/transitions/kghostkpenguin-concrete-current-v1 "
            f"{args.geometry} {args.actual} --table "
            f"work/solve-concrete-current-v1/kghostkpenguin.normalized.uftb "
            "--extra-substates 8 --scratch "
            "work/solve-concrete-current-v1/kghostkpenguin "
            "--root-slot current --bdd-slot b"
        )
        commands.append(
            "python3 -c 'p=open(\""
            f"{ROOT.rsplit('/work', 1)[0]}/work/concrete-current-v1/restore/"
            "tablebases/kghostkpenguin.uftb\",\"rb\"); i=318855161; "
            "p.seek(48+i//4); print(\"original_witness_index\",i,"
            "\"wdl\",(p.read(1)[0]>>(2*(i%4)))&3)'"
        )
    instance = "i-024a2073283e4336e" if args.concrete else INSTANCE
    print(remote.send(instance, commands, timeout=300))


if __name__ == "__main__":
    main()
