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
    @staticmethod
    def readme(detail_first: str, detail_digest: str) -> str:
        return (
            "<!-- COMPUTATION_LEDGER_START -->\n"
            "| Key | Class | Domain | File | Status | Indexed states | "
            "Result domain | First starts W / L / D | Second starts W / L / D | "
            "Reachable / unreachable (first; second) | Canonical storage |\n"
            "| --- | --- | --- | --- | --- | ---: | --- | ---: | ---: | ---: | --- |\n"
            "| `single:rook` | King+Rook vs King | single | `krk.uftb` | "
            "**CERTIFIED** | 2 | concrete | 1 / 0 / 0 | 0 / 1 / 0 | "
            "1 / 0; 1 / 0 | result sha256:" + "a" * 64 + " |\n"
            "<!-- COMPUTATION_LEDGER_END -->\n"
            "<!-- GENERATED_TABLE_START -->\n"
            "| File | Class | In-class edges | First material owner starts W / L / D | "
            "Second material owner / bare King starts W / L / D | SHA-256 |\n"
            "| --- | --- | ---: | ---: | ---: | --- |\n"
            "| `krk.uftb` | King+Rook vs King | 1 | " + detail_first +
            " | 0 / 1 / 0 | `" + detail_digest + "` |\n"
            "<!-- GENERATED_TABLE_END -->\n")

    def test_published_details_match_canonical_cells_and_result_sha(self) -> None:
        result = audit.audit_published_details(self.readme("1 / 0 / 0", "a" * 64))
        self.assertEqual({"detail_rows": 1, "result_digests": 1}, result)

    def test_published_details_reject_stale_counts(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "details differ"):
            audit.audit_published_details(self.readme("0 / 0 / 1", "a" * 64))

    def test_published_details_reject_stale_digest(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "result SHA differs"):
            audit.audit_published_details(self.readme("1 / 0 / 0", "b" * 64))

    def test_concrete_certificate_rejects_reverse_orientation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "certificate.json"
            path.write_text(json.dumps({
                "schema": "ultimate-concrete-k2-s3-certificate-v2",
                "completed": [{
                    "filename": "kpenguinkdragon.uftb",
                    "output": {"sha256": "a" * 64},
                    "archive": {"sha256": "b" * 64},
                    "s3": {"version_id": "version"},
                }],
            }))
            binding = {
                "filename": "kdragonkpenguin.uftb",
                "result_sha256": "a" * 64,
                "archive_sha256": "b" * 64,
                "archive_version_id": "version",
            }
            with self.assertRaisesRegex(RuntimeError, "binding differs"):
                audit.audit_concrete_result_certificate(path, binding)


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
