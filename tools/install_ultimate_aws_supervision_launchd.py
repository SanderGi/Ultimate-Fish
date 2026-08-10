#!/usr/bin/env python3
"""Render or install the host-network Ultimate AWS supervision LaunchAgent."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import plistlib
import subprocess
import tempfile


LABEL = "org.ultimatefish.aws-supervisor"
ROOT = Path(__file__).resolve().parents[1]


def launch_agent(repo: Path, python: Path) -> dict[str, object]:
    git = repo / ".git"
    return {
        "Label": LABEL,
        "ProgramArguments": [
            str(python), str(repo / "tools/ultimate_aws_supervision_bridge.py"),
            "collect", "--python", str(python), "--json"],
        "WorkingDirectory": str(repo),
        "EnvironmentVariables": {
            "PATH": "/usr/local/bin:/opt/homebrew/bin:/usr/bin:/bin",
        },
        "RunAtLoad": True,
        "StartInterval": 300,
        "ThrottleInterval": 30,
        "ProcessType": "Background",
        "StandardOutPath": str(git / "ultimate-aws-launchd.stdout.log"),
        "StandardErrorPath": str(git / "ultimate-aws-launchd.stderr.log"),
    }


def install(repo: Path, python: Path) -> Path:
    destination = Path.home() / "Library/LaunchAgents" / f"{LABEL}.plist"
    destination.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        prefix=destination.name + ".", dir=destination.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            plistlib.dump(launch_agent(repo, python), stream, sort_keys=True)
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
        print(plistlib.dumps(launch_agent(repo, python), sort_keys=True).decode(),
              end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
