#!/usr/bin/env python3

import hashlib
import importlib.util
from pathlib import Path
import stat
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "verify_rebound",
    ROOT / "tools/verify_ultimate_jester_ghost_rebound_aws.py")
verify_rebound = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(verify_rebound)


def digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


class VerifyReboundTest(unittest.TestCase):
    def manifests(self, root: Path):
        source, model, observation = digest(b"source"), digest(b"model"), digest(b"observation")
        ranges = []
        records = []
        cursor = 0
        base, remainder = divmod(verify_rebound.RAW_DOMAIN, 80)
        for index in range(80):
            count = base + (index < remainder)
            name = f"shard-{index:02d}"
            ranges.append([name, cursor, count])
            records.append({
                "prefix": name, "raw_begin": cursor, "raw_count": count,
                "complete": False, "geometries": count, "worlds": count,
                "edges": count, "strata": 0,
                "payload_sha256": digest(name.encode()),
                "header_sha256": digest((name + "h").encode()),
                "marker_sha256": digest((name + "m").encode()),
            })
            cursor += count
        records.append({
            "prefix": "kjesterghostk", "raw_begin": 0,
            "raw_count": verify_rebound.RAW_DOMAIN, "complete": True,
            "geometries": verify_rebound.EXPECTED_CANONICAL,
            "worlds": verify_rebound.EXPECTED_WORLDS,
            "edges": verify_rebound.EXPECTED_EDGES,
        })
        tree = root / "transitions"
        bundle = {
            "schema": verify_rebound.BUNDLE_SCHEMA,
            "raw_geometries": verify_rebound.RAW_DOMAIN,
            "merge_inputs": 80, "parallelism": 29,
            "source_sha256": source, "model_sha256": model,
            "observation_sha256": observation, "merge_ranges": ranges,
        }
        rebind = {
            "schema": verify_rebound.REBIND_SCHEMA,
            "status": verify_rebound.REBIND_STATUS,
            "source_sha256": source, "new_model_sha256": model,
            "observation_sha256": observation,
            "destination_tree": str(tree), "records": records,
        }
        return bundle, rebind, tree

    def test_exact_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            bundle, rebind, _ = self.manifests(Path(directory))
            shards, merged = verify_rebound.inventory(bundle, rebind)
            self.assertEqual(len(shards), 80)
            self.assertEqual(sum(item["raw_count"] for item in shards),
                             verify_rebound.RAW_DOMAIN)
            self.assertTrue(merged["complete"])

    def test_inventory_rejects_gap(self):
        with tempfile.TemporaryDirectory() as directory:
            bundle, rebind, _ = self.manifests(Path(directory))
            bundle["merge_ranges"][1][1] += 1
            with self.assertRaisesRegex(ValueError, "contiguous"):
                verify_rebound.inventory(bundle, rebind)

    def test_native_certificate_rejects_residual(self):
        record = {
            "raw_count": 1, "geometries": 1, "worlds": 2, "edges": 3,
            "strata": 1, "payload_sha256": digest(b"payload"),
        }
        line = (
            "jester_ghost_transition raw 1 canonical 1 worlds 2 live 2 "
            "actions 1 observations 1 edges 3 codec_checks 12 action_checks 8 "
            "decision_checks 8 transition_checks 12 symmetry_checks 4 "
            "codec_residual 0 action_residual 0 decision_residual 0 "
            "transition_residual 1 symmetry_residual 0 payload_sha256 " +
            record["payload_sha256"])
        with self.assertRaisesRegex(ValueError, "transition_residual"):
            verify_rebound.parse_native_certificate(line, record)

    def test_completed_marker_is_resumable_and_log_authenticated(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "verifier"
            binary.write_text("#!/bin/sh\nexit 99\n")
            binary.chmod(binary.stat().st_mode | stat.S_IXUSR)
            record = {
                "prefix": "shard-00", "raw_begin": 0, "raw_count": 1,
                "header_sha256": digest(b"h"), "marker_sha256": digest(b"m"),
                "payload_sha256": digest(b"p"),
                "geometries": 1, "worlds": 2, "edges": 3, "strata": 1,
            }
            tree = root / "transitions"
            tree.mkdir()
            (tree / "shard-00.header").write_bytes(b"h")
            (tree / "shard-00.verified").write_bytes(b"m")
            state = root / "state"
            log = state / "logs" / "shard-00.log"
            log.parent.mkdir(parents=True)
            log.write_text(
                "jester_ghost_transition raw 1 canonical 1 worlds 2 live 2 "
                "actions 1 observations 1 edges 3 codec_checks 12 "
                "action_checks 8 decision_checks 8 transition_checks 12 "
                "symmetry_checks 4 codec_residual 0 action_residual 0 "
                "decision_residual 0 transition_residual 0 "
                "symmetry_residual 0 payload_sha256 " +
                record["payload_sha256"] + "\n")
            binary_sha = verify_rebound.sha256_file(binary)
            source, model, observation = digest(b"s"), digest(b"n"), digest(b"o")
            marker = {
                **verify_rebound.marker_binding(
                    record, binary_sha, source, model, observation),
                "status": "exhaustive-native-regeneration-certified",
                "certificate": verify_rebound.parse_native_certificate(
                    log.read_text(), record), "log": "logs/shard-00.log",
                "log_sha256": verify_rebound.sha256_file(log),
            }
            marker_path = state / "completed" / "shard-00.json"
            verify_rebound.write_json_atomic(marker_path, marker)
            self.assertEqual(
                verify_rebound.verify_one(binary, tree, state,
                                          record, binary_sha, source, model,
                                          observation), marker)


if __name__ == "__main__":
    unittest.main()
