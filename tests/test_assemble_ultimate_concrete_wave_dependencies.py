from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import assemble_ultimate_concrete_wave_dependencies as assemble  # noqa: E402
import run_ultimate_concrete_tablebase_shard_aws as runner  # noqa: E402


MODEL = "ac9b2d323aeca8705b79c8cb720242985f82ec24fc561ef6b3417d5d603948e6"
INVENTORY = "b82a3d87a42ba342b5b611d068783876e3ecf2701e7684400534b937bbda4fd0"


def fake_item(filename: str, ordinal: int) -> dict[str, object]:
    output_sha = hashlib.sha256(f"output:{filename}".encode()).hexdigest()
    archive_sha = hashlib.sha256(f"archive:{filename}:{ordinal}".encode()).hexdigest()
    return {
        "filename": filename,
        "status": "generated-preserved",
        "output": {"bytes": 1000 + ordinal, "sha256": output_sha},
        "archive": {"bytes": 2000 + ordinal, "sha256": archive_sha},
        "s3": {
            "bucket": "private-versioned-bucket",
            "key": f"results/{archive_sha}.tar.zst",
            "version_id": f"version-{ordinal}",
            "bytes": 2000 + ordinal,
            "sha256": archive_sha,
            "head_full_sha_residual": 0,
            "download_full_sha_residual": 0,
            "archive_restore_residual": 0,
        },
    }


def certificate(items: list[dict[str, object]]) -> dict[str, object]:
    return {
        "schema": runner.CERTIFICATE_SCHEMA,
        "status": "head-download-full-sha-archive-restore-verified",
        "generator_model_sha256": MODEL,
        "inventory_sha256": INVENTORY,
        "selection": {"wave": 0},
        "completed": items,
    }


class AssembleConcreteWaveDependenciesTest(unittest.TestCase):
    def complete_items(self) -> list[dict[str, object]]:
        return [
            fake_item(str(record["filename"]), index)
            for index, record in enumerate(runner.wave_inventory(0))
        ]

    def write_certificate(self, root: Path, name: str,
                          document: dict[str, object]) -> None:
        path = root / name / "wave-certificate.json"
        path.parent.mkdir(parents=True)
        path.write_text(json.dumps(document))

    def test_complete_wave_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.write_certificate(root, "complete", certificate(self.complete_items()))
            records = assemble.collect_results(
                root, 0, model=MODEL, inventory=INVENTORY)
            self.assertEqual(len(records), 226)
            self.assertEqual(set(records), {
                str(record["filename"]) for record in runner.wave_inventory(0)})

    def test_missing_output_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.write_certificate(
                root, "incomplete", certificate(self.complete_items()[:-1]))
            with self.assertRaisesRegex(RuntimeError, "missing=1"):
                assemble.collect_results(root, 0, model=MODEL, inventory=INVENTORY)

    def test_conflicting_duplicate_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            items = self.complete_items()
            self.write_certificate(root, "complete", certificate(items))
            conflicting = dict(items[0])
            conflicting["output"] = {
                "bytes": int(items[0]["output"]["bytes"]),
                "sha256": "f" * 64,
            }
            self.write_certificate(root, "conflict", certificate([conflicting]))
            with self.assertRaisesRegex(RuntimeError, "conflicting preserved"):
                assemble.collect_results(root, 0, model=MODEL, inventory=INVENTORY)

    def test_exact_download_pins_certificate_version(self) -> None:
        payload = b"version-pinned-payload"
        digest = hashlib.sha256(payload).hexdigest()
        record = {
            "bucket": "private-versioned-bucket",
            "key": "results/object",
            "version_id": "exact-version-id",
            "bytes": len(payload),
            "sha256": digest,
        }
        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "download"

            def checked_output(command: list[str], **_kwargs: object) -> str:
                self.assertIn("--version-id", command)
                self.assertEqual(command[command.index("--version-id") + 1],
                                 "exact-version-id")
                return json.dumps({
                    "VersionId": "exact-version-id",
                    "ContentLength": len(payload),
                    "Metadata": {"sha256": digest},
                })

            def run(command: list[str], **_kwargs: object) -> None:
                self.assertIn("--version-id", command)
                self.assertEqual(command[command.index("--version-id") + 1],
                                 "exact-version-id")
                Path(command[-1]).write_bytes(payload)

            with mock.patch.object(assemble.subprocess, "check_output",
                                   side_effect=checked_output), \
                    mock.patch.object(assemble.subprocess, "run", side_effect=run):
                result = assemble.exact_s3_download(record, target)
            self.assertEqual(result["version_id"], "exact-version-id")
            self.assertEqual(target.read_bytes(), payload)

    def test_assembler_is_outside_generator_model_sources(self) -> None:
        self.assertNotIn(
            "tools/assemble_ultimate_concrete_wave_dependencies.py",
            runner.MODEL_SOURCES)
        self.assertEqual(runner.inventory_sha256(), INVENTORY)


if __name__ == "__main__":
    unittest.main()
