#!/usr/bin/env python3
"""Preserve and import one already completed exact UFIW2 information solve."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time

import certify_ultimate_primary_jester_archive as certify
import generate_ultimate_information_tablebases as generate
import ultimate_information_tablebases as information
import update_ultimate_tablebase_ledger as ledger


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "tablebases/README.md"
PLOT = ROOT / "tools/tablebases/plot_ultimate_tablebases.py"


def aws(*arguments: str) -> str:
    return subprocess.run(["aws", *arguments], check=True, text=True,
                          capture_output=True).stdout


def wait(args: argparse.Namespace, command_id: str) -> str:
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        result = json.loads(aws(
            "ssm", "get-command-invocation", "--command-id", command_id,
            "--instance-id", args.instance, "--region", args.region,
            "--output", "json"))
        status = result.get("Status")
        if status == "Success":
            return str(result.get("StandardOutputContent", ""))
        if status in {"Cancelled", "Cancelling", "Failed", "TimedOut"}:
            raise RuntimeError(str(result.get("StandardErrorContent", status)))
        time.sleep(3)
    raise TimeoutError("existing information finalizer timed out")


def section(text: str, begin: str, end: str) -> str:
    try:
        return text.split(begin + "\n", 1)[1].split("\n" + end, 1)[0]
    except IndexError as error:
        raise ValueError(f"finalizer output lacks {begin}") from error


def remote_script(args: argparse.Namespace) -> str:
    stem = Path(args.filename).stem
    prefix = args.s3_prefix.strip("/")
    unit_gate = ""
    if args.unit:
        unit_gate = f"""test "$(systemctl show {args.unit} -p ActiveState --value)" = inactive
test "$(systemctl show {args.unit} -p Result --value)" = success
"""
    return f"""set -euo pipefail
overlay={args.overlay}
proof={args.proof_log}
table={args.source_table}
binary={args.binary}
bundle={args.source_bundle}
{unit_gate}test -s "$overlay"
test -s "$proof"
test -s "$table"
test -s "$binary"
test -s "$bundle"
stage=$(dirname "$overlay")/finalize-{stem}
mkdir -p "$stage"
install -m 0644 "$overlay" "$stage/{stem}.ufiw"
install -m 0644 "$proof" "$stage/{stem}.proof.log"
archive=$(dirname "$overlay")/{stem}.existing-information.tar.zst
tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -C "$stage" \
  -cf - {stem}.ufiw {stem}.proof.log | zstd -T0 -19 -q -o "$archive"
archive_sha=$(sha256sum "$archive" | cut -d' ' -f1)
overlay_sha=$(sha256sum "$overlay" | cut -d' ' -f1)
proof_sha=$(sha256sum "$proof" | cut -d' ' -f1)
table_sha=$(sha256sum "$table" | cut -d' ' -f1)
binary_sha=$(sha256sum "$binary" | cut -d' ' -f1)
bundle_sha=$(sha256sum "$bundle" | cut -d' ' -f1)
key={prefix}/existing-ufiw/sha256/$archive_sha/{stem}.information.tar.zst
if aws s3api head-object --bucket {args.bucket} --key "$key" \
    --region {args.region} >"$stage/head.json" 2>/dev/null; then
  python3 -c 'import json,sys; h=json.load(open(sys.argv[1])); assert h["Metadata"]["sha256"] == sys.argv[2]' "$stage/head.json" "$archive_sha"
  cp "$stage/head.json" "$stage/put.json"
else
  aws s3api put-object --bucket {args.bucket} --key "$key" --body "$archive" \
    --metadata sha256=$archive_sha,overlay-sha256=$overlay_sha,proof-sha256=$proof_sha \
    --region {args.region} >"$stage/put.json"
