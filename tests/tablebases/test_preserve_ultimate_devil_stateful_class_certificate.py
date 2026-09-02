import copy
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "preserve_devil_stateful_class",
    TOOLS / "preserve_ultimate_devil_stateful_class_certificate.py")
assert SPEC and SPEC.loader
preserver = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(preserver)


class PreserveUltimateDevilStatefulClassCertificateTests(unittest.TestCase):
    def complete_manifest(self):
        data = json.loads(preserver.finalizer.DEFAULT_MANIFEST.read_text())
        template = next(row for row in data["partitions"]
                        if "census_sha256" in row)
        for row, square in zip(data["partitions"],
                               preserver.finalizer.FIXED_SQUARES):
            if row.get("status") != "PRESERVED" or "census_sha256" not in row:
                row.clear()
                row.update(copy.deepcopy(template))
            row["square"] = square
            row["label"] = f"{chr(ord('A') + square % 8)}{square // 8 + 1}"
        return data

    def test_preserve_uses_exact_version_restore(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = root / "manifest.json"
            output = root / "certificate.json"
            manifest.write_text(json.dumps(self.complete_manifest()))

            def fake_aws(*arguments):
                if arguments[1] == "put-object":
                    return "version-1"
                if arguments[1] == "get-object":
                    shutil.copyfile(output, Path(arguments[-1]))
                    return ""
                self.fail(f"unexpected AWS call: {arguments}")

            with mock.patch.object(preserver, "aws", side_effect=fake_aws):
                receipt = preserver.preserve(manifest, output)
            self.assertEqual("version-1", receipt["version_id"])
            self.assertEqual(0, receipt["restore_residual"])
            self.assertEqual(0, receipt["fixed_square_coverage_residual"])
            self.assertTrue(output.is_file())


if __name__ == "__main__":
    unittest.main()
