import hashlib
import importlib.util
from pathlib import Path
import tarfile
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools" / "package_ultimate_bishop_ghost_preservation.py"
SPEC = importlib.util.spec_from_file_location("bishop_ghost_preservation_package", MODULE_PATH)
assert SPEC and SPEC.loader
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class BishopGhostPreservationPackageTest(unittest.TestCase):
    def test_archive_is_deterministic_and_self_inventorying(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sidecar = root / "result.ufgx"
            sidecar.write_bytes(b"UFGX2-test-payload")
            digest = hashlib.sha256(sidecar.read_bytes()).hexdigest()
            manifest = root / "proof.manifest"
            manifest.write_text(
                f"{PACKAGE.MANIFEST_SCHEMA}\n"
                f"sidecar_bytes={sidecar.stat().st_size}\n"
                f"sidecar_sha256={digest}\n",
                encoding="utf-8",
            )
            first = root / "first.tar"
            second = root / "second.tar"
            first_sha = PACKAGE.build_archive(
                repo=ROOT, sidecar=sidecar, proof_manifest=manifest, output=first
            )
            second_sha = PACKAGE.build_archive(
                repo=ROOT, sidecar=sidecar, proof_manifest=manifest, output=second
            )
            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertEqual(first_sha, second_sha)
            self.assertEqual(PACKAGE.verify_archive(first), first_sha)
            with tarfile.open(first, "r") as archive:
                names = archive.getnames()
                self.assertEqual(names, sorted(names))
                self.assertIn("ARCHIVE.MANIFEST", names)
                self.assertIn("tablebases/kbishopghostk.ufgx", names)
                for member in archive.getmembers():
                    self.assertEqual(member.mtime, 0)
                    self.assertEqual(member.uid, 0)
                    self.assertEqual(member.gid, 0)
                inventory = archive.extractfile("ARCHIVE.MANIFEST")
                assert inventory
                lines = inventory.read().decode("utf-8").splitlines()
                self.assertEqual(lines[0], PACKAGE.ARCHIVE_SCHEMA)
                archived = {line.split(" ", 2)[2]: line for line in lines[1:]}
                for name in names:
                    if name == "ARCHIVE.MANIFEST":
                        continue
                    self.assertIn(name, archived)

    def test_manifest_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sidecar = root / "result.ufgx"
            sidecar.write_bytes(b"result")
            manifest = root / "proof.manifest"
            manifest.write_text(
                f"{PACKAGE.MANIFEST_SCHEMA}\n"
                "sidecar_bytes=6\n"
                f"sidecar_sha256={'0' * 64}\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                PACKAGE.build_archive(
                    repo=ROOT,
                    sidecar=sidecar,
                    proof_manifest=manifest,
                    output=root / "bad.tar",
                )


if __name__ == "__main__":
    unittest.main()
