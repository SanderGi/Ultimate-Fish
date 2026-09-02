import importlib.util
from pathlib import Path
import sys
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "export_preserve_devil_sidecar",
    TOOLS / "export_preserve_ultimate_devil_stateful_sidecar_aws.py")
assert SPEC and SPEC.loader
exporter = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(exporter)


class ExportPreserveUltimateDevilStatefulSidecarAwsTests(unittest.TestCase):
    def test_launch_uses_explicit_memory_gates(self):
        with mock.patch.object(exporter.base, "send", return_value="ok") as send:
            self.assertEqual("ok", exporter.launch(
                "i-test", 11, "/mnt/ultimatefish/test", "0-15",
                68719476736, 85899345920))
        script = "\n".join(send.call_args.args[1])
        self.assertIn("MemoryHigh=68719476736", script)
        self.assertIn("MemoryMax=85899345920", script)

    def test_launch_rejects_inverted_memory_gates(self):
        with self.assertRaisesRegex(ValueError, "invalid sidecar memory gates"):
            exporter.launch("i-test", 11, "/mnt/ultimatefish/test", "0", 2, 1)


if __name__ == "__main__":
    unittest.main()