fi
echo __PROOF__
cat "$proof"
echo __PUT__
cat "$stage/put.json"
echo __BINDINGS__
printf '%s\n' "$archive_sha" "$overlay_sha" "$proof_sha" "$table_sha" "$binary_sha" "$bundle_sha" "$key"
"""


def send(args: argparse.Namespace) -> str:
    parameters = json.dumps({"commands": [remote_script(args)]},
                            separators=(",", ":"))
    result = json.loads(aws(
        "ssm", "send-command", "--instance-ids", args.instance,
        "--document-name", "AWS-RunShellScript", "--region", args.region,
        "--parameters", parameters, "--output", "json"))
    command_id = result["Command"]["CommandId"]
    print(f"AWS finalizer command {command_id}", flush=True)
    return wait(args, command_id)


def entry_from_proof(args: argparse.Namespace, proof: str,
                     overlay: bytes) -> tuple[dict[str, object], dict[str, object]]:
    record = dict(generate._records()[args.filename])
    summaries: dict[int, dict[str, int]] = {}
    fixed = False
    for line in proof.splitlines():
        if match := generate.FIXED_RE.match(line):
            fixed = True
            if int(match["bellman"]) or int(match["rank"]):
                raise ValueError("information fixed-point proof residual")
        if match := generate.SUMMARY_RE.match(line):
            summaries[int(match["side"])] = {
                key: int(value) for key, value in match.groupdict().items()
                if key != "side"
            }
    if not fixed or set(summaries) != {0, 1}:
        raise ValueError("information proof lacks one exact fixed point and two sides")
    states = information.states_per_side(record)
    source = overlay[32:96].decode()
    model = overlay[96:160].decode()
    if source != args.expected_source_sha256 or model != args.expected_model_sha256:
        raise ValueError("information overlay expected binding residual")
    sides: dict[str, object] = {}
    for index, name in enumerate(information.SIDES):
        summary = summaries[index]
        if summary["concrete"] != states:
            raise ValueError("information proof concrete-domain residual")
        outcomes = {
            outcome: {"legal": summary[outcome],
                      "unreachable": summary[f"u{outcome}"]}
            for outcome in information.OUTCOMES
        }
        legal = sum(summary[outcome] for outcome in information.OUTCOMES)
        unreachable = sum(summary[f"u{outcome}"]
                          for outcome in information.OUTCOMES)
        sides[name] = {
            "outcomes": outcomes,
            "certificate": {
                "information_sets": summary["sets"],
                "concrete_realizations": states,
                "legal_realizations": legal,
                "unreachable_realizations": unreachable,
                "unresolved_information_sets": 0,
                "partition_residual": 0,
                "conservation_residual": 0,
                "bellman_residual": summary["bellman"],
                "rank_residual": summary["rank"],
                "observation_residual": 0,
            },
        }
    entry = {"tablebase_sha256": source, "solver_model_sha256": model,
             "states_per_side": states, "sides": sides}
    certify.validate_entry(args.filename, entry, record,
                           {args.filename: source})
    certify.validate_overlay(overlay, args.filename, entry, record)
    return entry, record


def finalize(args: argparse.Namespace, output: str) -> dict[str, object]:
    proof = section(output, "__PROOF__", "__PUT__")
    put = json.loads(section(output, "__PUT__", "__BINDINGS__"))
    bindings = output.split("__BINDINGS__\n", 1)[1].strip().splitlines()
    if len(bindings) != 7 or not put.get("VersionId"):
        raise ValueError("existing information S3 binding residual")
    (archive_sha, overlay_sha, proof_sha, table_sha, binary_sha,
     bundle_sha, key) = bindings
    with tempfile.TemporaryDirectory() as directory:
        archive = Path(directory) / "result.tar.zst"
        subprocess.run([
            "aws", "s3api", "get-object", "--bucket", args.bucket,
            "--key", key, "--version-id", put["VersionId"],
            "--region", args.region, str(archive)], check=True,
            text=True, capture_output=True)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != archive_sha:
            raise ValueError("fresh information archive SHA residual")
        overlay = subprocess.run(
            ["tar", "-xOf", str(archive),
             f"{Path(args.filename).stem}.ufiw"],
            check=True, capture_output=True).stdout
        restored_proof = subprocess.run(
            ["tar", "-xOf", str(archive),
             f"{Path(args.filename).stem}.proof.log"],
            check=True, capture_output=True).stdout
    if (hashlib.sha256(overlay).hexdigest() != overlay_sha or
            hashlib.sha256(restored_proof).hexdigest() != proof_sha or
            restored_proof.decode().rstrip("\n") != proof):
        raise ValueError("fresh information archive restore residual")
    entry, _record = entry_from_proof(args, proof, overlay)
    certificate = {
        "schema": "ultimate-existing-ufiw-certificate-v1",
        "filename": args.filename,
        "semantics": information.SEMANTICS_ID,
        "entry": entry,
        "archive": {"sha256": archive_sha, "key": key,
                    "version_id": put["VersionId"]},
        "bindings": {"overlay_sha256": overlay_sha,
                     "proof_sha256": proof_sha,
                     "source_table_full_sha256": table_sha,
                     "solver_binary_sha256": binary_sha,
                     "source_bundle_sha256": bundle_sha},
        "verification": {"proof_residuals": 0, "conservation_residual": 0,
                         "overlay_residual": 0, "fresh_restore_residual": 0},
    }
    payload = (json.dumps(certificate, sort_keys=True, indent=2) + "\n").encode()
    digest = hashlib.sha256(payload).hexdigest()
    cert_key = (f"{args.s3_prefix.strip('/')}/existing-ufiw/certificates/"
                f"sha256/{digest}/certificate.json")
    with tempfile.NamedTemporaryFile() as stream:
        stream.write(payload); stream.flush()
        cert_put = json.loads(aws(
            "s3api", "put-object", "--bucket", args.bucket, "--key", cert_key,
            "--body", stream.name, "--metadata", f"sha256={digest}",
            "--region", args.region, "--output", "json"))
    first = certify.outcome_cell(entry, "first")
    second = certify.outcome_cell(entry, "second")
    storage = (f"S3 information archive sha256:{archive_sha} VersionId "
               f"{put['VersionId']}; certificate sha256:{digest} VersionId "
               f"{cert_put['VersionId']}")
    value = {"result_kind": "information v2", "first": first,
             "second": second,
             "reachability": ledger.reachability(first, second),
             "storage": storage}
    ledger.update(args.readme, [], [], certified_values=[
        args.filename + "=" + json.dumps(value, separators=(",", ":"))])
    subprocess.run(["python3", str(PLOT)], cwd=ROOT, check=True)
    return {"filename": args.filename, **value,
            "certificate_sha256": digest}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--filename", required=True)
    parser.add_argument("--overlay", required=True)
    parser.add_argument("--proof-log", required=True)
    parser.add_argument("--source-table", required=True)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--source-bundle", required=True)
    parser.add_argument("--expected-source-sha256", required=True)
    parser.add_argument("--expected-model-sha256", required=True)
    parser.add_argument("--unit")
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--s3-prefix", required=True)
    parser.add_argument("--region", default="us-west-2")
    parser.add_argument("--timeout", type=int, default=3600)
    parser.add_argument("--readme", type=Path, default=README)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    print(json.dumps(finalize(args, send(args)), sort_keys=True))


if __name__ == "__main__":
    main()
