#!/usr/bin/env python3
"""Audit the preserved legacy tablebase snapshot without regenerating it.

The legacy payloads already live in one version-pinned S3 archive.  This tool
restores that archive on two otherwise-idle high-disk EC2 hosts, authenticates
every physical member against the preservation certificate, and runs the
native full causal reachability audit once per logical table.  It is
idempotent: authenticated restores and complete per-table sidecars are reused.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time
from typing import Any

import plan_ultimate_tablebases as plan


ROOT = Path(__file__).resolve().parents[2]
BUCKET = "ultimatefish-info-20260808-a4e679c6-831688117652"
REGION = "us-west-2"
ARCHIVE_SHA = "e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263"
ARCHIVE_VERSION = "YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ"
CERTIFICATE_SHA = "1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d"
CERTIFICATE_VERSION = "5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY"
CERTIFICATE_BYTES = 39822
PREFIX = f"legacy-local-tablebases/v1/snapshots/sha256/{ARCHIVE_SHA}"
ARCHIVE_KEY = f"{PREFIX}/local-tablebases-{ARCHIVE_SHA}.tar.zst"
CERTIFICATE_KEY = f"{PREFIX}/certificates/{CERTIFICATE_SHA}.json"
OLD_REACHABILITY_SHA = "b201a70b15ff1b271653aed5db15916c8804bb29c8b08510f3ef74c3afd952c9"
OLD_REACHABILITY_VERSION = "xAe0D04RtOrnywzebGYzQwKgrfBfd1GV"
OLD_REACHABILITY_KEY = (
    f"{PREFIX}/sidecars/sha256/{OLD_REACHABILITY_SHA}/reachability.json")
REMOTE_ROOT = "/mnt/ultimatefish/legacy-reachability-v2"
HOSTS = {
    "i-024a2073283e4336e": (
        "/mnt/ultimatefish/concrete-wave0-batch-staging/"
        "final-ec511c36/preflight-4strata-ec511c36-4strata-20260811-0518-"
        "i-024a2073283e4336e/binary/ultimate_tablebase"),
    "i-0986ed3d272721f02": (
        "/mnt/ultimatefish/concrete-wave0-batch-staging/"
        "final-ec511c36/preflight-4strata-ec511c36-4strata-20260811-0518-"
        "i-0986ed3d272721f02/binary/ultimate_tablebase"),
}
OBSOLETE_LEGACY = frozenset({
    "kpenguink.uftb",
    "kbishopkpenguin.uftb", "kbishoppenguink.uftb",
    "kbombkpenguin.uftb", "kbombpenguink.uftb",
    "kdragonkpenguin.uftb",
})


REMOTE_DRIVER = r'''#!/usr/bin/env python3
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import subprocess
import struct
import sys

root = Path(sys.argv[1])
manifest = json.loads((root / "assignment.json").read_text())

def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            value.update(block)
    return value.hexdigest()

def download(key, version, destination):
    if destination.exists():
        return
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    subprocess.run(["aws", "s3api", "get-object", "--region", manifest["region"],
                    "--bucket", manifest["bucket"], "--key", key,
                    "--version-id", version, str(temporary)], check=True,
                   stdout=subprocess.DEVNULL)
    temporary.replace(destination)

root.mkdir(parents=True, exist_ok=True)
archive = root / "snapshot.tar.zst"
certificate_path = root / "snapshot-certificate.json"
download(manifest["archive_key"], manifest["archive_version"], archive)
download(manifest["certificate_key"], manifest["certificate_version"],
         certificate_path)
if digest(archive) != manifest["archive_sha256"]:
    raise RuntimeError("legacy archive SHA-256 residual")
if digest(certificate_path) != manifest["certificate_sha256"]:
    raise RuntimeError("legacy certificate SHA-256 residual")
certificate = json.loads(certificate_path.read_text())
payload = root / "payload"
verified = root / "RESTORE_VERIFIED"
if not verified.exists():
    payload.mkdir(exist_ok=True)
    subprocess.run(["tar", "--use-compress-program=unzstd", "-xf", str(archive),
                    "-C", str(payload)], check=True)
    for record in certificate["manifest"]["artifacts"]:
        path = payload / record["path"]
        if (not path.is_file() or path.stat().st_size != record["bytes"] or
                digest(path) != record["sha256"]):
            raise RuntimeError("legacy restored member residual: " + record["path"])
    verified.write_text(json.dumps({"files": len(certificate["manifest"]["artifacts"]),
                                    "residual": 0}, sort_keys=True) + "\n")

sidecars = root / "sidecars"
sidecars.mkdir(exist_ok=True)

def logical_table(filename):
    path = payload / "tablebases" / filename
    with path.open("rb") as stream:
        header = stream.read(24)
        if header[:8] != b"UFTBS1\0\0":
            return path
        _magic, version, count, total = struct.unpack("<8sIIQ", header)
        if version != 1 or not count:
            raise RuntimeError("invalid split table manifest: " + filename)
        records = []
        for _ in range(count):
            fixed = stream.read(42)
            name_length, size, expected = struct.unpack("<HQ32s", fixed)
            name = stream.read(name_length).decode()
            records.append((name, size, expected.hex()))
        if stream.read(1):
            raise RuntimeError("split table manifest trailing bytes: " + filename)
    destination = root / "logical" / filename
    if destination.is_file() and destination.stat().st_size == total:
        return destination
    destination.parent.mkdir(exist_ok=True)
    temporary = destination.with_suffix(".tmp")
    with temporary.open("wb") as output:
        for name, size, expected in records:
            part = payload / "tablebases" / name
            if part.stat().st_size != size or digest(part) != expected:
                raise RuntimeError("split table part residual: " + name)
            with part.open("rb") as source:
                for block in iter(lambda: source.read(4 << 20), b""):
                    output.write(block)
    if temporary.stat().st_size != total:
        raise RuntimeError("split table logical extent residual: " + filename)
    temporary.replace(destination)
    return destination

def audit(record):
    filename = record["filename"]
    output = sidecars / (Path(filename).stem + ".txt")
    if output.is_file() and output.stat().st_size:
        text = output.read_text()
        if text.count("reachability side ") == 2:
            return filename
    command = [manifest["binary"], "--piece", record["primary"]]
    if record["secondary"]:
        command += ["--piece2", record["secondary"]]
    if record["opposing"]:
        command.append("--opposing")
    command += ["--audit-reachability", str(logical_table(filename))]
    temporary = output.with_suffix(".tmp")
    with temporary.open("wb") as stream:
        completed = subprocess.run(command, stdout=stream,
                                   stderr=subprocess.STDOUT)
    text = temporary.read_text()
    if completed.returncode or text.count("reachability side ") != 2:
        raise RuntimeError("reachability audit failed: " + filename)
    temporary.replace(output)
    return filename

with concurrent.futures.ThreadPoolExecutor(max_workers=15) as pool:
    completed = list(pool.map(audit, manifest["records"]))
(root / "AUDIT_COMPLETE.json").write_text(json.dumps({
    "files": len(completed), "filenames": sorted(completed), "residual": 0,
}, sort_keys=True) + "\n")
'''


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    digest.update(path.read_bytes())
    return digest.hexdigest()


def catalog() -> dict[str, dict[str, object]]:
    rows = (*plan.stateful_candidates(), *plan.mirror_copycat_candidates(),
            *plan.inventory())
    return {str(row["filename"]): row for row in rows}


def logical_snapshot_files(certificate: dict[str, Any]) -> list[str]:
    names = {
        Path(str(item["path"]).split(".part", 1)[0]).name
        for item in certificate["manifest"]["artifacts"]
        if ".uftb" in str(item["path"])
    }
    return sorted(names)


def assignments(certificate: dict[str, Any]) -> dict[str, list[dict[str, object]]]:
    records = catalog()
    selected = sorted(set(logical_snapshot_files(certificate)) - OBSOLETE_LEGACY)
    unknown = sorted(set(selected) - set(records))
    if unknown:
        raise RuntimeError(f"legacy snapshot has unknown tables: {unknown}")
    result = {host: [] for host in HOSTS}
    weights = {host: 0 for host in HOSTS}
    # Greedy largest-first assignment balances dense-state audit work while
    # keeping one full authenticated restore on each host.
    for filename in sorted(selected, key=lambda name: int(records[name]["states"]),
                           reverse=True):
        host = min(HOSTS, key=lambda item: (weights[item], item))
        record = records[filename]
        result[host].append({
            "filename": filename, "primary": record["primary"],
            "secondary": record["secondary"], "opposing": record["opposing"],
            "states": record["states"],
        })
        weights[host] += int(record["states"])
    return result


def aws_json(arguments: list[str]) -> dict[str, Any]:
    value = json.loads(subprocess.check_output(arguments, text=True))
    if not isinstance(value, dict):
        raise RuntimeError("AWS command did not return an object")
    return value


def send_and_wait(instance: str, commands: list[str]) -> str:
    response = aws_json([
        "aws", "ssm", "send-command", "--region", REGION,
        "--instance-ids", instance, "--document-name", "AWS-RunShellScript",
        "--parameters", json.dumps({"commands": commands}), "--output", "json",
    ])
    command_id = str(response["Command"]["CommandId"])
    deadline = time.monotonic() + 180
    while True:
        invocation = aws_json([
            "aws", "ssm", "get-command-invocation", "--region", REGION,
            "--command-id", command_id, "--instance-id", instance,
            "--output", "json",
        ])
        status = str(invocation.get("Status"))
        if status == "Success":
            return str(invocation.get("StandardOutputContent", ""))
        if status not in {"Pending", "InProgress", "Delayed"}:
            raise RuntimeError(
                f"staging {status} on {instance}: "
                f"{invocation.get('StandardErrorContent', '')}")
        if time.monotonic() >= deadline:
            raise RuntimeError(f"staging timed out on {instance}")
        time.sleep(1)


def encoded_write(path: str, payload: bytes) -> str:
    encoded = base64.b64encode(payload).decode()
    program = ("import base64,pathlib;pathlib.Path(" + repr(path) +
               ").write_bytes(base64.b64decode(" + repr(encoded) + "))")
    return "python3 -c " + repr(program)


PUBLISH_HELPER = r'''import hashlib,json,pathlib,subprocess,sys
root=pathlib.Path(sys.argv[1]);instance=sys.argv[2]
if not (root/"AUDIT_COMPLETE.json").is_file():
 raise RuntimeError("legacy audits are not complete")
assignment=json.loads((root/"assignment.json").read_text())
records=[]
for item in assignment["records"]:
 path=root/"sidecars"/(pathlib.Path(item["filename"]).stem+".txt")
 records.append({"filename":item["filename"],"text":path.read_text()})
document={"schema":"ultimate-legacy-reachability-host-v2","instance_id":instance,
          "records":records}
payload=(json.dumps(document,sort_keys=True,separators=(",",":"))+"\n").encode()
digest=hashlib.sha256(payload).hexdigest();path=root/"host-sidecars-v2.json"
path.write_bytes(payload)
key=("legacy-local-tablebases/v1/snapshots/sha256/"+
 assignment["archive_sha256"]+"/reachability-v2/hosts/"+instance+
 "/sha256/"+digest+"/host-sidecars-v2.json")
put=json.loads(subprocess.check_output(["aws","s3api","put-object","--region",
 assignment["region"],"--bucket",assignment["bucket"],"--key",key,"--body",
 str(path),"--metadata","sha256="+digest+",schema=ultimate-legacy-reachability-host-v2",
 "--output","json"],text=True))
print(json.dumps({"bucket":assignment["bucket"],"key":key,
 "version_id":put["VersionId"],"sha256":digest,"bytes":len(payload)},sort_keys=True))
'''


def exact_download(record: dict[str, object], destination: Path) -> None:
    aws_json([
        "aws", "s3api", "get-object", "--region", REGION,
        "--bucket", str(record["bucket"]), "--key", str(record["key"]),
        "--version-id", str(record["version_id"]), str(destination),
        "--output", "json",
    ])
    if (destination.stat().st_size != int(record["bytes"]) or
            sha256(destination) != str(record["sha256"])):
        raise RuntimeError("version-pinned host sidecar download residual")


def publish(certificate: dict[str, Any], output: Path) -> dict[str, object]:
    assigned = assignments(certificate)
    host_records: list[dict[str, object]] = []
    for instance in assigned:
        text = send_and_wait(
            instance, [
                encoded_write(f"{REMOTE_ROOT}/publish-helper.py",
                              PUBLISH_HELPER.encode()),
                f"python3 {REMOTE_ROOT}/publish-helper.py "
                f"{REMOTE_ROOT} {instance}",
            ])
        lines = [line for line in text.splitlines() if line.startswith("{")]
        if len(lines) != 1:
            raise RuntimeError(f"malformed host publish receipt: {instance}")
        host_records.append(json.loads(lines[0]))
    with tempfile.TemporaryDirectory(prefix="ultimate-legacy-reach-") as raw:
        temporary = Path(raw)
        records: list[dict[str, str]] = []
        for index, remote in enumerate(host_records):
            download = temporary / f"host-{index}.json"
            exact_download(remote, download)
            document = json.loads(download.read_text())
            if document.get("schema") != "ultimate-legacy-reachability-host-v2":
                raise RuntimeError("host reachability schema residual")
            records.extend(document["records"])
        expected = sorted(set(logical_snapshot_files(certificate)) -
                          OBSOLETE_LEGACY)
        names = [str(record["filename"]) for record in records]
        if len(names) != len(set(names)) or sorted(names) != expected:
            raise RuntimeError("combined legacy reachability coverage residual")
        document = {
            "schema": "ultimate-legacy-reachability-v2",
            "predicate": "native-full-causal-reachability",
            "archive": {
                "bucket": BUCKET, "key": ARCHIVE_KEY,
                "version_id": ARCHIVE_VERSION, "sha256": ARCHIVE_SHA,
                "bytes": certificate["archive_bytes"],
                "stream_restore_residual": 0,
            },
            "preservation_certificate": {
                "bucket": BUCKET, "key": CERTIFICATE_KEY,
                "version_id": CERTIFICATE_VERSION,
                "sha256": CERTIFICATE_SHA,
                "bytes": CERTIFICATE_BYTES,
            },
            "supersedes_reachability": {
                "bucket": BUCKET, "key": OLD_REACHABILITY_KEY,
                "version_id": OLD_REACHABILITY_VERSION,
                "sha256": OLD_REACHABILITY_SHA,
            },
            "host_sidecars": host_records,
            "records": sorted(records, key=lambda item: str(item["filename"])),
        }
        published = temporary / "legacy-reachability-v2.json"
        published.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")
        digest = sha256(published)
        key = f"{PREFIX}/reachability-v2/sha256/{digest}/{output.name}"
        put = aws_json([
            "aws", "s3api", "put-object", "--region", REGION,
            "--bucket", BUCKET, "--key", key, "--body", str(published),
            "--metadata", f"sha256={digest},schema={document['schema']}",
            "--output", "json",
        ])
        remote = {
            "bucket": BUCKET, "key": key, "version_id": put["VersionId"],
            "sha256": digest, "bytes": published.stat().st_size,
        }
        exact_download(remote, temporary / "combined.download.json")
    document["s3"] = remote
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")
    return {"files": len(records), "s3": remote, "output": str(output)}


def launch(certificate: dict[str, Any]) -> dict[str, object]:
    assigned = assignments(certificate)
    command_ids: dict[str, str] = {}
    for instance, records in assigned.items():
        document = {
            "region": REGION, "bucket": BUCKET, "archive_key": ARCHIVE_KEY,
            "archive_version": ARCHIVE_VERSION, "archive_sha256": ARCHIVE_SHA,
            "certificate_key": CERTIFICATE_KEY,
            "certificate_version": CERTIFICATE_VERSION,
            "certificate_sha256": CERTIFICATE_SHA, "binary": HOSTS[instance],
            "records": records,
        }
        unit = "ultimatefish-legacy-reachability-v2"
        commands = [
            f"mkdir -p {REMOTE_ROOT}",
            encoded_write(f"{REMOTE_ROOT}/driver.py", REMOTE_DRIVER.encode()),
            encoded_write(f"{REMOTE_ROOT}/assignment.json",
                          (json.dumps(document, sort_keys=True) + "\n").encode()),
            (f"if systemctl is-active --quiet {unit}.service; then echo RUNNING; "
             f"else systemd-run --quiet --collect --unit={unit} "
             f"--property=AllowedCPUs=1-31 --property=Nice=10 /bin/sh -c "
             f"'python3 {REMOTE_ROOT}/driver.py {REMOTE_ROOT} > "
             f"{REMOTE_ROOT}/service.log 2>&1'; echo STARTED; fi"),
        ]
        command_ids[instance] = send_and_wait(instance, commands).strip()
    return {
        "files": sum(map(len, assigned.values())),
        "files_per_instance": {host: len(rows) for host, rows in assigned.items()},
        "launch": command_ids,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--certificate", type=Path, required=True)
    parser.add_argument("--launch", action="store_true")
    parser.add_argument("--publish", type=Path)
    args = parser.parse_args()
    if sha256(args.certificate) != CERTIFICATE_SHA:
        raise RuntimeError("local legacy certificate SHA-256 residual")
    certificate = json.loads(args.certificate.read_text())
    assigned = assignments(certificate)
    if args.launch and args.publish:
        parser.error("choose only one of --launch or --publish")
    if args.launch:
        print(json.dumps(launch(certificate), indent=2, sort_keys=True))
    elif args.publish:
        print(json.dumps(publish(certificate, args.publish),
                         indent=2, sort_keys=True))
    else:
        print(json.dumps({
            "files": sum(map(len, assigned.values())),
            "files_per_instance": {host: len(rows)
                                   for host, rows in assigned.items()},
            "states_per_instance": {
                host: sum(int(row["states"]) for row in rows)
                for host, rows in assigned.items()},
        }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
