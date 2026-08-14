#!/usr/bin/env python3
"""Verify the checked-in 5.731 interaction ledger against an IL2CPP dump.

The proprietary dump remains outside the repository.  This tool hashes the
complete set of piece-specific virtual overrides, checks every recovered
SimulatedPiece subclass, and validates each semantic family's named source
anchors.  A native update can therefore not silently add or move a callback
without invalidating the audit.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LEDGER = ROOT / "tests" / "ultimate_native_interaction_audit.json"

CLASS_RE = re.compile(r"^public class (Simulated\w+) : SimulatedPiece")
RVA_RE = re.compile(r"RVA: (0x[0-9A-Fa-f]+)")
OVERRIDE_RE = re.compile(r"^\s*public override (.+?)\s*\{ \}")
ANCHOR_RE = re.compile(
    r"^(Simulated\w+)\.([A-Za-z0-9_]+)@(0x[0-9a-f]+)$"
)


def parse_native_dump(path: Path) -> tuple[set[str], list[tuple[str, str, str]]]:
    classes: set[str] = set()
    overrides: list[tuple[str, str, str]] = []
    current: str | None = None
    rva: str | None = None
    for line in path.read_text(errors="strict").splitlines():
        match = CLASS_RE.match(line)
        if match:
            current = match.group(1)
            classes.add(current)
            rva = None
            continue
        if current is not None and line.startswith("// Namespace:"):
            current = None
            rva = None
            continue
        if current is None:
            continue
        match = RVA_RE.search(line)
        if match:
            rva = match.group(1).lower()
            continue
        match = OVERRIDE_RE.match(line)
        if match:
            if rva is None:
                raise ValueError(
                    f"override {current}.{match.group(1)} has no preceding RVA"
                )
            overrides.append((current, match.group(1), rva))
    return classes, overrides


def override_digest(overrides: list[tuple[str, str, str]]) -> str:
    payload = "".join(
        f"{class_name}|{signature}|{rva}\n"
        for class_name, signature, rva in overrides
    )
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def method_name(signature: str) -> str:
    prefix = signature.split("(", 1)[0]
    return prefix.rsplit(" ", 1)[-1]


def validate_dump(ledger: dict, dump_path: Path) -> dict[str, object]:
    classes, overrides = parse_native_dump(dump_path)
    expected_classes = set(ledger["native_classes"])
    if classes != expected_classes:
        missing = sorted(expected_classes - classes)
        extra = sorted(classes - expected_classes)
        raise ValueError(
            f"native class mismatch; missing={missing}, extra={extra}"
        )
    expected_count = ledger["native_override_count"]
    if len(overrides) != expected_count:
        raise ValueError(
            f"native override count is {len(overrides)}, expected {expected_count}"
        )
    digest = override_digest(overrides)
    if digest != ledger["native_override_sha256"]:
        raise ValueError(
            f"native override digest is {digest}, expected "
            f"{ledger['native_override_sha256']}"
        )

    available = {
        (class_name, method_name(signature), rva)
        for class_name, signature, rva in overrides
    }
    checked_anchors = 0
    for family in ledger["families"]:
        for anchor in family["native_anchors"]:
            match = ANCHOR_RE.match(anchor)
            if not match:
                raise ValueError(
                    f"{family['id']} has malformed native anchor {anchor!r}"
                )
            key = (match.group(1), match.group(2), match.group(3))
            if key not in available:
                raise ValueError(
                    f"{family['id']} native anchor is absent: {anchor}"
                )
            checked_anchors += 1
    return {
        "app_version": ledger["app_version"],
        "native_classes": len(classes),
        "native_overrides": len(overrides),
        "native_override_sha256": digest,
        "semantic_families": len(ledger["families"]),
        "checked_anchors": checked_anchors,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump_cs", type=Path)
    parser.add_argument("--ledger", type=Path, default=DEFAULT_LEDGER)
    args = parser.parse_args()
    ledger = json.loads(args.ledger.read_text())
    print(json.dumps(validate_dump(ledger, args.dump_cs), indent=2))


if __name__ == "__main__":
    main()
