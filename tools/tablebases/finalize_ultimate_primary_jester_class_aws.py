#!/usr/bin/env python3
"""Preserve and import one completed primary-Jester information solve."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import fcntl
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
SUPERVISION = ROOT / "tools/tablebases/ultimate_aws_supervision.json"


def aws(*arguments: str) -> str:
    return subprocess.run(["aws", *arguments], check=True, text=True,
                          capture_output=True).stdout


def wait(args: argparse.Namespace, command_id: str) -> str:
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        try:
            result = json.loads(aws(
                "ssm", "get-command-invocation", "--command-id", command_id,
                "--instance-id", args.instance, "--region", args.region,
                "--output", "json"))
        except subprocess.CalledProcessError:
            result = {"Status": "Pending"}
        status = result.get("Status")
        if status == "Success":
            return str(result.get("StandardOutputContent", ""))
        if status in {"Cancelled", "Cancelling", "Failed", "TimedOut"}:
            raise RuntimeError(str(result.get("StandardErrorContent", status)))
        time.sleep(3)
    raise TimeoutError("information finalizer timed out")


def section(text: str, begin: str, end: str) -> str:
    try:
        return text.split(begin + "\n", 1)[1].split("\n" + end, 1)[0]
    except IndexError as error:
        raise ValueError(f"finalizer output lacks {begin}") from error


def remote_script(args: argparse.Namespace) -> str:
    stem = Path(args.filename).stem
    prefix = args.s3_prefix.strip("/")
    checkpoint = args.checkpoint or f"{args.source_root}/checkpoint/information_summary.json"
    overlays = args.overlays or f"{args.source_root}/overlays"
    service_gate = f"""test "$(systemctl show {args.unit} -p ActiveState --value)" = inactive
test "$(systemctl show {args.unit} -p Result --value)" = success"""
    if args.allow_post_solve_checkpoint_failure:
        service_gate = f"""test "$(systemctl show {args.unit} -p ActiveState --value)" = failed
