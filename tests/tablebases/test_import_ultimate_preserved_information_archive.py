#!/usr/bin/env python3

from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tablebases"))

import import_ultimate_preserved_information_archive as importer


class PreservedInformationImportTest(unittest.TestCase):
    def test_rejects_preservation_certificate_for_another_filename(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            archive = root / "archive.tar.zst"
            archive.write_bytes(b"not reached")
            archive_sha = importer.sha256_path(archive)
            certificate = root / "certificate.json"
            certificate.write_text(
                "{\n"
                '  "filename": "kghostkrook.uftb",\n'
                f'  "sha256": "{archive_sha}",\n'
                '  "s3": {\n'
                f'    "sha256": "{archive_sha}",\n'
                '    "key": "results/archive",\n'
                '    "version_id": "version",\n'
                '    "archive_restore_residual": 0,\n'
                '    "download_residual": 0,\n'
                '    "head_residual": 0\n'
                "  }\n"
                "}\n")
            args = SimpleNamespace(
                archive=archive,
                archive_sha256=archive_sha,
                preservation_certificate=certificate,
                preservation_certificate_sha256=importer.sha256_path(
                    certificate),
                filename="kghostkpenguin.uftb",
                archive_key="results/archive",
                archive_version_id="version",
            )
            with self.assertRaisesRegex(
                    ValueError, "preservation certificate S3 residual"):
                importer.validate_and_extract(args, root / "extract")

    def test_legacy_certificate_must_bind_exact_archive_basename(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            archive = root / "archive.tar.zst"
            archive.write_bytes(b"not reached")
            archive_sha = importer.sha256_path(archive)
            certificate = root / "certificate.json"
            certificate.write_text(
                "{\n"
                '  "archive": "another.tar.zst",\n'
                f'  "sha256": "{archive_sha}",\n'
                '  "s3": {\n'
                f'    "sha256": "{archive_sha}",\n'
                '    "key": "results/archive.tar.zst",\n'
                '    "version_id": "version",\n'
                '    "archive_restore_residual": 0,\n'
                '    "download_residual": 0,\n'
                '    "head_residual": 0\n'
                "  }\n"
                "}\n")
            args = SimpleNamespace(
                archive=archive,
                archive_sha256=archive_sha,
                preservation_certificate=certificate,
                preservation_certificate_sha256=importer.sha256_path(
                    certificate),
                filename="kghostprincek.uftb",
                archive_key="results/archive.tar.zst",
                archive_version_id="version",
            )
            with self.assertRaisesRegex(
                    ValueError, "preservation certificate S3 residual"):
                importer.validate_and_extract(args, root / "extract")


if __name__ == "__main__":
    unittest.main()
