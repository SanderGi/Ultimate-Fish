import importlib.util
from pathlib import Path
import sys
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "launch_devil_stateful_recompute",
    TOOLS / "launch_ultimate_devil_stateful_recompute_aws.py")
assert SPEC and SPEC.loader
launch = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(launch)


class LaunchUltimateDevilStatefulRecomputeAwsTests(unittest.TestCase):
    def test_tune_changes_live_gates_without_restart(self):
        with mock.patch.object(launch.base, "send", return_value="ok") as send:
            self.assertEqual("ok", launch.tune(
                "i-test", 19, "32-63", 256 << 30, 320 << 30))
        script = "\n".join(send.call_args.args[1])
        self.assertIn("systemctl is-active --quiet", script)
        self.assertIn("AllowedCPUs=32,33,34", script)
        self.assertIn(f"MemoryHigh={256 << 30}", script)
        self.assertIn(f"MemoryMax={320 << 30}", script)
        self.assertNotIn("systemctl stop", script)
        self.assertNotIn("systemd-run", script)

    def test_tune_rejects_invalid_memory_or_cpu_gate(self):
        with self.assertRaisesRegex(ValueError, "memory gates"):
            launch.tune("i-test", 19, "32-63", 8 << 30, 7 << 30)
        with self.assertRaisesRegex(ValueError, "CPU set"):
            launch.tune("i-test", 19, "64", 8 << 30, 9 << 30)


if __name__ == "__main__":
    unittest.main()
