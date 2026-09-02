import importlib.util
from pathlib import Path
import sys
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "stage_devil_primary", TOOLS / "stage_ultimate_devil_stateful_primary_aws.py")
assert SPEC and SPEC.loader
stage = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(stage)


class StageUltimateDevilStatefulPrimaryAwsTests(unittest.TestCase):
    def test_only_preserved_partition_can_stage(self):
        self.assertEqual(7, stage.partition(0)["key_record_bytes"])
        with self.assertRaisesRegex(ValueError, "not an exactly preserved"):
            stage.partition(20)

    def test_stage_binds_exact_versions_hashes_and_extents(self):
        with mock.patch.object(stage.base, "send", return_value="ok") as send:
            self.assertEqual(
                "ok", stage.stage("i-test", 0, "/mnt/checkpoint-migrate"))
        instance, commands = send.call_args.args[:2]
        self.assertEqual("i-test", instance)
        script = "\n".join(commands)
        row = stage.partition(0)
        self.assertIn(row["key_sha256"], script)
        self.assertIn(row["node_sha256"], script)
        self.assertIn(row["key_version_id"], script)
        self.assertIn(row["node_version_id"], script)
        self.assertIn(str(row["states"] * row["key_record_bytes"]), script)
        self.assertIn(str(row["states"] * 8), script)
        self.assertIn("restore_residual:0", script)

    def test_stage_rejects_unscoped_mount(self):
        with self.assertRaisesRegex(ValueError, "outside the authorized"):
            stage.stage("i-test", 0, "/tmp")


if __name__ == "__main__":
    unittest.main()
