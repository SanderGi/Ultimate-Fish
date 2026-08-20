import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools/tablebases"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "finalize_ultimate_primary_jester_class_aws",
    TOOLS / "finalize_ultimate_primary_jester_class_aws.py")
assert SPEC and SPEC.loader
finalizer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(finalizer)


class DirectNativeLogTests(unittest.TestCase):
    @staticmethod
    def _args(direct_log):
        return SimpleNamespace(
            filename="kjesterangelk.uftb",
            s3_prefix="results/jester-angel-information-v1",
            source_root="/work",
            checkpoint=None,
            overlays="/work",
            unit="ultimatefish-jester-angel.service",
            allow_post_solve_checkpoint_failure=False,
            direct_log=direct_log,
            bucket="versioned-bucket",
            region="us-west-2",
        )

    @staticmethod
    def _parser(script):
        return script.split("<<'PY'\n", 1)[1].split("\nPY", 1)[0]

    def test_direct_log_script_has_valid_shell_and_python(self):
        script = finalizer.remote_script(self._args("/work/solve.log"))
        subprocess.run(["bash", "-n"], input=script, text=True, check=True)
        compile(self._parser(script), "<direct-native-log-parser>", "exec")
        self.assertNotIn('test -s "$checkpoint"', script)
        self.assertIn('install -m 0644 "$solve_log"', script)

    def test_direct_log_keeps_legal_and_unreachable_counts_separate(self):
        script = finalizer.remote_script(self._args("/work/solve.log"))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "solve.log"
            overlay = root / "result.ufiw"
            entry = root / "entry.json"
            log.write_text(
                "information_fixed_point iterations 4 bellman_residual 0 "
                "rank_residual 0\n"
                "information_summary side 0 win 1 loss 0 draw 0 "
                "unreachable_win 0 unreachable_loss 1 unreachable_draw 0 "
                "sets 2 concrete 2 bellman_residual 0 rank_residual 0 "
                "belief_cap none exhaustive 1\n"
                "information_summary side 1 win 0 loss 1 draw 1 "
                "unreachable_win 0 unreachable_loss 0 unreachable_draw 0 "
                "sets 2 concrete 2 bellman_residual 0 rank_residual 0 "
                "belief_cap none exhaustive 1\n")
            header = bytearray(160)
            struct.pack_into(
                "<8s6I", header, 0, b"UFIW2\0\0\0", 2, 1, 26, 0, 4, 0)
            header[32:96] = b"a" * 64
            header[96:160] = b"b" * 64
            overlay.write_bytes(header + b"\0" * 4)
            subprocess.run(
                [sys.executable, "-", str(log), str(overlay), str(entry)],
                input=self._parser(script), text=True, check=True)
            result = json.loads(entry.read_text())
        first = result["sides"]["first"]
        self.assertEqual({"legal": 1, "unreachable": 0},
                         first["outcomes"]["win"])
        self.assertEqual({"legal": 0, "unreachable": 1},
                         first["outcomes"]["loss"])
        self.assertEqual(1, first["certificate"]["legal_realizations"])
        self.assertEqual(1, first["certificate"]["unreachable_realizations"])


if __name__ == "__main__":
    unittest.main()
