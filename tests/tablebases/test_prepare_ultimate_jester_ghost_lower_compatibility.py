#!/usr/bin/env python3

import argparse
import importlib.util
from pathlib import Path
import tempfile
import unittest

from artifact_support import require_artifacts


ROOT = Path(__file__).resolve().parents[2]
OVERLAY = Path(
    "/private/tmp/ultimatefish-information-overlays.exact-progress-v2/kjesterk.ufiw")


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


compatibility = load(
    "jg_lower_compatibility",
    ROOT / "tools/tablebases/prepare_ultimate_jester_ghost_lower_compatibility.py")
measurement = load(
    "jg_compat_measurement",
    ROOT / "tools/tablebases/run_ultimate_jester_ghost_measurement_aws.py")


class LowerJesterCompatibilityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        require_artifacts(ROOT, "tablebases/kjesterk.uftb")

    def setUp(self):
        if not OVERLAY.is_file():
            self.skipTest("certified lower Jester overlay unavailable")

    def prepare(self, root: Path):
        tool = ROOT / "tools/tablebases/prepare_ultimate_jester_ghost_lower_compatibility.py"
        args = argparse.Namespace(
            tool_source=tool,
            tool_sha256=compatibility.sha256_file(tool),
            lower_table=ROOT / "tablebases/kjesterk.uftb",
            lower_overlay=OVERLAY,
            output_dir=root / "compatible")
        result = compatibility.prepare(args)
        return args, result

    def test_production_v4_payload_and_overlay_flags_are_unchanged(self):
        with tempfile.TemporaryDirectory() as directory:
            args, result = self.prepare(Path(directory))
            old_table = args.lower_table.read_bytes()
            new_table = (args.output_dir / "kjesterk-v5-compat.uftb").read_bytes()
            old_overlay = args.lower_overlay.read_bytes()
            new_overlay = (args.output_dir / "kjesterk-v5-compat.ufiw").read_bytes()
            self.assertEqual(4, compatibility.u32(old_table, 8))
            self.assertEqual(5, compatibility.u32(new_table, 8))
            self.assertEqual(old_table[40:], new_table[48:])
            self.assertEqual(old_overlay[160:], new_overlay[160:])
            self.assertEqual(old_overlay[:32], new_overlay[:32])
            self.assertEqual(old_overlay[96:160], new_overlay[96:160])
            self.assertEqual(result["table_sha256"],
                             new_overlay[32:96].decode("ascii"))

    def test_measurement_authenticates_compatibility_certificate(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            args, result = self.prepare(root)
            certificate = args.output_dir / "compatibility-certificate.json"
            manifest = {
                "lower_jester_overlay_sha256": compatibility.OLD_OVERLAY_SHA256,
                "files": [{"path": "tablebases/kjesterk.uftb",
                           "sha256": compatibility.OLD_TABLE_SHA256}],
            }
            measured = argparse.Namespace(
                lower_compatibility_certificate=certificate,
                lower_compatibility_certificate_sha256=
                result["certificate_sha256"],
                lower_compatibility_tool_sha256=args.tool_sha256,
                lower_jester_model_sha256=compatibility.LOWER_MODEL_SHA256,
                lower_jester_table=args.output_dir / "kjesterk-v5-compat.uftb",
                lower_jester_overlay=args.output_dir / "kjesterk-v5-compat.ufiw")
            table, overlay, proof = measurement.compatibility_inputs(
                measured, manifest)
            self.assertEqual(result["table_sha256"], table)
            self.assertEqual(result["overlay_sha256"], overlay)
            self.assertEqual(result["certificate_sha256"], proof)

    def test_existing_destination_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "compatible").mkdir()
            tool = ROOT / "tools/tablebases/prepare_ultimate_jester_ghost_lower_compatibility.py"
            args = argparse.Namespace(
                tool_source=tool,
                tool_sha256=compatibility.sha256_file(tool),
                lower_table=ROOT / "tablebases/kjesterk.uftb",
                lower_overlay=OVERLAY,
                output_dir=root / "compatible")
            with self.assertRaisesRegex(ValueError, "already exists"):
                compatibility.prepare(args)


if __name__ == "__main__":
    unittest.main()
