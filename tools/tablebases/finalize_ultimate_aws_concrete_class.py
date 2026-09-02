#!/usr/bin/env python3
"""Audit, preserve, and import one completed AWS concrete tablebase.

The expensive class solve is never repeated.  This command accepts the host
and work directory of one successful class runner, runs the native full-causal
reachability audit against the retained output, uploads that text sidecar, and
installs the exact W/L/D split plus all three versioned-S3 bindings in the
canonical README ledger and supervision record.  This atomically releases the
finished job's scheduler reservation instead of leaving a certified output
looking active.  Hidden-information rows fail closed here: their concrete
worlds are dependencies, not publishable public results.
"""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import fcntl
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
SUPERVISION = ROOT / "tools/tablebases/ultimate_aws_supervision.json"
AUDIT = re.compile(
    r"reachability side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)")
TOTAL = re.compile(
    r"reachability_total side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)")
EXCLUDED = re.compile(
    r"reachability_excluded side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)")
ADMITTED = re.compile(
    r"reachability_admitted side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)")
TRIVIAL = re.compile(
    r"reachability_trivial side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)")
SUBSTATE = re.compile(
    r"reachability_(primary|secondary|combined)_substate(_total|_trivial)? "
    r"substate (\d+) side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)")
INFORMATION = re.compile(
    r"information_reachability_(admitted|excluded|trivial) side ([01]) "
    r"unknown (\d+) win (\d+) loss (\d+) draw (\d+)")


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


def encoded_record_for(filename: str) -> Mapping[str, object]:
    encoded = concrete.encoded_filename(filename)
    if encoded == filename:
        return record_for(filename)
    matches = [row for row in concrete.supported_inventory()
               if row["filename"] == encoded]
    if len(matches) != 1:
        raise ValueError(f"missing encoded concrete alias: {filename} -> {encoded}")
    return matches[0]


def ledger_side_order(filename: str) -> tuple[int, int]:
    return ((1, 0) if concrete.encoded_filename(filename) != filename
            else (0, 1))


def certificate_conservation_states(
        filename: str, record: Mapping[str, object],
        completed: Mapping[str, object]) -> int:
    """Return the authenticated packed extent used by native audit totals.

    Most catalog rows index their complete dense codec.  Lone Devil is the
    deliberate exception: its catalog extent is the first-three-ranks root
    frontier, while the preserved UFTB keeps the full ten-rank dense codec so
    all out-of-domain placements remain explicit excluded sentinels.  Bind the
    conservation gate to the certificate's parsed header and require that
    exact 10/3 relationship; never infer it merely from the material name.
    """
    output = completed.get("output")
    if not isinstance(output, Mapping):
        raise ValueError("concrete certificate lacks parsed output")
    states = int(output.get("states", 0))
    catalog_states = int(record["states"])
    if states <= 0:
        raise ValueError("concrete certificate has invalid packed extent")
    if filename == "kdevilk.uftb":
        if states * 3 != catalog_states * 10:
            raise ValueError("Devil root-domain/dense-codec extent residual")
    elif states != catalog_states:
        raise ValueError("certificate/catalog packed extent residual")
    return states


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


def substate_counts(text: str, axis: str, kind: str,
                    substate: int = 0) -> list[list[int]]:
    """Return one exact native reachability substate slice.

    ``kind`` is ``excluded``, ``total``, or ``trivial``.  The native audit's
    unsuffixed substate row retains the legacy meaning "excluded".
    """
    suffix = {"excluded": "", "total": "_total",
              "trivial": "_trivial"}[kind]
    result = [[0, 0, 0, 0] for _ in range(2)]
    matches = [match for match in SUBSTATE.findall(text)
               if match[0] == axis and match[1] == suffix and
               int(match[2]) == substate]
    if len(matches) != 2:
        raise ValueError(
            f"native reachability output lacks {axis} substate {substate} "
            f"{kind} rows")
    for _, _, _, side, unknown, win, loss, draw in matches:
        result[int(side)] = list(map(int, (unknown, win, loss, draw)))
    if any(side[0] for side in result):
        raise ValueError("native reachability substate output contains unknown states")
    return result


