#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "archive_ultimate_aws_result",
    ROOT / "tools" / "archive_ultimate_aws_result.py")
assert SPEC and SPEC.loader
archive = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(archive)


def digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


class ResultArchiveTest(unittest.TestCase):
    def fixture(self, root: Path) -> Path:
        payloads = {
            "work/logs/solve.log": b"proof complete\n",
            "work/results/example.ufiw": bytes(range(64)),
            "work/transitions/example.verified": b"verified\0",
        }
        records = []
        for relative, payload in payloads.items():
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
            records.append({"path": relative, "bytes": len(payload),
                            "sha256": digest(payload)})
        manifest = root / "work" / "artifact-manifest.json"
        manifest.write_text(json.dumps({
            "source_sha256": "1" * 64,
            "model_sha256": "2" * 64,
            "artifacts": list(reversed(records)),
        }))
        return manifest

    def test_deterministic_build_and_restore(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "root"
            root.mkdir()
            manifest = self.fixture(root)
            first, first_certificate = archive.build_archive(
                root, manifest, "example-result", base / "first")
            second, second_certificate = archive.build_archive(
                root, manifest, "example-result", base / "second")
            self.assertEqual(first_certificate["sha256"],
                             second_certificate["sha256"])
            self.assertEqual(first.read_bytes(), second.read_bytes())
            verified = archive.verify_archive(first, base / "third-restore")
            self.assertEqual(verified["archive_restore_residual"], 0)
            self.assertEqual(
                (base / "third-restore/work/results/example.ufiw").read_bytes(),
                bytes(range(64)))

    def test_manifest_mismatch_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = self.fixture(root)
            (root / "work/results/example.ufiw").write_bytes(b"corrupt")
            with self.assertRaisesRegex(RuntimeError, "extent/full-SHA"):
                archive.authenticated_inventory(root, manifest, "example")

    def test_invalid_compression_level_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = self.fixture(root)
            with self.assertRaisesRegex(RuntimeError, "compression level"):
                archive.build_archive(
                    root, manifest, "example", root / "archive", 0)

    def test_traversal_and_symlink_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            root.mkdir(exist_ok=True)
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps({"artifacts": [{
                "path": "../outside", "bytes": 0, "sha256": digest(b"")}] }))
            with self.assertRaisesRegex(RuntimeError, "unsafe artifact path"):
                archive.authenticated_inventory(root, manifest, "example")

    def test_content_addressed_s3_keys(self) -> None:
        bucket, key, uri = archive.s3_object(
            "s3://private-bucket/ultimate", "results/sha256/abc/result.tar.zst")
        self.assertEqual("private-bucket", bucket)
        self.assertEqual("ultimate/results/sha256/abc/result.tar.zst", key)
        self.assertEqual("s3://private-bucket/" + key, uri)


if __name__ == "__main__":
    unittest.main()
