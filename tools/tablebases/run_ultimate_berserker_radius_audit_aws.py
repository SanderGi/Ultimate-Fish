#!/usr/bin/env python3
"""Download exact certified artifacts and audit Berserker radii 1, 2, and 3."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed


BERSERKER = 7
LINE = re.compile(
    r"reachability_(primary|secondary)_substate(_total|_trivial)? substate (\d+) "
    r"side (\d) unknown (\d+) win (\d+) loss (\d+) draw (\d+)")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def aws_json(*arguments: str) -> dict[str, object]:
    output = subprocess.check_output(["aws", *arguments], text=True)
    return json.loads(output)


def archive_candidates(objects: list[dict[str, object]], filename: str) -> list[str]:
    stem = Path(filename).stem
    return [str(item["Key"]) for item in objects
            if stem in str(item["Key"]) and
            str(item["Key"]).endswith((".tar.zst", ".tar.gz"))]


def extract(archive: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    if archive.name.endswith(".tar.zst"):
        subprocess.run(["tar", "--zstd", "-xf", str(archive), "-C", str(destination)],
                       check=True)
    else:
        subprocess.run(["tar", "-xzf", str(archive), "-C", str(destination)],
                       check=True)


def archive_manifest_entry(extracted: Path, relative: str) -> tuple[str, int] | None:
    """Return the content hash/size authenticated by ARCHIVE.MANIFEST."""
    manifest = extracted / "ARCHIVE.MANIFEST"
    if not manifest.exists():
        return None
    for line in manifest.read_text(encoding="utf-8").splitlines():
        parts = line.split(" ", 2)
        if len(parts) == 3 and parts[2] == relative:
            return parts[0], int(parts[1])
    return None


def fetch_record(record: dict[str, object], objects: list[dict[str, object]],
                 root: Path, bucket: str, region: str,
                 single_table: Path) -> dict[str, object]:
    filename = str(record["filename"])
    if record.get("excluded"):
        return {"excluded": True, "reason": record["exclusion_reason"]}
    table = root / "tables" / filename
    if record.get("hf_revision"):
        revision = str(record["hf_revision"])
        if not re.fullmatch(r"[0-9a-f]{40}", revision):
            raise ValueError("HF audit source must use an immutable revision")
        dataset = str(record["hf_dataset"])
        if dataset != "SanderGi/Ultimate-Fish-Tablebases" or Path(filename).name != filename:
            raise ValueError("unexpected HF audit source")
        if not table.exists():
            table.parent.mkdir(parents=True, exist_ok=True)
            temporary = table.with_suffix(table.suffix + ".part")
            url = f"https://huggingface.co/datasets/{dataset}/resolve/{revision}/tablebases/{filename}"
            with urllib.request.urlopen(url, timeout=120) as response, temporary.open("wb") as output:
                shutil.copyfileobj(response, output)
            if sha256(temporary) != record["expected_payload_sha256"]:
                raise RuntimeError("HF Berserker source hash mismatch")
            temporary.replace(table)
        if sha256(table) != record["expected_payload_sha256"]:
            raise RuntimeError("HF Berserker source hash mismatch")
        return {"table": str(table), "source_sha256": record["expected_payload_sha256"],
                "source": "huggingface", "dataset": dataset, "dataset_revision": revision}
    if filename == "kberserkerk.uftb":
        payload_sha = sha256(single_table)
        if payload_sha != record.get("expected_payload_sha256"):
            raise RuntimeError("single-Berserker source hash mismatch")
        if not table.exists():
            shutil.copyfile(single_table, table)
        if sha256(table) != payload_sha:
            raise RuntimeError("copied single-Berserker source hash mismatch")
        return {
            "table": str(table),
            "source_sha256": payload_sha,
            "source": "existing-version-pinned-dependency",
            "source_archive_sha256": record["expected_sha256"],
            "source_archive_version_id": record["expected_archive_version_id"],
        }

    exact_key = record.get("expected_archive_key")
    candidates = ([str(exact_key)] if exact_key
                  else archive_candidates(objects, filename))
    if not candidates:
        raise RuntimeError(f"no S3 archive candidate for {filename}")
    expected_table = record.get("expected_sha256")
    expected_archive = record.get("expected_archive_sha256")
    for number, key in enumerate(candidates):
        try:
            head = aws_json("s3api", "head-object", "--region", region,
                            "--bucket", bucket, "--key", key, "--version-id",
                            str(record["expected_archive_version_id"]))
        except subprocess.CalledProcessError:
            # VersionIds are scoped to object keys.  A filename can occur in
            # several historical keys, so a version missing from one key is
            # only a rejected candidate, not a failed audit.
            continue
        if head.get("VersionId") != record["expected_archive_version_id"]:
            continue
        metadata_sha = head.get("Metadata", {}).get("sha256")
        expected_object = expected_archive or expected_table
        suffix = ".tar.zst" if key.endswith(".tar.zst") else ".tar.gz"
        archive = root / "archives" / f"{Path(filename).stem}-{number}{suffix}"
        if not archive.exists():
            subprocess.run([
                "aws", "s3api", "get-object", "--region", region,
                "--bucket", bucket, "--key", key, "--version-id",
                str(record["expected_archive_version_id"]), str(archive)],
                check=True, stdout=subprocess.DEVNULL)
        if expected_archive and sha256(archive) != expected_archive:
            continue
        extracted = root / "extracted" / f"{Path(filename).stem}-{number}"
        if not extracted.exists():
            extract(archive, extracted)
        tables = list(extracted.rglob(filename))
        overlays = list(extracted.rglob(f"{Path(filename).stem}.ufiw"))
        if expected_table:
            # Concrete ledger rows bind the S3 object identity (and commonly
            # its metadata), while the uncompressed UFTB content has its own
            # SHA in the certified archive manifest/proof.  Validate both
            # layers instead of incorrectly comparing those distinct hashes.
            matches = []
            for candidate in tables:
                relative = str(candidate.relative_to(extracted))
                entry = archive_manifest_entry(extracted, relative)
                actual_sha = sha256(candidate)
                if entry and entry != (actual_sha, candidate.stat().st_size):
                    raise RuntimeError(f"archive manifest mismatch for {filename}")
                proof = next(iter(extracted.rglob(f"{Path(filename).stem}.json")), None)
                if proof:
                    proof_data = json.loads(proof.read_text(encoding="utf-8"))
                    proof_sha = proof_data.get("output", {}).get("sha256")
                    if proof_sha and proof_sha != actual_sha:
                        raise RuntimeError(f"proof output hash mismatch for {filename}")
                matches.append(candidate)
            if not matches:
                continue
            if not table.exists():
                os.link(matches[0], table)
        elif not overlays:
            continue
        # Historical ledger prose used "S3 table sha256" for two different
        # identities: some rows name the uncompressed UFTB payload, while
        # others name the S3 object's logical/content metadata.  Require the
        # recorded digest to match one authenticated layer, and retain all
        # layers in the result so this convention cannot be ambiguous again.
        identities = {sha256(archive), metadata_sha}
        identities.update(sha256(candidate) for candidate in tables)
        identities.update(sha256(candidate) for candidate in overlays)
        if expected_object and expected_object not in identities:
            continue
        result: dict[str, object] = {
            "s3_key": key,
            "s3_version_id": head.get("VersionId"),
            "archive_sha256": sha256(archive),
            "s3_metadata_sha256": metadata_sha,
        }
        if expected_table:
            result.update(table=str(table), source_sha256=sha256(table))
        else:
            overlay = root / "tables" / f"{Path(filename).stem}.ufiw"
            if not overlay.exists():
                os.link(overlays[0], overlay)
            result.update(overlay=str(overlay), overlay_sha256=sha256(overlay))
        return result
    raise RuntimeError(f"no exact certified artifact matched {filename}")


def concrete_command(
    binary: Path,
    record: dict[str, object],
    table: Path,
    workers: int,
) -> list[str]:
    command = [str(binary), "--piece", str(record["primary"]),
               "--checkpoint-every", "0", "--workers", str(workers)]
    if record["secondary"]:
        command += ["--piece2", str(record["secondary"])]
    if record["opposing"]:
        command.append("--opposing")
    audit_option = (
        "--audit-turn-boundary-reachability"
        if requires_turn_boundary(record)
        else "--audit-reachability"
    )
    return command + [audit_option, str(table)]


def requires_turn_boundary(record: dict[str, object]) -> bool:
    return "prince" in {
        str(record["primary"]), str(record.get("secondary") or "")
    }


def reusable_audit(text: str, record: dict[str, object]) -> bool:
    if "reachability_" not in text:
        return False
    boundary_scopes = text.count("reachability_scope turn_boundary\n")
    return boundary_scopes == 1 if requires_turn_boundary(record) else True


def parse_concrete(
    log: Path, slot: str,
) -> dict[int, dict[str, tuple[list[int], list[int]]]]:
    rows: dict[tuple[int, int], dict[str, list[int]]] = {}
    for match in LINE.finditer(log.read_text(encoding="utf-8")):
        if match.group(1) != slot:
            continue
        substate, side = int(match.group(3)), int(match.group(4))
        counts = [int(match.group(index)) for index in range(5, 9)]
        kind = {None: "unreachable", "_total": "total",
                "_trivial": "trivial"}[match.group(2)]
        rows.setdefault((substate, side), {})[kind] = counts
    output: dict[int, dict[str, tuple[list[int], list[int]]]] = {}
    for substate in sorted({substate for substate, _side in rows}):
        admitted_sides = []
        trivial_sides = []
        display_sides = []
        for side in range(2):
            row = rows[(substate, side)]
            if set(row) != {"total", "unreachable", "trivial"}:
                raise RuntimeError(
                    f"incomplete radius audit for substate {substate} side {side}")
            admitted = [total - unreachable for total, unreachable in
                        zip(row["total"], row["unreachable"])]
            trivial = row["trivial"]
            if any(value > admitted[index]
                   for index, value in enumerate(trivial)):
                raise RuntimeError(
                    f"trivial radius count exceeds admitted count for "
                    f"substate {substate} side {side}")
            display = [value - trivial[index]
                       for index, value in enumerate(admitted)]
            admitted_sides.append(admitted)
            trivial_sides.append(trivial)
            display_sides.append(display)
        output[substate] = {
            "admitted": (admitted_sides[0], admitted_sides[1]),
            "trivial": (trivial_sides[0], trivial_sides[1]),
            "display": (display_sides[0], display_sides[1]),
        }
    return output


def parse_information(
    path: Path, slot: str,
) -> dict[int, dict[str, tuple[list[int], list[int]]]]:
    data = path.read_bytes()
    magic = data[:8]
    if magic not in (b"UFIW1\0\0\0", b"UFIW2\0\0\0"):
        raise RuntimeError(f"invalid information overlay: {path}")
    version, primary, secondary, _color, count, substates = struct.unpack_from(
        "<IIIIII", data, 8)
    offset = 32 if magic == b"UFIW1\0\0\0" else 160
    flags = data[offset:]
    if version not in (1, 2) or len(flags) != count:
        raise RuntimeError(f"invalid information overlay dimensions: {path}")
    primary_factor = 10 if primary == BERSERKER else 1
    secondary_factor = 10 if secondary == BERSERKER else 1
    if primary_factor * secondary_factor != substates:
        raise RuntimeError(f"unexpected information substate layout: {path}")
    output = {substate: ([0, 0, 0, 0], [0, 0, 0, 0])
              for substate in range(10)}
    half = count // 2
    for side in range(2):
        result_table = bytearray(256)
        for flag in range(1, 256):
            white_forces = bool(flag & 1)
            black_forces = bool(flag & 2)
            mover_forces = white_forces if side == 0 else black_forces
            opponent_forces = black_forces if side == 0 else white_forces
            result_table[flag] = 1 if mover_forces else 2 if opponent_forces else 3
        translated = flags[side * half:(side + 1) * half].translate(result_table)
        for combined in range(substates):
            substate = (combined // secondary_factor if slot == "primary"
                        else combined % secondary_factor)
            if substate not in output:
                continue
            lane = translated[combined::substates]
            for result in range(1, 4):
                output[substate][side][result] += lane.count(result)
    wrapped = {}
    for substate, admitted in output.items():
        zero = ([0, 0, 0, 0], [0, 0, 0, 0])
        wrapped[substate] = {
            "admitted": admitted,
            "trivial": zero,
            "display": admitted,
        }
    return wrapped


def apply_information_trivial_receipt(
    rows: dict[int, dict[str, tuple[list[int], list[int]]]],
    receipt_path: Path,
    sidecar_path: Path,
    filename: str,
    binding: dict[str, object],
) -> tuple[dict[int, dict[str, tuple[list[int], list[int]]]], dict[str, object]]:
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    if (receipt.get("schema") != "ultimate-information-trivial-receipt-v2" or
            receipt.get("filename") != filename or
            receipt.get("source_sha256") != binding.get("source_sha256") or
            receipt.get("overlay_sha256") != binding.get("overlay_sha256") or
            receipt.get("substates") != 10):
        raise RuntimeError(f"information trivial receipt binding residual: {filename}")
    if (not sidecar_path.is_file() or
            sha256(sidecar_path) != receipt.get("sidecar_sha256")):
        raise RuntimeError(f"information trivial sidecar SHA residual: {filename}")
    sidecar = sidecar_path.read_text(encoding="utf-8")
    required = (
        f"information_reachability_binding source_sha256 "
        f"{receipt['source_sha256']} model_sha256 {receipt['model_sha256']}",
        f"information_trivial_overlay_sha256 {receipt['overlay_sha256']}",
        f"information_trivial_binary_sha256 {receipt['binary_sha256']}",
        f"information_trivial_source_bundle_sha256 "
        f"{receipt['source_bundle_sha256']}",
    )
    if any(value not in sidecar for value in required):
        raise RuntimeError(f"information trivial sidecar binding residual: {filename}")
    detail = receipt.get("substate_counts")
    if not isinstance(detail, dict) or set(detail) != {
            str(value) for value in range(10)}:
        raise RuntimeError(f"information trivial substate coverage residual: {filename}")
    updated = {}
    for substate in range(10):
        admitted_sides = []
        trivial_sides = []
        display_sides = []
        for side in range(2):
            buckets = detail[str(substate)][str(side)]
            admitted = [buckets["admitted"][name]
                        for name in ("unknown", "win", "loss", "draw")]
            trivial = [buckets["trivial"][name]
                       for name in ("unknown", "win", "loss", "draw")]
            if admitted != rows[substate]["admitted"][side]:
                raise RuntimeError(
                    f"information radius admitted residual: {filename} "
                    f"substate {substate} side {side}")
            if any(value > admitted[index]
                   for index, value in enumerate(trivial)):
                raise RuntimeError(
                    f"information radius trivial subset residual: {filename} "
                    f"substate {substate} side {side}")
            admitted_sides.append(admitted)
            trivial_sides.append(trivial)
            display_sides.append([
                value - trivial[index] for index, value in enumerate(admitted)
            ])
        updated[substate] = {
            "admitted": (admitted_sides[0], admitted_sides[1]),
            "trivial": (trivial_sides[0], trivial_sides[1]),
            "display": (display_sides[0], display_sides[1]),
        }
    evidence = {
        "information_trivial_sha256": receipt["sidecar_sha256"],
        "information_trivial_s3_key": receipt["s3_key"],
        "information_trivial_s3_version_id": receipt["s3_version_id"],
        "information_trivial_binary_sha256": receipt["binary_sha256"],
        "information_trivial_source_bundle_sha256":
            receipt["source_bundle_sha256"],
    }
    return updated, evidence


def wdl(counts: list[int]) -> dict[str, int]:
    return {"wins": counts[1], "losses": counts[2], "draws": counts[3]}


def validate_aggregate(
    filename: str,
    rows: dict[int, dict[str, tuple[list[int], list[int]]]],
    expected: dict[str, dict[str, int]],
) -> None:
    for side, key in enumerate(("first_starts", "second_starts")):
        actual = [sum(row["display"][side][result] for row in rows.values())
                  for result in range(4)]
        wanted = expected[key]
        if wdl(actual) != wanted:
            raise RuntimeError(
                f"radius slices do not reproduce aggregate for {filename} {key}: "
                f"{wdl(actual)} != {wanted}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--single-table", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=6)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--information-trivial-dir", type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    root = args.root
    for directory in ("archives", "extracted", "tables", "audits"):
        (root / directory).mkdir(parents=True, exist_ok=True)
    listed = aws_json("s3api", "list-objects-v2", "--region", manifest["region"],
                      "--bucket", manifest["bucket"])
    objects = listed.get("Contents", [])
    records = manifest["files"]
    bindings: dict[str, dict[str, object]] = {}
    for filename, record in records.items():
        bindings[filename] = fetch_record(
            record, objects, root, manifest["bucket"], manifest["region"],
            args.single_table)
        print(f"staged {filename}", flush=True)

    futures = {}
    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        for filename, record in records.items():
            if record.get("excluded") or record["result_kind"].startswith("information"):
                continue
            log = root / "audits" / f"{Path(filename).stem}.radius.txt"
            if log.exists() and reusable_audit(
                log.read_text(encoding="utf-8"), record
            ):
                continue
            command = concrete_command(
                args.binary,
                record,
                Path(bindings[filename]["table"]),
                args.workers,
            )
            futures[executor.submit(subprocess.run, command, check=True,
                                    capture_output=True, text=True)] = (filename, log)
        for future in as_completed(futures):
            filename, log = futures[future]
            completed = future.result()
            log.write_text(completed.stdout, encoding="utf-8")
            print(f"audited {filename}", flush=True)

    output: dict[str, object] = {
        "schema": 2,
        "description": (
            "Exact reachability-admitted Berserker radius slices with "
            "authenticated trivial positions removed"),
        "semantics": "reachability-admitted-minus-trivial-v3",
        "radius_to_power_substate": {"1": 0, "2": 1, "3": 2},
        "audit_binary_sha256": sha256(args.binary),
        "files": {},
    }
    for filename, record in records.items():
        binding = bindings[filename]
        if record.get("excluded"):
            output["files"][filename] = binding
            continue
        if record["result_kind"].startswith("information"):
            rows = parse_information(Path(binding["overlay"]), record["berserker_slot"])
            if args.information_trivial_dir is None:
                raise RuntimeError(
                    f"authenticated per-substate trivial receipt required: {filename}")
            stem = Path(filename).stem
            receipt = args.information_trivial_dir / f"{stem}.receipt.json"
            sidecar = (args.information_trivial_dir /
                       f"{stem}.information-trivial-v2.txt")
            rows, evidence = apply_information_trivial_receipt(
                rows, receipt, sidecar, filename, binding)
            binding.update(evidence)
        else:
            log = root / "audits" / f"{Path(filename).stem}.radius.txt"
            native = log.read_text(encoding="utf-8")
            if not reusable_audit(native, record):
                raise RuntimeError(
                    f"invalid reporting scope in radius audit for {filename}"
                )
            rows = parse_concrete(log, record["berserker_slot"])
            binding["audit_sha256"] = sha256(log)
            if requires_turn_boundary(record):
                binding["reporting_scope"] = (
                    "turn-boundary-continuation-none"
                )
        validate_aggregate(filename, rows, record["aggregate"])
        radius_rows = {}
        for radius, substate in manifest["radii"].items():
            row = rows[substate]
            def side_record(side: int) -> dict[str, object]:
                return {
                    "admitted": wdl(row["admitted"][side]),
                    "trivial": wdl(row["trivial"][side]),
                    "display": wdl(row["display"][side]),
                }
            radius_rows[radius] = {
                "power_substate": substate,
                "first_starts": side_record(0),
                "second_starts": side_record(1),
            }
        output["files"][filename] = {
            **binding,
            "trivial_semantics": "authenticated-per-substate-v3",
            "radii": radius_rows,
        }
    result = root / "berserker-radius-summary.json"
    result.write_text(json.dumps(output, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print(result)


if __name__ == "__main__":
    main()