def reporting_counts(filename: str, text: str
                     ) -> tuple[list[list[int]], list[list[int]], list[list[int]]]:
    """Return W/L/D inputs for exact ordinary turn boundaries.

    Prince continuation states remain in the tablebase, but are not starting
    positions and therefore must not enter the README or plot aggregates.
    Existing native audits expose either Prince's marginal substate slice.  A
    two-Prince class requires the joint combined-substate slice so both pieces
    are ordinary simultaneously.
    """
    totals = counts(TOTAL, text)
    excluded = counts(EXCLUDED if "reachability_excluded side " in text
                      else AUDIT, text)
    validate_explicit_reachability_semantics(text, totals, excluded)
    trivial = (counts(TRIVIAL, text)
               if "reachability_trivial side " in text
               else [[0, 0, 0, 0] for _ in range(2)])
    record = encoded_record_for(filename)
    materials = (str(record["primary"]), str(record.get("secondary") or ""))
    prince_count = materials.count("prince")
    boundary_scopes = text.count("reachability_scope turn_boundary\n")
    if boundary_scopes:
        if boundary_scopes != 1 or not prince_count:
            raise ValueError("invalid turn-boundary reachability scope")
        return totals, excluded, trivial
    if not prince_count:
        return totals, excluded, trivial
    if prince_count == 2:
        try:
            boundary_totals = substate_counts(text, "combined", "total")
            boundary_excluded = substate_counts(
                text, "combined", "excluded")
            boundary_trivial = substate_counts(text, "combined", "trivial")
        except ValueError as error:
            if "lacks combined substate" not in str(error):
                raise
            # V3 receipts made before the joint slice was added still contain
            # both exact marginals.  The (1,1) Prince state is impossible
            # because Position has one global forced continuation; the dense
            # codec consequently stores that entire quadrant as unreachable
            # draw sentinels.  Inclusion/exclusion therefore recovers (0,0)
            # exactly, without touching or recomputing the tablebase.
            primary = [substate_counts(text, "primary", kind)
                       for kind in ("total", "excluded", "trivial")]
            secondary = [substate_counts(text, "secondary", kind)
                         for kind in ("total", "excluded", "trivial")]
            invalid_per_side = int(record["states"]) // 8
            invalid = [0, 0, 0, invalid_per_side]
            boundary = []
            for bucket, aggregate in enumerate((totals, excluded, trivial)):
                correction = invalid if bucket < 2 else [0, 0, 0, 0]
                boundary.append([
                    [primary[bucket][side][outcome] +
                     secondary[bucket][side][outcome] -
                     aggregate[side][outcome] + correction[outcome]
                     for outcome in range(4)]
                    for side in range(2)])
            boundary_totals, boundary_excluded, boundary_trivial = boundary
    else:
        axis = "primary" if materials[0] == "prince" else "secondary"
        boundary_totals = substate_counts(text, axis, "total")
        boundary_excluded = substate_counts(text, axis, "excluded")
        boundary_trivial = substate_counts(text, axis, "trivial")
    for side in range(2):
        for outcome in range(4):
            whole = boundary_totals[side][outcome]
            omitted = boundary_excluded[side][outcome]
            immediate = boundary_trivial[side][outcome]
            if not 0 <= omitted <= whole:
                raise ValueError("turn-boundary unreachable count exceeds total")
            if not 0 <= immediate <= whole - omitted:
                raise ValueError("turn-boundary trivial count exceeds admitted bucket")
    return boundary_totals, boundary_excluded, boundary_trivial


