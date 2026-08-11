#!/usr/bin/env python3
"""Stage a distinct Bomb/Ghost measurement resume from authenticated v2 data."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile


TRANSITION_SUFFIXES = (
    ".header", ".meta", ".strata", ".index", ".blocks", ".verified")
ROWS = {
    "kbombghostk.uftb": {
        "orientation": "same",
        "source_prefix": Path(
            "/mnt/ultimatefish/hidden-kbombghostk-7af5dec2/resume-v2/"
            "work/transitions/kbombghost"),
        "destination": Path(
            "/mnt/ultimatefish/hidden-kbombghostk-7af5dec2/resume-v3"),
        "unit": "ultimatefish-hidden-kbombghostk-resume-v3.service",
    },
    "kbombkghost.uftb": {
        "orientation": "opposing",
        "source_prefix": Path(
            "/mnt/ultimatefish/hidden-kbombkghost-7af5dec2/resume-v2/"
            "work/transitions/kbombkghost"),
        "destination": Path(
            "/mnt/ultimatefish/hidden-kbombkghost-7af5dec2/resume-v3"),
        "unit": "ultimatefish-hidden-kbombkghost-resume-v3.service",
    },
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1 << 20):
            digest.update(block)
    return digest.hexdigest()


def transition_resume_manifest(filename: str, model_sha256: str,
                               observation_sha256: str
                               ) -> dict[str, object]:
    row = ROWS[filename]
    prefix = row["source_prefix"]
    assert isinstance(prefix, Path)
    files = []
    paths = [Path(f"{prefix}{suffix}") for suffix in TRANSITION_SUFFIXES]
    paths.append(prefix.parent.parent / "logs" / "merge.log")
    for path in paths:
        if not path.is_file() or path.stat().st_size <= 0:
            raise RuntimeError(f"preserved Bomb transition is incomplete: {path}")
        files.append({"name": path.name, "bytes": path.stat().st_size,
                      "sha256": sha256(path)})
    return {
        "schema": "ultimate-bomb-ghost-transition-resume-v1",
        "filename": filename,
        "orientation": row["orientation"],
        "model_sha256": model_sha256,
        "observation_sha256": observation_sha256,
        "source_prefix": str(prefix),
        "files": files,
    }


def safe_extract(archive_path: Path, destination: Path) -> None:
    if destination.exists():
        raise RuntimeError(f"Bomb v3 destination already exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(
        prefix=f".{destination.name}.stage-", dir=destination.parent))
    try:
        with tarfile.open(archive_path) as archive:
            for member in archive.getmembers():
                relative = Path(member.name)
                if (relative.is_absolute() or ".." in relative.parts or
                        not (member.isfile() or member.isdir())):
                    raise RuntimeError(
                        f"unsafe Bomb resume bundle member: {member.name}")
            archive.extractall(temporary)
        os.replace(temporary, destination)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


def verify_bundle(root: Path) -> dict[str, object]:
    manifest_path = root / "bundle-manifest.json"
    manifest = json.loads(manifest_path.read_text())
    if manifest.get("schema") != "ultimate-bomb-ghost-aws-v3":
        raise RuntimeError("staged Bomb bundle has the wrong schema")
    for record in manifest.get("files", []):
        path = root / str(record["path"])
        if (not path.is_file() or path.stat().st_size != int(record["bytes"]) or
                sha256(path) != str(record["sha256"])):
            raise RuntimeError(f"staged Bomb bundle residual: {path}")
    return manifest


def inactive(unit: str) -> bool:
    completed = subprocess.run(
        ["systemctl", "show", unit, "--property=ActiveState", "--value"],
        text=True, capture_output=True, check=False)
    return completed.returncode == 0 and completed.stdout.strip() not in {
        "active", "activating", "reloading"}


def stage(bundle: Path, filename: str) -> dict[str, object]:
    row = ROWS[filename]
    destination = row["destination"]
    unit = str(row["unit"])
    assert isinstance(destination, Path)
    if not inactive(unit):
        raise RuntimeError(f"Bomb v3 unit is already active: {unit}")
    safe_extract(bundle, destination)
    manifest = verify_bundle(destination)
    if manifest.get("filename") != filename:
        raise RuntimeError("Bomb bundle filename residual")
    resume = transition_resume_manifest(
        filename, str(manifest["model_sha256"]),
        str(manifest["observation_sha256"]))
    resume_path = destination / "transition-resume-manifest.json"
    resume_path.write_text(json.dumps(resume, indent=2, sort_keys=True) + "\n")
    service = destination / "tools" / "tablebases" / unit
    installed = Path("/etc/systemd/system") / unit
    if installed.exists() and installed.read_bytes() != service.read_bytes():
        raise RuntimeError(f"different Bomb v3 unit already installed: {installed}")
    if not installed.exists():
        shutil.copyfile(service, installed)
    subprocess.run(["systemctl", "daemon-reload"], check=True)
    if not inactive(unit):
        raise RuntimeError(f"Bomb v3 unit activated during staging: {unit}")
    return {
        "filename": filename,
        "destination": str(destination),
        "unit": unit,
        "bundle_sha256": sha256(bundle),
        "resume_manifest_sha256": sha256(resume_path),
        "resume": resume,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--filename", choices=tuple(ROWS), required=True)
    parser.add_argument("--inventory", action="store_true")
    parser.add_argument("--bundle", type=Path)
    args = parser.parse_args()
    if args.inventory == (args.bundle is not None):
        parser.error("choose exactly one of --inventory or --bundle")
    if args.inventory:
        # The model/observation values are supplied only by the authenticated
        # v3 bundle during staging; inventory mode is a read-only extent/hash
        # report for composing that committed binding.
        row = ROWS[args.filename]
        prefix = row["source_prefix"]
        assert isinstance(prefix, Path)
        records = []
        for path in ([Path(f"{prefix}{suffix}")
                      for suffix in TRANSITION_SUFFIXES] +
                     [prefix.parent.parent / "logs" / "merge.log"]):
            if not path.is_file() or path.stat().st_size <= 0:
                raise RuntimeError(f"missing preserved transition: {path}")
            records.append({"name": path.name, "bytes": path.stat().st_size,
                            "sha256": sha256(path)})
        print(json.dumps({"filename": args.filename,
                          "source_prefix": str(prefix), "files": records},
                         sort_keys=True))
    else:
        print(json.dumps(stage(args.bundle.resolve(), args.filename),
                         sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
