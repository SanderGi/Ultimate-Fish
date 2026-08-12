#!/usr/bin/env python3
"""Audit, preserve, and import one completed AWS concrete tablebase.

The expensive class solve is never repeated.  This command accepts the host
and work directory of one successful class runner, runs the native full-causal
reachability audit against the retained output, uploads that text sidecar, and
installs the exact W/L/D split plus all three versioned-S3 bindings in the
canonical README ledger.  Hidden-information rows fail closed here: their
concrete worlds are dependencies, not publishable public results.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import time
from typing import Mapping

import plan_ultimate_tablebases as plan
import run_ultimate_concrete_tablebase_shard_aws as concrete
import update_ultimate_tablebase_ledger as ledger


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "tablebases/README.md"
AUDIT = re.compile(
    r"reachability side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)")
TOTAL = re.compile(
    r"reachability_total side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)")


def record_for(filename: str) -> Mapping[str, object]:
    rows = (*plan.inventory(0), *concrete.supported_inventory())
    matches = [row for row in rows if row["filename"] == filename]
    if not matches:
        raise ValueError(f"unknown concrete class: {filename}")
    # A few historically requested Penguin fixtures also appear in the
    # complete stateful inventory.  Their phase/note labels differ, but their
    # material codec and extent are identical and therefore name one result.
    identity = ("filename", "primary", "secondary", "opposing", "states",
                "packed_bytes")
    signatures = {
        json.dumps({key: row.get(key) for key in identity}, sort_keys=True)
        for row in matches
    }
    if len(signatures) != 1:
        raise ValueError(f"ambiguous concrete class: {filename}")
    return matches[0]


def counts(pattern: re.Pattern[str], text: str) -> list[list[int]]:
    result = [[0, 0, 0, 0] for _ in range(2)]
    matches = pattern.findall(text)
    if len(matches) != 2:
        raise ValueError("native reachability output lacks two side rows")
    for side, unknown, win, loss, draw in matches:
        result[int(side)] = list(map(int, (unknown, win, loss, draw)))
    if any(side[0] for side in result):
        raise ValueError("native reachability output contains unknown states")
    return result


def render(total: list[int], unreachable: list[int]) -> str:
    values: list[str] = []
    for whole, omitted in zip(total[1:], unreachable[1:]):
        if not 0 <= omitted <= whole:
            raise ValueError("unreachable W/L/D exceeds encoded total")
        legal = whole - omitted
        values.append(f"{legal:,}" + (f" ({omitted:,})" if omitted else ""))
    return " / ".join(values)


def aws(*arguments: str) -> str:
    return subprocess.run(
        ["aws", *arguments], check=True, text=True, capture_output=True).stdout


def preflight(args: argparse.Namespace) -> None:
    rows = {row.filename: row for row in ledger.entries(args.readme.read_text())}
    row = rows[args.filename]
    if row.result_kind != "concrete":
        raise ValueError(
            f"{args.filename}: hidden-information row cannot be concrete-certified")
    if row.status == "certified":
        raise ValueError(f"{args.filename}: already CERTIFIED; refusing a second upload")


def remote_script(args: argparse.Namespace, record: Mapping[str, object]) -> str:
    command = ["$work/binary/ultimate_tablebase", "--piece",
               str(record["primary"]), "--checkpoint-every", "0"]
    if record.get("secondary"):
        command += ["--piece2", str(record["secondary"])]
    if record.get("opposing"):
        command.append("--opposing")
    command += ["--audit-reachability", f"$work/outputs/{args.filename}"]
    audit_command = " ".join(command)
    unit_check = ""
    if args.unit:
        unit_check = (
            f'if systemctl cat {args.unit} >/dev/null 2>&1; then\n'
            f'  test "$(systemctl show {args.unit} -p ActiveState --value)" = inactive\n'
            f'  test "$(systemctl show {args.unit} -p Result --value)" = success\n'
            f'fi\n')
    prefix = args.s3_prefix.strip("/")
    return f"""set -euo pipefail
work={args.work_directory}
test -f "$work/certificates/wave-certificate.json"
test -f "$work/outputs/{args.filename}"
{unit_check}{audit_command} >"$work/{Path(args.filename).stem}.reachability-v2.txt"
sidecar="$work/{Path(args.filename).stem}.reachability-v2.txt"
side_sha=$(sha256sum "$sidecar" | cut -d' ' -f1)
cert_sha=$(sha256sum "$work/certificates/wave-certificate.json" | cut -d' ' -f1)
cert_key={prefix}/concrete/v2/certificates/sha256/$cert_sha/wave-certificate.json
side_key={prefix}/reachability/sha256/$side_sha/{Path(args.filename).stem}.reachability-v2.txt
aws s3api head-object --bucket {args.bucket} --key "$cert_key" --region {args.region} >"$work/certificate-head.json"
if aws s3api head-object --bucket {args.bucket} --key "$side_key" --region {args.region} >"$work/reachability-put.json" 2>/dev/null; then
  python3 -c 'import json,sys; h=json.load(open(sys.argv[1])); assert h["Metadata"]["sha256"] == sys.argv[2]' "$work/reachability-put.json" "$side_sha"
else
  aws s3api put-object --bucket {args.bucket} --key "$side_key" --body "$sidecar" --metadata sha256=$side_sha,predicate=native-full-causal-reachability --region {args.region} >"$work/reachability-put.json"
