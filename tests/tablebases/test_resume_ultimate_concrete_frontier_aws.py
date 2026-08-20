#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "frontier_resume",
    ROOT / "tools/tablebases/resume_ultimate_concrete_frontier_aws.py")
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

    def test_transport_manifest_authenticates_only_checkpoint_planes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            document = self.fixture(Path(temporary))
            document["discarded_reverse_graph"] = {
                name: {
                    key: value for key, value in document["planes"][name].items()
                    if key != "sha256"
                }
                for name in ("offsets", "predecessors")
            }
            for name in ("offsets", "predecessors"):
                path = (Path(document["source_work_directory"]) /
                        document["planes"][name]["relative_path"])
                path.unlink()
                del document["planes"][name]
            with mock.patch.object(
                    RESUME.concrete, "generator_model_sha256",
                    return_value=document["generator_model_sha256"]):
                result = RESUME.authenticate_manifest(document)
            self.assertEqual(
                {"nodes", "degrees"},
                set(result["files"]).intersection({
                    "nodes", "degrees", "offsets", "predecessors"}))
            self.assertIn("retrograde-not-started", result["frontier_status"])

    def test_transport_manifest_rejects_complete_reverse_graph(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            document = self.fixture(Path(temporary))
            document["discarded_reverse_graph"] = {
                name: {
                    key: value for key, value in document["planes"][name].items()
                    if key != "sha256"
                }
                for name in ("offsets", "predecessors")
            }
            document["discarded_reverse_graph"]["predecessors"][
                "allocated_bytes"] = document[
                    "discarded_reverse_graph"]["predecessors"]["bytes"]
            for name in ("offsets", "predecessors"):
                del document["planes"][name]
            with mock.patch.object(
                    RESUME.concrete, "generator_model_sha256",
                    return_value=document["generator_model_sha256"]):
                with self.assertRaisesRegex(RuntimeError, "not proven incomplete"):
                    RESUME.authenticate_manifest(document)

    def test_transport_manifest_accepts_authenticated_incomplete_marker(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            document = self.fixture(Path(temporary))
            log = (Path(document["source_work_directory"]) /
                   document["generator_log"]["relative_path"])
            log.write_bytes(log.read_bytes() + b"reverse states 1/2 elapsed 3s\n")
            document["generator_log"].update({
                "bytes": log.stat().st_size,
                "sha256": RESUME.sha256_path(log),
            })
            document["last_reverse_marker"] = "reverse states 1/2 elapsed 3s"
            document["discarded_reverse_graph"] = {
                name: {
                    key: value for key, value in document["planes"][name].items()
                    if key != "sha256"
                }
                for name in ("offsets", "predecessors")
            }
            document["discarded_reverse_graph"]["predecessors"][
                "allocated_bytes"] = document[
                    "discarded_reverse_graph"]["predecessors"]["bytes"]
            for name in ("offsets", "predecessors"):
                del document["planes"][name]
            with mock.patch.object(
                    RESUME.concrete, "generator_model_sha256",
                    return_value=document["generator_model_sha256"]):
                result = RESUME.authenticate_manifest(document)
            self.assertIn("reverse-incomplete", result["frontier_status"])

    def test_transport_manifest_accepts_later_reverse_plane_mtimes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            document = self.fixture(Path(temporary))
            source = Path(document["source_work_directory"])
            frontier_time = 1_000_000_000
            reverse_times = {
                "offsets": 2_000_000_000,
                "predecessors": 3_000_000_000,
            }
            for name in ("nodes", "degrees"):
                path = source / document["planes"][name]["relative_path"]
                os.utime(path, ns=(frontier_time, frontier_time))
            for name, timestamp in reverse_times.items():
                path = source / document["planes"][name]["relative_path"]
                os.utime(path, ns=(timestamp, timestamp))
            document["discarded_reverse_graph"] = {
                name: {
                    key: value for key, value in document["planes"][name].items()
                    if key != "sha256"
                }
                for name in ("offsets", "predecessors")
            }
            document["discarded_reverse_graph"]["predecessors"][
                "allocated_bytes"] = document[
                    "discarded_reverse_graph"]["predecessors"]["bytes"]
            for name in ("offsets", "predecessors"):
                del document["planes"][name]
            document["reverse_phase_evidence"] = {
                "schema": "ultimate-reverse-phase-mtime-v1",
                "frontier_planes_max_mtime_ns": frontier_time,
                "reverse_plane_mtime_ns": reverse_times,
            }
            with mock.patch.object(
                    RESUME.concrete, "generator_model_sha256",
                    return_value=document["generator_model_sha256"]):
                result = RESUME.authenticate_manifest(document)
            self.assertIn("complete-frontier", result["frontier_status"])

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
        self.assertFalse(args.preserve_completed)
        self.assertFalse(args.preserve_completed_artifacts_only)
        self.assertFalse(args.local_only)
        self.assertIsNone(args.work_directory)
        self.assertEqual(args.workers, 1)
        self.assertEqual(
            RESUME.parse_args(["--workers", "7"]).workers,
            7,
        )
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                RESUME.parse_args(["--workers", "9"])

    def test_replacement_execution_requires_complete_equivalence_binding(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = self.fixture(root)
            args = RESUME.parse_args([])
            args.execution_bundle_root = root / "replacement"
            with self.assertRaisesRegex(RuntimeError, "requires every"):
                RESUME.execution_context(args, document)

    def test_replacement_execution_authenticates_both_models_and_binary(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = self.fixture(root)
            bundle = root / "replacement"
            bundle.mkdir()
            binary = root / "ultimate_tablebase"
            binary.write_bytes(b"parallel-resume-binary")
            binary_sha = RESUME.sha256_path(binary)
            model_sha = "c" * 64
            inventory_sha = "d" * 64
            certificate = root / "equivalence.json"
            certificate.write_text(json.dumps({
                "schema": RESUME.EXECUTION_EQUIVALENCE_SCHEMA,
                "status": "frontier-semantics-byte-identical-verified",
                "frontier_model_sha256": document["generator_model_sha256"],
                "execution_model_sha256": model_sha,
                "frontier_inventory_sha256": document["inventory_sha256"],
                "execution_inventory_sha256": inventory_sha,
                "execution_binary_sha256": binary_sha,
                "output_residual": 0,
                "bellman_residual": 0,
            }))
            args = RESUME.parse_args([
                "--execution-bundle-root", str(bundle),
                "--execution-binary", str(binary),
                "--execution-model-sha256", model_sha,
                "--execution-inventory-sha256", inventory_sha,
                "--execution-binary-sha256", binary_sha,
                "--execution-equivalence-certificate", str(certificate),
                "--execution-equivalence-sha256",
                RESUME.sha256_path(certificate),
            ])
            with mock.patch.object(
                    RESUME.concrete, "generator_model_sha256",
                    return_value=model_sha), mock.patch.object(
                    RESUME.concrete, "inventory_sha256",
                    return_value=inventory_sha):
                execution = RESUME.execution_context(args, document)
            self.assertEqual(model_sha, execution["model_sha256"])
            self.assertEqual(
                document["generator_model_sha256"],
                execution["frontier_model_sha256"])
            self.assertEqual(binary_sha, execution["binary_sha256"])
            self.assertEqual(inventory_sha, execution["inventory_sha256"])

    def test_completed_local_resume_is_preserved_without_frontier_reread(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = self.fixture(root)
            source = Path(document["source_work_directory"])
            work = root / "completed"
            record = document["record"]
            stem = Path(record["filename"]).stem
            manifest = root / "resume.json"
            manifest.write_text(json.dumps(document))
            manifest_sha = RESUME.sha256_path(manifest)
            binary = work / "binary/ultimate_tablebase"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"binary")
            output = work / "outputs" / record["filename"]
            output.parent.mkdir()
            output.write_bytes(b"verified table")
            log = work / "logs/generate" / f"{stem}.log"
            log.parent.mkdir(parents=True)
            log.write_text("complete proof")
            dependency = source / "dependencies/manifest.json"
            dependency.parent.mkdir(exist_ok=True)
            dependency.write_bytes(b"dependency")
            verification = {"bytes": len(b"verified table"),
                            "sha256": RESUME.sha256_path(output),
                            "verification_residual": 0}
            result = {
                "schema": RESUME.RESULT_SCHEMA,
                "record": record,
                "generator_model_sha256": document["generator_model_sha256"],
                "inventory_sha256": document["inventory_sha256"],
                "resume_manifest_sha256": manifest_sha,
                "bellman_verification_residual": 0,
                "output": verification,
            }
            result_path = work / "results" / f"{stem}.json"
            result_path.parent.mkdir()
            result_path.write_text(json.dumps(result))
            (work / "resume-plan.json").write_text(json.dumps({
                "manifest_sha256": manifest_sha, "record": record,
                "status": "authenticated-native-checkpoint-composed",
            }))
            archive = work / "archives" / f"{stem}-archive.tar.zst"

            archived_files = {}

            def make_archive(_directory, _stem, files, _schema):
                archived_files.update(files)
                archive.parent.mkdir(parents=True)
                archive.write_bytes(b"archive")
                return archive, "c" * 64

            args = RESUME.parse_args([
                "--preserve-completed-artifacts-only",
                "--manifest", str(manifest),
                "--work-directory", str(work), "--aws-execution-ack", "EC2",
                "--s3-prefix", "s3://example/results",
            ])
            with mock.patch.object(RESUME.sys, "platform", "linux"), \
                    mock.patch.object(
                        RESUME.concrete, "generator_model_sha256",
                        return_value=document["generator_model_sha256"]), \
                    mock.patch.object(
                        RESUME.concrete, "parse_uftb",
                        return_value=verification), \
                    mock.patch.object(
                        RESUME.preservation, "content_address_archive",
                        side_effect=make_archive), \
                    mock.patch.object(
                        RESUME.preservation, "restore_zstd_archive",
                        return_value={
                            f"tablebases/{record['filename']}": output,
                            f"proof/{log.name}": log,
                        }), \
                    mock.patch.object(
                        RESUME.preservation, "upload_head_download_verify",
                        return_value={"version_id": "version"}), \
                    mock.patch.object(
                        RESUME, "authenticate_manifest") as authenticate:
                preserved = RESUME.preserve_completed(
                    args, document, artifacts_only=True)
            authenticate.assert_not_called()
            self.assertNotIn("binary/ultimate_tablebase", archived_files)
            self.assertFalse(any(
                path.startswith("sources/") for path in archived_files))
            self.assertFalse(preserved["generator_rerun"])
            self.assertFalse(preserved["preserved_frontier_reread"])
            self.assertTrue(
                (work / "certificates/wave-certificate.json").is_file())

    def test_local_only_full_gate_does_not_require_s3(self) -> None:
        args = RESUME.parse_args([
            "--full", "--local-only", "--aws-execution-ack", "EC2",
            "--resident-limit", str(16 << 30),
            "--scratch-limit", str(1 << 40),
            "--reverse-edge-bytes-limit", str(1 << 40),
            "--minimum-free-bytes", str(1 << 30),
            "--minimum-host-memory-available-bytes", str(24 << 30),
        ])
        document = {
            "planes": {"nodes": {"bytes": 1}, "degrees": {"bytes": 1}},
            "discarded_reverse_graph": {
                "offsets": {"bytes": 1}, "predecessors": {"bytes": 1}},
            "record": {"packed_bytes": 1},
        }
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
                RESUME.sys, "platform", "linux"), mock.patch.object(
                RESUME, "host_memory_available_bytes", return_value=24 << 30), \
                mock.patch.object(RESUME.shutil, "disk_usage") as disk_usage:
            disk_usage.return_value.free = 2 << 40
            RESUME.validate_full_gates(
                args, Path(temporary) / "work", document)

    def test_full_gate_probes_existing_ancestor_of_new_campaign_root(self) -> None:
        args = RESUME.parse_args([
            "--full", "--local-only", "--aws-execution-ack", "EC2",
            "--resident-limit", str(16 << 30),
            "--scratch-limit", str(1 << 40),
            "--reverse-edge-bytes-limit", str(1 << 40),
            "--minimum-free-bytes", str(1 << 30),
            "--minimum-host-memory-available-bytes", str(24 << 30),
        ])
        document = {
            "planes": {"nodes": {"bytes": 1}, "degrees": {"bytes": 1}},
            "discarded_reverse_graph": {
                "offsets": {"bytes": 1}, "predecessors": {"bytes": 1}},
            "record": {"packed_bytes": 1},
        }
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
                RESUME.sys, "platform", "linux"), mock.patch.object(
                RESUME, "host_memory_available_bytes", return_value=24 << 30), \
                mock.patch.object(RESUME.shutil, "disk_usage") as disk_usage:
            disk_usage.return_value.free = 2 << 40
            root = Path(temporary)
            RESUME.validate_full_gates(
                args, root / "new-campaign/class", document)
            disk_usage.assert_called_once_with(root)

    def test_full_gate_reserves_host_memory_beyond_resident_limit(self) -> None:
        args = RESUME.parse_args([
            "--resident-limit", str(40 << 30),
            "--scratch-limit", str(1 << 40),
            "--reverse-edge-bytes-limit", str(1 << 40),
            "--minimum-free-bytes", str(1 << 30),
            "--minimum-host-memory-available-bytes", str(48 << 30),
            "--aws-execution-ack", "EC2",
            "--monitor-interval", "5", "--s3-prefix", "s3://example",
        ])
        document = {
            "planes": {
                "nodes": {"bytes": 1}, "degrees": {"bytes": 1},
            },
            "discarded_reverse_graph": {
                "offsets": {"bytes": 1}, "predecessors": {"bytes": 1},
            },
            "record": {"packed_bytes": 1},
        }
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
                RESUME.sys, "platform", "linux"), mock.patch.object(
                RESUME, "host_memory_available_bytes",
                return_value=(48 << 30) - 1):
            with self.assertRaisesRegex(RuntimeError, "memory headroom"):
                RESUME.validate_full_gates(args, Path(temporary) / "work",
                                           document)
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
                RESUME.sys, "platform", "linux"), mock.patch.object(
                RESUME, "host_memory_available_bytes", return_value=48 << 30), \
                mock.patch.object(RESUME.shutil, "disk_usage") as disk_usage:
            disk_usage.return_value.free = 2 << 40
            result = RESUME.validate_full_gates(
                args, Path(temporary) / "work", document)
            self.assertEqual(48 << 30,
                             result["host_memory_available_bytes"])

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
            ROOT / "tools/tablebases/ultimate_concrete_wave0_003_resume.json").read_text())
        self.assertEqual(
            RESUME.concrete.normalized_record(
                RESUME.concrete.wave_inventory(0)[3]), manifest["record"])
        self.assertEqual(
            "/mnt/ultimatefish-overflow/concrete-penguin-1083b6f8/"
            "wave0-003-full-v3", manifest["source_work_directory"])
        self.assertLess(
            manifest["planes"]["predecessors"]["allocated_bytes"],
            manifest["planes"]["predecessors"]["bytes"])
        service = (ROOT / "tools/tablebases/ultimatefish-concrete-wave0-003-"
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
            ROOT / "tools/tablebases/ultimate_concrete_wave0_006_resume.json").read_text())
        self.assertEqual(
            RESUME.concrete.normalized_record(
                RESUME.concrete.wave_inventory(0)[6]), manifest["record"])
        self.assertLess(
            manifest["planes"]["predecessors"]["allocated_bytes"],
            manifest["planes"]["predecessors"]["bytes"])
        service = (ROOT / "tools/tablebases/ultimatefish-concrete-wave0-006-"
                   "ac9b2d32-resume-v1.service").read_text()
        self.assertIn("Requires=mnt-ultimatefish\\x2dresume.mount", service)
        self.assertIn("--minimum-free-bytes 214748364800", service)
        self.assertIn("--reverse-edge-bytes-limit 34359738368", service)
        self.assertIn("Restart=no", service)
        mount = (ROOT / "tools/tablebases/mnt-ultimatefish\\x2dresume.mount").read_text()
        self.assertIn(
            "What=/dev/disk/by-uuid/d71ca950-588b-4ff8-9160-e9b7d2d78fe2",
            mount)
        self.assertIn("Where=/mnt/ultimatefish-resume", mount)

    def test_class011_resume_is_exact_and_has_large_reverse_gate(self) -> None:
        manifest = json.loads((
            ROOT / "tools/tablebases/ultimate_concrete_wave0_011_resume.json").read_text())
        self.assertEqual(
            RESUME.concrete.normalized_record(
                RESUME.concrete.wave_inventory(0)[11]), manifest["record"])
        self.assertLess(
            manifest["planes"]["predecessors"]["allocated_bytes"],
            manifest["planes"]["predecessors"]["bytes"])
        service = (ROOT / "tools/tablebases/ultimatefish-concrete-wave0-011-"
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
            ROOT / "tools/tablebases/ultimate_concrete_wave0_001_resume.json").read_text())
        self.assertEqual(
            RESUME.concrete.normalized_record(
                RESUME.concrete.wave_inventory(0)[1]), manifest["record"])
        self.assertLess(
            manifest["planes"]["predecessors"]["allocated_bytes"],
            manifest["planes"]["predecessors"]["bytes"])
        service = (ROOT / "tools/tablebases/ultimatefish-concrete-wave0-001-"
                   "ac9b2d32-resume-v1.service").read_text()
        self.assertIn("Requires=mnt-ultimatefish\\x2dresume.mount", service)
        self.assertIn("--minimum-free-bytes 214748364800", service)
        self.assertIn("--scratch-limit 85899345920", service)
        self.assertIn("--reverse-edge-bytes-limit 68719476736", service)
        self.assertIn("Restart=no", service)
        self.assertIn(
            "ReadOnlyPaths=" + manifest["source_work_directory"], service)

    def test_class005_resume_reuses_complete_berserker_ghost_frontier(self) -> None:
        manifest = json.loads((
            ROOT / "tools/tablebases/ultimate_concrete_wave0_005_resume.json").read_text())
        self.assertEqual(
            RESUME.concrete.normalized_record(
                RESUME.concrete.wave_inventory(0)[5]), manifest["record"])
        self.assertEqual(20, manifest["substates"])
        reverse_done, reverse_total = map(
            int, manifest["last_reverse_marker"].split()[2].split("/"))
        self.assertLess(reverse_done, reverse_total)
        service = (ROOT / "tools/tablebases/ultimatefish-resume-berserker-ghost-"
                   "same-local-v1.service").read_text()
        self.assertIn("--resident-limit 60129542144", service)
        self.assertIn("--scratch-limit 161061273600", service)
        self.assertIn("--reverse-edge-bytes-limit 118111600640", service)
        self.assertIn("AllowedCPUs=31", service)
        self.assertIn(
            "ReadOnlyPaths=" + manifest["source_work_directory"], service)

    def test_class023_transport_resume_targets_high_memory_i03(self) -> None:
        manifest = json.loads((
            ROOT / "tools/tablebases/ultimate_concrete_wave0_023_resume_i03.json").read_text())
        self.assertEqual(
            RESUME.concrete.normalized_record(
                RESUME.concrete.wave_inventory(0)[23]), manifest["record"])
        self.assertEqual(10, manifest["substates"])
        self.assertEqual(
            "/mnt/ultimatefish/transport-parasite-same-a3dcf230/source",
            manifest["source_work_directory"])
        service = (ROOT / "tools/tablebases/ultimatefish-resume-parasite-same-"
                   "i03-v1.service").read_text()
        self.assertIn("--resident-limit 51539607552", service)
        self.assertIn("--scratch-limit 85899345920", service)
        self.assertIn("--reverse-edge-bytes-limit 68719476736", service)
        self.assertIn("AllowedCPUs=30", service)
        self.assertIn(
            "ReadOnlyPaths=" + manifest["source_work_directory"], service)


if __name__ == "__main__":
    unittest.main()
