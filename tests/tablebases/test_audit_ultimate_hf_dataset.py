import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "tablebases" / "audit_ultimate_hf_dataset.py"
SPEC = importlib.util.spec_from_file_location("audit_hf_dataset", SCRIPT)
assert SPEC and SPEC.loader
auditor = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(auditor)


def item(name, size, digest):
    return SimpleNamespace(
        rfilename=f"tablebases/{name}",
        size=size,
        lfs={"sha256": digest},
    )


class AuditUltimateHfDatasetTests(unittest.TestCase):
    def test_exact_catalog_and_manifest(self):
        digest = "a" * 64
        devil_digest = "b" * 64
        manifest = auditor.audit([
            item("krookk.uftb", 2048, digest),
            item("krookk.ufiw", 4096, digest),
            item("kdevilk-a1.ufds", 8192, devil_digest),
        ], {"krookk.uftb"}, {
            "kdevilk-a1.ufds": {
                "size": 8192,
                "sha256": devil_digest,
                "square": 0,
            },
        }, "revision-1", "owner/dataset")
        self.assertEqual(3, manifest["managed_files"])
        self.assertEqual(1, manifest["certified_uftb_files"])
        self.assertEqual(1, manifest["certified_devil_partitions"])
        self.assertEqual("revision-1", manifest["catalog_revision"])

    def test_rejects_unbound_table_and_sidecar(self):
        digest = "a" * 64
        with self.assertRaisesRegex(RuntimeError, "uncertified/stale UFTB"):
            auditor.audit([
                item("stale.uftb", 2048, digest),
                item("orphan.ufiw", 2048, digest),
            ], set(), {}, "revision-1", "owner/dataset")

    def test_authenticates_oversized_devil_shards(self):
        table_digest = "a" * 64
        full_digest = "b" * 64
        first_digest = "c" * 64
        second_digest = "d" * 64
        manifest_digest = "e" * 64
        full_size = auditor.LFS_MAX_FILE_BYTES + 2048
        first_size = 32_000_000_000
        second_size = full_size - first_size
        shard_manifest = {
            "schema": "ultimate-fish-ufds-shard-manifest-v1",
            "filename": "kdevilk-c2.ufds",
            "bytes": full_size,
            "sha256": full_digest,
            "square": 10,
            "parts": [
                {
                    "filename": "kdevilk-c2-part000.ufdsp",
                    "bytes": first_size,
                    "sha256": first_digest,
                },
                {
                    "filename": "kdevilk-c2-part001.ufdsp",
                    "bytes": second_size,
                    "sha256": second_digest,
                },
            ],
        }
        result = auditor.audit([
            item("krookk.uftb", 2048, table_digest),
            item("kdevilk-c2.ufdsm", 565, manifest_digest),
            item("kdevilk-c2-part000.ufdsp", first_size, first_digest),
            item("kdevilk-c2-part001.ufdsp", second_size, second_digest),
        ], {"krookk.uftb"}, {
            "kdevilk-c2.ufds": {
                "size": full_size,
                "sha256": full_digest,
                "square": 10,
            },
        }, "revision-2", "owner/dataset", {
            "kdevilk-c2.ufdsm": shard_manifest,
        })
        self.assertEqual(1, result["sharded_devil_partitions"])
        self.assertEqual(4, result["managed_files"])

    def test_rejects_shard_conservation_failure(self):
        digest = "a" * 64
        full_size = auditor.LFS_MAX_FILE_BYTES + 2048
        manifest = {
            "schema": "ultimate-fish-ufds-shard-manifest-v1",
            "filename": "kdevilk-c2.ufds",
            "bytes": full_size,
            "sha256": digest,
            "square": 10,
            "parts": [
                {"filename": "kdevilk-c2-part000.ufdsp", "bytes": 2048,
                 "sha256": digest},
                {"filename": "kdevilk-c2-part001.ufdsp", "bytes": 2048,
                 "sha256": digest},
            ],
        }
        with self.assertRaisesRegex(RuntimeError, "invalid Devil shard manifest"):
            auditor.audit([
                item("kdevilk-c2.ufdsm", 565, digest),
                item("kdevilk-c2-part000.ufdsp", 2048, digest),
                item("kdevilk-c2-part001.ufdsp", 2048, digest),
            ], set(), {
                "kdevilk-c2.ufds": {
                    "size": full_size,
                    "sha256": digest,
                    "square": 10,
                },
            }, "revision-3", "owner/dataset", {
                "kdevilk-c2.ufdsm": manifest,
            })


if __name__ == "__main__":
    unittest.main()
