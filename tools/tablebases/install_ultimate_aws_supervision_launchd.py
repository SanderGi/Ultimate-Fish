#!/usr/bin/env python3
"""Render or install the host-network Ultimate AWS supervision LaunchAgent."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile


LABEL = "org.ultimatefish.aws-supervisor"
ROOT = Path(__file__).resolve().parents[2]
RUNTIME = Path("/private/tmp/ultimatefish-aws-supervision")
STAGED_FILES = (
    "tools/tablebases/supervise_ultimate_aws.py",
    "tools/tablebases/ultimate_aws_supervision.json",
    "tools/tablebases/ultimate_aws_supervision_bridge.py",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def git(repo: Path, *arguments: str) -> str:
    completed = subprocess.run(
        ["git", *arguments], cwd=repo, text=True, capture_output=True,
        check=True)
    return completed.stdout.strip()


def stage(repo: Path) -> Path:
    if git(repo, "branch", "--show-current") != "master":
        raise RuntimeError("host supervisor must be staged from master")
    commit = git(repo, "rev-parse", "HEAD")
    if git(repo, "rev-parse", "origin/master") != commit:
        raise RuntimeError("master must equal origin/master before staging")
    if git(repo, "status", "--porcelain", "--", *STAGED_FILES):
        raise RuntimeError("supervisor inputs must be clean committed files")
    destination = (Path.home() / "Library/Application Support/UltimateFishAWS" /
                   commit)
    tools = destination / "tools"
    tools.mkdir(parents=True, exist_ok=True)
    records = []
    for relative in STAGED_FILES:
        source = repo / relative
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            if sha256(target) != sha256(source):
                raise RuntimeError(
                    f"immutable staged file differs: {target}")
        else:
            shutil.copyfile(source, target)
        target.chmod(0o444)
        records.append({"path": relative, "sha256": sha256(target),
                        "bytes": target.stat().st_size})
    state_source = repo / ".git/ultimate-aws-supervision.json"
    state_directory = destination / ".git"
    state_directory.mkdir(exist_ok=True)
    if state_source.exists():
        shutil.copyfile(state_source,
                        state_directory / "ultimate-aws-supervision.json")
    manifest = {"schema": "ultimate-aws-supervision-stage-v1",
                "commit": commit, "canonical_branch": "origin/master",
                "files": records}
    (destination / "stage-manifest.json").write_text(
        json.dumps(manifest, sort_keys=True, indent=2) + "\n",
        encoding="utf-8")
    return destination


def launch_agent(staged: Path, python: Path) -> dict[str, object]:
    return {
        "Label": LABEL,
        "ProgramArguments": [
            str(python),
            str(staged / "tools/tablebases/ultimate_aws_supervision_bridge.py"),
            "collect", "--python", str(python),
            "--health", str(RUNTIME / "health.json"),
            "--events", str(RUNTIME / "events"), "--json"],
        "WorkingDirectory": str(staged),
        "EnvironmentVariables": {
            "PATH": "/usr/local/bin:/opt/homebrew/bin:/usr/bin:/bin",
        },
        "RunAtLoad": True,
        "StartInterval": 300,
        "ThrottleInterval": 30,
        "ProcessType": "Background",
        "StandardOutPath": str(RUNTIME / "launchd.stdout.log"),
        "StandardErrorPath": str(RUNTIME / "launchd.stderr.log"),
    }


def install(repo: Path, python: Path) -> Path:
    staged = stage(repo)
    RUNTIME.mkdir(parents=True, exist_ok=True)
    destination = Path.home() / "Library/LaunchAgents" / f"{LABEL}.plist"
    destination.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix=destination.name + ".", dir=destination.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            plistlib.dump(launch_agent(staged, python), stream, sort_keys=True)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, destination)
    finally:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
    domain = f"gui/{os.getuid()}"
    subprocess.run(["launchctl", "bootout", domain, str(destination)],
                   check=False, capture_output=True)
    subprocess.run(["launchctl", "bootstrap", domain, str(destination)],
                   check=True)
    subprocess.run(["launchctl", "kickstart", "-k", f"{domain}/{LABEL}"],
                   check=True)
    return destination


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=ROOT)
    parser.add_argument("--python", type=Path, required=True)
    parser.add_argument("--install", action="store_true")
    args = parser.parse_args()
    repo = args.repo.resolve()
    python = args.python.resolve()
    if args.install:
        print(install(repo, python))
    else:
        # Preview uses the current repo path; installation performs the strict
        # committed staging check and substitutes the content-addressed path.
        print(plistlib.dumps(launch_agent(repo, python), sort_keys=True).decode(),
              end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