def information_reporting_counts(
        filename: str, text: str
        ) -> tuple[list[list[int]], list[list[int]], list[list[int]]]:
    """Return information W/L/D at exact ordinary turn boundaries."""
    buckets = {
        name: [[0, 0, 0, 0] for _ in range(2)]
        for name in ("admitted", "excluded", "trivial")
    }
    matches = INFORMATION.findall(text)
    if len(matches) != 6:
        raise ValueError("information reachability output lacks six side rows")
    seen: set[tuple[str, int]] = set()
    for name, side_text, unknown, win, loss, draw in matches:
        side = int(side_text)
        if (name, side) in seen:
            raise ValueError("duplicate information reachability detail row")
        seen.add((name, side))
        buckets[name][side] = list(map(int, (unknown, win, loss, draw)))
    if any(side[0] for bucket in buckets.values() for side in bucket):
        raise ValueError("information reachability output contains unknown states")
    record = encoded_record_for(filename)
    prince_count = sum(
        name == "prince" for name in
        (str(record["primary"]), str(record.get("secondary") or "")))
    scopes = text.count("information_reachability_scope turn_boundary\n")
    if prince_count and scopes != 1:
        raise ValueError("Prince information reachability lacks turn-boundary scope")
    if not prince_count and scopes:
        raise ValueError("non-Prince information reachability has boundary scope")
    admitted, excluded, trivial = (
        buckets[name] for name in ("admitted", "excluded", "trivial"))
    totals = [
        [admitted[side][outcome] + excluded[side][outcome]
         for outcome in range(4)]
        for side in range(2)
    ]
    expected = int(record["states"]) // (2 * (2 ** prince_count))
    if any(sum(side) != expected for side in totals):
        raise ValueError("information turn-boundary conservation residual")
    for side in range(2):
        for outcome in range(4):
            if trivial[side][outcome] > admitted[side][outcome]:
                raise ValueError("information trivial subset exceeds admitted bucket")
    return totals, excluded, trivial


def render(total: list[int], unreachable: list[int],
           trivial: list[int] | None = None) -> str:
    has_trivial = trivial is not None
    trivial = trivial or [0, 0, 0, 0]
    values: list[str] = []
    for whole, omitted, immediate in zip(
            total[1:], unreachable[1:], trivial[1:]):
        if not 0 <= omitted <= whole:
            raise ValueError("unreachable W/L/D exceeds encoded total")
        legal = whole - omitted
        if not 0 <= immediate <= legal:
            raise ValueError("trivial W/L/D exceeds admitted total")
        values.append(
            f"{legal:,}" + (f" [{immediate:,}]" if has_trivial else "") +
            (f" ({omitted:,})" if omitted else ""))
    return " / ".join(values)


def validate_explicit_reachability_semantics(
        text: str, totals: list[list[int]], omitted: list[list[int]]) -> None:
    """Cross-check new unambiguous rows while accepting pinned legacy sidecars."""
    has_explicit = "reachability_excluded side " in text or \
        "reachability_admitted side " in text
    if not has_explicit:
        return
    excluded = counts(EXCLUDED, text)
    admitted = counts(ADMITTED, text)
    if excluded != omitted:
        raise ValueError("explicit reachability exclusion residual")
    if any([admitted[side][outcome] + excluded[side][outcome]
            for outcome in range(4)] != totals[side]
           for side in range(2)):
        raise ValueError("explicit reachability admission residual")
    if "reachability_trivial side " in text:
        trivial = counts(TRIVIAL, text)
        for side in range(2):
            for outcome in range(4):
                if trivial[side][outcome] > admitted[side][outcome]:
                    raise ValueError("trivial reachability exceeds admitted bucket")


def aws(*arguments: str) -> str:
    return subprocess.run(
        ["aws", *arguments], check=True, text=True, capture_output=True).stdout


