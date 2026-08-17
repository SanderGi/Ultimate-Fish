import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest


SCRIPT = (Path(__file__).parents[2] / "tools" / "tablebases" /
          "rebind_ultimate_ghost_ordinary_artifact_manifest.py")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class RebindOrdinaryGhostManifestTest(unittest.TestCase):
    def test_rebinds_only_after_header_and_inventory_authentication(self):
        values = {
            "source_sha256": "1" * 64,
            "normalized_source_sha256": "2" * 64,
            "model_sha256": "3" * 64,
            "observation_sha256": "4" * 64,
            "lower_sha256": "5" * 64,
            "lower_model_sha256": "6" * 64,
            "lower_ghost_source_sha256": "7" * 64,
            "lower_ghost_model_sha256": "8" * 64,
            "lower_ghost_observation_sha256": "9" * 64,
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = root / "work/results/test.ufgd"
            result.parent.mkdir(parents=True)
            arbitrary = bytearray(1248)
            arbitrary[:8] = b"UFGD1\0\0\0"
            struct.pack_into("<II", arbitrary, 8, 1, 1248)
            offsets = ((160, "source_sha256"),
                       (224, "normalized_source_sha256"),
                       (288, "model_sha256"),
                       (352, "observation_sha256"))
            for offset, key in offsets:
                arbitrary[offset:offset + 64] = values[key].encode()
            lower_ghost = root / "kghostk.ufgm"
            ghost = bytearray(320)
            ghost[:8] = b"UFGM1\0\0\0"
            struct.pack_into("<II", ghost, 8, 1, 320)
            for offset, key in ((96, "lower_ghost_source_sha256"),
                                (160, "lower_ghost_model_sha256"),
                                (224, "lower_ghost_observation_sha256")):
                ghost[offset:offset + 64] = values[key].encode()
            lower_ghost.write_bytes(ghost)
            lower_ghost_sha = digest(lower_ghost)
            arbitrary[416:480] = lower_ghost_sha.encode()
            arbitrary[480:544] = values["lower_sha256"].encode()
            arbitrary[544:608] = values["lower_sha256"].encode()
            arbitrary[608:672] = values["lower_model_sha256"].encode()
            result.write_bytes(arbitrary)
            manifest = root / "work/artifact-manifest.json"
            manifest.write_text(json.dumps({
                "schema": "ultimate-ordinary-ghost-artifacts-v1",
                "source_sha256": values["source_sha256"],
                "model_sha256": values["model_sha256"],
                "lower_sha256": values["lower_sha256"],
                "lower_model_sha256": values["lower_model_sha256"],
                "files": {"work/results/test.ufgd": {
                    "bytes": result.stat().st_size, "sha256": digest(result)}},
            }), encoding="utf-8")
            evidence = root / "evidence/original.json"
            subprocess.run([
                sys.executable, str(SCRIPT), "--root", str(root),
                "--manifest", str(manifest), "--arbitrary", str(result),
                "--lower-ghost-sidecar", str(lower_ghost),
                "--evidence", str(evidence)], check=True,
                capture_output=True, text=True)
            rebound = json.loads(manifest.read_text(encoding="utf-8"))
            self.assertEqual(rebound["observation_sha256"], "4" * 64)
            self.assertEqual(rebound["lower_ghost_sidecar_sha256"],
                             lower_ghost_sha)
            self.assertEqual(rebound["lower_ghost_source_sha256"], "7" * 64)
            self.assertTrue(evidence.is_file())

    def test_rejects_inventory_mismatch_without_evidence_or_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "manifest.json"
            original = {"schema": "ultimate-ordinary-ghost-artifacts-v1",
                        "files": {"missing": {"bytes": 1, "sha256": "0" * 64}}}
            manifest.write_text(json.dumps(original), encoding="utf-8")
            evidence = root / "evidence.json"
            completed = subprocess.run([
                sys.executable, str(SCRIPT), "--root", str(root),
                "--manifest", str(manifest), "--arbitrary", str(root / "x"),
                "--lower-ghost-sidecar", str(root / "y"),
                "--evidence", str(evidence)], capture_output=True, text=True)
            self.assertNotEqual(completed.returncode, 0)
            self.assertEqual(json.loads(manifest.read_text()), original)
            self.assertFalse(evidence.exists())


if __name__ == "__main__":
    unittest.main()
