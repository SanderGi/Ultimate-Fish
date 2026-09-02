#!/usr/bin/env python3
"""Authenticate and import one completed preserved information archive."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile
from types import SimpleNamespace

import certify_ultimate_primary_jester_archive as certify
import finalize_ultimate_aws_concrete_class as reachability
import finalize_ultimate_existing_information_overlay_aws as finalizer
import import_ultimate_information_trivial_reachability_aws as trivial_import
import update_ultimate_tablebase_ledger as ledger


ROOT = Path(__file__).resolve().parents[2]
def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def aws(*arguments: str) -> dict[str, object]:
    return json.loads(subprocess.run(
        ["aws", *arguments], check=True, text=True,
        capture_output=True).stdout)


def validate_and_extract(args: argparse.Namespace,
                         directory: Path
                         ) -> tuple[bytes, str, str, dict[str, object], int]:
    if (sha256_path(args.archive) != args.archive_sha256 or
            sha256_path(args.preservation_certificate) !=
            args.preservation_certificate_sha256):
        raise ValueError("preserved information object SHA residual")
    old_certificate = json.loads(args.preservation_certificate.read_text())
    s3 = old_certificate.get("s3", {})
    certificate_filename = old_certificate.get("filename")
    archive_name = old_certificate.get("archive")
    if ((certificate_filename is not None and
         certificate_filename != args.filename) or
            (certificate_filename is None and
             (not isinstance(archive_name, str) or
              PurePosixPath(archive_name).name !=
              PurePosixPath(args.archive_key).name)) or
            old_certificate.get("sha256") != args.archive_sha256 or
            s3.get("sha256") != args.archive_sha256 or
            s3.get("key") != args.archive_key or
            s3.get("version_id") != args.archive_version_id or
            s3.get("archive_restore_residual") != 0 or
            s3.get("download_residual") != 0 or
            s3.get("head_residual") != 0):
        raise ValueError("preservation certificate S3 residual")
    listing = subprocess.run(
        ["tar", "--zstd", "-tf", str(args.archive)], check=True,
        text=True, capture_output=True).stdout.splitlines()
    if len(listing) != len(set(listing)) or "archive-manifest.json" not in listing:
        raise ValueError("preserved information member residual")
    for raw in listing:
        path = PurePosixPath(raw)
        if path.is_absolute() or ".." in path.parts:
            raise ValueError("unsafe preserved information member")
    subprocess.run(["tar", "--zstd", "-xf", str(args.archive),
                    "-C", str(directory)], check=True)
    manifest_bytes = (directory / "archive-manifest.json").read_bytes()
    manifest = json.loads(manifest_bytes)
    artifacts = manifest.get("artifacts")
    if not isinstance(artifacts, list):
        raise ValueError("preserved information manifest residual")
    expected = {"archive-manifest.json"}
    paths: set[str] = set()
    for artifact in artifacts:
        relative = str(artifact["path"])
        if relative in paths:
            raise ValueError("duplicate preserved artifact")
        paths.add(relative)
        expected.add("payload/" + relative)
        path = directory / "payload" / relative
        if (not path.is_file() or path.stat().st_size != int(artifact["bytes"]) or
                sha256_path(path) != artifact["sha256"]):
            raise ValueError("preserved artifact binding residual")
    if set(listing) != expected:
        raise ValueError("preserved archive coverage residual")

    overlay = (directory / "payload" / args.overlay_member).read_bytes()
    proof = (directory / "payload" / args.proof_member).read_text()
    arbitrary_path = directory / "payload" / args.arbitrary_member
    # Arbitrary-belief sidecars are routinely several GiB.  They have already
    # been authenticated against the archive manifest above and are validated
    # structurally from their extracted path below, so retaining a second full
    # copy in Python memory merely to calculate the import-certificate binding
    # is both wasteful and an avoidable OOM risk.
    arbitrary_sha256 = sha256_path(arbitrary_path)
    if overlay[:8] != b"UFIW2\0\0\0":
        raise ValueError("preserved information overlay header residual")
    state_count = int.from_bytes(overlay[24:28], "little")
    source = overlay[32:96].decode()
    model = overlay[96:160].decode()
    record = finalizer.generate._records()[args.filename]
    finalizer.validate_arbitrary(
        arbitrary_path, source, model, bool(record["opposing"]))
    parsed_args = SimpleNamespace(
        filename=args.filename, expected_source_sha256=source,
        expected_model_sha256=model)
    entry, _ = finalizer.entry_from_proof(parsed_args, proof, overlay)
    return manifest_bytes, proof, arbitrary_sha256, entry, state_count


def cell_cardinality(cell: str) -> int:
    components = cell.split("/")
    if len(components) != 3:
        raise ValueError("preserved information W/L/D cell residual")
    total = 0
    for component in components:
        match = re.fullmatch(
            r"\s*([0-9][0-9,]*)(?: \[[0-9][0-9,]*\])?"
            r"(?: \(([0-9][0-9,]*)\))?\s*", component)
        if match is None:
            raise ValueError("preserved information W/L/D cell residual")
        total += int(match.group(1).replace(",", ""))
        total += int((match.group(2) or "0").replace(",", ""))
    return total


def validate_turn_boundary_receipt(
        args: argparse.Namespace, overlay: bytes, state_count: int,
        directory: Path) -> dict[str, object]:
    """Authenticate the exact Prince boundary audit and render its W/L/D.

    The information solver proves its fixed point over the dense codec, which
    includes Prince's compulsory second-move substates.  Those substates are
    useful internally but are not legal tablebase query roots.  Publication
    therefore needs the independently generated information-trivial-v2 audit
    to restrict the already-proved overlay to ordinary turn boundaries.
    """
    receipt_path = args.turn_boundary_receipt
    if receipt_path is None:
        raise ValueError("preserved Prince turn-boundary receipt required")
    receipt = trivial_import.validate_receipt(
        json.loads(receipt_path.read_text()))
    source = overlay[32:96].decode()
    model = overlay[96:160].decode()
    overlay_sha256 = sha256_bytes(overlay)
    substates = int.from_bytes(overlay[28:32], "little")
    if (receipt["schema"] != "ultimate-information-trivial-receipt-v2" or
            receipt["filename"] != args.filename or
            receipt["states"] != state_count or
            receipt["substates"] != substates or
            receipt["source_sha256"] != source or
            receipt["model_sha256"] != model or
            receipt["overlay_sha256"] != overlay_sha256):
        raise ValueError("preserved Prince turn-boundary binding residual")

    sidecar = directory / "turn-boundary-information-trivial-v2.txt"
    head = trivial_import.download_sidecar(receipt, sidecar)
    text = sidecar.read_text(encoding="utf-8")
    required = (
        f"information_reachability_binding source_sha256 {source} "
        f"model_sha256 {model}",
        "information_reachability_scope turn_boundary",
        f"information_trivial_overlay_sha256 {overlay_sha256}",
        f"information_trivial_binary_sha256 {receipt['binary_sha256']}",
        f"information_trivial_source_bundle_sha256 "
        f"{receipt['source_bundle_sha256']}",
    )
    if any(binding not in text for binding in required):
        raise ValueError("preserved Prince turn-boundary sidecar residual")
    totals, excluded, trivial = reachability.information_reporting_counts(
        args.filename, text)
    for side in range(2):
        for bucket, parsed in zip(
                ("admitted", "excluded", "trivial"),
                ([totals[side][index] - excluded[side][index]
                  for index in range(4)], excluded[side], trivial[side])):
            recorded = receipt["counts"][str(side)][bucket]
            if parsed != [recorded[name] for name in
                          ("unknown", "win", "loss", "draw")]:
                raise ValueError(
                    "preserved Prince turn-boundary receipt/text residual")
    order = reachability.ledger_side_order(args.filename)
    cells = tuple(reachability.render(
        totals[side], excluded[side], trivial[side]) for side in order)
    boundary_per_side = state_count // 4
    if any(cell_cardinality(cell) != boundary_per_side for cell in cells):
        raise ValueError("preserved Prince turn-boundary conservation residual")
    evidence = {
        "bucket": args.bucket,
        "key": receipt["s3_key"],
        "version_id": receipt["s3_version_id"],
        "sha256": receipt["sidecar_sha256"],
        "size": int(head["ContentLength"]),
        "filename": args.filename,
        "output_sha256": source,
        "model_sha256": model,
        "overlay_sha256": overlay_sha256,
        "binary_sha256": receipt["binary_sha256"],
        "source_bundle_sha256": receipt["source_bundle_sha256"],
    }
    return {
        "first": cells[0], "second": cells[1], "evidence": evidence,
        "receipt_sha256": sha256_path(receipt_path),
        "states_per_side": boundary_per_side,
    }


def update_config(args: argparse.Namespace, value: dict[str, object],
                  certificate: dict[str, object],
                  boundary: dict[str, object] | None = None) -> None:
    config = json.loads(args.config.read_text())
    matches = [job for job in config["jobs"] if job["id"] == args.job]
    if len(matches) != 1 or args.filename not in matches[0]["ledger_files"]:
        raise ValueError("preserved information supervision job residual")
    job = matches[0]
    job["ledger_certifies"] = True
    job.setdefault("ledger_results", {})[args.filename] = value
    object_ = certificate["certificate"]
    s3_object = {
        "bucket": args.bucket, "key": object_["key"],
        "version_id": object_["version_id"],
        "sha256": object_["sha256"], "size": object_["size"],
    }
    if not any(item.get("key") == s3_object["key"]
               for item in job.setdefault("s3_certificates", [])):
        job["s3_certificates"].append(s3_object)
    if boundary is not None:
        evidence = boundary["evidence"]
        sidecars = config.setdefault(
            "ledger_information_reachability_sidecars", [])
        matches = [item for item in sidecars
                   if item.get("filename") == args.filename]
        if matches and matches != [evidence]:
            raise ValueError("preserved Prince duplicate boundary sidecar")
        if not matches:
            sidecars.append(evidence)
            sidecars.sort(key=lambda item: str(item["filename"]))
    args.config.write_text(json.dumps(config, indent=2) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--filename", required=True)
    parser.add_argument("--job", required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-sha256", required=True)
    parser.add_argument("--archive-key", required=True)
    parser.add_argument("--archive-version-id", required=True)
    parser.add_argument("--preservation-certificate", type=Path, required=True)
    parser.add_argument("--preservation-certificate-sha256", required=True)
    parser.add_argument("--preservation-certificate-version-id", required=True)
    parser.add_argument("--overlay-member", required=True)
    parser.add_argument("--proof-member", required=True)
    parser.add_argument("--arbitrary-member", required=True)
    parser.add_argument("--turn-boundary-receipt", type=Path,
                        help="authenticated information-trivial-v2 receipt; "
                             "required for Prince classes")
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--certificate-prefix", required=True)
    parser.add_argument("--region", default="us-west-2")
    parser.add_argument("--config", type=Path, default=ROOT /
                        "tools/tablebases/ultimate_aws_supervision.json")
    parser.add_argument("--readme", type=Path,
                        default=ROOT / "tablebases/README.md")
    args = parser.parse_args()

    boundary = None
    with tempfile.TemporaryDirectory() as raw_directory:
        manifest, proof, arbitrary_sha256, entry, state_count = validate_and_extract(
            args, Path(raw_directory))
        if "prince" in args.filename:
            overlay = (Path(raw_directory) / "payload" /
                       args.overlay_member).read_bytes()
            boundary = validate_turn_boundary_receipt(
                args, overlay, state_count, Path(raw_directory))
    first = certify.outcome_cell(entry, "first")
    second = certify.outcome_cell(entry, "second")
    if "prince" in args.filename:
        # Prince's nonzero continuation substate is an internal half-turn, not
        # a legal tablebase query boundary.  A preservation proof that counts
        # it can converge algebraically while still certifying the wrong
        # public domain, as the rejected kghostprincek archive demonstrated.
        if boundary is None:
            raise ValueError("preserved Prince turn-boundary residual")
        first = str(boundary["first"])
        second = str(boundary["second"])
    certificate = {
        "schema": "ultimate-preserved-information-import-v1",
        "filename": args.filename,
        "importer_sha256": sha256_path(Path(__file__)),
        "entry": entry,
        "archive": {"sha256": args.archive_sha256,
                    "key": args.archive_key,
                    "version_id": args.archive_version_id},
        "preservation_certificate": {
            "sha256": args.preservation_certificate_sha256,
            "version_id": args.preservation_certificate_version_id},
        "bindings": {"archive_manifest_sha256": sha256_bytes(manifest),
                     "proof_sha256": sha256_bytes(proof.encode()),
                     "arbitrary_sha256": arbitrary_sha256},
        "verification": {"archive_coverage_residual": 0,
                         "artifact_hash_residual": 0,
                         "fresh_restore_residual": 0,
                         "proof_residual": 0,
                         "information_overlay_residual": 0,
                         "arbitrary_sidecar_residual": 0},
    }
    if boundary is not None:
        certificate["turn_boundary"] = {
            "scope": "ordinary-turn-boundary-v1",
            "states_per_side": boundary["states_per_side"],
            "receipt_sha256": boundary["receipt_sha256"],
            "sidecar": boundary["evidence"],
        }
        certificate["verification"]["turn_boundary_residual"] = 0
    payload = (json.dumps(certificate, sort_keys=True, indent=2) + "\n").encode()
    digest = sha256_bytes(payload)
    key = (args.certificate_prefix.strip("/") +
           f"/sha256/{digest}/certificate.json")
    with tempfile.NamedTemporaryFile() as stream:
        stream.write(payload)
        stream.flush()
        put = aws("s3api", "put-object", "--bucket", args.bucket,
                  "--key", key, "--body", stream.name,
                  "--metadata", f"sha256={digest}", "--region", args.region,
                  "--output", "json")
    version = str(put.get("VersionId", ""))
    if not version:
        raise ValueError("preserved information certificate lacks VersionId")
    head = aws("s3api", "head-object", "--bucket", args.bucket,
               "--key", key, "--version-id", version,
               "--region", args.region, "--output", "json")
    if (int(head.get("ContentLength", -1)) != len(payload) or
            head.get("Metadata", {}).get("sha256") != digest):
        raise ValueError("preserved information certificate head residual")
    with tempfile.NamedTemporaryFile() as restored:
        aws("s3api", "get-object", "--bucket", args.bucket,
            "--key", key, "--version-id", version, restored.name,
            "--region", args.region, "--output", "json")
        restored.flush()
        if sha256_path(Path(restored.name)) != digest:
            raise ValueError(
                "preserved information certificate download residual")
    storage = (f"S3 information archive sha256:{args.archive_sha256} "
               f"VersionId {args.archive_version_id}; preservation certificate "
               f"sha256:{args.preservation_certificate_sha256} VersionId "
               f"{args.preservation_certificate_version_id}; result certificate "
               f"sha256:{digest} VersionId {version}; arbitrary sidecar "
               f"sha256:{certificate['bindings']['arbitrary_sha256']}")
    if boundary is not None:
        evidence = boundary["evidence"]
        storage += (f"; information trivial v2 sha256:{evidence['sha256']} "
                    f"VersionId {evidence['version_id']}")
    value = {"result_kind": "information v2", "first": first,
             "second": second,
             "reachability": ledger.reachability(first, second),
             "storage": storage}
    object_ = {"sha256": digest, "key": key, "version_id": version,
               "size": len(payload)}
    update_config(args, value, {"certificate": object_}, boundary)
    ledger.update(args.readme, [], [], certified_values=[
        args.filename + "=" + json.dumps(value, separators=(",", ":"))])
    print(json.dumps({"filename": args.filename, "first": first,
                      "second": second, "certificate": object_},
                     sort_keys=True))


if __name__ == "__main__":
    main()
