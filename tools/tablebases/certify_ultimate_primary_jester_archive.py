#!/usr/bin/env python3
"""Verify, publish, and import the completed primary-Jester information set.

The retained final-v7 archive contains one UFIW2 overlay and one partial
information-v2 summary for every primary-Jester class.  This command verifies
the archive without solving anything: all proof residuals, dense-state
conservation, overlay headers, payload sizes, migrated payload hashes, and the
concrete-table hashes from the certified legacy snapshot.  It then publishes
one content-addressed certificate and installs the 25 exact rows in the README.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import subprocess
import tarfile
import tempfile

import generate_ultimate_information_tablebases as generate
import ultimate_information_tablebases as information
import update_ultimate_tablebase_ledger as ledger


ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "tablebases/README.md"
PLOT = ROOT / "tools/tablebases/plot_ultimate_tablebases.py"
SUMMARY = "ultimatefish-information-summary.primary-final-v7.partial.json"
MIGRATION = "ultimatefish-primary-jester-af8d-to-final-v7-migration.json"


def sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def primary_records() -> dict[str, dict[str, object]]:
    domains = dict(information.supported_solver_inventory())
    return {
        str(row["filename"]): row
        for row in information.affected_inventory(require_files=False)
        if domains.get(str(row["filename"]), "").startswith("primary-jester")
    }


def outcome_cell(entry: dict[str, object], side: str) -> str:
    outcomes = entry["sides"][side]["outcomes"]
    values = []
    for name in information.OUTCOMES:
        count = outcomes[name]
        legal, unreachable = int(count["legal"]), int(count["unreachable"])
        values.append(f"{legal:,}" +
                      (f" ({unreachable:,})" if unreachable else ""))
    return " / ".join(values)


def validate_entry(filename: str, entry: dict[str, object],
                   record: dict[str, object], concrete: dict[str, str]) -> None:
    states = information.states_per_side(record)
    if (entry.get("states_per_side") != states or
            entry.get("tablebase_sha256") != concrete.get(filename)):
        raise ValueError(f"{filename}: concrete source binding residual")
    model = entry.get("solver_model_sha256")
    if not isinstance(model, str) or len(model) != 64:
        raise ValueError(f"{filename}: solver model residual")
    for side in information.SIDES:
        data = entry["sides"][side]
        legal = unreachable = 0
        for outcome in information.OUTCOMES:
            counts = data["outcomes"][outcome]
            legal += int(counts["legal"])
            unreachable += int(counts["unreachable"])
        certificate = data["certificate"]
        expected = {
            "concrete_realizations": states,
            "legal_realizations": legal,
            "unreachable_realizations": unreachable,
            "unresolved_information_sets": 0,
            **{name: 0 for name in information.CERTIFICATE_RESIDUALS},
        }
        if legal + unreachable != states:
            raise ValueError(f"{filename}: W/L/D conservation residual")
        if any(certificate.get(name) != value
               for name, value in expected.items()):
            raise ValueError(f"{filename}: exact-proof residual")


def validate_overlay(payload: bytes, filename: str,
                     entry: dict[str, object], record: dict[str, object]) -> None:
    if len(payload) < 160:
        raise ValueError(f"{filename}: truncated overlay")
    magic, version, primary, secondary, color, count, _substates = \
        struct.unpack_from("<8s6I", payload)
    expected_count = information.states_per_side(record) * 2
    expected = (
        b"UFIW2\0\0\0", 2,
        generate.PIECE_TYPE_IDS[str(record["primary"])],
        generate.PIECE_TYPE_IDS[str(record["secondary"])],
        int(bool(record["opposing"])), expected_count,
    )
    if ((magic, version, primary, secondary, color, count) != expected or
            len(payload) != 160 + expected_count or
            payload[32:96].decode() != entry["tablebase_sha256"] or
            payload[96:160].decode() != entry["solver_model_sha256"]):
        raise ValueError(f"{filename}: UFIW2 header/extent residual")


def verify(archive: Path, legacy_certificate: Path) -> dict[str, object]:
    archive_bytes = archive.read_bytes()
    legacy = json.loads(legacy_certificate.read_text())
    concrete = {
        Path(item["path"]).name: item["sha256"]
        for item in legacy["manifest"]["artifacts"]
        if item["path"].endswith(".uftb")
    }
    records = primary_records()
    with tarfile.open(fileobj=io.BytesIO(archive_bytes),
                      mode="r:gz") as bundle:
        members = {Path(item.name).name: item for item in bundle.getmembers()
                   if item.isfile()}
        summary_bytes = bundle.extractfile(members[SUMMARY]).read()
        migration_bytes = bundle.extractfile(members[MIGRATION]).read()
        summary = json.loads(summary_bytes)
        migration = json.loads(migration_bytes)
        if (summary.get("schema_version") != information.SCHEMA_VERSION or
                summary.get("semantics") != information.SEMANTICS or
                summary.get("solver", {}).get("version") !=
                information.SOLVER_VERSION or
                summary["solver"].get("exhaustive") is not True or
                summary["solver"].get("belief_cap") is not None or
                set(summary.get("files", {})) != set(records)):
            raise ValueError("primary-Jester summary contract residual")
        overlays = {}
        for filename, record in records.items():
            entry = summary["files"][filename]
            validate_entry(filename, entry, record, concrete)
            overlay_name = Path(filename).with_suffix(".ufiw").name
            payload = bundle.extractfile(members[overlay_name]).read()
            validate_overlay(payload, filename, entry, record)
            overlays[overlay_name] = {
                "bytes": len(payload), "sha256": sha256(payload),
                "payload_sha256": sha256(payload[160:]),
            }
        if (migration.get("classes") != len(records) or
                migration.get("output_checkpoint_sha256") !=
                sha256(summary_bytes)):
            raise ValueError("primary-Jester migration summary residual")
        for name, proof in migration.get("overlays", {}).items():
            actual = overlays[name]
            if (proof.get("new_full_sha256") != actual["sha256"] or
                    proof.get("payload_sha256") != actual["payload_sha256"] or
                    proof.get("payload_identical") is not True):
                raise ValueError(f"{name}: migration payload residual")
    return {
        "schema": "ultimate-primary-jester-information-certificate-v1",
        "predicate": "fresh-maximal-public-view-v2-exact-primary-jester",
        "archive": {"bytes": len(archive_bytes), "sha256": sha256(archive_bytes)},
        "legacy_concrete_snapshot": {
            "archive_sha256": legacy["archive_sha256"],
            "certificate_sha256": sha256(legacy_certificate.read_bytes()),
            "stream_restore_residual": legacy["local"]["stream_restore_residual"],
        },
        "summary": {"bytes": len(summary_bytes), "sha256": sha256(summary_bytes)},
        "migration": {"bytes": len(migration_bytes),
                      "sha256": sha256(migration_bytes)},
        "overlays": overlays,
        "files": summary["files"],
        "verification": {
            "classes": len(records), "unknown_states": 0,
            "proof_residuals": 0, "overlay_header_residuals": 0,
            "overlay_extent_residuals": 0, "concrete_hash_residuals": 0,
            "migration_payload_residuals": 0,
        },
    }


def aws(*args: str) -> dict[str, object]:
    output = subprocess.run(["aws", *args], check=True, text=True,
                            capture_output=True).stdout
    return json.loads(output)


def publish(args: argparse.Namespace, certificate: dict[str, object]) -> str:
    archive = certificate["archive"]
    head = aws("s3api", "head-object", "--bucket", args.bucket,
               "--key", args.archive_key, "--version-id", args.archive_version,
               "--region", args.region, "--output", "json")
    if (head.get("VersionId") != args.archive_version or
            head.get("ContentLength") != archive["bytes"] or
            head.get("Metadata", {}).get("sha256") != archive["sha256"]):
        raise ValueError("version-pinned primary-Jester archive HEAD residual")
    certificate["archive"]["s3"] = {
        "bucket": args.bucket, "key": args.archive_key,
        "version_id": args.archive_version,
    }
    payload = (json.dumps(certificate, indent=2, sort_keys=True) + "\n").encode()
    digest = sha256(payload)
    key = f"{args.certificate_prefix.strip('/')}/sha256/{digest}/certificate.json"
    with tempfile.NamedTemporaryFile() as stream:
        stream.write(payload)
        stream.flush()
        try:
            result = aws("s3api", "head-object", "--bucket", args.bucket,
                         "--key", key, "--region", args.region,
                         "--output", "json")
            if result.get("Metadata", {}).get("sha256") != digest:
                raise ValueError("existing certificate metadata residual")
        except subprocess.CalledProcessError:
            result = aws("s3api", "put-object", "--bucket", args.bucket,
                         "--key", key, "--body", stream.name,
                         "--metadata", f"sha256={digest}",
                         "--region", args.region, "--output", "json")
    version = result.get("VersionId")
    if not version:
        raise ValueError("certificate upload lacks VersionId")
    storage = (
        f"S3 information archive sha256:{archive['sha256']} "
        f"VersionId {args.archive_version}; certificate sha256:{digest} "
        f"VersionId {version}")
    values = []
    for filename, entry in certificate["files"].items():
        first = outcome_cell(entry, "first")
        second = outcome_cell(entry, "second")
        value = {"result_kind": "information v2", "first": first,
                 "second": second,
                 "reachability": ledger.reachability(first, second),
                 "storage": storage}
        values.append(filename + "=" + json.dumps(value, separators=(",", ":")))
    ledger.update(args.readme, [], [], certified_values=values)
    subprocess.run(["python3", str(PLOT)], cwd=ROOT, check=True)
    return json.dumps({"classes": len(values), "certificate_sha256": digest,
                       "certificate_version_id": version,
                       "archive_sha256": archive["sha256"]}, sort_keys=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--legacy-certificate", type=Path, required=True)
    parser.add_argument("--bucket", required=True)
    parser.add_argument("--archive-key", required=True)
    parser.add_argument("--archive-version", required=True)
    parser.add_argument("--certificate-prefix", required=True)
    parser.add_argument("--region", default="us-west-2")
    parser.add_argument("--readme", type=Path, default=README)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    print(publish(args, verify(args.archive, args.legacy_certificate)))


if __name__ == "__main__":
    main()