test "$(systemctl show {args.unit} -p Result --value)" = exit-code
test "$(systemctl show {args.unit} -p ExecMainStatus --value)" = 1
journalctl -u {args.unit} -o cat --no-pager >"$root/{stem}.recovery.log"
grep -Fqx 'RuntimeError: {args.source_root}/checkpoint/information_summary.json: stale solver model for completed kjesterk.uftb' "$root/{stem}.recovery.log"
grep -Eq '^\[{args.filename}\] information_fixed_point .* bellman_residual 0 rank_residual 0$' "$root/{stem}.recovery.log"
test "$(grep -Ec '^\[{args.filename}\] information_summary side [01] .* bellman_residual 0 rank_residual 0 belief_cap none exhaustive 1$' "$root/{stem}.recovery.log")" = 2
grep -Fqx '[{args.filename}] information_overlay {overlays}/{stem}.ufiw bytes 151831840' "$root/{stem}.recovery.log"
"""
    if args.direct_log:
        entry_builder = r'''solve_log="''' + args.direct_log + r'''"
test -s "$solve_log"
python3 - "$solve_log" "$overlay" "$root/''' + stem + r'''.entry.json" <<'PY'
import json
import re
import struct
import sys

summary_re = re.compile(
    r"^information_summary side (?P<side>[01]) "
    r"win (?P<win>\d+) loss (?P<loss>\d+) draw (?P<draw>\d+) "
    r"unreachable_win (?P<uwin>\d+) "
    r"unreachable_loss (?P<uloss>\d+) "
    r"unreachable_draw (?P<udraw>\d+) "
    r"sets (?P<sets>\d+) concrete (?P<concrete>\d+) "
    r"bellman_residual (?P<bellman>\d+) rank_residual (?P<rank>\d+) "
    r"belief_cap none exhaustive 1$")
fixed_re = re.compile(
    r"^information_fixed_point .* bellman_residual (?P<bellman>\d+) "
    r"rank_residual (?P<rank>\d+)$")

summaries = {}
fixed = []
for line in open(sys.argv[1], encoding="utf-8"):
    line = line.rstrip("\n")
    if match := summary_re.fullmatch(line):
        side = int(match["side"])
        if side in summaries:
            raise SystemExit("duplicate information summary side")
        summaries[side] = {
            key: int(value) for key, value in match.groupdict().items()
            if key != "side"
        }
    if match := fixed_re.fullmatch(line):
        fixed.append((int(match["bellman"]), int(match["rank"])))
if set(summaries) != {0, 1} or not fixed or any(pair != (0, 0) for pair in fixed):
    raise SystemExit("information proof certificate residual")

with open(sys.argv[2], "rb") as stream:
    header = stream.read(160)
if len(header) != 160:
    raise SystemExit("truncated UFIW2 header")
magic, version, _primary, _secondary, _color, count, _substates = \
    struct.unpack_from("<8s6I", header)
if magic != b"UFIW2\0\0\0" or version != 2 or count % 2:
    raise SystemExit("invalid UFIW2 header")
states = count // 2
if __import__("os").stat(sys.argv[2]).st_size != 160 + count:
    raise SystemExit("UFIW2 extent residual")
source = header[32:96].decode("ascii")
model = header[96:160].decode("ascii")
if not re.fullmatch(r"[0-9a-f]{64}", source) or \
        not re.fullmatch(r"[0-9a-f]{64}", model):
    raise SystemExit("UFIW2 hash binding residual")

sides = {}
for side_index, side_name in enumerate(("first", "second")):
    summary = summaries[side_index]
    if summary["concrete"] != states:
        raise SystemExit("information concrete-domain residual")
    outcomes = {
        name: {"legal": summary[name], "unreachable": summary["u" + name]}
        for name in ("win", "loss", "draw")
    }
    legal = sum(summary[name] for name in ("win", "loss", "draw"))
    unreachable = sum(summary["u" + name] for name in ("win", "loss", "draw"))
    if legal + unreachable != states or summary["bellman"] or summary["rank"]:
        raise SystemExit("information W/L/D conservation residual")
    sides[side_name] = {
        "outcomes": outcomes,
        "certificate": {
            "information_sets": summary["sets"],
            "concrete_realizations": states,
            "legal_realizations": legal,
            "unreachable_realizations": unreachable,
            "unresolved_information_sets": 0,
            "partition_residual": 0,
            "conservation_residual": 0,
            "bellman_residual": 0,
            "rank_residual": 0,
            "observation_residual": 0,
        },
    }
entry = {
    "tablebase_sha256": source,
    "solver_model_sha256": model,
    "states_per_side": states,
    "sides": sides,
}
with open(sys.argv[3], "w", encoding="utf-8") as stream:
    json.dump(entry, stream, sort_keys=True, indent=2)
    stream.write("\n")
PY'''
        preserve_log = 'solve_log="$solve_log"'
    else:
        entry_builder = f'''test -s "$checkpoint"
python3 - "$checkpoint" {args.filename} "$root/{stem}.entry.json" <<'PY'
import json,sys
d=json.load(open(sys.argv[1])); e=d['files'][sys.argv[2]]
json.dump(e,open(sys.argv[3],'w'),sort_keys=True,indent=2); open(sys.argv[3],'a').write('\\n')
PY'''
        preserve_log = (f'journalctl -u {args.unit} --no-pager '
                        f'>"$root/{stem}.solve.log"\n'
                        f'solve_log="$root/{stem}.solve.log"')
    return f"""set -euo pipefail
root={args.source_root}
overlay="{overlays}/{stem}.ufiw"
checkpoint="{checkpoint}"
{service_gate}
test -s "$overlay"
{entry_builder}
{preserve_log}
stage="$root/finalize-{stem}"
mkdir -p "$stage/overlays"
install -m 0644 "$root/{stem}.entry.json" "$stage/{stem}.entry.json"
install -m 0644 "$solve_log" "$stage/{stem}.solve.log"
install -m 0644 "$overlay" "$stage/overlays/{stem}.ufiw"
archive="$root/{stem}.information.tar.zst"
tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -C "$stage" \
  -cf - "{stem}.entry.json" "{stem}.solve.log" "overlays/{stem}.ufiw" | \
  zstd -T0 -19 -q -o "$archive"
archive_sha=$(sha256sum "$archive" | cut -d' ' -f1)
overlay_sha=$(sha256sum "$overlay" | cut -d' ' -f1)
key={prefix}/primary-jester/sha256/$archive_sha/{stem}.information.tar.zst
aws s3api put-object --bucket {args.bucket} --key "$key" --body "$archive" \
  --metadata sha256=$archive_sha,overlay-sha256=$overlay_sha,semantics=fresh-maximal-public-view-v2 \
  --region {args.region} >"$root/{stem}.put.json"
