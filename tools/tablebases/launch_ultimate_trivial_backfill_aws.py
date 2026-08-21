#!/usr/bin/env python3
"""Stage and launch authenticated trivial-reachability backfill audits.

The auditor is a content-addressed Linux binary built from the exact concrete
model plus the reviewed trivial-position overlay.  Every transient service
binds its sidecar to the retained UFTB SHA-256, uploads it to a content-addressed
S3 key, downloads that exact VersionId, and rehashes it before succeeding.

The legacy auditor batch deliberately excludes Angel, Copycat, Ghost, and
Jester classes.  Angel and Copycat rows are handled by the separately pinned
all-codec source bundle/binary; hidden-information Ghost and Jester rows remain
outside the concrete reachability ledger.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import time
from typing import Any

import run_ultimate_concrete_tablebase_shard_aws as concrete
import update_ultimate_tablebase_ledger as ledger


REGION = "us-west-2"
BUCKET = "ultimatefish-info-20260808-a4e679c6-831688117652"
BINARY_SHA256 = "64bfd07c98f14c1ecb970cc5b71dd4c42eef05c80211625f486dacf9b9401843"
BINARY_SOURCE_INSTANCE = "i-03c81f90d2c59a2e7"
BINARY_SOURCE_PATH = (
    "/mnt/ultimatefish/berserker-penguin-opposed-v2/work/"
    "trivial-audit-source-039374c2/ultimate_tablebase-trivial-v1"
)
BINARY_KEY = (
    "sources/auditors/trivial-v1/linux-x86_64/sha256/"
    f"{BINARY_SHA256}/ultimate_tablebase-trivial-v1"
)
MODEL_SHA256 = "ba1f775d14c35ea5ba18e01e5b68efb280cb4ca5116d5a0fd94f620f49d10157"
OVERLAY_SHA256 = "039374c2c4cdaaf150dd0705711e29d5fd2b6d8e2f5e019bd53cd1b7e2f2fc46"
INVENTORY_SHA256 = "ba8d13582359fcb16c0312e54fb0f23073cd910f0dbf173329c5cbb583a5e143"
SPECIAL_SOURCE_SHA256 = (
    "2e7fecc251cff69ddb2bfd956e2dfd40092d495cf45fb0ac652cc4f5a096e05d"
)
SPECIAL_OVERLAY_SHA256 = (
    "9c8d9de646e2d1447fa287f0acbd704129ed736044fd1b92899a4ed36ea17284"
)
SPECIAL_BINARY_SHA256 = (
    "d2f4202810dd8006d3674e7e383c48c2015bc036dd84e081b59fec4fd27a2965"
)
SPECIAL_BINARY_VERSION = "_fpGJzvmM9NL2XCl4akmFAHDm46JskfX"
SPECIAL_BINARY_KEY = (
    "sources/binaries/devil-closure-v1/sha256/"
    f"{SPECIAL_BINARY_SHA256}/ultimate_tablebase-devil-v1"
)
LEGACY_ARCHIVE_SHA256 = (
    "e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263"
)
LEGACY_ARCHIVE_VERSION = "YTpSbC_DShKsF8QwShKRSIBUVf8PH0VJ"
LEGACY_ARCHIVE_KEY = (
    "legacy-local-tablebases/v1/snapshots/sha256/"
    f"{LEGACY_ARCHIVE_SHA256}/local-tablebases-{LEGACY_ARCHIVE_SHA256}.tar.zst"
)
LEGACY_CERTIFICATE_SHA256 = (
    "1232600dfdc1be8be002701245e6523bfde8334dd4594c8241429a88b5cd825d"
)
LEGACY_CERTIFICATE_VERSION = "5hp3sJXKSZLcNwkzKEcs8AXrYuGea3RY"
LEGACY_CERTIFICATE_KEY = (
    "legacy-local-tablebases/v1/snapshots/sha256/"
    f"{LEGACY_ARCHIVE_SHA256}/certificates/{LEGACY_CERTIFICATE_SHA256}.json"
)
LEGACY_ROOTS = {
    "i-024a2073283e4336e": "/mnt/ultimatefish/trivial-backfill-legacy-e296",
    "i-03c81f90d2c59a2e7": "/mnt/ultimatefish/trivial-backfill-legacy-e296",
    "i-08c0f44a1776cb34a": "/mnt/ultimatefish/trivial-backfill-legacy-e296",
    "i-0986ed3d272721f02": "/mnt/ultimatefish/trivial-backfill-legacy-e296",
    "i-0b4523116b2f7765c": "/mnt/ultimatefish-jg/trivial-backfill-legacy-e296",
}
RESTORED_CPU = {
    "i-024a2073283e4336e": ("4-31", 28),
    "i-03c81f90d2c59a2e7": ("3-13,15-18,20-22,24-31", 26),
    "i-08c0f44a1776cb34a": ("0-13,15-31", 31),
    "i-0986ed3d272721f02": ("3-31", 29),
    "i-0b4523116b2f7765c": ("13-20,22-23,25-28,30-31", 16),
}
RESTORED_RUNNER = Path(__file__).with_name(
    "run_ultimate_trivial_backfill_batch_aws.py")

# CPU sets are disjoint from the long-lived services observed immediately
# before this batch.  One audit per host keeps memory and storage pressure low.
BATCH: tuple[dict[str, Any], ...] = (
    {
        "instance": "i-03c81f90d2c59a2e7",
        "filename": "kberserkerkfisherman.uftb",
        "work": "/mnt/ultimatefish-penguin/berserker-radius-audit-v1/trivial-backfill-kberserkerkfisherman-v1",
        "output": "/mnt/ultimatefish-penguin/berserker-radius-audit-v1/tables/kberserkerkfisherman.uftb",
        "cpus": "6-31",
        "workers": 26,
    },
    {
        "instance": "i-0986ed3d272721f02",
        "filename": "kberserkerkdragon.uftb",
        "work": "/mnt/ultimatefish/trivial-backfill-kberserkerkdragon-v1",
        "output": "/mnt/ultimatefish/trivial-backfill-kberserkerkdragon-v1/outputs/kberserkerkdragon.uftb",
        "input_s3": {
            "bucket": "ultimatefish-info-20260808-a4e679c6-831688117652",
            "key": "staging/trivial-reachability-v3/uftb/kberserkerkdragon.uftb/sha256/463cca983d48f134bf94576fc29ce2fa3b48590a2ed22579e433baa3194ce8e9/kberserkerkdragon.uftb",
            "sha256": "463cca983d48f134bf94576fc29ce2fa3b48590a2ed22579e433baa3194ce8e9",
            "version_id": "KcX4wA7h.UIzFQY1asfNk6.k85_mlR2T"
        },
        "cpus": "3-31",
        "workers": 29,
    },
    {
        "instance": "i-024a2073283e4336e",
        "filename": "kberserkerkturtle.uftb",
        "work": "/mnt/ultimatefish/trivial-backfill-kberserkerkturtle-v1",
        "output": "/mnt/ultimatefish/trivial-backfill-kberserkerkturtle-v1/outputs/kberserkerkturtle.uftb",
        "input_s3": {
            "bucket": "ultimatefish-info-20260808-a4e679c6-831688117652",
            "key": "staging/trivial-reachability-v3/uftb/kberserkerkturtle.uftb/sha256/7b38de854d35a8d8b94bb2ddae3edf69202ce5cbd8d3a58f329b3d12b5f977c1/kberserkerkturtle.uftb",
            "sha256": "7b38de854d35a8d8b94bb2ddae3edf69202ce5cbd8d3a58f329b3d12b5f977c1",
            "version_id": "WGhBE82H7PZo4fk6_np5BxadcM0.TZ8Z"
        },
        "cpus": "4-31",
        "workers": 28,
    },
    {
        "instance": "i-08c0f44a1776cb34a",
        "filename": "kbishoppenguink.uftb",
        "work": "/mnt/ultimatefish/current-wave0-36",
        "cpus": "0-12,14-31",
        "workers": 31,
    },
    {
        "instance": "i-0b4523116b2f7765c",
        "filename": "kbishopprincek.uftb",
        "work": "/mnt/ultimatefish/current-dependencies-queen-v1/trivial-backfill-kbishopprince-v1",
        "output": "/mnt/ultimatefish/current-dependencies-queen-v1/kbishopprincek.uftb",
        "cpus": "13-20,23-31",
        "workers": 17,
    },
)


def aws_json(arguments: list[str]) -> dict[str, Any]:
    value = json.loads(subprocess.check_output(arguments, text=True))
    if not isinstance(value, dict):
        raise RuntimeError("AWS command did not return a JSON object")
    return value


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def put_versioned(path: Path, key: str, metadata: dict[str, str]) -> dict[str, str]:
    digest = sha256(path)
    response = aws_json([
        "aws", "s3api", "put-object", "--region", REGION, "--bucket", BUCKET,
        "--key", key, "--body", str(path), "--metadata",
        ",".join(f"{name}={value}" for name, value in metadata.items()),
        "--output", "json",
    ])
    version = str(response["VersionId"])
    return {"key": key, "sha256": digest, "version_id": version}


def legacy_expected_digests(rows: dict[str, ledger.Entry]) -> dict[str, str]:
    with tempfile.TemporaryDirectory() as directory:
        target = Path(directory) / "certificate.json"
        subprocess.check_call([
            "aws", "s3api", "get-object", "--region", REGION,
            "--bucket", BUCKET, "--key", LEGACY_CERTIFICATE_KEY,
            "--version-id", LEGACY_CERTIFICATE_VERSION, str(target),
        ], stdout=subprocess.DEVNULL)
        if sha256(target) != LEGACY_CERTIFICATE_SHA256:
            raise RuntimeError("legacy certificate restore SHA-256 residual")
        certificate = json.loads(target.read_text())
    if (certificate.get("schema") != "ultimate-local-tablebase-snapshot-v1" or
            certificate.get("archive_sha256") != LEGACY_ARCHIVE_SHA256):
        raise RuntimeError("legacy preservation certificate binding residual")
    artifacts = {
        Path(str(item["path"])).name: str(item["sha256"])
        for item in certificate["manifest"]["artifacts"]
        if str(item["path"]).endswith(".uftb") and int(item["bytes"]) > 1024
    }
    return {
        filename: digest for filename, digest in artifacts.items()
        if filename in rows and LEGACY_ARCHIVE_SHA256 in rows[filename].storage and
        LEGACY_ARCHIVE_VERSION in rows[filename].storage and
        LEGACY_CERTIFICATE_SHA256 in rows[filename].storage and
        LEGACY_CERTIFICATE_VERSION in rows[filename].storage
    }


def send_and_wait(instance: str, commands: list[str], timeout: int = 300) -> str:
    response = aws_json([
        "aws", "ssm", "send-command", "--region", REGION,
        "--instance-ids", instance, "--document-name", "AWS-RunShellScript",
        "--parameters", json.dumps({"commands": commands}), "--output", "json",
    ])
    command_id = str(response["Command"]["CommandId"])
    deadline = time.monotonic() + timeout
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
                f"SSM {status} on {instance}: "
                f"{invocation.get('StandardErrorContent', '')}")
        if time.monotonic() >= deadline:
            raise RuntimeError(f"SSM timed out on {instance}")
        time.sleep(1)


def stage_binary() -> dict[str, str]:
    verify = "/tmp/ultimate_tablebase-trivial-v1.s3-verify"
    command = "\n".join((
        "set -euo pipefail",
        f"test \"$(sha256sum {shlex.quote(BINARY_SOURCE_PATH)} | cut -d' ' -f1)\" = {BINARY_SHA256}",
        f"version=$(aws s3api put-object --region {REGION} --bucket {BUCKET} "
        f"--key {BINARY_KEY} --body {shlex.quote(BINARY_SOURCE_PATH)} "
        f"--metadata sha256={BINARY_SHA256},model-sha256={MODEL_SHA256},overlay-sha256={OVERLAY_SHA256} "
        "--query VersionId --output text)",
        "test \"$version\" != None",
        f"test \"$(aws s3api head-object --region {REGION} --bucket {BUCKET} "
        f"--key {BINARY_KEY} --version-id \"$version\" --query Metadata.sha256 --output text)\" = {BINARY_SHA256}",
        f"aws s3api get-object --region {REGION} --bucket {BUCKET} --key {BINARY_KEY} "
        f"--version-id \"$version\" {verify} >/dev/null",
        f"test \"$(sha256sum {verify} | cut -d' ' -f1)\" = {BINARY_SHA256}",
        f"rm -f {verify}",
        "echo BINARY_STAGED_VERSION_ID=$version",
    ))
    output = send_and_wait(BINARY_SOURCE_INSTANCE, [command])
    match = re.search(r"^BINARY_STAGED_VERSION_ID=(\S+)$", output, re.MULTILINE)
    if not match:
        raise RuntimeError(f"binary staging receipt missing: {output}")
    return {"bucket": BUCKET, "key": BINARY_KEY,
            "version_id": match.group(1), "sha256": BINARY_SHA256}


def stage_tables(paths: list[str]) -> list[dict[str, str]]:
    receipts: list[dict[str, str]] = []
    for index, path in enumerate(paths):
        filename = Path(path).name
        if Path(filename).suffix != ".uftb":
            raise ValueError(f"not a UFTB path: {path}")
        verify = f"/tmp/{filename}.trivial-stage-{index}"
        command = "\n".join((
            "set -euo pipefail",
            f"test -f {shlex.quote(path)}",
            f"digest=$(sha256sum {shlex.quote(path)} | cut -d' ' -f1)",
            f"key=staging/trivial-reachability-v3/uftb/{filename}/sha256/$digest/{filename}",
            f"version=$(aws s3api put-object --region {REGION} --bucket {BUCKET} "
            f"--key \"$key\" --body {shlex.quote(path)} "
            "--metadata sha256=$digest,purpose=trivial-reachability-v3 "
            "--query VersionId --output text)",
            "test \"$version\" != None",
            f"aws s3api get-object --region {REGION} --bucket {BUCKET} --key \"$key\" "
            f"--version-id \"$version\" {verify} >/dev/null",
            f"test \"$(sha256sum {verify} | cut -d' ' -f1)\" = \"$digest\"",
            f"rm -f {verify}",
            f"echo TABLE_STAGED filename={filename} sha256=$digest version_id=$version key=$key",
        ))
        output = send_and_wait(BINARY_SOURCE_INSTANCE, [command], timeout=900)
        match = re.search(
            rf"^TABLE_STAGED filename={re.escape(filename)} sha256=([0-9a-f]{{64}}) "
            r"version_id=(\S+) key=(\S+)$", output, re.MULTILINE)
        if not match:
            raise RuntimeError(f"table staging receipt missing: {output}")
        receipts.append({"filename": filename, "sha256": match.group(1),
                         "version_id": match.group(2), "key": match.group(3),
                         "bucket": BUCKET})
    return receipts


def inventory() -> dict[str, dict[str, object]]:
    # The current shard runner catalog intentionally contains only stateful,
    # Copycat, and Angel K+K+2 rows.  The preservation snapshot also contains
    # the older stateless and K+piece-v-K inventory, which uses the same exact
    # generator/auditor codec.  Merge both catalogs with the current records
    # taking precedence for filenames whose state model was superseded.
    rows: dict[str, dict[str, object]] = {}
    for row in (*concrete.plan.inventory(), *concrete.supported_inventory()):
        rows[str(row["filename"])] = row
    return rows


def launch_command(spec: dict[str, Any], version: str) -> str:
    record = inventory()[str(spec["filename"])]
    if any(str(record.get(field) or "") in {"angel", "copycat", "ghost", "jester"}
           for field in ("primary", "secondary")):
        raise RuntimeError(f"special material excluded from batch: {spec['filename']}")
    filename = str(spec["filename"])
    stem = Path(filename).stem
    work = str(spec["work"])
    output = str(spec.get("output", f"{work}/outputs/{filename}"))
    sidecar = f"{work}/{stem}.reachability-trivial-v3.txt"
    verify = f"{work}/{stem}.reachability-trivial-v3.s3-verify.txt"
    binary_dir = f"{work}/trivial-auditor-{BINARY_SHA256[:8]}"
    binary = f"{binary_dir}/ultimate_tablebase-trivial-v1"
    unit = f"ultimatefish-trivial-backfill-{stem}{spec.get('unit_suffix', '')}"
    command = [binary, "--piece", str(record["primary"]),
               "--workers", str(spec["workers"]), "--checkpoint-every", "0"]
    if record.get("secondary"):
        command.extend(("--piece2", str(record["secondary"])))
    if record.get("opposing"):
        command.append("--opposing")
    command.extend(("--audit-reachability", output))
    quoted_command = " ".join(map(shlex.quote, command))
    input_steps: list[str] = []
    if spec.get("input_s3"):
        source = spec["input_s3"]
        input_steps = [
            f"mkdir -p {shlex.quote(str(Path(output).parent))}",
            f"if test ! -f {shlex.quote(output)}; then aws s3api get-object --region {REGION} "
            f"--bucket {source['bucket']} --key {source['key']} "
            f"--version-id {source['version_id']} {shlex.quote(output)} >/dev/null; fi",
            f"test \"$(sha256sum {shlex.quote(output)} | cut -d' ' -f1)\" = {source['sha256']}",
        ]
    payload = "\n".join((
        "set -euo pipefail",
        *input_steps,
        f"test -f {shlex.quote(output)}",
        f"mkdir -p {shlex.quote(binary_dir)}",
        f"if test ! -f {shlex.quote(binary)}; then aws s3api get-object --region {REGION} "
        f"--bucket {BUCKET} --key {BINARY_KEY} --version-id {shlex.quote(version)} "
        f"{shlex.quote(binary)} >/dev/null; chmod 0555 {shlex.quote(binary)}; fi",
        f"test \"$(sha256sum {shlex.quote(binary)} | cut -d' ' -f1)\" = {BINARY_SHA256}",
        f"table_sha=$(sha256sum {shlex.quote(output)} | cut -d' ' -f1)",
        "{",
        f"  echo 'reachability_binding filename {filename} output_sha256 '$table_sha",
        f"  echo 'trivial_audit_binding source_overlay_sha256 {OVERLAY_SHA256} model_sha256 {MODEL_SHA256} inventory_sha256 {INVENTORY_SHA256} binary_sha256 {BINARY_SHA256}'",
        f"  {quoted_command}",
        f"}} >{shlex.quote(sidecar)}.tmp",
        f"test \"$(grep -c '^reachability_trivial side ' {shlex.quote(sidecar)}.tmp)\" = 2",
        f"mv {shlex.quote(sidecar)}.tmp {shlex.quote(sidecar)}",
        f"side_sha=$(sha256sum {shlex.quote(sidecar)} | cut -d' ' -f1)",
        f"side_bytes=$(stat -c %s {shlex.quote(sidecar)})",
        f"key=results/trivial-reachability-v3/{filename}/sha256/$side_sha/{stem}.reachability-trivial-v3.txt",
        f"side_version=$(aws s3api put-object --region {REGION} --bucket {BUCKET} --key \"$key\" "
        f"--body {shlex.quote(sidecar)} --metadata sha256=$side_sha,output-sha256=$table_sha,model-sha256={MODEL_SHA256} "
        "--query VersionId --output text)",
        "test \"$side_version\" != None",
        f"test \"$(aws s3api head-object --region {REGION} --bucket {BUCKET} --key \"$key\" "
        "--version-id \"$side_version\" --query ContentLength --output text)\" = \"$side_bytes\"",
        f"aws s3api get-object --region {REGION} --bucket {BUCKET} --key \"$key\" "
        f"--version-id \"$side_version\" {shlex.quote(verify)} >/dev/null",
        f"test \"$(sha256sum {shlex.quote(verify)} | cut -d' ' -f1)\" = \"$side_sha\"",
        f"echo TRIVIAL_BACKFILL_OK filename={filename} table_sha256=$table_sha sidecar_sha256=$side_sha sidecar_bytes=$side_bytes version_id=$side_version key=$key",
    ))
    start = (
        f"if test -s {shlex.quote(sidecar)} && test -s {shlex.quote(verify)}; then "
        f"echo COMPLETE:{filename}; "
        f"elif test -e {shlex.quote(sidecar)} || test -e {shlex.quote(verify)}; then "
        f"echo INVALID_PARTIAL:{filename}; exit 1; "
        f"elif systemctl is-active --quiet {unit}.service; then echo RUNNING:{filename}; "
        "else systemd-run --quiet --collect "
        f"--unit={unit} --property=AllowedCPUs={shlex.quote(str(spec['cpus']))} "
        "--property=Nice=10 --property=OOMPolicy=stop "
        f"/bin/bash -lc {shlex.quote(payload)}; "
        f"echo STARTED:{filename}:cpus={spec['cpus']}:workers={spec['workers']}; fi"
    )
    return start


def launch_batch(version: str) -> dict[str, str]:
    def one(spec: dict[str, Any]) -> tuple[str, str]:
        return str(spec["instance"]), send_and_wait(
            str(spec["instance"]), [launch_command(spec, version)])
    with ThreadPoolExecutor(max_workers=len(BATCH)) as pool:
        return dict(pool.map(one, BATCH))


def status_batch() -> dict[str, str]:
    def one(spec: dict[str, Any]) -> tuple[str, str]:
        stem = Path(str(spec["filename"])).stem
        unit = (f"ultimatefish-trivial-backfill-{stem}"
                f"{spec.get('unit_suffix', '')}.service")
        work = str(spec["work"])
        filename = str(spec["filename"])
        output = str(spec.get("output", work + "/outputs/" + filename))
        command = (
            f"systemctl show {unit} --property=ActiveState,SubState,Result,ExecMainStatus,AllowedCPUs,CPUUsageNSec,MemoryCurrent --no-pager; "
            f"journalctl -u {unit} -n 30 -o cat --no-pager; "
            f"ls -l {shlex.quote(output)} "
            f"{shlex.quote(work + '/' + stem + '.reachability-trivial-v3.txt')} "
            f"{shlex.quote(work + '/' + stem + '.reachability-trivial-v3.txt.tmp')} 2>&1 || true; "
            f"test -f {shlex.quote(output)} || "
            f"find /mnt -type f -name {shlex.quote(filename)} -exec sha256sum '{{}}' + 2>/dev/null"
        )
        return str(spec["instance"]), send_and_wait(str(spec["instance"]), [command])
    with ThreadPoolExecutor(max_workers=len(BATCH)) as pool:
        return dict(pool.map(one, BATCH))


def restore_legacy() -> dict[str, str]:
    """Restore the exact preservation snapshot without trusting a launch marker."""
    def one(item: tuple[str, str]) -> tuple[str, str]:
        instance, root = item
        archive = f"{root}/legacy.tar.zst"
        certificate = f"{root}/certificate.json"
        extracted = f"{root}/extracted"
        marker = f"{root}/RESTORE.COMPLETE"
        unit = "ultimatefish-trivial-restore-legacy-e296"
        validate_certificate = (
            "import json,sys; d=json.load(open(sys.argv[1])); "
            "assert d['schema'] == 'ultimate-local-tablebase-snapshot-v1'; "
            "assert d['archive_sha256'] == sys.argv[2]; "
            "assert d['local']['files'] == 217; "
            "assert d['local']['stream_restore_residual'] == 0"
        )
        payload = "\n".join((
            "set -euo pipefail",
            f"mkdir -p {shlex.quote(root)} {shlex.quote(extracted)}",
            f"if test ! -f {shlex.quote(archive)}; then aws s3api get-object "
            f"--region {REGION} --bucket {BUCKET} --key {LEGACY_ARCHIVE_KEY} "
            f"--version-id {LEGACY_ARCHIVE_VERSION} {shlex.quote(archive)} >/dev/null; fi",
            f"test \"$(sha256sum {shlex.quote(archive)} | cut -d' ' -f1)\" = {LEGACY_ARCHIVE_SHA256}",
            f"if test ! -f {shlex.quote(certificate)}; then aws s3api get-object "
            f"--region {REGION} --bucket {BUCKET} --key {LEGACY_CERTIFICATE_KEY} "
            f"--version-id {LEGACY_CERTIFICATE_VERSION} {shlex.quote(certificate)} >/dev/null; fi",
            f"test \"$(sha256sum {shlex.quote(certificate)} | cut -d' ' -f1)\" = {LEGACY_CERTIFICATE_SHA256}",
            f"python3 -c {shlex.quote(validate_certificate)} "
            f"{shlex.quote(certificate)} {LEGACY_ARCHIVE_SHA256}",
            f"tar --zstd -xf {shlex.quote(archive)} -C {shlex.quote(extracted)}",
            f"test \"$(find {shlex.quote(extracted)} -type f | wc -l)\" = 218",
            f"printf '%s\\n' archive_sha256={LEGACY_ARCHIVE_SHA256} certificate_sha256={LEGACY_CERTIFICATE_SHA256} >{shlex.quote(marker)}.tmp",
            f"mv {shlex.quote(marker)}.tmp {shlex.quote(marker)}",
        ))
        command = (
            f"if test -s {shlex.quote(marker)}; then echo COMPLETE; "
            f"elif systemctl is-active --quiet {unit}.service; then echo RUNNING; "
            "else systemd-run --quiet --collect "
            f"--unit={unit} --property=Nice=10 --property=OOMPolicy=stop "
            f"/bin/bash -lc {shlex.quote(payload)}; echo STARTED; fi"
        )
        return instance, send_and_wait(instance, [command])

    with ThreadPoolExecutor(max_workers=len(LEGACY_ROOTS)) as pool:
        return dict(pool.map(one, LEGACY_ROOTS.items()))


def legacy_status() -> dict[str, str]:
    def one(item: tuple[str, str]) -> tuple[str, str]:
        instance, root = item
        unit = "ultimatefish-trivial-restore-legacy-e296.service"
        command = (
            f"systemctl show {unit} --property=ActiveState,SubState,Result,ExecMainStatus,CPUUsageNSec,MemoryCurrent --no-pager; "
            f"journalctl -u {unit} -n 20 -o cat --no-pager; "
            f"ls -lh {shlex.quote(root + '/legacy.tar.zst')} {shlex.quote(root + '/certificate.json')} {shlex.quote(root + '/RESTORE.COMPLETE')} 2>&1 || true; "
            f"find {shlex.quote(root + '/extracted')} -type f 2>/dev/null | wc -l"
        )
        return instance, send_and_wait(instance, [command])

    with ThreadPoolExecutor(max_workers=len(LEGACY_ROOTS)) as pool:
        return dict(pool.map(one, LEGACY_ROOTS.items()))


def launch_restored(version: str) -> dict[str, str]:
    retained = retained_inventory()
    by_filename = {
        instance: {Path(path).name: path for path in paths}
        for instance, paths in retained.items()
    }
    filenames = sorted(set().union(*(set(items) for items in by_filename.values())))
    # The three other hosts are occupied by the Angel/Copycat wave.  The
    # authenticated preservation archive is replicated, so these two idle
    # hosts can drain every legacy row without overlapping CPU allocations.
    hosts = ["i-024a2073283e4336e", "i-0b4523116b2f7765c"]
    assignments: dict[str, list[str]] = {host: [] for host in hosts}
    for index, filename in enumerate(filenames):
        available = [host for host in hosts if filename in by_filename[host]]
        if not available:
            raise RuntimeError(f"no restored host for {filename}")
        preferred = hosts[index % len(hosts)]
        host = preferred if preferred in available else available[0]
        assignments[host].append(filename)

    rows = {row.filename: row for row in ledger.entries(
        (Path(__file__).resolve().parents[2] / "tablebases/README.md").read_text())}
    expected = legacy_expected_digests(rows)
    details = ledger.result_rows(
        (Path(__file__).resolve().parents[2] / "tablebases/README.md").read_text())
    supervision = json.loads((Path(__file__).with_name(
        "ultimate_aws_supervision.json")).read_text())
    configured: dict[str, set[str]] = {}
    for job in supervision.get("jobs", []):
        if job.get("superseded_by"):
            continue
        for filename, value in job.get("ledger_results", {}).items():
            storage = str(value.get("storage", ""))
            match = re.search(
                r"(?:^|[; ])(?:result|output|table) sha256:"
                r"([0-9a-f]{64})(?:[; ]|$)", storage)
            if match:
                configured.setdefault(filename, set()).add(match.group(1))
    for filename in filenames:
        if filename in expected:
            continue
        detail = details.get(filename)
        if detail is not None:
            expected[filename] = detail.digest
            continue
        for label in ("result", "output", "table"):
            match = re.search(
                rf"(?:^|[; ]){label} sha256:([0-9a-f]{{64}})(?:[; ]|$)",
                rows[filename].storage)
            if match:
                expected[filename] = match.group(1)
                break
        if filename not in expected and len(configured.get(filename, ())) == 1:
            expected[filename] = next(iter(configured[filename]))
        if filename not in expected:
            raise RuntimeError(f"missing authenticated result digest for {filename}")
    catalog = inventory()
    runner_sha = sha256(RESTORED_RUNNER)
    runner_key = (
        "sources/tools/trivial-backfill-batch-v1/sha256/"
        f"{runner_sha}/{RESTORED_RUNNER.name}")
    runner = put_versioned(
        RESTORED_RUNNER, runner_key,
        {"sha256": runner_sha, "purpose": "trivial-reachability-v3"})

    staged: dict[str, tuple[dict[str, str], int]] = {}
    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)
        for host in hosts:
            cpus, workers = RESTORED_CPU[host]
            items = []
            for filename in assignments[host]:
                record = catalog[filename]
                items.append({
                    "filename": filename,
                    "path": by_filename[host][filename],
                    "sha256": expected[filename],
                    "primary": record["primary"],
                    "secondary": record["secondary"],
                    "opposing": record["opposing"],
                })
            document = {
                "schema": "ultimate-trivial-backfill-batch-v1",
                "region": REGION, "bucket": BUCKET, "workers": workers,
                "binary_sha256": BINARY_SHA256,
                "model_sha256": MODEL_SHA256,
                "overlay_sha256": OVERLAY_SHA256,
                "inventory_sha256": INVENTORY_SHA256,
                "items": items,
            }
            path = temporary / f"plan-{host}.json"
            path.write_text(json.dumps(document, sort_keys=True) + "\n")
            digest = sha256(path)
            key = ("orchestration/trivial-backfill-v3/plans/sha256/"
                   f"{digest}/plan-{host}.json")
            staged[host] = (put_versioned(
                path, key, {"sha256": digest, "runner-sha256": runner_sha}),
                len(items))

    def one(host: str) -> tuple[str, str]:
        cpus, _workers = RESTORED_CPU[host]
        plan, count = staged[host]
        root = (LEGACY_ROOTS[host] + "/restored-batches/" +
                plan["sha256"][:12])
        binary = f"{root}/ultimate_tablebase-trivial-v1"
        remote_runner = f"{root}/{RESTORED_RUNNER.name}"
        remote_plan = f"{root}/plan.json"
        log = f"{root}/batch.log"
        unit = "ultimatefish-trivial-restored-batch-v1"
        payload = "\n".join((
            "set -euo pipefail", f"mkdir -p {shlex.quote(root)}",
            f"aws s3api get-object --region {REGION} --bucket {BUCKET} "
            f"--key {runner['key']} --version-id {runner['version_id']} "
            f"{shlex.quote(remote_runner)} >/dev/null",
            f"test \"$(sha256sum {shlex.quote(remote_runner)} | cut -d' ' -f1)\" = {runner_sha}",
            f"aws s3api get-object --region {REGION} --bucket {BUCKET} "
            f"--key {plan['key']} --version-id {plan['version_id']} "
            f"{shlex.quote(remote_plan)} >/dev/null",
            f"test \"$(sha256sum {shlex.quote(remote_plan)} | cut -d' ' -f1)\" = {plan['sha256']}",
            f"aws s3api get-object --region {REGION} --bucket {BUCKET} "
            f"--key {BINARY_KEY} --version-id {shlex.quote(version)} "
            f"{shlex.quote(binary)} >/dev/null",
            f"test \"$(sha256sum {shlex.quote(binary)} | cut -d' ' -f1)\" = {BINARY_SHA256}",
            f"chmod 0555 {shlex.quote(binary)} {shlex.quote(remote_runner)}",
            f"python3 {shlex.quote(remote_runner)} --plan {shlex.quote(remote_plan)} "
            f"--binary {shlex.quote(binary)} --work {shlex.quote(root)} "
            f"2>&1 | tee -a {shlex.quote(log)}",
        ))
        command = (
            f"if test \"$(find {shlex.quote(root)} -name '*.receipt.json' 2>/dev/null | wc -l)\" = {count}; "
            f"then echo COMPLETE:{count}; "
            f"elif systemctl is-active --quiet {unit}.service; then echo RUNNING:{count}; "
            "else systemd-run --quiet --collect "
            f"--unit={unit} --property=AllowedCPUs={shlex.quote(cpus)} "
            "--property=Nice=10 --property=OOMPolicy=stop "
            f"/bin/bash -lc {shlex.quote(payload)}; echo STARTED:{count}:cpus={cpus}; fi"
        )
        return host, send_and_wait(host, [command])

    with ThreadPoolExecutor(max_workers=len(hosts)) as pool:
        return dict(pool.map(one, hosts))


def restored_status() -> dict[str, str]:
    def one(host: str) -> tuple[str, str]:
        root = LEGACY_ROOTS[host] + "/restored-batches"
        unit = "ultimatefish-trivial-restored-batch-v1.service"
        command = (
            f"systemctl show {unit} --property=ActiveState,SubState,Result,ExecMainStatus,AllowedCPUs,CPUUsageNSec,MemoryCurrent --no-pager; "
            f"journalctl -u {unit} -n 8 -o cat --no-pager; "
            f"echo receipts=$(find {shlex.quote(root)} -name '*.receipt.json' 2>/dev/null | wc -l); "
            f"tail -n 8 {shlex.quote(root + '/batch.log')} 2>/dev/null || true"
        )
        return host, send_and_wait(host, [command])
    with ThreadPoolExecutor(max_workers=len(LEGACY_ROOTS)) as pool:
        return dict(pool.map(one, sorted(LEGACY_ROOTS)))


def s3_status(*, special: bool = False) -> dict[str, str]:
    def one(host: str) -> tuple[str, str]:
        root = LEGACY_ROOTS[host] + (
            "/special-s3-batches" if special else "/s3-batches")
        unit = ("ultimatefish-trivial-special-s3-batch-v1.service" if special
                else "ultimatefish-trivial-s3-batch-v1.service")
        command = (
            f"systemctl show {unit} --property=ActiveState,SubState,Result,ExecMainStatus,AllowedCPUs,CPUUsageNSec,MemoryCurrent --no-pager; "
            f"journalctl -u {unit} -n 6 -o cat --no-pager; "
            f"echo receipts=$(find {root} -name '*.receipt.json' 2>/dev/null | wc -l); "
            f"find {root} -name batch.log -type f -exec tail -n 3 '{{}}' \; 2>/dev/null || true")
        return host, send_and_wait(host, [command])
    with ThreadPoolExecutor(max_workers=len(LEGACY_ROOTS)) as pool:
        return dict(pool.map(one, sorted(LEGACY_ROOTS)))


def restored_receipts() -> list[dict[str, str]]:
    def one(host: str) -> list[dict[str, str]]:
        roots = (LEGACY_ROOTS[host] + "/restored-batches " +
                 LEGACY_ROOTS[host] + "/batch-v1 " +
                 LEGACY_ROOTS[host] + "/s3-batches " +
                 LEGACY_ROOTS[host] + "/special-s3-batches " +
                 LEGACY_ROOTS[host] + "/gap-batches")
        listing = send_and_wait(host, [
            f"find {roots} -name '*.receipt.json' -type f -print 2>/dev/null || true"])
        paths = listing.splitlines()
        output = ""
        for begin in range(0, len(paths), 15):
            output += send_and_wait(host, [
                "cat " + " ".join(map(
                    shlex.quote, paths[begin:begin + 15]))])
        values = []
        for line in output.splitlines():
            if not line.startswith("{"):
                continue
            value = json.loads(line)
            value["instance"] = host
            values.append(value)
        return values
    with ThreadPoolExecutor(max_workers=len(LEGACY_ROOTS)) as pool:
        batches = list(pool.map(one, sorted(LEGACY_ROOTS)))
    by_filename: dict[str, dict[str, str]] = {}
    for value in (item for batch in batches for item in batch):
        filename = str(value["filename"])
        previous = by_filename.get(filename)
        if previous is not None and (
                previous["sidecar_sha256"] != value["sidecar_sha256"] or
                previous["output_sha256"] != value["output_sha256"]):
            raise RuntimeError(f"conflicting restored receipts for {filename}")
        by_filename[filename] = value
    return [by_filename[name] for name in sorted(by_filename)]


def launch_s3(version: str, versions_path: Path, *, special: bool = False
              ) -> dict[str, str]:
    readme = (Path(__file__).resolve().parents[2] / "tablebases/README.md")
    rows = {row.filename: row for row in ledger.entries(readme.read_text())}
    details = ledger.result_rows(readme.read_text())
    catalog = inventory()
    versions = json.loads(versions_path.read_text())
    by_version: dict[str, list[dict[str, object]]] = {}
    for item in versions:
        by_version.setdefault(str(item["VersionId"]), []).append(item)
    pending = []
    certificate_cache: dict[str, dict[str, object]] = {}
    certificate_temporary = tempfile.TemporaryDirectory(
        prefix="ultimate-trivial-certificates-")
    certificate_root = Path(certificate_temporary.name)
    for filename, row in rows.items():
        special_material = (filename in catalog and any(
            str(catalog[filename].get(field) or "") in {"angel", "copycat"}
            for field in ("primary", "secondary")))
        hidden_material = (filename in catalog and any(
            str(catalog[filename].get(field) or "") in {"ghost", "jester"}
            for field in ("primary", "secondary")))
        if (row.status != "certified" or row.result_kind != "concrete" or
                "[" in row.first + row.second or filename not in catalog or
                hidden_material or special_material != special):
            continue
        match = re.search(
            r"S3 (?:(?:table|(?:legacy )?archive) )?sha256:([0-9a-f]{64}) "
            r"VersionId ([^; ]+)",
            row.storage)
        if not match:
            continue
        archive_sha, archive_version = match.groups()
        candidates = [item for item in by_version.get(archive_version, [])
                      if str(item["Key"]).endswith(".tar.zst") and
                      archive_sha in str(item["Key"])]
        if len(candidates) != 1:
            raise RuntimeError(
                f"{filename}: expected one archive for VersionId, got {len(candidates)}")
        archive = candidates[0]
        detail = details.get(filename)
        output_sha = detail.digest if detail is not None else None
        if output_sha is None:
            certificate_match = re.search(
                r"certificate sha256:([0-9a-f]{64}) VersionId ([^; ]+)",
                row.storage)
            if not certificate_match:
                raise RuntimeError(f"{filename}: missing result certificate binding")
            certificate_sha, certificate_version = certificate_match.groups()
            if certificate_version not in certificate_cache:
                certificate_items = [
                    item for item in by_version.get(certificate_version, [])
                    if str(item["Key"]).endswith(".json") and
                    certificate_sha in str(item["Key"])]
                if len(certificate_items) != 1:
                    raise RuntimeError(
                        f"{filename}: result certificate VersionId residual")
                certificate_item = certificate_items[0]
                target = certificate_root / f"{certificate_sha}.json"
                subprocess.check_call([
                    "aws", "s3api", "get-object", "--region", REGION,
                    "--bucket", BUCKET, "--key", str(certificate_item["Key"]),
                    "--version-id", certificate_version, str(target),
                ], stdout=subprocess.DEVNULL)
                if sha256(target) != certificate_sha:
                    raise RuntimeError("result certificate SHA-256 residual")
                certificate_cache[certificate_version] = json.loads(
                    target.read_text())
            encoded = concrete.encoded_filename(filename)
            completed = [
                item for item in certificate_cache[certificate_version].get(
                    "completed", [])
                if item.get("filename") == encoded]
            if len(completed) != 1:
                raise RuntimeError(f"{filename}: certificate completed-row residual")
            output_sha = str(completed[0]["output"]["sha256"])
        record = catalog[filename]
        pending.append({
            "filename": filename, "sha256": output_sha,
            "primary": record["primary"], "secondary": record["secondary"],
            "opposing": record["opposing"],
            "encoded_filename": concrete.encoded_filename(filename),
            "archive": {"key": archive["Key"],
                        "version_id": archive_version,
                        "sha256": archive_sha,
                        "size": int(archive["Size"])},
        })
    if not pending:
        return {host: "COMPLETE:0\n" for host in sorted(LEGACY_ROOTS)}

    # Start special-codec work immediately on the three hosts whose ordinary
    # wave is already complete.  The two remaining hosts keep their disjoint
    # ordinary CPU sets until those current-output audits drain.
    hosts = (sorted(LEGACY_ROOTS) if not special else [
        "i-03c81f90d2c59a2e7", "i-08c0f44a1776cb34a",
        "i-0986ed3d272721f02"])
    assigned: dict[str, list[dict[str, object]]] = {host: [] for host in hosts}
    weights = {host: 0 for host in hosts}
    for item in sorted(pending, key=lambda value: -int(value["archive"]["size"])):
        host = min(hosts, key=lambda value: (weights[value], value))
        assigned[host].append(item)
        weights[host] += int(item["archive"]["size"])

    binary_sha = SPECIAL_BINARY_SHA256 if special else BINARY_SHA256
    binary_key = SPECIAL_BINARY_KEY if special else BINARY_KEY
    model_sha = SPECIAL_SOURCE_SHA256 if special else MODEL_SHA256
    overlay_sha = SPECIAL_OVERLAY_SHA256 if special else OVERLAY_SHA256
    runner_sha = sha256(RESTORED_RUNNER)
    runner_key = ("sources/tools/trivial-backfill-batch-v1/sha256/"
                  f"{runner_sha}/{RESTORED_RUNNER.name}")
    runner = put_versioned(
        RESTORED_RUNNER, runner_key,
        {"sha256": runner_sha, "purpose": "trivial-reachability-v3"})
    staged: dict[str, tuple[dict[str, str], int]] = {}
    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)
        for host in hosts:
            _cpus, workers = RESTORED_CPU[host]
            host_items = []
            for source_item in assigned[host]:
                item = dict(source_item)
                archive_item = item.get("archive")
                if (isinstance(archive_item, dict) and
                        archive_item.get("sha256") == LEGACY_ARCHIVE_SHA256):
                    item.pop("archive")
                    item["path"] = (LEGACY_ROOTS[host] +
                                    "/extracted/tablebases/" +
                                    str(item["encoded_filename"]))
                host_items.append(item)
            document = {
                "schema": "ultimate-trivial-backfill-batch-v1",
                "region": REGION, "bucket": BUCKET, "workers": workers,
                "binary_sha256": binary_sha, "model_sha256": model_sha,
                "overlay_sha256": overlay_sha,
                "inventory_sha256": INVENTORY_SHA256,
                "items": host_items,
            }
            path = temporary / f"s3-plan-{host}.json"
            path.write_text(json.dumps(document, sort_keys=True) + "\n")
            digest = sha256(path)
            key = ("orchestration/trivial-backfill-v3/s3-plans/sha256/"
                   f"{digest}/plan-{host}.json")
            staged[host] = (put_versioned(
                path, key, {"sha256": digest, "runner-sha256": runner_sha}),
                len(assigned[host]))

    def one(host: str) -> tuple[str, str]:
        cpus, _workers = RESTORED_CPU[host]
        plan, count = staged[host]
        batch_kind = "special-s3-batches" if special else "s3-batches"
        root = (LEGACY_ROOTS[host] + f"/{batch_kind}/" +
                plan["sha256"][:12])
        binary = f"{root}/ultimate_tablebase-trivial-v1"
        remote_runner = f"{root}/{RESTORED_RUNNER.name}"
        remote_plan = f"{root}/plan.json"
        log = f"{root}/batch.log"
        unit = ("ultimatefish-trivial-special-s3-batch-v1" if special else
                "ultimatefish-trivial-s3-batch-v1")
        payload = "\n".join((
            "set -euo pipefail", f"mkdir -p {shlex.quote(root)}",
            f"aws s3api get-object --region {REGION} --bucket {BUCKET} --key {runner['key']} --version-id {runner['version_id']} {remote_runner} >/dev/null",
            f"test \"$(sha256sum {remote_runner} | cut -d' ' -f1)\" = {runner_sha}",
            f"aws s3api get-object --region {REGION} --bucket {BUCKET} --key {plan['key']} --version-id {plan['version_id']} {remote_plan} >/dev/null",
            f"test \"$(sha256sum {remote_plan} | cut -d' ' -f1)\" = {plan['sha256']}",
            f"aws s3api get-object --region {REGION} --bucket {BUCKET} --key {binary_key} --version-id {version} {binary} >/dev/null",
            f"test \"$(sha256sum {binary} | cut -d' ' -f1)\" = {binary_sha}",
            f"chmod 0555 {binary} {remote_runner}",
            f"python3 {remote_runner} --plan {remote_plan} --binary {binary} --work {root} 2>&1 | tee -a {log}",
        ))
        command = (
            f"if test \"$(find {root} -name '*.receipt.json' 2>/dev/null | wc -l)\" = {count}; then echo COMPLETE:{count}; "
            f"elif systemctl is-active --quiet {unit}.service; then echo RUNNING:{count}; "
            f"else systemd-run --quiet --collect --unit={unit} --property=AllowedCPUs={shlex.quote(cpus)} --property=Nice=10 --property=OOMPolicy=stop /bin/bash -lc {shlex.quote(payload)}; echo STARTED:{count}:cpus={cpus}; fi")
        return host, send_and_wait(host, [command])
    with ThreadPoolExecutor(max_workers=len(hosts)) as pool:
        return dict(pool.map(one, hosts))


def launch_legacy_gaps(version: str, versions_path: Path) -> str:
    """Audit two authenticated split payloads and the direct 2-Berserker object."""
    host = "i-024a2073283e4336e"
    root = LEGACY_ROOTS[host]
    text = (Path(__file__).resolve().parents[2] /
            "tablebases/README.md").read_text()
    details = ledger.result_rows(text)
    catalog = inventory()
    versions = json.loads(versions_path.read_text())
    split_stubs = {
        "kbombsniperk.uftb":
            "442aba83167a5e8c664114888c544b83e8c834601acd95ca90369ad3058a360e",
        "kbombcheckerk.uftb":
            "a22b7417781e536e0e18b6481f36181cedbdfaca0a213e2da66bcf62f77d5609",
    }
    items: list[dict[str, object]] = []
    for filename, stub_sha in split_stubs.items():
        record = catalog[filename]
        items.append({
            "filename": filename,
            "path": f"{root}/extracted/tablebases/{filename}",
            "sha256": details[filename].digest,
            "split_stub_sha256": stub_sha,
            "primary": record["primary"], "secondary": record["secondary"],
            "opposing": record["opposing"],
        })
    filename = "kberserkerberserkerk.uftb"
    output_sha = "a1f07eae1bc073927fe220cae1df17428b3b3b94183614c6f18698dafa3529ff"
    object_version = "qO5pVM0cQJT91GqwRmxWcYKyhyFTbaF9"
    objects = [item for item in versions
               if item.get("VersionId") == object_version and
               str(item.get("Key", "")).endswith("/" + filename) and
               output_sha in str(item.get("Key", ""))]
    if len(objects) != 1:
        raise RuntimeError("double-Berserker exact S3 object residual")
    record = catalog[filename]
    items.append({
        "filename": filename, "sha256": output_sha,
        "primary": record["primary"], "secondary": record["secondary"],
        "opposing": record["opposing"],
        "object": {"key": objects[0]["Key"], "version_id": object_version},
    })

    runner_sha = sha256(RESTORED_RUNNER)
    runner = put_versioned(
        RESTORED_RUNNER,
        "sources/tools/trivial-backfill-batch-v1/sha256/"
        f"{runner_sha}/{RESTORED_RUNNER.name}",
        {"sha256": runner_sha, "purpose": "trivial-reachability-v3"})
    cpus, workers = RESTORED_CPU[host]
    document = {
        "schema": "ultimate-trivial-backfill-batch-v1",
        "region": REGION, "bucket": BUCKET, "workers": workers,
        "binary_sha256": BINARY_SHA256, "model_sha256": MODEL_SHA256,
        "overlay_sha256": OVERLAY_SHA256,
        "inventory_sha256": INVENTORY_SHA256, "items": items,
    }
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "legacy-gap-plan.json"
        path.write_text(json.dumps(document, sort_keys=True) + "\n")
        digest = sha256(path)
        plan = put_versioned(
            path, "orchestration/trivial-backfill-v3/gap-plans/sha256/"
            f"{digest}/plan.json",
            {"sha256": digest, "runner-sha256": runner_sha})
    work = f"{root}/gap-batches/{plan['sha256'][:12]}"
    binary = f"{work}/ultimate_tablebase-trivial-v1"
    remote_runner = f"{work}/{RESTORED_RUNNER.name}"
    remote_plan = f"{work}/plan.json"
    log = f"{work}/batch.log"
    unit = "ultimatefish-trivial-gap-batch-v1"
    payload = "\n".join((
        "set -euo pipefail", f"mkdir -p {shlex.quote(work)}",
        f"aws s3api get-object --region {REGION} --bucket {BUCKET} --key {runner['key']} --version-id {runner['version_id']} {remote_runner} >/dev/null",
        f"test \"$(sha256sum {remote_runner} | cut -d' ' -f1)\" = {runner_sha}",
        f"aws s3api get-object --region {REGION} --bucket {BUCKET} --key {plan['key']} --version-id {plan['version_id']} {remote_plan} >/dev/null",
        f"test \"$(sha256sum {remote_plan} | cut -d' ' -f1)\" = {plan['sha256']}",
        f"aws s3api get-object --region {REGION} --bucket {BUCKET} --key {BINARY_KEY} --version-id {version} {binary} >/dev/null",
        f"test \"$(sha256sum {binary} | cut -d' ' -f1)\" = {BINARY_SHA256}",
        f"chmod 0555 {binary} {remote_runner}",
        f"python3 {remote_runner} --plan {remote_plan} --binary {binary} --work {work} 2>&1 | tee -a {log}",
    ))
    command = (
        f"if test \"$(find {work} -name '*.receipt.json' 2>/dev/null | wc -l)\" = 3; then echo COMPLETE:3; "
        f"elif systemctl is-active --quiet {unit}.service; then echo RUNNING:3; "
        f"else systemd-run --quiet --collect --unit={unit} --property=AllowedCPUs={shlex.quote(cpus)} --property=Nice=10 --property=OOMPolicy=stop /bin/bash -lc {shlex.quote(payload)}; echo STARTED:3:cpus={cpus}; fi")
    return send_and_wait(host, [command])


def gap_status() -> str:
    host = "i-024a2073283e4336e"
    root = LEGACY_ROOTS[host] + "/gap-batches"
    unit = "ultimatefish-trivial-gap-batch-v1.service"
    return send_and_wait(host, [
        f"systemctl show {unit} --property=ActiveState,SubState,Result,ExecMainStatus,AllowedCPUs,CPUUsageNSec,MemoryCurrent --no-pager; "
        f"journalctl -u {unit} -n 8 -o cat --no-pager; "
        f"echo receipts=$(find {root} -name '*.receipt.json' 2>/dev/null | wc -l); "
        f"find {root} -name batch.log -type f -exec tail -n 4 '{{}}' \\; 2>/dev/null || true"])


def retained_inventory() -> dict[str, list[str]]:
    readme_text = (Path(__file__).resolve().parents[2] /
                   "tablebases/README.md").read_text()
    rows = {row.filename: row for row in ledger.entries(readme_text)}
    details = ledger.result_rows(readme_text)
    catalog = inventory()
    supervision = json.loads((Path(__file__).with_name(
        "ultimate_aws_supervision.json")).read_text())
    configured_digests: dict[str, set[str]] = {}
    for job in supervision.get("jobs", []):
        if job.get("superseded_by"):
            continue
        for filename, value in job.get("ledger_results", {}).items():
            storage = str(value.get("storage", ""))
            for label in ("result", "output", "table"):
                match = re.search(
                    rf"(?:^|[; ]){label} sha256:([0-9a-f]{{64}})(?:[; ]|$)",
                    storage)
                if match:
                    configured_digests.setdefault(filename, set()).add(
                        match.group(1))
                    break
    expected: dict[str, str] = {}
    for filename, row in rows.items():
        if (row.status != "certified" or row.result_kind != "concrete" or
                "[" in row.first + row.second or filename not in catalog or
                any(str(catalog[filename].get(field) or "") in
                    {"angel", "copycat", "ghost", "jester"}
                    for field in ("primary", "secondary"))):
            continue
        detail = details.get(filename)
        digest = detail.digest if detail is not None else None
        if digest is None:
            # Archive hashes are not UFTB hashes.  Fail closed unless the
            # canonical storage certificate names the uncompressed result.
            for label in ("result", "output", "table"):
                match = re.search(
                    rf"(?:^|[; ]){label} sha256:([0-9a-f]{{64}})(?:[; ]|$)",
                    row.storage)
                if match:
                    digest = match.group(1)
                    break
        if digest is None and len(configured_digests.get(filename, ())) == 1:
            digest = next(iter(configured_digests[filename]))
        if digest is not None:
            expected[filename] = digest
    for filename, digest in legacy_expected_digests(rows).items():
        row = rows[filename]
        if (row.status != "certified" or row.result_kind != "concrete" or
                "[" in row.first + row.second or filename not in catalog or
                any(str(catalog[filename].get(field) or "") in
                    {"angel", "copycat", "ghost", "jester"}
                    for field in ("primary", "secondary"))):
            continue
        # The preservation manifest authenticates the actual packed bytes.
        # Some old Giant detail rows retained a stale pre-pack digest.
        expected[filename] = digest

    def one(instance: str) -> tuple[str, list[str]]:
        listing = send_and_wait(instance, [
            "find /mnt -type f -name '*.uftb' -size +1k -print 2>/dev/null"])
        candidates = [path for path in listing.splitlines()
                      if Path(path).name in expected]
        if not candidates:
            return instance, []
        # SSM truncates large stdout streams. Hash bounded chunks so a host
        # with many replicated dependency copies cannot hide later exact
        # restored paths. Prefer preservation extracts and generation outputs.
        candidates.sort(key=lambda path: (
            "/trivial-backfill-legacy-e296/extracted/" not in path,
            "/outputs/" not in path, len(path), path))
        output = ""
        for begin in range(0, len(candidates), 30):
            output += send_and_wait(instance, [
                "sha256sum " + " ".join(
                    map(shlex.quote, candidates[begin:begin + 30]))])
        paths: dict[str, str] = {}
        for line in output.splitlines():
            match = re.match(r"^([0-9a-f]{64})  (.+)$", line)
            if not match:
                continue
            digest, path = match.groups()
            filename = Path(path).name
            if digest == expected.get(filename):
                # Prefer generation outputs, then short canonical paths.
                score = ("/outputs/" not in path, len(path), path)
                if filename not in paths or score < (
                        "/outputs/" not in paths[filename],
                        len(paths[filename]), paths[filename]):
                    paths[filename] = path
        return instance, [paths[name] for name in sorted(paths)]

    instances = sorted({str(spec["instance"]) for spec in BATCH})
    with ThreadPoolExecutor(max_workers=len(instances)) as pool:
        return dict(pool.map(one, instances))


def host_utilization() -> dict[str, float]:
    program = (
        "import time;"
        "r=lambda:list(map(int,open('/proc/stat').readline().split()[1:]));"
        "a=r();time.sleep(2);b=r();"
        "t=sum(b)-sum(a);i=(b[3]+b[4])-(a[3]+a[4]);"
        "print(round(100*(t-i)/t,1))"
    )

    def one(instance: str) -> tuple[str, float]:
        output = send_and_wait(instance, ["python3 -c " + shlex.quote(program)])
        return instance, float(output.strip())

    instances = sorted({str(spec["instance"]) for spec in BATCH})
    with ThreadPoolExecutor(max_workers=len(instances)) as pool:
        return dict(pool.map(one, instances))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage-binary", action="store_true")
    parser.add_argument("--stage-table", action="append", default=[])
    parser.add_argument("--binary-version")
    parser.add_argument("--status", action="store_true")
    parser.add_argument("--inventory", action="store_true")
    parser.add_argument("--utilization", action="store_true")
    parser.add_argument("--restore-legacy", action="store_true")
    parser.add_argument("--legacy-status", action="store_true")
    parser.add_argument("--launch-restored", metavar="BINARY_VERSION")
    parser.add_argument("--restored-status", action="store_true")
    parser.add_argument("--restored-receipts", action="store_true")
    parser.add_argument("--s3-status", action="store_true")
    parser.add_argument("--special-status", action="store_true")
    parser.add_argument("--launch-s3-ordinary", metavar="BINARY_VERSION")
    parser.add_argument("--launch-s3-special", action="store_true")
    parser.add_argument("--launch-legacy-gaps", metavar="BINARY_VERSION")
    parser.add_argument("--gap-status", action="store_true")
    parser.add_argument("--versions-json", type=Path)
    args = parser.parse_args()
    selected = sum((args.stage_binary, bool(args.stage_table),
                    bool(args.binary_version), args.status, args.inventory))
    selected += int(args.utilization)
    selected += int(args.restore_legacy) + int(args.legacy_status)
    selected += int(bool(args.launch_restored)) + int(args.restored_status)
    selected += int(args.restored_receipts)
    selected += int(args.s3_status)
    selected += int(args.special_status)
    selected += int(bool(args.launch_s3_ordinary))
    selected += int(args.launch_s3_special)
    selected += int(bool(args.launch_legacy_gaps)) + int(args.gap_status)
    if selected != 1:
        parser.error("choose exactly one of --stage-binary, --binary-version, or --status")
    if args.stage_binary:
        result: object = stage_binary()
    elif args.stage_table:
        result = stage_tables(args.stage_table)
    elif args.inventory:
        result = retained_inventory()
    elif args.utilization:
        result = host_utilization()
    elif args.restore_legacy:
        result = restore_legacy()
    elif args.legacy_status:
        result = legacy_status()
    elif args.launch_restored:
        result = launch_restored(args.launch_restored)
    elif args.restored_status:
        result = restored_status()
    elif args.restored_receipts:
        result = restored_receipts()
    elif args.s3_status:
        result = s3_status()
    elif args.special_status:
        result = s3_status(special=True)
    elif args.launch_s3_ordinary:
        if args.versions_json is None:
            parser.error("--launch-s3-ordinary requires --versions-json")
        result = launch_s3(
            args.launch_s3_ordinary, args.versions_json.resolve())
    elif args.launch_s3_special:
        if args.versions_json is None:
            parser.error("--launch-s3-special requires --versions-json")
        result = launch_s3(
            SPECIAL_BINARY_VERSION, args.versions_json.resolve(), special=True)
    elif args.launch_legacy_gaps:
        if args.versions_json is None:
            parser.error("--launch-legacy-gaps requires --versions-json")
        result = launch_legacy_gaps(
            args.launch_legacy_gaps, args.versions_json.resolve())
    elif args.gap_status:
        result = gap_status()
    elif args.status:
        result = status_batch()
    else:
        result = launch_batch(str(args.binary_version))
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
