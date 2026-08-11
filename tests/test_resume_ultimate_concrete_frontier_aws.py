#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "frontier_resume",
    ROOT / "tools/resume_ultimate_concrete_frontier_aws.py")
assert SPEC and SPEC.loader
RESUME = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RESUME)


class FrontierResumeTests(unittest.TestCase):
    def fixture(self, root: Path) -> dict[str, object]:
        source = root / "failed"
        record = {
            "filename": "kberserkerkprince.uftb", "primary": "berserker",
            "secondary": "prince", "opposing": True, "states": 2,
            "packed_bytes": 3, "shards": 1, "phase": "kings+2-stateful",
        }
        model = "a" * 64
        inventory = "b" * 64
        binary_sha = RESUME.hashlib.sha256(b"binary").hexdigest()
        dependency_sha = RESUME.hashlib.sha256(b"dependency").hexdigest()
        files = {
            "run_plan": ("run-plan.json", json.dumps({
                "generator_model_sha256": model,
                "inventory_sha256": inventory,
                "binary_sha256": binary_sha,
                "dependency_manifest_sha256": dependency_sha,
                "selected": [record], "status": "full-preflight",
            }).encode()),
            "generator_log": (
                "logs/generate/kberserkerkprince.log",
                b"frontier states 1/2\n"),
            "resource_certificate": (
                "logs/generate/kberserkerkprince.resources.json",
                json.dumps({"returncode": -15, "violation": "rss gate",
                            "scratch_retained": True}).encode()),
            "binary": ("binary/ultimate_tablebase", b"binary"),
            "dependency_manifest": ("dependencies/manifest.json", b"dependency"),
        }
        document: dict[str, object] = {
            "schema": RESUME.SCHEMA,
            "source_work_directory": str(source),
            "generator_model_sha256": model,
            "inventory_sha256": inventory,
            "binary_sha256": binary_sha,
            "dependency_manifest_sha256": dependency_sha,
            "states": 2, "substates": 20,
            "record": record, "last_frontier_marker": "frontier states 1/2",
            "resource_violation": "rss gate", "planes": {},
        }
        for name, (relative, payload) in files.items():
            path = source / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
            document[name] = {
                "relative_path": relative, "bytes": len(payload),
                "sha256": RESUME.sha256_path(path),
            }
        plane_payloads = {
            "nodes": b"node-frontier-bytes",
            "degrees": b"degree-frontier-bytes",
            "offsets": b"partial-offsets",
            "predecessors": b"partial-predecessor-file",
        }
        for name, payload in plane_payloads.items():
            relative = f"scratch/kberserkerkprince.{name}"
            path = source / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
            allocated = path.stat().st_blocks * 512
            document["planes"][name] = {
                "relative_path": relative, "bytes": len(payload),
                "allocated_bytes": (len(payload) - 1 if name == "predecessors"
                                    else allocated),
                "sha256": RESUME.sha256_path(path),
            }
        # Sparse allocation is an AWS phase proof.  Unit fixtures are tiny and
        # cannot create the same sparse geometry, so bind their observed value.
        document["planes"]["predecessors"]["allocated_bytes"] = \
            (source / document["planes"]["predecessors"]["relative_path"]
             ).stat().st_blocks * 512
        # Keep the logical extent larger than its recorded allocation proof.
        document["planes"]["predecessors"]["bytes"] = \
            document["planes"]["predecessors"]["allocated_bytes"] + 1
        # authenticate() checks the logical extent, so use a sparse file with
        # that exact extent and refresh its hash/binding.
        predecessor = source / document["planes"]["predecessors"]["relative_path"]
        with predecessor.open("ab") as stream:
            stream.truncate(document["planes"]["predecessors"]["bytes"])
        document["planes"]["predecessors"].update({
            "allocated_bytes": predecessor.stat().st_blocks * 512,
            "sha256": RESUME.sha256_path(predecessor),
        })
        if document["planes"]["predecessors"]["allocated_bytes"] >= \
                document["planes"]["predecessors"]["bytes"]:
            # Filesystems may allocate one full block for this tiny fixture.
            document["planes"]["predecessors"]["bytes"] = 8193
            with predecessor.open("ab") as stream:
                stream.truncate(8193)
            document["planes"]["predecessors"].update({
                "allocated_bytes": predecessor.stat().st_blocks * 512,
                "sha256": RESUME.sha256_path(predecessor),
            })
        return document

    def test_authenticates_all_planes_and_refuses_mutation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = self.fixture(root)
            with mock.patch.object(
                    RESUME.concrete, "generator_model_sha256",
                    return_value=document["generator_model_sha256"]):
                result = RESUME.authenticate_manifest(document)
                self.assertIn("retrograde-not-started", result["frontier_status"])
                nodes = (Path(document["source_work_directory"]) /
                         document["planes"]["nodes"]["relative_path"])
                nodes.write_bytes(nodes.read_bytes() + b"damage")
                with self.assertRaisesRegex(RuntimeError, "extent mismatch"):
                    RESUME.authenticate_manifest(document)

    def test_native_checkpoint_is_exclusive_and_sources_stay_unchanged(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = self.fixture(root)
            source = Path(document["source_work_directory"])
            before = {
                name: RESUME.sha256_path(
                    source / document["planes"][name]["relative_path"])
                for name in document["planes"]
            }
            destination = root / "resume/scratch/kberserkerkprince"
            result = RESUME.compose_checkpoint(document, destination)
            payload = destination.read_bytes()
            header = struct.unpack("<8sIIIIII", payload[:32])
            self.assertEqual(RESUME.CHECKPOINT_MAGIC, header[0])
            self.assertEqual(RESUME.concrete.PIECE_INDEX["berserker"], header[1])
            self.assertEqual(RESUME.concrete.PIECE_INDEX["prince"], header[2])
            self.assertEqual((1, 2, 20, 2), header[3:])
            self.assertEqual(result["bytes"], len(payload))
            after = {
                name: RESUME.sha256_path(
                    source / document["planes"][name]["relative_path"])
                for name in document["planes"]
            }
            self.assertEqual(before, after)
            with self.assertRaises(FileExistsError):
                RESUME.compose_checkpoint(document, destination)

    def test_default_is_read_only_and_full_requires_work_directory(self) -> None:
        args = RESUME.parse_args([])
        self.assertFalse(args.full)
        self.assertIsNone(args.work_directory)

    def test_explicit_selection_binds_inventory_class(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            document = self.fixture(Path(temporary))
            record = RESUME.concrete.normalized_record(
                RESUME.concrete.wave_inventory(0)[20])
            document.update({
                "record": record, "states": record["states"], "substates": 10,
                "selection": {"wave": 0, "begin": 20, "end": 21,
                              "classes": 1},
            })
            run_plan_path = (Path(document["source_work_directory"]) /
                             document["run_plan"]["relative_path"])
            run_plan = json.loads(run_plan_path.read_text())
            run_plan["selected"] = [record]
            run_plan_path.write_text(json.dumps(run_plan))
            document["run_plan"].update({
                "bytes": run_plan_path.stat().st_size,
                "sha256": RESUME.sha256_path(run_plan_path),
            })
            with mock.patch.object(
                    RESUME.concrete, "generator_model_sha256",
                    return_value=document["generator_model_sha256"]):
                RESUME.authenticate_manifest(document)
            document["selection"]["begin"] = 21
            document["selection"]["end"] = 22
            with self.assertRaisesRegex(RuntimeError, "selection/class"):
                RESUME.authenticate_manifest(document)

    def test_class003_production_resume_is_bounded_and_preserves_source(self) -> None:
        manifest = json.loads((
            ROOT / "tools/ultimate_concrete_wave0_003_resume.json").read_text())
        self.assertEqual(
            RESUME.concrete.normalized_record(
                RESUME.concrete.wave_inventory(0)[3]), manifest["record"])
        self.assertEqual(
            "/mnt/ultimatefish-overflow/concrete-penguin-1083b6f8/"
            "wave0-003-full-v3", manifest["source_work_directory"])
        self.assertLess(
            manifest["planes"]["predecessors"]["allocated_bytes"],
            manifest["planes"]["predecessors"]["bytes"])
        service = (ROOT / "tools/ultimatefish-concrete-wave0-003-"
                   "ac9b2d32-resume-v1.service").read_text()
        self.assertIn("--minimum-free-bytes 322122547200", service)
        self.assertIn("--reverse-edge-bytes-limit 60129542144", service)
        self.assertIn("Restart=no", service)
        self.assertIn(
            "ReadOnlyPaths=" + manifest["source_work_directory"], service)
        self.assertIn(
            "--work-directory /mnt/ultimatefish/concrete-penguin-1083b6f8/"
            "wave0-003-resume-v1/work", service)

    def test_class006_resume_binds_retained_volume_and_mount(self) -> None:
        manifest = json.loads((
            ROOT / "tools/ultimate_concrete_wave0_006_resume.json").read_text())
        self.assertEqual(
            RESUME.concrete.normalized_record(
                RESUME.concrete.wave_inventory(0)[6]), manifest["record"])
        self.assertLess(
            manifest["planes"]["predecessors"]["allocated_bytes"],
            manifest["planes"]["predecessors"]["bytes"])
        service = (ROOT / "tools/ultimatefish-concrete-wave0-006-"
                   "ac9b2d32-resume-v1.service").read_text()
        self.assertIn("Requires=mnt-ultimatefish\\x2dresume.mount", service)
        self.assertIn("--minimum-free-bytes 214748364800", service)
        self.assertIn("--reverse-edge-bytes-limit 34359738368", service)
        self.assertIn("Restart=no", service)
        mount = (ROOT / "tools/mnt-ultimatefish\\x2dresume.mount").read_text()
        self.assertIn(
            "What=/dev/disk/by-uuid/d71ca950-588b-4ff8-9160-e9b7d2d78fe2",
            mount)
        self.assertIn("Where=/mnt/ultimatefish-resume", mount)

    def test_class011_resume_is_exact_and_has_large_reverse_gate(self) -> None:
        manifest = json.loads((
            ROOT / "tools/ultimate_concrete_wave0_011_resume.json").read_text())
        self.assertEqual(
            RESUME.concrete.normalized_record(
                RESUME.concrete.wave_inventory(0)[11]), manifest["record"])
        self.assertLess(
            manifest["planes"]["predecessors"]["allocated_bytes"],
            manifest["planes"]["predecessors"]["bytes"])
        service = (ROOT / "tools/ultimatefish-concrete-wave0-011-"
                   "ac9b2d32-resume-v1.service").read_text()
        self.assertIn("Requires=mnt-ultimatefish\\x2dresume.mount", service)
        self.assertIn("--minimum-free-bytes 214748364800", service)
        self.assertIn("--scratch-limit 85899345920", service)
        self.assertIn("--reverse-edge-bytes-limit 68719476736", service)
        self.assertIn("Restart=no", service)
        self.assertIn(
            "ReadOnlyPaths=" + manifest["source_work_directory"], service)

    def test_class001_resume_is_exact_and_has_large_reverse_gate(self) -> None:
        manifest = json.loads((
            ROOT / "tools/ultimate_concrete_wave0_001_resume.json").read_text())
        self.assertEqual(
            RESUME.concrete.normalized_record(
                RESUME.concrete.wave_inventory(0)[1]), manifest["record"])
        self.assertLess(
            manifest["planes"]["predecessors"]["allocated_bytes"],
            manifest["planes"]["predecessors"]["bytes"])
        service = (ROOT / "tools/ultimatefish-concrete-wave0-001-"
                   "ac9b2d32-resume-v1.service").read_text()
        self.assertIn("Requires=mnt-ultimatefish\\x2dresume.mount", service)
        self.assertIn("--minimum-free-bytes 214748364800", service)
        self.assertIn("--scratch-limit 85899345920", service)
        self.assertIn("--reverse-edge-bytes-limit 68719476736", service)
        self.assertIn("Restart=no", service)
        self.assertIn(
            "ReadOnlyPaths=" + manifest["source_work_directory"], service)


if __name__ == "__main__":
    unittest.main()
