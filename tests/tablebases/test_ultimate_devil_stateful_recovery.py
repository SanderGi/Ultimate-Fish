import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "tools" / "tablebases" / "ultimate_devil_stateful_recovery.json"


class UltimateDevilStatefulRecoveryTests(unittest.TestCase):
    def test_manifest_has_exact_fixed_square_domain(self):
        data = json.loads(MANIFEST.read_text())
        self.assertEqual("ultimate-devil-stateful-recovery-v1", data["schema"])
        self.assertFalse(data["entry_slice_certifies_stateful_material"])
        self.assertEqual(12, data["required_partitions"])
        rows = data["partitions"]
        self.assertEqual(12, len(rows))
        self.assertEqual(
            {0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19},
            {row["square"] for row in rows},
        )
        self.assertEqual(12, len({row["label"] for row in rows}))

    def test_preserved_rows_are_exact_version_bound_primary_planes(self):
        data = json.loads(MANIFEST.read_text())
        preserved = [row for row in data["partitions"]
                     if row["status"] == "PRESERVED"]
        self.assertGreaterEqual(len(preserved), 5)
        for row in preserved:
            self.assertGreater(row["states"], 0)
            self.assertIn(row["checkpoint_version"], {1, 2, 3, 4})
            expected_width = {1: 16, 2: 16, 3: 14, 4: 7}[
                row["checkpoint_version"]]
            self.assertEqual(expected_width, row["key_record_bytes"])
            for name in ("key_sha256", "node_sha256"):
                self.assertEqual(64, len(row[name]))
                int(row[name], 16)
            for name in ("key_version_id", "node_version_id",
                         "receipt_version_id"):
                self.assertTrue(row[name])
            evidence = json.dumps(row).lower()
            self.assertNotIn("bishop", evidence)
            self.assertNotIn("entry_slice", evidence)
            if "sidecar_sha256" in row:
                self.assertEqual(64, len(row["sidecar_sha256"]))
                int(row["sidecar_sha256"], 16)
                self.assertTrue(row["sidecar_version_id"])
                self.assertTrue(row["sidecar_receipt_version_id"])
            if "census_sha256" in row:
                self.assertIn("sidecar_sha256", row)
                self.assertEqual(64, len(row["census_sha256"]))
                int(row["census_sha256"], 16)
                self.assertTrue(row["census_version_id"])
                self.assertEqual(
                    row["states"],
                    row["census_wins"] + row["census_losses"] +
                    row["census_draws"],
                )
                self.assertGreaterEqual(row["census_max_dtw"], 0)


if __name__ == "__main__":
    unittest.main()
