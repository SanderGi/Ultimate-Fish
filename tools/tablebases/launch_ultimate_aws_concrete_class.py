#!/usr/bin/env python3
"""Launch one ledger-approved concrete class on an already staged EC2 host.

Hosts share one authenticated source root and dependency cache.  This command
selects exactly one inventory index, performs the runner's plan-only preflight
remotely, starts one bounded systemd unit on adjacent CPUs, and only
then changes the canonical ledger row from PLANNED to COMPUTING.  The transient
unit is deliberately retained after exit, and its post-launch state is checked,
so an immediate failure remains inspectable and cannot be published as live.
One to sixteen CPUs let the generator's deterministic frontier, reverse, and
verification scans use the admitted worker count; the retrograde queue remains
single-threaded. Completion is handled by
``finalize_ultimate_aws_concrete_class.py``. Sharing one graph lets CPU-heavy
Angel domains avoid stranding cores behind per-process memory limits.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import time

import run_ultimate_concrete_tablebase_shard_aws as concrete
import update_ultimate_tablebase_ledger as ledger


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "tablebases/README.md"
PLOT = ROOT / "tools/tablebases/plot_ultimate_tablebases.py"
SAFE_UNIT = re.compile(r"[A-Za-z0-9_.@-]+\Z")
def ledger_filename(filename: str) -> str:
    return concrete.ledger_filename(filename)


def aws(*arguments: str) -> str:
    return subprocess.run(
        ["aws", *arguments], check=True, text=True, capture_output=True).stdout


def selected(args: argparse.Namespace) -> tuple[dict[str, object], str]:
    rows = concrete.wave_inventory(args.wave)
    if not 0 <= args.index < len(rows):
        raise ValueError("class index is outside the selected wave")
    record = rows[args.index]
    ledger_rows = {
        row.filename: row for row in ledger.entries(args.readme.read_text())}
    row = ledger_rows[ledger_filename(str(record["filename"]))]
    allowed = {"planned"}
    if args.allow_computing_retry:
        allowed.add("computing")
    if row.status not in allowed:
        raise ValueError(
            f"{row.filename}: launch requires "
            f"{'PLANNED or COMPUTING' if args.allow_computing_retry else 'PLANNED'}, "
            f"got {row.status.upper()}")
    return record, row.status


def validate(args: argparse.Namespace) -> None:
    for value, label in ((args.source_root, "source root"),
                         (args.dependencies, "dependencies"),
                         (args.dependency_manifest, "dependency manifest"),
                         (args.work_directory, "work directory")):
        if not value.startswith("/") or "\n" in value:
            raise ValueError(f"unsafe {label}")
    if (not SAFE_UNIT.fullmatch(args.unit) or args.cpu < 0 or
            not 1 <= args.cpu_count <= 16 or
            min(args.scratch_limit, args.resident_limit, args.memory_max,
                args.reverse_edge_bytes_limit, args.minimum_free_bytes) <= 0):
        raise ValueError("invalid unit, CPU, or resource gate")
    if args.memory_max < args.resident_limit:
        raise ValueError("memory max must be at least the measured resident limit")
    if not args.s3_prefix.startswith("s3://"):
        raise ValueError("S3 prefix must start with s3://")


def remote_commands(args: argparse.Namespace,
                    record: dict[str, object]) -> list[str]:
    runner = f"{args.source_root}/tools/tablebases/" \
             "run_ultimate_concrete_tablebase_shard_aws.py"
    common = [
        "/usr/bin/python3", runner,
        "--work-directory", args.work_directory,
        "--dependencies", args.dependencies,
        "--dependency-manifest", args.dependency_manifest,
        "--wave", str(args.wave), "--range-begin", str(args.index),
        "--range-end", str(args.index + 1),
        "--workers", str(args.cpu_count),
    ]
    plan = shlex.join(common)
    full = [
        *common, "--full", "--aws-execution-ack", "EC2",
        "--scratch-limit", str(args.scratch_limit),
        "--resident-limit", str(args.resident_limit),
        "--reverse-edge-bytes-limit", str(args.reverse_edge_bytes_limit),
        "--minimum-free-bytes", str(args.minimum_free_bytes),
        "--monitor-interval", str(args.monitor_interval),
        "--s3-prefix", args.s3_prefix,
    ]
    cpu_end = args.cpu + args.cpu_count - 1
    cpu_set = str(args.cpu) if args.cpu_count == 1 else f"{args.cpu}-{cpu_end}"
    unit = [
        "systemd-run", "--unit", args.unit,
        f"--property=AllowedCPUs={cpu_set}",
        f"--property=MemoryMax={args.memory_max}",
        f"--property=CPUQuota={args.cpu_count * 100}%",
        "--property=Nice=5",
        *full,
    ]
    expected = str(record["filename"])
    plan_path = f"/tmp/{args.unit}.plan.json"
    check = (
        "import json;"
        f"p=json.load(open({plan_path!r}));"
        f"assert p['generator_model_sha256']=={args.expected_model_sha256!r};"
        f"assert not p['missing_dependencies'];"
        f"assert p['selected'][0]['filename']=={expected!r}"
    )
    dependency_checks = [
        f"test -f {shlex.quote(str(Path(args.dependencies) / name))}"
        for name in concrete.class_dependency_filenames(record)
    ]
    completion = str(
        Path(args.work_directory) / "certificates/wave-certificate.json")
    probe = (
        "import pathlib,subprocess;"
        f"u={args.unit!r};"
        "r=subprocess.run(['systemctl','show',u,'--property=LoadState',"
        "'--property=ActiveState','--property=Result',"
        "'--property=ExecMainStatus'],check=True,text=True,"
        "capture_output=True);"
        "s=dict(x.split('=',1) for x in r.stdout.splitlines() if '=' in x);"
        "assert s.get('LoadState')=='loaded',s;"
        "a=s.get('ActiveState');"
        f"done=pathlib.Path({completion!r}).is_file();"
        "assert a in ('active','activating') or "
        "(a=='inactive' and s.get('Result')=='success' and "
        "s.get('ExecMainStatus')=='0' and done),s"
    )
    return [
        "set -euo pipefail",
        f"test -f {shlex.quote(runner)}",
        f"test -f {shlex.quote(args.dependency_manifest)}",
        *dependency_checks,
        f"test ! -e {shlex.quote(args.work_directory)}",
        f"{plan} >{shlex.quote(plan_path)}",
        shlex.join(["python3", "-c", check]),
        shlex.join(unit),
        "sleep 2",
        shlex.join(["python3", "-c", probe]),
    ]


def send_and_wait(args: argparse.Namespace, commands: list[str]) -> str:
    parameters = json.dumps({"commands": commands}, separators=(",", ":"))
    response = json.loads(aws(
        "ssm", "send-command", "--instance-ids", args.instance,
        "--document-name", "AWS-RunShellScript", "--region", args.region,
        "--parameters", parameters, "--output", "json"))
    command_id = response["Command"]["CommandId"]
    deadline = time.monotonic() + args.timeout
    while True:
        try:
            invocation = json.loads(aws(
                "ssm", "get-command-invocation", "--command-id", command_id,
                "--instance-id", args.instance, "--region", args.region,
                "--output", "json"))
        except subprocess.CalledProcessError:
            invocation = {"Status": "Pending"}
        status = invocation.get("Status")
        if status == "Success":
            return command_id
        if status in {"Cancelled", "Cancelling", "Failed", "TimedOut"}:
            raise RuntimeError(
                f"remote launch {status}: "
                f"{invocation.get('StandardErrorContent', '')}")
        if time.monotonic() >= deadline:
            raise TimeoutError("remote launch did not finish in time")
        time.sleep(2)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--wave", type=int, choices=(0, 1, 2), required=True)
    parser.add_argument("--index", type=int, required=True)
    parser.add_argument("--unit", required=True)
    parser.add_argument(
        "--cpu", type=int, required=True,
        help="first logical CPU in the unit's adjacent CPU set",
    )
    parser.add_argument(
        "--cpu-count", type=int, choices=range(1, 17), default=1,
        help="adjacent CPUs to allocate to the parallel graph scans",
    )
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--dependencies", required=True)
    parser.add_argument("--dependency-manifest", required=True)
    parser.add_argument("--work-directory", required=True)
    parser.add_argument("--scratch-limit", type=int, required=True)
    parser.add_argument("--resident-limit", type=int, required=True)
    parser.add_argument("--memory-max", type=int, required=True)
    parser.add_argument("--reverse-edge-bytes-limit", type=int, required=True)
    parser.add_argument("--minimum-free-bytes", type=int, required=True)
    parser.add_argument("--monitor-interval", type=float, default=10)
    parser.add_argument("--s3-prefix", required=True)
    parser.add_argument("--expected-model-sha256", required=True)
    parser.add_argument(
        "--allow-computing-retry", action="store_true",
        help="allow a fresh non-duplicate retry for a row already marked COMPUTING",
    )
    parser.add_argument("--region", default="us-west-2")
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--readme", type=Path, default=README)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    validate(args)
    record, prior_status = selected(args)
    command_id = send_and_wait(args, remote_commands(args, record))
    if prior_status == "planned":
        ledger.update(
            args.readme,
            [f'{ledger_filename(str(record["filename"]))}=computing'], [],
        )
        subprocess.run(["python3", str(PLOT)], cwd=ROOT, check=True)
    print(json.dumps({
        "filename": record["filename"], "instance": args.instance,
        "unit": args.unit, "command_id": command_id,
        "cpu_begin": args.cpu, "cpu_count": args.cpu_count,
        "status": "computing",
    }, sort_keys=True))


if __name__ == "__main__":
    main()
