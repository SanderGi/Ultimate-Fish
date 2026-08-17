#!/usr/bin/env python3
"""Launch or inspect one native reachability audit per completed AWS output.

The expensive generator jobs already retain their exact binary, output, and
S3-restored wave certificate in one work root.  This helper adds only the
missing legal-reachability sidecar.  Launching is idempotent: an existing
nonempty sidecar or active transient unit is never replaced.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import time
from typing import Any

import plan_ultimate_tablebases as plan


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONFIG = ROOT / "tools/tablebases/ultimate_aws_supervision.json"
UNIT_SAFE = re.compile(r"[^a-z0-9]+")
AUDIT_LINE = re.compile(
    r"reachability side ([01]) unknown (\d+) win (\d+) loss (\d+) draw (\d+)")


def records() -> dict[str, dict[str, object]]:
    result: dict[str, dict[str, object]] = {}
    for record in (*plan.stateful_candidates(),
                   *plan.mirror_copycat_candidates(), *plan.inventory()):
        result[str(record["filename"])] = record
    return result


def completed_jobs(config: dict[str, Any], state: dict[str, Any]
                   ) -> list[dict[str, Any]]:
    observed = state.get("report", {}).get("jobs", {})
    catalog = records()
    result: list[dict[str, Any]] = []
    for raw in config.get("jobs", []):
        job = dict(raw)
        if observed.get(job.get("id"), {}).get("status") != \
                "COMPLETED_UNCERTIFIED":
            continue
        files = list(map(str, job.get("ledger_files", [])))
        checkpoints = list(map(str, job.get("checkpoint_paths", [])))
        if len(files) != 1 or files[0] not in catalog or not checkpoints:
            continue
        marker = "/scratch/"
        if marker not in checkpoints[0]:
            continue
        work = checkpoints[0].split(marker, 1)[0]
        filename = files[0]
        job["record"] = catalog[filename]
        job["work_root"] = work
        job["filename"] = filename
        result.append(job)
    return result


def audit_command(job: dict[str, Any], cpu: int) -> str:
    record = job["record"]
    work = str(job["work_root"])
    filename = str(job["filename"])
    unit_stem = UNIT_SAFE.sub("-", Path(filename).stem.lower()).strip("-")
    unit = f"ultimatefish-reach-{unit_stem}"
    arguments = [
        f"{work}/binary/ultimate_tablebase",
        "--piece", str(record["primary"]),
        "--piece2", str(record["secondary"]),
    ]
    if record["opposing"]:
        arguments.append("--opposing")
    output = f"{work}/outputs/{filename}"
    sidecar = f"{work}/certificates/reachability-v2.txt"
    arguments.extend(("--audit-reachability", output))
    quoted = " ".join(arguments)
    binding = (f"reachability_binding filename {filename} "
               "output_sha256 $table_sha")
    bound_audit = (
        f"table_sha=$(sha256sum {output} | cut -d' ' -f1); "
        f"{{ echo \"{binding}\"; {quoted}; }} > {sidecar}.tmp 2>&1 && "
        f"mv {sidecar}.tmp {sidecar}")
    # Every interpolated component comes from the validated repository plan;
    # material names and generated paths contain no shell metacharacters.
    return (
        f"table_sha=$(sha256sum {output} | cut -d' ' -f1); "
        f"if grep -Fx \"{binding}\" {sidecar} >/dev/null 2>&1; "
        f"then echo COMPLETE:{filename}; "
        f"elif test -e {sidecar}; then echo INVALID_BINDING:{filename}; "
        f"exit 1; "
        f"elif systemctl is-active --quiet {unit}.service; then "
        f"echo RUNNING:{filename}; else "
        f"systemd-run --quiet --collect --unit={unit} "
        f"--property=AllowedCPUs={cpu} --property=Nice=10 /bin/sh -c "
        f"'{bound_audit}'; "
        f"echo STARTED:{filename}:cpu={cpu}; fi"
    )


def aws_json(arguments: list[str]) -> dict[str, Any]:
    output = subprocess.check_output(arguments, text=True)
    value = json.loads(output)
    if not isinstance(value, dict):
        raise RuntimeError("AWS command did not return a JSON object")
    return value


def compact_collection_command(job: dict[str, Any]) -> str:
    work = str(job["work_root"])
    filename = str(job["filename"])
    output = f"{work}/outputs/{filename}"
    wave = f"{work}/certificates/wave-certificate.json"
    reach = f"{work}/certificates/reachability-v2.txt"
    program = (
        "import collections,hashlib,json,struct,sys;"
        "p,w,r=sys.argv[1:];d=open(p,'rb');h=d.read(64);"
        "v,n,s,b=struct.unpack_from('<IIII',h,8)[0],"
        "struct.unpack_from('<I',h,16)[0],"
        "struct.unpack_from('<I',h,24)[0],"
        "struct.unpack_from('<I',h,28)[0];"
        "o=40+(8 if v>=5 else 0)+(8 if v>=6 else 0)+(8 if v>=7 else 0);"
        "d.seek(o);x=d.read(b);"
        "assert n%8==0 and len(x)==b;"
        "l=[[sum(((y>>(2*i))&3)==q for i in range(4)) "
        "for q in range(4)] for y in range(256)];"
        "c=lambda a,z:(lambda f:[sum(f[m]*l[m][q] for m in f) "
        "for q in range(4)])"
        "(collections.Counter(x[a//4:z//4]));"
        "a=json.load(open(w));z=a['completed'][0];"
        "q=open(r).read().splitlines();"
        "assert q[0]=='reachability_binding filename '+z['filename']+"
        "' output_sha256 '+z['output']['sha256'];"
        "t=''.join(y+'\\n' for y in q if y.startswith('reachability side '));"
        "u=z['s3'];e={'filename':z['filename'],'output':z['output'],"
        "'s3':{k:u[k] for k in ('bucket','key','version_id','sha256','bytes')}};"
        "print(json.dumps({'filename':z['filename'],'artifact':e,"
        "'wave_sha256':hashlib.sha256(open(w,'rb').read()).hexdigest(),"
        "'reachability_text':t,'totals':[c(0,n//2),c(n//2,n)]},"
        "sort_keys=True))"
    )
    return "python3 -c " + repr(program) + " " + " ".join(
        map(repr, (output, wave, reach)))


def send_and_wait(region: str, instance: str, commands: list[str]) -> str:
    response = aws_json([
        "aws", "ssm", "send-command", "--region", region,
        "--instance-ids", instance, "--document-name", "AWS-RunShellScript",
        "--parameters", json.dumps({"commands": commands}), "--output", "json",
    ])
    command_id = str(response["Command"]["CommandId"])
    deadline = time.monotonic() + 300
    while True:
        invocation = aws_json([
            "aws", "ssm", "get-command-invocation", "--region", region,
            "--command-id", command_id, "--instance-id", instance,
            "--output", "json",
        ])
        status = str(invocation.get("Status"))
        if status == "Success":
            return str(invocation.get("StandardOutputContent", ""))
        if status not in {"Pending", "InProgress", "Delayed"}:
            raise RuntimeError(
                f"collection {status} on {instance}: "
                f"{invocation.get('StandardErrorContent', '')}")
        if time.monotonic() >= deadline:
            raise RuntimeError(f"collection timed out on {instance}")
        time.sleep(1)


def collect(region: str, jobs: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[str, list[dict[str, Any]]] = {}
    for job in jobs:
        grouped.setdefault(str(job["instance_id"]), []).append(job)
    def collect_host(pair: tuple[str, list[dict[str, Any]]]
                     ) -> list[dict[str, Any]]:
        instance, assigned = pair
        output = send_and_wait(region, instance,
                               [compact_collection_command(job)
                                for job in assigned])
        records = [json.loads(line) for line in output.splitlines()
                   if line.startswith("{")]
        if len(records) != len(assigned):
            raise RuntimeError(
                f"collected {len(records)}/{len(assigned)} audits on {instance}")
        return records

    result: list[dict[str, Any]] = []
    with ThreadPoolExecutor(max_workers=len(grouped) or 1) as pool:
        for records_on_host in pool.map(collect_host, grouped.items()):
            result.extend(records_on_host)
    return sorted(result, key=lambda item: str(item["filename"]))


def parse_reachability(text: str) -> list[list[int]]:
    result = [[0, 0, 0, 0] for _ in range(2)]
    matches = AUDIT_LINE.findall(text)
    if len(matches) != 2:
        raise RuntimeError("reachability sidecar lacks two exact side rows")
    for side, unknown, win, loss, draw in matches:
        result[int(side)] = list(map(int, (unknown, win, loss, draw)))
    return result


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def s3_prefix_from_archive(key: str) -> str:
    marker = "/concrete/v2/model/"
    if marker not in "/" + key:
        raise RuntimeError("unexpected concrete archive S3 key")
    return key.split(marker.lstrip("/"), 1)[0].rstrip("/")


def head(region: str, bucket: str, key: str) -> dict[str, Any]:
    return aws_json(["aws", "s3api", "head-object", "--region", region,
                     "--bucket", bucket, "--key", key, "--output", "json"])


def upload_sidecars(region: str, collected: list[dict[str, Any]],
                    output: Path) -> list[dict[str, Any]]:
    output.parent.mkdir(parents=True, exist_ok=True)
    def publish(item: dict[str, Any]) -> dict[str, Any]:
      with tempfile.TemporaryDirectory(prefix="ultimate-reachability-") as raw:
        temporary = Path(raw)
        artifact = item["artifact"]
        remote = artifact["s3"]
        bucket, archive_key = str(remote["bucket"]), str(remote["key"])
        prefix = s3_prefix_from_archive(archive_key)
        wave_sha = str(item["wave_sha256"])
        wave_key = (f"{prefix}/concrete/v2/certificates/sha256/"
                    f"{wave_sha}/wave-certificate.json")
        wave_head = head(region, bucket, wave_key)
        unreachable = parse_reachability(str(item["reachability_text"]))
        totals = item["totals"]
        if any(sum(map(int, totals[side])) !=
               int(artifact["output"]["states"]) // 2 for side in (0, 1)):
            raise RuntimeError(f"{item['filename']}: side WDL conservation failed")
        document = {
            "schema": "ultimate-concrete-reachability-sidecar-v1",
            "filename": item["filename"],
            "output_sha256": artifact["output"]["sha256"],
            "states": artifact["output"]["states"],
            "totals": totals,
            "unreachable": unreachable,
            "predicate": "native-full-causal-reachability",
            "table_archive": remote,
            "wave_certificate": {
                "sha256": wave_sha, "key": wave_key,
                "version_id": wave_head["VersionId"],
                "bytes": wave_head["ContentLength"],
            },
        }
        path = temporary / f"{Path(str(item['filename'])).stem}.reachability-v1.json"
        path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")
        digest = sha256(path)
        key = (f"{prefix}/concrete/v2/reachability/sha256/{digest}/"
               f"{path.name}")
        put = aws_json([
            "aws", "s3api", "put-object", "--region", region,
            "--bucket", bucket, "--key", key, "--body", str(path),
            "--metadata", (f"sha256={digest},schema={document['schema']},"
                           f"filename={document['filename']},"
                           f"output_sha256={document['output_sha256']}"),
            "--output", "json",
        ])
        version = str(put.get("VersionId", ""))
        if not version:
            raise RuntimeError(f"{item['filename']}: S3 did not return VersionId")
        downloaded = temporary / (path.name + ".download")
        aws_json(["aws", "s3api", "get-object", "--region", region,
                  "--bucket", bucket, "--key", key, "--version-id", version,
                  str(downloaded), "--output", "json"])
        if sha256(downloaded) != digest:
            raise RuntimeError(f"{item['filename']}: S3 sidecar restore mismatch")
        document["reachability_s3"] = {
            "bucket": bucket, "key": key, "version_id": version,
            "sha256": digest, "bytes": path.stat().st_size,
            "head_residual": 0, "download_residual": 0,
        }
        return document

    with ThreadPoolExecutor(max_workers=min(8, len(collected) or 1)) as pool:
        published = list(pool.map(publish, collected))
    output.write_text(json.dumps(published, indent=2, sort_keys=True) + "\n")
    return published


def launch(region: str, jobs: list[dict[str, Any]]) -> dict[str, object]:
    grouped: dict[str, list[dict[str, Any]]] = {}
    for job in jobs:
        grouped.setdefault(str(job["instance_id"]), []).append(job)
    commands: dict[str, list[str]] = {}
    receipts: dict[str, str] = {}
    for instance, assigned in grouped.items():
        # CPU 0 is retained for long-lived hidden-information work.  These
        # audits are read-mostly and have a small resident set, so one audit
        # per remaining CPU is the useful scheduling unit.
        if len(assigned) > 31:
            raise RuntimeError(f"too many simultaneous audits on {instance}")
        host_commands = [audit_command(job, index + 1)
                         for index, job in enumerate(sorted(
                             assigned, key=lambda item: str(item["filename"])))]
        commands[instance] = host_commands
        response = aws_json([
            "aws", "ssm", "send-command", "--region", region,
            "--instance-ids", instance, "--document-name", "AWS-RunShellScript",
            "--parameters", json.dumps({"commands": host_commands}),
            "--output", "json",
        ])
        receipts[instance] = str(response["Command"]["CommandId"])
    return {"jobs": len(jobs),
            "jobs_per_instance": {instance: len(items)
                                  for instance, items in commands.items()},
            "command_ids": receipts}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--state", type=Path, required=True,
                        help="full JSON state written by the AWS supervisor")
    parser.add_argument("--launch", action="store_true")
    parser.add_argument("--collect", action="store_true")
    parser.add_argument("--publish", type=Path,
                        help="collect and upload reachability sidecars; save receipt JSON")
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    state = json.loads(args.state.read_text())
    jobs = completed_jobs(config, state)
    if sum(bool(value) for value in (args.launch, args.collect, args.publish)) > 1:
        parser.error("choose only one of --launch, --collect, or --publish")
    if args.launch:
        print(json.dumps(launch(str(config["region"]), jobs), sort_keys=True))
    elif args.collect:
        print(json.dumps(collect(str(config["region"]), jobs),
                         indent=2, sort_keys=True))
    elif args.publish:
        collected = collect(str(config["region"]), jobs)
        published = upload_sidecars(str(config["region"]), collected,
                                    args.publish)
        print(json.dumps({"published": len(published),
                          "receipt": str(args.publish)}, sort_keys=True))
    else:
        print(json.dumps({
            "jobs": [{"id": job["id"], "instance_id": job["instance_id"],
                      "filename": job["filename"],
                      "work_root": job["work_root"]} for job in jobs]
        }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
