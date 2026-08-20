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
        self.assertIn('zstd -T0 -19 -q -f -o "$archive"', script)

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

    def test_expected_concrete_source_is_an_independent_binding(self):
        source = "a" * 64
        model = "b" * 64
        args = SimpleNamespace(
            filename="kjesterangelk.uftb",
            expected_source_sha256=source,
            expected_model_sha256=model,
        )
        entry = {
            "tablebase_sha256": source,
            "solver_model_sha256": model,
        }
        finalizer.validate_expected_bindings(args, entry)
        entry["tablebase_sha256"] = "c" * 64
        with self.assertRaisesRegex(ValueError, "concrete source binding"):
            finalizer.validate_expected_bindings(args, entry)

    def test_angel_record_uses_canonical_information_inventory(self):
        record = finalizer.record_for_filename("kjesterkangel.uftb")
        self.assertEqual("jester", record["primary"])
        self.assertEqual("angel", record["secondary"])
        self.assertTrue(record["opposing"])
        self.assertEqual(75_915_840, record["states"])

    def test_certified_result_retires_matching_supervision_job(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "supervision.json"
            path.write_text(json.dumps({"jobs": [{
                "id": "same-angel",
                "unit": "same.service",
                "ledger_files": ["kjesterangelk.uftb"],
                "ledger_certifies": False,
                "s3_certificates": [],
            }]}))
            args = SimpleNamespace(
                supervision_config=path,
                unit="same.service",
                filename="kjesterangelk.uftb",
            )
            value = {
                "result_kind": "information v2",
                "first": "1 / 0 / 0",
                "second": "0 / 1 / 0",
                "reachability": "1 / 0; 1 / 0",
                "storage": "exact",
            }
            certificates = [{
                "bucket": "bucket", "key": "results/kjesterangelk.tar.zst",
                "version_id": "version", "sha256": "d" * 64, "size": 1,
            }]
            finalizer.update_supervision(args, value, certificates)
            job = json.loads(path.read_text())["jobs"][0]
        self.assertTrue(job["ledger_certifies"])
        self.assertTrue(job["s3_only_certified"])
        self.assertEqual(value, job["ledger_results"]["kjesterangelk.uftb"])
        self.assertEqual(
            ["results/kjesterangelk.tar.zst"],
            job["result_certificate_keys"]["kjesterangelk.uftb"])


if __name__ == "__main__":
    unittest.main()
