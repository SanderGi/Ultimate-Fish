#!/usr/bin/env python3

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import stat
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


audit = load("jg_audit", ROOT / "tools/certify_ultimate_jester_ghost_verification_aws.py")
verify = load("jg_verify", ROOT / "tools/verify_ultimate_jester_ghost_rebound_aws.py")


def sha(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def portions(total: int) -> list[int]:
    base, remainder = divmod(total, 80)
    return [base + (index < remainder) for index in range(80)]


class CertificationTest(unittest.TestCase):
    def fixture(self, root: Path) -> argparse.Namespace:
        source, model, observation = sha(b"source"), sha(b"model"), sha(b"observation")
        binary = root / "binary"
        binary.write_bytes(b"binary")
        binary.chmod(binary.stat().st_mode | stat.S_IXUSR)
        binary_sha = audit.sha256_file(binary)
        tools = root / "tools"
        tools.mkdir()
        verifier = tools / "verify_ultimate_jester_ghost_rebound_aws.py"
        verifier.write_bytes((
            ROOT / "tools/verify_ultimate_jester_ghost_rebound_aws.py").read_bytes())
        verifier_sha = audit.sha256_file(verifier)
        audit_source = tools / "certify_ultimate_jester_ghost_verification_aws.py"
        audit_source.write_bytes((
            ROOT / "tools/certify_ultimate_jester_ghost_verification_aws.py").read_bytes())
        audit_sha = audit.sha256_file(audit_source)
        tree = root / "transitions"
        state = root / "state"
        (state / "completed").mkdir(parents=True)
        (state / "logs").mkdir()
        (state / "service.log").write_text("service complete\n")
        tree.mkdir()
        raw_parts = portions(verify.RAW_DOMAIN)
        canonical_parts = portions(verify.EXPECTED_CANONICAL)
        world_parts = portions(verify.EXPECTED_WORLDS)
        edge_parts = portions(verify.EXPECTED_EDGES)
        ranges, records, marker_digests, certificates = [], [], {}, []
        cursor = 0
        for index in range(80):
            name = f"shard-{index:02d}"
            raw = raw_parts[index]
            canonical = canonical_parts[index]
            worlds = world_parts[index]
            edges = edge_parts[index]
            header_payload = f"header-{name}".encode()
            native_marker_payload = f"marker-{name}".encode()
            (tree / f"{name}.header").write_bytes(header_payload)
            (tree / f"{name}.verified").write_bytes(native_marker_payload)
            record = {
                "prefix": name, "raw_begin": cursor, "raw_count": raw,
                "complete": False, "geometries": canonical,
                "worlds": worlds, "edges": edges,
                "payload_sha256": sha(f"payload-{name}".encode()),
                "header_sha256": sha(header_payload),
                "marker_sha256": sha(native_marker_payload),
            }
            certificate = {
                "raw": raw, "canonical": canonical, "worlds": worlds,
                "live": worlds, "actions": worlds, "observations": worlds,
                "edges": edges, "codec_checks": 4 * (worlds + canonical),
                "action_checks": 4 * worlds,
                "decision_checks": 4 * canonical,
                "transition_checks": 4 * edges,
                "symmetry_checks": 4 * canonical,
                "codec_residual": 0, "action_residual": 0,
                "decision_residual": 0, "transition_residual": 0,
                "symmetry_residual": 0, "payload": record["payload_sha256"],
            }
            line = (
                "jester_ghost_transition " + " ".join(
                    f"{field} {certificate[field]}" for field in (
                        "raw", "canonical", "worlds", "live", "actions",
                        "observations", "edges", "codec_checks",
                        "action_checks", "decision_checks", "transition_checks",
                        "symmetry_checks", "codec_residual", "action_residual",
                        "decision_residual", "transition_residual",
                        "symmetry_residual")) +
                f" payload_sha256 {certificate['payload']}\n")
            log = state / "logs" / f"{name}.log"
            log.write_text(line)
            marker = {
                **verify.marker_binding(
                    record, binary_sha, source, model, observation),
                "status": "exhaustive-native-regeneration-certified",
                "certificate": certificate, "log": f"logs/{name}.log",
                "log_sha256": audit.sha256_file(log),
            }
            marker_path = state / "completed" / f"{name}.json"
            marker_path.write_text(json.dumps(marker, indent=2, sort_keys=True) + "\n")
            marker_digests[name] = audit.sha256_file(marker_path)
            ranges.append([name, cursor, raw])
            records.append(record)
            certificates.append(certificate)
            cursor += raw
        records.append({
            "prefix": "kjesterghostk", "raw_begin": 0,
            "raw_count": verify.RAW_DOMAIN, "complete": True,
            "geometries": verify.EXPECTED_CANONICAL,
            "worlds": verify.EXPECTED_WORLDS, "edges": verify.EXPECTED_EDGES,
            "payload_sha256": sha(b"merged-payload"),
            "header_sha256": sha(b"merged-header"),
            "marker_sha256": sha(b"merged-marker"),
        })
        bundle = {
            "schema": verify.BUNDLE_SCHEMA, "raw_geometries": verify.RAW_DOMAIN,
            "merge_inputs": 80, "parallelism": 29,
            "source_sha256": source, "model_sha256": model,
            "observation_sha256": observation, "merge_ranges": ranges,
        }
        bundle_path = root / "bundle.json"
        bundle_path.write_text(json.dumps(bundle))
        rebind = {
            "schema": verify.REBIND_SCHEMA, "status": verify.REBIND_STATUS,
            "source_sha256": source, "new_model_sha256": model,
            "observation_sha256": observation,
            "destination_tree": str(tree), "records": records,
        }
        rebind_path = root / "rebind.json"
        rebind_path.write_text(json.dumps(rebind))
        totals = {field: sum(item[field] for item in certificates)
                  for field in ("raw", "canonical", "worlds", "live", "actions",
                                "observations", "edges", "codec_checks",
                                "action_checks", "decision_checks",
                                "transition_checks", "symmetry_checks")}
        summary = {
            "schema": verify.SCHEMA,
            "status": "all-80-rebound-shards-exhaustively-regenerated",
            "shards": 80, "parallelism": 29,
            "binary_sha256": binary_sha, "source_sha256": source,
            "model_sha256": model, "observation_sha256": observation,
            "rebind_manifest_sha256": audit.sha256_file(rebind_path),
            "totals": totals, "completion_marker_sha256": marker_digests,
        }
        (state / "verification-summary.json").write_text(json.dumps(summary))
        return argparse.Namespace(
            root=root, audit_source=audit_source, audit_sha256=audit_sha,
            verifier=verifier, verifier_sha256=verifier_sha,
            bundle_manifest=bundle_path, rebind_manifest=rebind_path,
            transition_tree=tree, binary=binary, binary_sha256=binary_sha,
            state_dir=state, certificate_output=root / "cert/certificate.json",
            artifact_manifest=root / "cert/artifacts.json")

    def test_exact_evidence_is_certified_and_inventoried(self):
        with tempfile.TemporaryDirectory() as directory:
            args = self.fixture(Path(directory))
            result = audit.certify(args)
            self.assertEqual(audit.STATUS, result["status"])
            self.assertEqual(80, result["shards"])
            manifest = json.loads(args.artifact_manifest.read_text())
            self.assertEqual(328, len(manifest["artifacts"]))

    def test_corrupt_log_fails_before_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            args = self.fixture(Path(directory))
            (args.state_dir / "logs/shard-00.log").write_text("corrupt\n")
            with self.assertRaisesRegex(ValueError, "log SHA-256"):
                audit.certify(args)
            self.assertFalse(args.certificate_output.exists())
            self.assertFalse(args.artifact_manifest.exists())


if __name__ == "__main__":
    unittest.main()
