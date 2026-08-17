#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "run_ultimate_jester_ghost_current_aws",
    ROOT / "tools/tablebases/run_ultimate_jester_ghost_current_aws.py")
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class CurrentJesterGhostRunnerTests(unittest.TestCase):
    def test_fresh_ranges_cover_every_raw_frame_once(self) -> None:
        zero, active, merged = runner.ranges()
        self.assertEqual((len(zero), len(active), len(merged)), (40, 40, 80))
        self.assertEqual(merged[0][1], 0)
        self.assertEqual(merged[-1][1] + merged[-1][2], 38_450_880)
        self.assertEqual(len({name for name, _, _ in merged}), 80)

    def test_lower_jester_binding_is_checked_before_expensive_work(self) -> None:
        table_sha = hashlib.sha256(b"table").hexdigest()
        model_sha = hashlib.sha256(b"model").hexdigest()
        header = bytearray(b"UFIW2\0\0\0")
        header.extend(struct.pack(
            "<6I", 2, 1, 30, 0, runner.LOWER_JESTER_STATES, 1))
        header.extend(table_sha.encode("ascii"))
        header.extend(model_sha.encode("ascii"))
        with tempfile.TemporaryDirectory() as temporary:
            overlay = Path(temporary) / "kjesterk.ufiw"
            with overlay.open("wb") as stream:
                stream.write(header)
                stream.truncate(160 + runner.LOWER_JESTER_STATES)
            runner.require_lower_jester_binding(
                overlay, table_sha, model_sha)
            with self.assertRaisesRegex(RuntimeError, "binding mismatch"):
                runner.require_lower_jester_binding(
                    overlay, table_sha, hashlib.sha256(b"wrong").hexdigest())


if __name__ == "__main__":
    unittest.main()