@contextmanager
def readme_lock(path: Path):
    """Serialize imports from independent remote audits."""
    lock = path.with_suffix(path.suffix + ".lock")
    with lock.open("a", encoding="utf-8") as stream:
        fcntl.flock(stream.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(stream.fileno(), fcntl.LOCK_UN)


def preflight(args: argparse.Namespace) -> None:
    rows = {row.filename: row for row in ledger.entries(args.readme.read_text())}
    row = rows[args.filename]
    preserve = getattr(args, "preserve_information_dependency", False)
    if row.result_kind != "concrete" and not preserve:
        raise ValueError(
            f"{args.filename}: hidden-information row cannot be concrete-certified")
    if (preserve and
            row.result_kind != "information required"):
        raise ValueError(
            f"{args.filename}: preservation mode requires an information row")
    if row.status == "certified":
        raise ValueError(f"{args.filename}: already CERTIFIED; refusing a second upload")


def remote_script(args: argparse.Namespace, record: Mapping[str, object]) -> str:
    encoded = concrete.encoded_filename(args.filename)
    audit_binary = args.audit_binary or "$work/binary/ultimate_tablebase"
    command = [audit_binary, "--piece",
               str(record["primary"]), "--workers",
               str(getattr(args, "audit_workers", 4)),
               "--checkpoint-every", "0"]
    if record.get("secondary"):
        command += ["--piece2", str(record["secondary"])]
    if record.get("opposing"):
        command.append("--opposing")
    command += ["--audit-reachability", f"$work/outputs/{encoded}"]
    audit_command = " ".join(command)
    sidecar = f'$work/{Path(args.filename).stem}.reachability-v2.txt'
    binding = (f'reachability_binding filename {args.filename} '
               'output_sha256 $table_sha')
    if args.reuse_reachability_sidecar:
        audit_step = f'grep -Fx "{binding}" "{sidecar}"'
    else:
        audit_step = f'{{ echo "{binding}"; {audit_command}; }} >"{sidecar}"'
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
test -f "$work/outputs/{encoded}"
table_sha=$(sha256sum "$work/outputs/{encoded}" | cut -d' ' -f1)
{unit_check}{audit_step}
sidecar="$work/{Path(args.filename).stem}.reachability-v2.txt"
side_sha=$(sha256sum "$sidecar" | cut -d' ' -f1)
cert_sha=$(sha256sum "$work/certificates/wave-certificate.json" | cut -d' ' -f1)
cert_key={prefix}/concrete/v2/certificates/sha256/$cert_sha/wave-certificate.json
side_key={prefix}/reachability/sha256/$side_sha/{Path(args.filename).stem}.reachability-v2.txt
aws s3api head-object --bucket {args.bucket} --key "$cert_key" --region {args.region} >"$work/certificate-head.json"
if aws s3api head-object --bucket {args.bucket} --key "$side_key" --region {args.region} >"$work/reachability-put.json" 2>/dev/null; then
  python3 -c 'import json,sys; h=json.load(open(sys.argv[1])); m=h["Metadata"]; assert m["sha256"] == sys.argv[2] and m["filename"] == sys.argv[3] and m["output_sha256"] == sys.argv[4]' "$work/reachability-put.json" "$side_sha" {args.filename} "$table_sha"
else
  aws s3api put-object --bucket {args.bucket} --key "$side_key" --body "$sidecar" --metadata sha256=$side_sha,predicate=native-full-causal-reachability,filename={args.filename},output_sha256=$table_sha --region {args.region} >"$work/reachability-put.json"
fi
side_version=$(python3 -c 'import json,sys; v=json.load(open(sys.argv[1])).get("VersionId"); assert v; print(v)' "$work/reachability-put.json")
aws s3api head-object --bucket {args.bucket} --key "$side_key" --version-id "$side_version" --region {args.region} >"$work/reachability-head.json"
python3 -c 'import json,sys; h=json.load(open(sys.argv[1])); m=h["Metadata"]; assert h["VersionId"] == sys.argv[2] and m["sha256"] == sys.argv[3] and m["filename"] == sys.argv[4] and m["output_sha256"] == sys.argv[5]' "$work/reachability-head.json" "$side_version" "$side_sha" {args.filename} "$table_sha"
echo __ULTIMATE_CERTIFICATE__
cat "$work/certificates/wave-certificate.json"
echo __ULTIMATE_CERTIFICATE_HEAD__
cat "$work/certificate-head.json"
echo __ULTIMATE_REACHABILITY__
cat "$sidecar"
echo __ULTIMATE_REACHABILITY_PUT__
cat "$work/reachability-head.json"
echo __ULTIMATE_KEYS__
printf '%s\\n%s\\n%s\\n%s\\n%s\\n' "$cert_sha" "$cert_key" "$side_sha" "$side_key" "$table_sha"
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
    if len(keys) != 5:
        raise ValueError("remote finalization key binding residual")
    cert_sha, cert_key, side_sha, side_key, table_sha = keys
    if (certificate.get("schema") != concrete.CERTIFICATE_SCHEMA or
            len(certificate.get("completed", [])) != 1):
        raise ValueError("concrete certificate schema/cardinality residual")
    completed = certificate["completed"][0]
    encoded = concrete.encoded_filename(args.filename)
    if completed.get("filename") != encoded:
        raise ValueError("concrete certificate filename residual")
    if completed.get("output", {}).get("sha256") != table_sha:
        raise ValueError("reachability sidecar output SHA-256 residual")
    table = completed["s3"]
    if any(table.get(key) in (None, "") for key in ("sha256", "version_id")):
        raise ValueError("concrete table S3 binding residual")
    if any(value in (None, "") for value in
           (head.get("VersionId"), side_put.get("VersionId"), cert_sha, side_sha)):
        raise ValueError("certificate/sidecar S3 version binding residual")

    totals, omitted, trivial = reporting_counts(args.filename, audit_text)
    states = certificate_conservation_states(args.filename, record, completed)
    prince_factor = 2 ** sum(
        name == "prince" for name in
        (str(record["primary"]), str(record.get("secondary") or "")))
    if any(sum(side) != states // (2 * prince_factor) for side in totals):
        raise ValueError("native total W/L/D conservation residual")
    first, second = (render(totals[side], omitted[side], trivial[side])
                     for side in ledger_side_order(args.filename))
    preserve = getattr(args, "preserve_information_dependency", False)
    storage = (
        f"S3 table sha256:{table['sha256']} VersionId {table['version_id']}; "
        f"certificate sha256:{cert_sha} VersionId {head['VersionId']}; "
        f"reachability sha256:{side_sha} VersionId {side_put['VersionId']}")
    value = {
        "result_kind": "concrete", "first": first, "second": second,
        "reachability": ledger.reachability(first, second), "storage": storage,
    }
    result_certificates = [
        {
            "bucket": str(table["bucket"]), "key": str(table["key"]),
            "version_id": str(table["version_id"]),
            "sha256": str(table["sha256"]), "size": int(table["bytes"]),
        },
        {
            "bucket": args.bucket, "key": cert_key,
            "version_id": str(head["VersionId"]), "sha256": cert_sha,
            "size": int(head["ContentLength"]),
        },
        {
            "bucket": args.bucket, "key": side_key,
            "version_id": str(side_put["VersionId"]), "sha256": side_sha,
            "size": int(side_put["ContentLength"]),
        },
    ]
    with readme_lock(args.readme):
        rows = {row.filename: row
                for row in ledger.entries(args.readme.read_text())}
        row = rows[args.filename]
        if row.result_kind != "concrete" and not preserve:
            raise ValueError(
                f"{args.filename}: hidden-information row cannot be concrete-certified")
        if preserve:
            ledger.update(args.readme,
                          [f"{args.filename}=preserving"],
                          [f"{args.filename}={storage}"])
            return {
                "filename": args.filename,
                "result_kind": "concrete dependency only",
                "status": "preserving",
                "storage": storage,
            }
        ledger.update(args.readme, [], [], certified_values=[
            args.filename + "=" + json.dumps(value, separators=(",", ":"))])
    update_supervision(args, value, result_certificates)
    return {"filename": args.filename, **value}


def update_supervision(args: argparse.Namespace, value: Mapping[str, object],
                       certificates: list[dict[str, object]]) -> None:
    """Bind a public result and release its completed job reservation."""
    path = args.supervision_config
    lock = path.with_suffix(path.suffix + ".lock")
    with lock.open("a", encoding="utf-8") as stream:
        fcntl.flock(stream.fileno(), fcntl.LOCK_EX)
        try:
            document = json.loads(path.read_text())
            matches = [job for job in document.get("jobs", [])
                       if (args.unit and job.get("unit") == args.unit) or
                       (not args.unit and args.filename in
                        job.get("ledger_files", []) and
                        not job.get("superseded_by"))]
            if len(matches) != 1:
                raise ValueError(
                    f"{args.filename}: expected one supervision job, got "
                    f"{len(matches)}")
            job = matches[0]
            if args.filename not in job.get("ledger_files", []):
                raise ValueError("supervision job/ledger filename residual")
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
            stem = Path(args.filename).stem
            result_keys = sorted({
                str(item["key"]) for item in certificates
                if str(item.get("key", "")).endswith(
                    (".tar", ".tar.gz", ".tar.zst")) and
                any(component == stem or component.startswith(f"{stem}-")
                    for component in str(item.get("key", "")).split("/"))
            })
            if not result_keys:
                raise ValueError(
                    f"{args.filename}: finalization lacks its result archive")
            result_map = job.setdefault("result_certificate_keys", {})
            for filename in map(str, job.get("ledger_files", [])):
                result_map.setdefault(filename, [])
            result_map[args.filename] = result_keys
            job["ledger_certifies"] = True
            job.setdefault("ledger_results", {})[args.filename] = dict(value)
            # Exact, version-pinned S3 result evidence is now authoritative.
            # Retain the local source bindings as provenance, but retire this
            # completed unit from bounded live host probes and reservations.
            ledger_files = set(map(str, job.get("ledger_files", [])))
            if (set(result_map) == ledger_files and
                    all(result_map[filename] for filename in ledger_files)):
                job["s3_only_certified"] = True
            else:
                job.pop("s3_only_certified", None)
            temporary = path.with_suffix(path.suffix + ".tmp")
            temporary.write_text(
                json.dumps(document, indent=2, sort_keys=False) + "\n")
            temporary.replace(path)
        finally:
            fcntl.flock(stream.fileno(), fcntl.LOCK_UN)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--work-directory", required=True)
    parser.add_argument("--filename", required=True)
    parser.add_argument("--unit")
    parser.add_argument(
        "--audit-binary",
        help="remote current native binary used only for causal audit output")
    parser.add_argument(
        "--audit-workers", type=int, choices=range(1, 33), default=4,
        help="native reachability-audit worker threads (default: 4)")
    parser.add_argument(
        "--preserve-information-dependency", action="store_true",
        help="preserve/audit a concrete hidden-material oracle without publishing W/L/D")
    parser.add_argument(
        "--command-id",
        help="resume/import an already submitted remote finalization command")
    parser.add_argument(
        "--reuse-reachability-sidecar", action="store_true",
        help="reuse a retained nonempty native audit sidecar; parsed totals and zero unknown states are still verified")
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--s3-prefix", required=True)
    parser.add_argument("--region", default="us-west-2")
    parser.add_argument("--timeout", type=int, default=3600)
    parser.add_argument("--readme", type=Path, default=README)
    parser.add_argument("--supervision-config", type=Path, default=SUPERVISION)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    record = encoded_record_for(args.filename)
    preflight(args)
    output = (wait_for_command(args, args.command_id) if args.command_id else
              send_and_wait(args, remote_script(args, record)))
    print(json.dumps(import_result(args, output, record), sort_keys=True))


if __name__ == "__main__":
    main()
