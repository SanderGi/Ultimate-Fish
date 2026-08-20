from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools/tablebases"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "audit_ultimate_tablebase_ledger_sync",
    TOOLS / "audit_ultimate_tablebase_ledger_sync.py")
audit = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(audit)


class LedgerSyncAuditTest(unittest.TestCase):
    def test_legacy_side_rows_are_exclusions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.reachability-v2.txt"
            path.write_text(
                "reachability side 0 unknown 0 win 2 loss 0 draw 5\n"
                "reachability side 1 unknown 0 win 1 loss 4 draw 0\n"
                "reachability_total side 0 unknown 0 win 10 loss 20 draw 30\n"
                "reachability_total side 1 unknown 0 win 11 loss 22 draw 33\n")
            self.assertEqual(
                ("8 (2) / 20 / 25 (5)", "10 (1) / 18 (4) / 33",
                 "53 / 7; 61 / 5"),
                audit.rendered(path, "krookbishopk.uftb"))

    def test_json_sidecar_uses_unreachable_as_parentheses(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.reachability-v1.json"
            path.write_text(json.dumps({
                "totals": [[0, 10, 20, 30], [0, 11, 22, 33]],
                "unreachable": [[0, 2, 0, 5], [0, 1, 4, 0]],
            }))
            self.assertEqual(
                ("8 (2) / 20 / 25 (5)", "10 (1) / 18 (4) / 33",
                 "53 / 7; 61 / 5"),
                audit.rendered(path, "krookbishopk.uftb"))

    def test_explicit_admitted_residual_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.reachability-v2.txt"
            path.write_text(
                "reachability side 0 unknown 0 win 2 loss 0 draw 5\n"
                "reachability side 1 unknown 0 win 1 loss 4 draw 0\n"
                "reachability_total side 0 unknown 0 win 10 loss 20 draw 30\n"
                "reachability_total side 1 unknown 0 win 11 loss 22 draw 33\n"
                "reachability_excluded side 0 unknown 0 win 2 loss 0 draw 5\n"
                "reachability_excluded side 1 unknown 0 win 1 loss 4 draw 0\n"
                "reachability_admitted side 0 unknown 0 win 9 loss 20 draw 25\n"
                "reachability_admitted side 1 unknown 0 win 10 loss 18 draw 33\n")
            with self.assertRaisesRegex(ValueError, "admission residual"):
                audit.rendered(path, "krookbishopk.uftb")


if __name__ == "__main__":
    unittest.main()
