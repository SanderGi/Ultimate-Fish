import importlib.util
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "audit_ultimate_giant_v7_batch",
    ROOT / "tools" / "tablebases" / "audit_ultimate_giant_v7_batch.py")
assert SPEC and SPEC.loader
audit = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = audit
SPEC.loader.exec_module(audit)


class GiantV7AuditTests(unittest.TestCase):
    def test_both_native_audit_prefixes_parse_exactly(self):
        for prefix in ("reachability", "predecessor_safety"):
            output = (
                f"{prefix} side 0 unknown 0 win 12 loss 34 draw 56\n"
                f"{prefix} side 1 unknown 0 win 78 loss 90 draw 123\n")
            self.assertEqual(
                audit.parse_audit(output, "fixture", prefix),
                [[0, 12, 34, 56], [0, 78, 90, 123]])

    def test_audit_prefixes_cannot_be_substituted(self):
        output = (
            "reachability side 0 unknown 0 win 1 loss 2 draw 3\n"
            "reachability side 1 unknown 0 win 4 loss 5 draw 6\n")
        with self.assertRaisesRegex(ValueError, "one audit row per side"):
            audit.parse_audit(output, "fixture", "predecessor_safety")


if __name__ == "__main__":
    unittest.main()
