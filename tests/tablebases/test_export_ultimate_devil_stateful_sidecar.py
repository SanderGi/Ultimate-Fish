#!/usr/bin/env python3
"""Regression tests for legacy and compact stateful Devil sidecar export."""

from __future__ import annotations

import struct
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / "src/ultimate_devil_stateful_sidecar_exporter"


class DevilStatefulExporterTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        subprocess.run(
            ["make", "-C", "src", "ultimate-devil-stateful-sidecar-exporter"],
            cwd=ROOT,
            check=True,
        )

    def export(self, key: bytes, width: int, output: Path) -> bytes:
        keys = output.with_suffix(".keys")
        nodes = output.with_suffix(".nodes")
        keys.write_bytes(key)
        nodes.write_bytes(struct.pack("<BxHHH", 1, 5, 0, 0))
        subprocess.run(
            [
                str(BINARY), "--keys", str(keys), "--nodes", str(nodes),
                "--output", str(output), "--square", "9", "--workers", "2",
                "--key-record-bytes", str(width),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        return output.read_bytes()

    def test_v1_legacy_key_normalizes_to_v4_compact_key(self) -> None:
        # White king A1, black king B1, live fixed Devil B2, no Minions,
        # no secondary piece. This is the exact v1 16-byte proof-key layout.
        high = (1 << 24) | (9 << 31) | (80 << 40)
        expected = (80 << 38) | (1 << 48)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            legacy = self.export(struct.pack("<QQ", 0, high), 16, root / "legacy.ufds")
            compact = self.export(expected.to_bytes(7, "little"), 7, root / "compact.ufds")
        self.assertEqual(legacy, compact)
        magic, version, square, count, width, reserved = struct.unpack("<8sIIQII", legacy[:32])
        self.assertEqual((magic, version, square, count, width, reserved),
                         (b"UFDSV1\0\0", 1, 9, 1, 10, 0))
        self.assertEqual(int.from_bytes(legacy[32:39], "little"), expected)
        self.assertEqual(legacy[39], 1)
        self.assertEqual(struct.unpack("<H", legacy[40:42])[0], 5)


if __name__ == "__main__":
    unittest.main()
