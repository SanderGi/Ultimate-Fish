#!/usr/bin/env python3
"""Tests for concrete S3 certificate aggregation."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "summary", ROOT / "tools" / "summarize_ultimate_concrete_certificates.py",
)
assert SPEC and SPEC.loader
SUMMARY = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = SUMMARY
SPEC.loader.exec_module(SUMMARY)


def certificate(filename: str = "kbishopkghost.uftb") -> dict[str, object]:
    archive_sha = "a" * 64
    return {
        "schema": "ultimate-concrete-k2-s3-certificate-v2",
        "generator_model_sha256": "b" * 64,
        "inventory_sha256": "c" * 64,
        "local_outputs_retained": True,
        "local_scratch_retained": True,
        "safe_to_delete_gate": False,
        "status": "head-download-full-sha-archive-restore-verified",
        "completed": [{
            "filename": filename,
            "status": "generated-preserved",
            "output": {
                "bytes": 140,
                "draw": 2,
                "exceptions": 0,
                "loss": 3,
                "sha256": "d" * 64,
                "states": 10,
                "verification_residual": 0,
                "win": 5,
            },
            "archive": {"bytes": 80, "sha256": archive_sha},
            "s3": {
                "archive_restore_residual": 0,
                "bucket": "private-versioned-bucket",
                "bytes": 80,
                "download_full_sha_residual": 0,
                "head_full_sha_residual": 0,
                "key": f"concrete/v2/sha256/{archive_sha}/{filename}.tar.zst",
                "sha256": archive_sha,
                "version_id": "version-1",
            },
        }],
    }


class CertificateSummaryTests(unittest.TestCase):
    def write_certificate(self, root: Path, document: dict[str, object]) -> Path:
        payload = (json.dumps(document, indent=2, sort_keys=True) + "\n").encode()
        digest = hashlib.sha256(payload).hexdigest()
        directory = root / "sha256" / digest
        directory.mkdir(parents=True)
        path = directory / "wave-certificate.json"
        path.write_bytes(payload)
        return path

    def test_valid_certificate_produces_exact_totals(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.write_certificate(root, certificate())
            report = SUMMARY.load_report(root)
            self.assertEqual(1, report.certificates)
            self.assertEqual(10, report.states)
            self.assertEqual((5, 3, 2), (report.win, report.loss, report.draw))
            self.assertEqual((140, 80), (report.raw_bytes, report.compressed_bytes))
            rendered = SUMMARY.markdown(report)
            self.assertIn("`kbishopkghost.uftb`", rendered)
            self.assertIn("**80**", rendered)

    def test_corrupt_or_unrestored_certificate_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = self.write_certificate(root, certificate())
            path.write_text(path.read_text() + " ")
            with self.assertRaisesRegex(ValueError, "not stored under its SHA-256"):
                SUMMARY.load_report(root)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = certificate()
            document["completed"][0]["s3"]["version_id"] = ""
            self.write_certificate(root, document)
            with self.assertRaisesRegex(ValueError, "not versioned"):
                SUMMARY.load_report(root)

    def test_identical_duplicate_uses_smallest_verified_archive(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.write_certificate(root, certificate())
            duplicate = certificate()
            duplicate["completed"][0]["archive"]["sha256"] = "e" * 64
            duplicate["completed"][0]["archive"]["bytes"] = 70
            duplicate["completed"][0]["s3"]["sha256"] = "e" * 64
            duplicate["completed"][0]["s3"]["bytes"] = 70
            duplicate["completed"][0]["s3"]["key"] = (
                "concrete/v2/sha256/" + "e" * 64 + "/duplicate.tar.zst"
            )
            self.write_certificate(root, duplicate)
            report = SUMMARY.load_report(root)
            self.assertEqual(2, report.certificates)
            self.assertEqual(1, len(report.artifacts))
            self.assertEqual(2, report.artifacts[0].preserved_copies)
            self.assertEqual(70, report.compressed_bytes)
            self.assertEqual("e" * 64, report.artifacts[0].archive_sha256)

    def test_conflicting_duplicate_filename_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.write_certificate(root, certificate())
            duplicate = certificate()
            duplicate["completed"][0]["output"]["sha256"] = "e" * 64
            duplicate["completed"][0]["archive"]["sha256"] = "f" * 64
            duplicate["completed"][0]["s3"]["sha256"] = "f" * 64
            duplicate["completed"][0]["s3"]["key"] = (
                "concrete/v2/sha256/" + "f" * 64 + "/duplicate.tar.zst"
            )
            self.write_certificate(root, duplicate)
            with self.assertRaisesRegex(ValueError, "conflicting duplicate"):
                SUMMARY.load_report(root)


if __name__ == "__main__":
    unittest.main()
