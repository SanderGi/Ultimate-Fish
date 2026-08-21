#!/usr/bin/env python3
"""Run a version-pinned batch of trivial audits against restored UFTBs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


SCHEMA = "ultimate-trivial-backfill-batch-v1"
SPLIT_MAGIC = b"UFTBS1\0\0"
SPLIT_HEADER = struct.Struct("<8sIIQ")
SPLIT_ENTRY = struct.Struct("<HQ32s")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def assemble_split(stub: Path, target: Path, expected_stub: str) -> Path:
    data = stub.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected_stub or \
            len(data) < SPLIT_HEADER.size:
        raise RuntimeError(f"{stub.name}: split stub authentication residual")
    magic, version, count, total = SPLIT_HEADER.unpack_from(data)
    if magic != SPLIT_MAGIC or version != 1 or not count:
        raise RuntimeError(f"{stub.name}: invalid split header")
    offset = SPLIT_HEADER.size
    entries: list[tuple[str, int, bytes]] = []
    for _ in range(count):
        if offset + SPLIT_ENTRY.size > len(data):
            raise RuntimeError(f"{stub.name}: truncated split entry")
        name_length, size, digest = SPLIT_ENTRY.unpack_from(data, offset)
        offset += SPLIT_ENTRY.size
        name = data[offset:offset + name_length].decode("utf-8")
        offset += name_length
        if Path(name).name != name:
            raise RuntimeError(f"{stub.name}: unsafe split part name")
        entries.append((name, size, digest))
    if offset != len(data) or sum(size for _name, size, _digest in entries) != total:
        raise RuntimeError(f"{stub.name}: split extent residual")
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_suffix(target.suffix + ".tmp")
    with temporary.open("wb") as output:
        for name, size, digest in entries:
            part = stub.parent / name
            if part.stat().st_size != size or \
                    bytes.fromhex(sha256(part)) != digest:
                raise RuntimeError(f"{stub.name}: split part residual")
            with part.open("rb") as stream:
                for block in iter(lambda: stream.read(4 << 20), b""):
                    output.write(block)
    if temporary.stat().st_size != total:
        raise RuntimeError(f"{stub.name}: assembled extent residual")
    temporary.replace(target)
    return target


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    plan = json.loads(args.plan.read_text())
    if plan.get("schema") != SCHEMA:
        raise RuntimeError("trivial batch plan schema mismatch")
    args.work.mkdir(parents=True, exist_ok=True)

    for item in plan["items"]:
        filename = str(item["filename"])
        stem = Path(filename).stem
        source = Path(item.get("path", ""))
        expected = str(item["sha256"])
        if item.get("object"):
            object_item = item["object"]
            restore = args.work / "restored" / stem
            restore.mkdir(parents=True, exist_ok=True)
            source = restore / filename
            if not source.exists():
                subprocess.check_call([
                    "aws", "s3api", "get-object", "--region", plan["region"],
                    "--bucket", plan["bucket"], "--key", object_item["key"],
                    "--version-id", object_item["version_id"], str(source),
                ], stdout=subprocess.DEVNULL)
        if item.get("archive"):
            archive_item = item["archive"]
            restore = args.work / "restored" / stem
            restore.mkdir(parents=True, exist_ok=True)
            archive = restore / "result.tar.zst"
            if not archive.exists():
                subprocess.check_call([
                    "aws", "s3api", "get-object", "--region", plan["region"],
                    "--bucket", plan["bucket"], "--key", archive_item["key"],
                    "--version-id", archive_item["version_id"], str(archive),
                ], stdout=subprocess.DEVNULL)
            if sha256(archive) != archive_item["sha256"]:
                raise RuntimeError(f"{filename}: archive digest residual")
            encoded = str(item.get("encoded_filename", filename))
            source = restore / "tablebases" / encoded
            if not source.exists():
                subprocess.check_call([
                    "tar", "--zstd", "-xf", str(archive), "-C", str(restore),
                    "ARCHIVE.MANIFEST", f"tablebases/{encoded}",
                ])
        if item.get("split_stub_sha256"):
            source = assemble_split(
                source, args.work / "assembled" / filename,
                str(item["split_stub_sha256"]))
        if source.name != filename or sha256(source) != expected:
            # Encoded orientation aliases may have a different on-disk name;
            # the exact output digest and requested material arguments remain
            # authoritative.
            if not item.get("archive") or sha256(source) != expected:
                raise RuntimeError(f"{filename}: restored UFTB digest residual")
        receipt = args.work / f"{stem}.receipt.json"
        if receipt.exists():
            saved = json.loads(receipt.read_text())
            if (saved.get("filename") != filename or
                    saved.get("output_sha256") != expected):
                raise RuntimeError(f"{filename}: stale receipt residual")
            print("TRIVIAL_BACKFILL_RESUME " + json.dumps(saved, sort_keys=True),
                  flush=True)
            continue

        sidecar = args.work / f"{stem}.reachability-trivial-v3.txt"
        command = [str(args.binary), "--piece", str(item["primary"]),
                   "--workers", str(plan["workers"]),
                   "--checkpoint-every", "0"]
        if item.get("secondary"):
            command.extend(("--piece2", str(item["secondary"])))
        if item.get("opposing"):
            command.append("--opposing")
        audit_option = ("--audit-turn-boundary-reachability"
                        if "prince" in (str(item["primary"]),
                                        str(item.get("secondary") or ""))
                        else "--audit-reachability")
        command.extend((audit_option, str(source)))
        completed = subprocess.run(command, check=True, text=True,
                                   capture_output=True)
        text = (
            f"reachability_binding filename {filename} output_sha256 {expected}\n"
            f"trivial_audit_binding source_overlay_sha256 {plan['overlay_sha256']} "
            f"model_sha256 {plan['model_sha256']} "
            f"inventory_sha256 {plan['inventory_sha256']} "
            f"binary_sha256 {plan['binary_sha256']}\n" + completed.stdout
        )
        if text.count("\nreachability_trivial side ") != 2:
            raise RuntimeError(f"{filename}: incomplete trivial sidecar")
        temporary = sidecar.with_suffix(sidecar.suffix + ".tmp")
        temporary.write_text(text)
        temporary.replace(sidecar)
        side_sha = sha256(sidecar)
        key = (f"results/trivial-reachability-v3/{filename}/sha256/"
               f"{side_sha}/{stem}.reachability-trivial-v3.txt")
        put = subprocess.check_output([
            "aws", "s3api", "put-object", "--region", plan["region"],
            "--bucket", plan["bucket"], "--key", key, "--body", str(sidecar),
            "--metadata", (f"sha256={side_sha},output-sha256={expected},"
                           f"model-sha256={plan['model_sha256']}"),
            "--output", "json"], text=True)
        version = str(json.loads(put)["VersionId"])
        verify = args.work / f"{stem}.s3-verify.txt"
        subprocess.check_call([
            "aws", "s3api", "get-object", "--region", plan["region"],
            "--bucket", plan["bucket"], "--key", key,
            "--version-id", version, str(verify)], stdout=subprocess.DEVNULL)
        if sha256(verify) != side_sha:
            raise RuntimeError(f"{filename}: S3 sidecar digest residual")
        saved = {
            "filename": filename, "output_sha256": expected,
            "sidecar_sha256": side_sha, "sidecar_version_id": version,
            "sidecar_key": key,
        }
        receipt.write_text(json.dumps(saved, sort_keys=True) + "\n")
        print("TRIVIAL_BACKFILL_OK " + json.dumps(saved, sort_keys=True),
              flush=True)


if __name__ == "__main__":
    main()
