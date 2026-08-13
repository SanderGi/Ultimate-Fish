#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "ordinary_from_concrete",
    ROOT / "tools/tablebases/run_ultimate_ghost_ordinary_from_concrete.py")
assert SPEC and SPEC.loader
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class CertifiedConcreteSourceTests(unittest.TestCase):
    def test_accepts_only_hash_matching_preserved_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            table = root / "kberserkerghostk.uftb"
            table.write_bytes(b"concrete table")
            digest = hashlib.sha256(table.read_bytes()).hexdigest()
            certificate = root / "wave-certificate.json"
            certificate.write_text(json.dumps({
                "schema": module.CERTIFICATE_SCHEMA,
                "status": module.CERTIFICATE_STATUS,
                "generator_model_sha256": "a" * 64,
                "completed": [{
                    "filename": table.name,
                    "status": "generated-preserved",
                    "output": {"sha256": digest},
                }],
            }))
            self.assertEqual(module.certified_source(
                certificate, table, table.name, "a" * 64), digest)
            table.write_bytes(b"tampered")
            with self.assertRaisesRegex(RuntimeError, "SHA-256 mismatch"):
                module.certified_source(
                    certificate, table, table.name, "a" * 64)

    def test_rejects_unpreserved_result(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            table = root / "x.uftb"
            table.write_bytes(b"x")
            certificate = root / "wave-certificate.json"
            certificate.write_text(json.dumps({
                "schema": module.CERTIFICATE_SCHEMA,
                "status": module.CERTIFICATE_STATUS,
                "generator_model_sha256": "a" * 64,
                "completed": [{"filename": table.name,
                               "status": "generated"}],
            }))
            with self.assertRaisesRegex(RuntimeError, "not preserved"):
                module.certified_source(
                    certificate, table, table.name, "a" * 64)


if __name__ == "__main__":
    unittest.main()