echo __ENTRY__
cat "$root/{stem}.entry.json"
echo __PUT__
cat "$root/{stem}.put.json"
echo __BINDINGS__
printf '%s\n%s\n%s\n' "$archive_sha" "$overlay_sha" "$key"
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


def validate_expected_bindings(args: argparse.Namespace,
                               entry: dict[str, object]) -> None:
    expected_model = (args.expected_model_sha256 or
                      information.solver_model_fingerprint(args.filename))
    expected_source = getattr(args, "expected_source_sha256", None)
    if (expected_source and
            entry.get("tablebase_sha256") != expected_source):
        raise ValueError("information concrete source binding residual")
    if entry.get("solver_model_sha256") != expected_model:
        raise ValueError("information solver model binding residual")


@contextmanager
def file_lock(path: Path):
    lock = path.with_suffix(path.suffix + ".lock")
    with lock.open("a", encoding="utf-8") as stream:
        fcntl.flock(stream.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(stream.fileno(), fcntl.LOCK_UN)


def update_supervision(args: argparse.Namespace, value: dict[str, str],
                       certificates: list[dict[str, object]]) -> None:
    path = args.supervision_config
    with file_lock(path):
        document = json.loads(path.read_text())
        matches = [
            job for job in document.get("jobs", [])
            if job.get("unit") == args.unit and
               args.filename in job.get("ledger_files", [])
        ]
        if len(matches) != 1:
            raise ValueError(
                f"{args.filename}: expected one information supervision job, "
                f"got {len(matches)}")
        job = matches[0]
        existing = {
            (item.get("bucket"), item.get("key"), item.get("version_id"))
            for item in job.get("s3_certificates", [])
        }
        target = job.setdefault("s3_certificates", [])
        for item in certificates:
            identity = (item["bucket"], item["key"], item["version_id"])
            if identity not in existing:
                target.append(item)
                existing.add(identity)
        result_key = str(certificates[0]["key"])
        job.setdefault("result_certificate_keys", {})[args.filename] = [
            result_key]
        job["ledger_certifies"] = True
        job.setdefault("ledger_results", {})[args.filename] = dict(value)
        job["s3_only_certified"] = True
        temporary = path.with_suffix(path.suffix + ".tmp")
        temporary.write_text(json.dumps(document, indent=2) + "\n")
        temporary.replace(path)


def finalize(args: argparse.Namespace, output: str) -> dict[str, object]:
    if (args.allow_post_solve_checkpoint_failure and
            args.filename != "kjestercheckerk.uftb"):
        raise ValueError("checkpoint-failure recovery is restricted to the audited Jester/Checker solve")
    entry = json.loads(section(output, "__ENTRY__", "__PUT__"))
    put = json.loads(section(output, "__PUT__", "__BINDINGS__"))
    bindings = output.split("__BINDINGS__\n", 1)[1].strip().splitlines()
    if len(bindings) != 3 or not put.get("VersionId"):
        raise ValueError("information archive S3 binding residual")
    archive_sha, overlay_sha, key = bindings
    record = dict(generate._records()[args.filename])
    if information.solver_domain(args.filename) not in {
            "primary-jester", "primary-jester-giant"}:
        raise ValueError("class is not a primary-Jester information stratum")
    validate_expected_bindings(args, entry)
    certify.validate_entry(args.filename, entry, record,
                           {args.filename: entry["tablebase_sha256"]})
    # Validate the uploaded overlay bytes via a version-pinned fresh download.
    with tempfile.TemporaryDirectory() as directory:
        archive = Path(directory) / "result.tar.zst"
        subprocess.run([
            "aws", "s3api", "get-object", "--bucket", args.bucket,
            "--key", key, "--version-id", put["VersionId"],
            "--region", args.region, str(archive)], check=True,
            text=True, capture_output=True)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != archive_sha:
            raise ValueError("fresh information archive SHA residual")
        archive_bytes = archive.stat().st_size
        overlay = subprocess.run(
            ["tar", "-xOf", str(archive),
             f"overlays/{Path(args.filename).stem}.ufiw"],
            check=True, capture_output=True).stdout
        if hashlib.sha256(overlay).hexdigest() != overlay_sha:
            raise ValueError("information overlay SHA residual")
        certify.validate_overlay(overlay, args.filename, entry, record)
    certificate = {
        "schema": "ultimate-primary-jester-class-certificate-v1",
        "filename": args.filename,
        "semantics": information.SEMANTICS_ID,
        "entry": entry,
        "archive": {"sha256": archive_sha, "key": key,
                    "version_id": put["VersionId"]},
        "overlay_sha256": overlay_sha,
        "source_bindings": args.source_binding,
        "post_solve_checkpoint_recovery":
            bool(args.allow_post_solve_checkpoint_failure),
        "verification": {"proof_residuals": 0, "conservation_residual": 0,
                         "overlay_residual": 0, "fresh_restore_residual": 0},
    }
    payload = (json.dumps(certificate, sort_keys=True, indent=2) + "\n").encode()
    digest = hashlib.sha256(payload).hexdigest()
    cert_key = (f"{args.s3_prefix.strip('/')}/primary-jester/certificates/"
                f"sha256/{digest}/certificate.json")
    with tempfile.NamedTemporaryFile() as stream:
        stream.write(payload); stream.flush()
        cert_put = json.loads(aws(
            "s3api", "put-object", "--bucket", args.bucket, "--key", cert_key,
            "--body", stream.name, "--metadata", f"sha256={digest}",
            "--region", args.region, "--output", "json"))
    if not cert_put.get("VersionId"):
        raise ValueError("information certificate upload lacks VersionId")
    first = certify.outcome_cell(entry, "first")
    second = certify.outcome_cell(entry, "second")
    storage = (f"S3 information archive sha256:{archive_sha} VersionId "
               f"{put['VersionId']}; certificate sha256:{digest} VersionId "
               f"{cert_put['VersionId']}")
    value = {"result_kind": "information v2", "first": first,
             "second": second, "reachability": ledger.reachability(first, second),
             "storage": storage}
    ledger.update(args.readme, [], [], certified_values=[
        args.filename + "=" + json.dumps(value, separators=(",", ":"))])
    update_supervision(args, value, [
        {"bucket": args.bucket, "key": key,
         "version_id": put["VersionId"], "sha256": archive_sha,
         "size": archive_bytes},
        {"bucket": args.bucket, "key": cert_key,
         "version_id": cert_put["VersionId"], "sha256": digest,
         "size": len(payload)},
    ])
    subprocess.run(["python3", str(PLOT)], cwd=ROOT, check=True)
    return {"filename": args.filename, **value,
            "certificate_sha256": digest}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--filename", required=True)
    parser.add_argument("--unit", required=True)
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--s3-prefix", required=True)
    parser.add_argument("--checkpoint",
                        help="remote checkpoint path; defaults below source root")
    parser.add_argument("--direct-log",
                        help="parse an exact native solver log instead of a driver checkpoint")
    parser.add_argument("--overlays",
                        help="remote overlay directory; defaults below source root")
    parser.add_argument("--expected-model-sha256",
                        help="model hash computed inside an immutable staged source")
    parser.add_argument("--expected-source-sha256",
                        help="logical SHA-256 of the authenticated concrete source table")
    parser.add_argument("--source-binding", action="append", default=[],
                        help="authenticated staged-source binding recorded in the certificate")
    parser.add_argument("--command-id",
                        help="reuse an already completed remote preservation command")
    parser.add_argument("--allow-post-solve-checkpoint-failure",
                        action="store_true",
                        help="recover only an exact zero-residual solve that failed while merging a stale lower checkpoint")
    parser.add_argument("--region", default="us-west-2")
    parser.add_argument("--timeout", type=int, default=3600)
    parser.add_argument("--readme", type=Path, default=README)
    parser.add_argument("--supervision-config", type=Path,
                        default=SUPERVISION)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if (args.allow_post_solve_checkpoint_failure and
            args.filename != "kjestercheckerk.uftb"):
        raise SystemExit(
            "checkpoint-failure recovery is restricted to kjestercheckerk.uftb")
    output = wait(args, args.command_id) if args.command_id else send(args)
    print(json.dumps(finalize(args, output), sort_keys=True))


if __name__ == "__main__":
    main()
