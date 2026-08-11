"""Fail-closed tests for the v4 archive/root staging helper."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import stage_ultimate_concrete_wave0_batch_v4 as stage  # noqa: E402


def digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def make_archive(path: Path, files: dict[str, bytes]) -> None:
    with tarfile.open(path, "w") as archive:
        for name, payload in sorted(files.items()):
            info = tarfile.TarInfo(name)
            info.size = len(payload)
            info.mode = 0o444
            archive.addfile(info, __import__("io").BytesIO(payload))


def manifest_for(files: dict[str, bytes]) -> bytes:
    return (json.dumps({
        "schema": stage.DEPENDENCY_SCHEMA,
        "files": [{"filename": name, "bytes": len(payload),
                   "sha256": digest(payload)}
                  for name, payload in sorted(files.items())],
    }, indent=2, sort_keys=True) + "\n").encode()


class StageV4Test(unittest.TestCase):
    def test_archive_manifest_payload_parity_is_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = {"kghostk.uftb": b"ghost"}
            valid = root / "valid.tar"
            make_archive(valid, {**payload, "manifest.json": manifest_for(payload)})
            candidate = root / "candidate"
            stage.extract_archive(valid, candidate)
            records = stage.validate_dependency_archive(candidate)
            self.assertEqual(records["kghostk.uftb"]["bytes"], 5)

            invalid = root / "invalid.tar"
            declared = {**payload, "kmissing.uftb": b"not-present"}
            make_archive(invalid, {
                "kghostk.uftb": payload["kghostk.uftb"],
                "manifest.json": manifest_for(declared),
            })
            invalid_candidate = root / "invalid-candidate"
            stage.extract_archive(invalid, invalid_candidate)
            with self.assertRaisesRegex(RuntimeError, "manifest/payload mismatch"):
                stage.validate_dependency_archive(invalid_candidate)

    def test_duplicate_identical_candidates_are_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = b"same dependency"
            first = root / "first"
            second = root / "second"
            first.mkdir()
            second.mkdir()
            (first / "kghostk.uftb").write_bytes(payload)
            (second / "kghostk.uftb").write_bytes(payload)
            record = {"filename": "kghostk.uftb", "bytes": len(payload),
                      "sha256": digest(payload)}
            selected = stage.resolve_exact_candidate([first, second], record)
            self.assertEqual(selected, first / "kghostk.uftb")

    def test_divergent_and_missing_candidates_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            good = root / "good"
            bad = root / "bad"
            good.mkdir()
            bad.mkdir()
            (good / "kbombk.uftb").write_bytes(b"good")
            (bad / "kbombk.uftb").write_bytes(b"bad")
            record = {"filename": "kbombk.uftb", "bytes": 4,
                      "sha256": digest(b"good")}
            with self.assertRaisesRegex(RuntimeError, "divergent"):
                stage.resolve_exact_candidate([good, bad], record)
            with self.assertRaisesRegex(RuntimeError, "missing"):
                stage.resolve_exact_candidate([good], {
                    "filename": "kqk.uftb", "bytes": 4, "sha256": digest(b"q")})

    def test_dependency_root_is_idempotent_but_never_overwritten(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "dependencies-v4" / "batch-v4-01"
            candidate = Path(temporary) / "candidate"
            candidate.mkdir()
            payload = b"exact bytes"
            source = candidate / "kghostk.uftb"
            source.write_bytes(payload)
            record = {"filename": source.name, "bytes": len(payload),
                      "sha256": digest(payload)}
            expected_manifest = hashlib.sha256(
                stage._manifest_payload([record])).hexdigest()
            stage.materialize_dependency_root(
                root, [record], {source.name: source}, expected_manifest)
            before = (root / source.name).read_bytes()
            stage.materialize_dependency_root(
                root, [record], {source.name: source}, expected_manifest)
            self.assertEqual(before, (root / source.name).read_bytes())
            (root / source.name).chmod(0o644)
            (root / source.name).write_bytes(b"tampered")
            with self.assertRaisesRegex(RuntimeError, "pre-existing destination mismatch"):
                stage.materialize_dependency_root(
                    root, [record], {source.name: source}, expected_manifest)

    def test_unit_install_is_exact_and_does_not_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = root / "units-candidate"
            candidate.mkdir()
            text = "[Unit]\nDescription=v4\n"
            name = "ultimatefish-concrete-wave0-batch-v4-01-class123.service"
            (candidate / name).write_text(text)
            units = [{"unit": name, "service_text": text,
                      "service_sha256": digest(text.encode())}]
            target = root / "system"
            stage.materialize_units(target, candidate, units)
            stage.materialize_units(target, candidate, units)
            self.assertEqual(text, (target / name).read_text())
            (target / name).chmod(0o644)
            (target / name).write_text("tampered")
            with self.assertRaisesRegex(RuntimeError, "pre-existing destination mismatch"):
                stage.materialize_units(target, candidate, units)

    def test_source_materialization_includes_bound_plan_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = root / "source-candidate"
            source = candidate / "ultimatefish"
            (source / "tools").mkdir(parents=True)
            (source / "src").mkdir(parents=True)
            plan = b"{\"schema\":\"ultimate-concrete-wave0-batch-plan-v4\"}\n"
            (source / "tools/ultimate_concrete_wave0_batch_v4.json").write_bytes(plan)
            (source / "src/probe.cpp").write_bytes(b"probe")
            target = root / "concrete-wave0-batch/source-v4/ultimatefish"
            stage.materialize_source_root(
                target, candidate, {"src/probe.cpp": digest(b"probe")},
                extra_hashes={
                    "tools/ultimate_concrete_wave0_batch_v4.json": digest(plan)})
            self.assertEqual(plan,
                             (target / "tools/ultimate_concrete_wave0_batch_v4.json").read_bytes())


if __name__ == "__main__":
    unittest.main()
