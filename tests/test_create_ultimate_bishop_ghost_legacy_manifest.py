#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import importlib.util
from io import BytesIO
from pathlib import Path
import tarfile
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "bishop_legacy_manifest",
    ROOT / "tools/create_ultimate_bishop_ghost_legacy_manifest.py")
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


class BishopGhostLegacyManifestTest(unittest.TestCase):
    def fixture(self, root: Path) -> Path:
        result = b"legacy-bishop-ghost"
        result_path = root / MODULE.OUTPUT
        result_path.write_bytes(result)
        declaration = f"{digest(result)}  {result_path}\n".encode()
        (root / MODULE.OUTPUT_SHA).write_bytes(declaration)
        dependency = root / "dependency.uftb"
        dependency.write_bytes(b"dependency")
        (root / MODULE.INPUTS).write_text(
            f"{digest(dependency.read_bytes())}  {dependency}\n", encoding="ascii")
        log = (
            "information_symbolic_certificate iterations 2 bellman_residual 0 "
            "monotonicity_residual 0 singleton_residual 0 "
            "compaction_root_residual 0 belief_cap none powerset_exact 1\n"
            "information_summary side 0 bellman_residual 0 rank_residual 0 "
            "belief_cap none exhaustive 1\n"
            "information_summary side 1 bellman_residual 0 rank_residual 0 "
            "belief_cap none exhaustive 1\n"
            "ghost_extra_external_root_conservation admitted 1 total 1 "
            "independent_grouping_residual 0 realization_residual 0 "
            "conservation_residual 0\n"
            f"information_overlay {result_path} bytes {len(result)} "
            "root_grouping_residual 0 conservation_residual 0\n"
        ).encode()
        payloads = {name: b"" for name in MODULE.EXPECTED_PROOF_MEMBERS}
        payloads["solve.log"] = log
        payloads[MODULE.OUTPUT_SHA] = declaration
        with tarfile.open(root / MODULE.PROOF, "w:gz") as archive:
            for name, payload in payloads.items():
                info = tarfile.TarInfo(name)
                info.size = len(payload)
                archive.addfile(info, BytesIO(payload))
        return dependency

    def test_authenticates_legacy_result(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.fixture(root)
            output = root / "manifest.json"
            manifest = MODULE.build_manifest(root, output)
            self.assertEqual(4, len(manifest["artifacts"]))
            self.assertEqual(digest((root / MODULE.OUTPUT).read_bytes()),
                             manifest["output_sha256"])
            self.assertTrue(output.is_file())

    def test_output_mismatch_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.fixture(root)
            (root / MODULE.OUTPUT).write_bytes(b"corrupt")
            with self.assertRaisesRegex(RuntimeError, "output SHA residual"):
                MODULE.build_manifest(root, root / "manifest.json")

    def test_proof_inventory_mismatch_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.fixture(root)
            proof = root / MODULE.PROOF
            with tarfile.open(proof, "r:gz") as archive:
                payloads = {
                    member.name: archive.extractfile(member).read()
                    for member in archive.getmembers()
                }
            payloads["extra"] = b""
            with tarfile.open(proof, "w:gz") as archive:
                for name, payload in payloads.items():
                    info = tarfile.TarInfo(name)
                    info.size = len(payload)
                    archive.addfile(info, BytesIO(payload))
            with self.assertRaisesRegex(RuntimeError, "proof archive inventory"):
                MODULE.build_manifest(root, root / "manifest.json")


if __name__ == "__main__":
    unittest.main()