fi
echo __ULTIMATE_CERTIFICATE__
cat "$work/certificates/wave-certificate.json"
echo __ULTIMATE_CERTIFICATE_HEAD__
cat "$work/certificate-head.json"
echo __ULTIMATE_REACHABILITY__
cat "$sidecar"
echo __ULTIMATE_REACHABILITY_PUT__
cat "$work/reachability-put.json"
echo __ULTIMATE_KEYS__
printf '%s\\n%s\\n%s\\n%s\\n' "$cert_sha" "$cert_key" "$side_sha" "$side_key"
"""


def wait_for_command(args: argparse.Namespace, command_id: str) -> str:
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
            return str(invocation.get("StandardOutputContent", ""))
        if status in {"Cancelled", "Cancelling", "Failed", "TimedOut"}:
            raise RuntimeError(
                f"remote finalization {status}: "
                f"{invocation.get('StandardErrorContent', '')}")
        if time.monotonic() >= deadline:
            raise TimeoutError("remote finalization did not finish in time")
        time.sleep(3)


def send_and_wait(args: argparse.Namespace, script: str) -> str:
    parameters = json.dumps({"commands": [script]}, separators=(",", ":"))
    response = json.loads(aws(
        "ssm", "send-command", "--instance-ids", args.instance,
        "--document-name", "AWS-RunShellScript", "--region", args.region,
        "--parameters", parameters, "--output", "json"))
    return wait_for_command(args, response["Command"]["CommandId"])


def section(text: str, begin: str, end: str) -> str:
    try:
        return text.split(begin + "\n", 1)[1].split("\n" + end, 1)[0]
    except IndexError as error:
        raise ValueError(f"remote finalization lacks {begin}") from error


def import_result(args: argparse.Namespace, output: str,
                  record: Mapping[str, object]) -> dict[str, object]:
    certificate = json.loads(section(
        output, "__ULTIMATE_CERTIFICATE__", "__ULTIMATE_CERTIFICATE_HEAD__"))
    head = json.loads(section(
        output, "__ULTIMATE_CERTIFICATE_HEAD__", "__ULTIMATE_REACHABILITY__"))
    audit_text = section(
        output, "__ULTIMATE_REACHABILITY__", "__ULTIMATE_REACHABILITY_PUT__")
    side_put = json.loads(section(
        output, "__ULTIMATE_REACHABILITY_PUT__", "__ULTIMATE_KEYS__"))
    keys = output.split("__ULTIMATE_KEYS__\n", 1)[1].strip().splitlines()
    if len(keys) != 4:
        raise ValueError("remote finalization key binding residual")
    cert_sha, _cert_key, side_sha, _side_key = keys
    if (certificate.get("schema") != concrete.CERTIFICATE_SCHEMA or
            len(certificate.get("completed", [])) != 1):
        raise ValueError("concrete certificate schema/cardinality residual")
    completed = certificate["completed"][0]
    if completed.get("filename") != args.filename:
        raise ValueError("concrete certificate filename residual")
    table = completed["s3"]
    if any(table.get(key) in (None, "") for key in ("sha256", "version_id")):
        raise ValueError("concrete table S3 binding residual")
    if any(value in (None, "") for value in
           (head.get("VersionId"), side_put.get("VersionId"), cert_sha, side_sha)):
        raise ValueError("certificate/sidecar S3 version binding residual")

    totals = counts(TOTAL, audit_text)
    omitted = counts(AUDIT, audit_text)
    states = int(record["states"])
    if any(sum(side) != states // 2 for side in totals):
        raise ValueError("native total W/L/D conservation residual")
    first, second = (render(totals[side], omitted[side]) for side in range(2))
    rows = {row.filename: row for row in ledger.entries(args.readme.read_text())}
    row = rows[args.filename]
    if row.result_kind != "concrete":
        raise ValueError(
            f"{args.filename}: hidden-information row cannot be concrete-certified")
    storage = (
        f"S3 table sha256:{table['sha256']} VersionId {table['version_id']}; "
        f"certificate sha256:{cert_sha} VersionId {head['VersionId']}; "
        f"reachability sha256:{side_sha} VersionId {side_put['VersionId']}")
    value = {
        "result_kind": "concrete", "first": first, "second": second,
        "reachability": ledger.reachability(first, second), "storage": storage,
    }
    ledger.update(args.readme, [], [], certified_values=[
        args.filename + "=" + json.dumps(value, separators=(",", ":"))])
    return {"filename": args.filename, **value}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--work-directory", required=True)
    parser.add_argument("--filename", required=True)
    parser.add_argument("--unit")
    parser.add_argument(
        "--command-id",
        help="resume/import an already submitted remote finalization command")
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--s3-prefix", required=True)
    parser.add_argument("--region", default="us-west-2")
    parser.add_argument("--timeout", type=int, default=3600)
    parser.add_argument("--readme", type=Path, default=README)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    record = record_for(args.filename)
    preflight(args)
    output = (wait_for_command(args, args.command_id) if args.command_id else
              send_and_wait(args, remote_script(args, record)))
    print(json.dumps(import_result(args, output, record), sort_keys=True))


if __name__ == "__main__":
    main()
