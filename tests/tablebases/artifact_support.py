"""Helpers for tests whose immutable tablebase payloads live in S3.

The repository intentionally does not keep multi-gigabyte production payloads.
Unit tests remain runnable without them; data-backed integration tests clearly
skip until their exact fixtures have been restored and authenticated locally.
"""

from __future__ import annotations

from pathlib import Path
import unittest


def require_artifacts(root: Path, *relative_paths: str) -> None:
    missing = [path for path in relative_paths if not (root / path).is_file()]
    if missing:
        raise unittest.SkipTest(
            "S3-canonical tablebase fixture not restored: " + ", ".join(missing))
