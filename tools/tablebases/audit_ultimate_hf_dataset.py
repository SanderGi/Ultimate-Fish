#!/usr/bin/env python3
"""Fail-closed audit and manifest generation for the public tablebase dataset."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import update_ultimate_tablebase_ledger as ledger  # noqa: E402


REPO_ID = "SanderGi/Ultimate-Fish-Tablebases"
README = ROOT / "tablebases/README.md"
DEVIL_CERTIFICATE = ROOT / "tablebases/ultimate-devil-stateful-class-certificate.json"
MANAGED_EXTENSIONS = {
    ".uftb", ".ufcross", ".ufgb", ".ufgd", ".ufds", ".ufdsm", ".ufdsp",
    ".ufgf", ".ufgg",
    ".ufgi", ".ufgm", ".ufgp", ".ufgx", ".uficapture", ".ufiw", ".ufja",
    ".ufjg", ".ufmg", ".ufog",
}
LFS_MAX_FILE_BYTES = 50_000_000_000


def devil_name(partition: dict[str, Any]) -> str:
    return f"kdevilk-{str(partition['label']).lower()}.ufds"


def expected_catalog(readme: Path, devil_certificate: Path) -> tuple[set[str], dict[str, dict[str, Any]]]:
    rows = ledger.entries(readme.read_text(encoding="utf-8"))
    tables = {
        row.filename for row in rows
        if row.status == "certified" and row.filename.endswith(".uftb")
    }
    certificate = json.loads(devil_certificate.read_text(encoding="utf-8"))
    if certificate.get("schema") != "ultimate-devil-stateful-class-certificate-v1":
        raise RuntimeError("invalid Devil class certificate schema")
    partitions = {
        devil_name(partition): {
            "size": 32 + int(partition["states"]) * 10,
            "sha256": str(partition["sidecar_sha256"]),
            "square": int(partition["square"]),
        }
        for partition in certificate["partitions"]
    }
    if len(partitions) != 12:
        raise RuntimeError("Devil class certificate does not bind twelve partitions")
    return tables, partitions


def lfs_sha(item: Any) -> str:
    value = item.lfs.get("sha256") if item.lfs else ""
    return value if isinstance(value, str) and re.fullmatch(
        r"[0-9a-f]{64}", value) else ""


def audit(items: list[Any], expected_tables: set[str],
          expected_devil: dict[str, dict[str, Any]], revision: str,
          repo_id: str,
          devil_manifests: dict[str, dict[str, Any]] | None = None) -> dict[str, Any]:
    devil_manifests = devil_manifests or {}
    files: dict[str, dict[str, Any]] = {}
    errors: list[str] = []
    for item in items:
        if not item.rfilename.startswith("tablebases/"):
            continue
        relative = item.rfilename.removeprefix("tablebases/")
        if "/" in relative or Path(relative).suffix not in MANAGED_EXTENSIONS:
            errors.append(f"unexpected managed path: {item.rfilename}")
            continue
        digest = lfs_sha(item)
        # Payloads smaller than 1 KiB are invariably truncated artifacts.  A
        # UFDS shard manifest is deliberately tiny, but is still authenticated
        # by its LFS SHA-256 and then checked against the class certificate.
        minimum_size = 64 if relative.endswith(".ufdsm") else 1024
        if item.size <= minimum_size:
            errors.append(f"undersized/partial artifact: {item.rfilename} ({item.size})")
        if not digest:
            errors.append(f"artifact lacks LFS SHA-256: {item.rfilename}")
        if relative in files:
            errors.append(f"duplicate artifact path: {relative}")
        files[relative] = {"path": item.rfilename, "bytes": item.size,
                           "sha256": digest}

    actual_tables = {name for name in files if name.endswith(".uftb")}
    if actual_tables != expected_tables:
        for name in sorted(expected_tables - actual_tables):
            errors.append(f"missing certified UFTB: {name}")
        for name in sorted(actual_tables - expected_tables):
            errors.append(f"uncertified/stale UFTB: {name}")

    expected_direct = {
        name for name, expected in expected_devil.items()
        if expected["size"] <= LFS_MAX_FILE_BYTES
    }
    expected_sharded = set(expected_devil) - expected_direct
    expected_manifest_names = {
        name.removesuffix(".ufds") + ".ufdsm" for name in expected_sharded
    }
    actual_devil = {name for name in files if name.endswith(".ufds")}
    if actual_devil != expected_direct:
        for name in sorted(expected_direct - actual_devil):
            errors.append(f"missing certified Devil partition: {name}")
        for name in sorted(actual_devil - expected_direct):
            errors.append(f"unexpected Devil partition: {name}")
    for name in expected_direct:
        expected = expected_devil[name]
        actual = files.get(name)
        if actual and (actual["bytes"] != expected["size"] or
                       actual["sha256"] != expected["sha256"]):
            errors.append(f"Devil partition binding mismatch: {name}")

    actual_manifest_names = {name for name in files if name.endswith(".ufdsm")}
    for name in sorted(expected_manifest_names - actual_manifest_names):
        errors.append(f"missing certified Devil shard manifest: {name}")
    for name in sorted(actual_manifest_names - expected_manifest_names):
        errors.append(f"unexpected Devil shard manifest: {name}")
    expected_parts: dict[str, dict[str, Any]] = {}
    for logical_name in sorted(expected_sharded):
        manifest_name = logical_name.removesuffix(".ufds") + ".ufdsm"
        manifest = devil_manifests.get(manifest_name)
        if manifest is None:
            if manifest_name in actual_manifest_names:
                errors.append(f"unreadable Devil shard manifest: {manifest_name}")
            continue
        expected = expected_devil[logical_name]
        parts = manifest.get("parts", ())
        if (manifest.get("schema") != "ultimate-fish-ufds-shard-manifest-v1" or
                manifest.get("filename") != logical_name or
                manifest.get("bytes") != expected["size"] or
                manifest.get("sha256") != expected["sha256"] or
                manifest.get("square") != expected["square"] or
                len(parts) < 2 or
                sum(int(part.get("bytes", 0)) for part in parts) != expected["size"]):
            errors.append(f"invalid Devil shard manifest: {manifest_name}")
            continue
        stem = logical_name.removesuffix(".ufds")
        for index, part in enumerate(parts):
            part_name = str(part.get("filename", ""))
            part_size = int(part.get("bytes", 0))
            part_sha = str(part.get("sha256", ""))
            if (part_name != f"{stem}-part{index:03d}.ufdsp" or
                    part_name in expected_parts or part_size <= 0 or
                    part_size > LFS_MAX_FILE_BYTES or
                    not re.fullmatch(r"[0-9a-f]{64}", part_sha)):
                errors.append(f"invalid Devil shard entry: {manifest_name} part {index}")
                continue
            expected_parts[part_name] = {"size": part_size, "sha256": part_sha}
    actual_parts = {name for name in files if name.endswith(".ufdsp")}
    for name in sorted(set(expected_parts) - actual_parts):
        errors.append(f"missing certified Devil shard: {name}")
    for name in sorted(actual_parts - set(expected_parts)):
        errors.append(f"unexpected Devil shard: {name}")
    for name, expected in expected_parts.items():
        actual = files.get(name)
        if actual and (actual["bytes"] != expected["size"] or
                       actual["sha256"] != expected["sha256"]):
            errors.append(f"Devil shard binding mismatch: {name}")

    certified_stems = {Path(name).stem for name in expected_tables}
    for name in files:
        if name.endswith((".uftb", ".ufds", ".ufdsm", ".ufdsp")):
            continue
        if Path(name).stem not in certified_stems:
            errors.append(f"orphan sidecar without certified UFTB: {name}")

    if errors:
        raise RuntimeError("Hugging Face tablebase audit failed:\n- " + "\n- ".join(errors))
    ordered = [files[name] for name in sorted(files)]
    return {
        "schema": "ultimate-fish-hugging-face-tablebase-manifest-v1",
        "repository": repo_id,
        "catalog_revision": revision,
        "certified_uftb_files": len(expected_tables),
        "certified_devil_partitions": len(expected_devil),
        "sharded_devil_partitions": len(expected_sharded),
        "managed_files": len(ordered),
        "managed_bytes": sum(file["bytes"] for file in ordered),
        "files": ordered,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-id", default=REPO_ID)
    parser.add_argument("--readme", type=Path, default=README)
    parser.add_argument("--devil-certificate", type=Path, default=DEVIL_CERTIFICATE)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    try:
        from huggingface_hub import HfApi
    except ImportError as error:
        raise RuntimeError("huggingface_hub is required") from error
    expected_tables, expected_devil = expected_catalog(
        args.readme, args.devil_certificate)
    api = HfApi()
    info = api.repo_info(args.repo_id, repo_type="dataset", files_metadata=True)
    remote_names = {item.rfilename for item in info.siblings}
    devil_manifests = {}
    from huggingface_hub import hf_hub_download
    for name, expected in expected_devil.items():
        if expected["size"] <= LFS_MAX_FILE_BYTES:
            continue
        manifest_name = name.removesuffix(".ufds") + ".ufdsm"
        remote_path = f"tablebases/{manifest_name}"
        if remote_path not in remote_names:
            continue
        path = hf_hub_download(
            args.repo_id, remote_path, repo_type="dataset", revision=info.sha)
        devil_manifests[manifest_name] = json.loads(Path(path).read_text())
    manifest = audit(info.siblings, expected_tables, expected_devil,
                     info.sha, args.repo_id, devil_manifests)
    if args.manifest:
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({key: manifest[key] for key in (
        "catalog_revision", "certified_uftb_files", "certified_devil_partitions",
        "sharded_devil_partitions", "managed_files", "managed_bytes")},
        sort_keys=True))


if __name__ == "__main__":
    main()
