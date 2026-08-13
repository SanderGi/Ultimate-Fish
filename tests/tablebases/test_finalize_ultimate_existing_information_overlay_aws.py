import hashlib
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools/tablebases"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "finalize_ultimate_existing_information_overlay_aws",
    TOOLS / "finalize_ultimate_existing_information_overlay_aws.py")
assert SPEC and SPEC.loader
finalizer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(finalizer)


class ExistingInformationArbitraryTests(unittest.TestCase):
    def test_accepts_zero_residual_reciprocal_fixed_point_proof(self):
        proof = "\n".join([
            "reciprocal_ghost_extra_iteration 15 bdd_nodes 200000000 "
            "changed_owner 12 changed_observer 3 changed_visible 1 "
            "peak_rss_bytes 7883100160",
            "reciprocal_ghost_extra_iteration 16 bdd_nodes 253219271 "
            "changed_owner 0 changed_observer 0 changed_visible 0 "
            "peak_rss_bytes 7883100160",
            "information_summary side 0 win 0 loss 16770 draw 29208176 "
            "unreachable_win 7430290 unreachable_loss 229350 "
            "unreachable_draw 1073334 sets 8599404 concrete 37957920 "
            "bellman_residual 0 rank_residual 0 belief_cap none exhaustive 1",
            "information_summary side 1 win 6505412 loss 0 draw 26749656 "
            "unreachable_win 4702852 unreachable_loss 0 unreachable_draw 0 "
            "sets 8797464 concrete 37957920 bellman_residual 0 "
            "rank_residual 0 belief_cap none exhaustive 1",
            "reciprocal_bishop_ghost_certificate dual_force_residual 0 "
            "structural_residual 0 singleton_residual 0 "
            "transition_payload_sha256 " + "a" * 64 + " arbitrary_sha256 " +
            "b" * 64,
        ])
        summaries = finalizer.exact_proof_summaries(proof)
        self.assertEqual(16770, summaries[0]["loss"])
        self.assertEqual(6505412, summaries[1]["win"])

    def test_rejects_reciprocal_iteration_without_final_certificate(self):
        proof = "\n".join([
            "reciprocal_ghost_extra_iteration 16 bdd_nodes 1 changed_owner 0 "
            "changed_observer 0 changed_visible 0 peak_rss_bytes 1",
            "information_summary side 0 win 0 loss 0 draw 1 unreachable_win 0 "
            "unreachable_loss 0 unreachable_draw 0 sets 1 concrete 1 "
            "bellman_residual 0 rank_residual 0 belief_cap none exhaustive 1",
            "information_summary side 1 win 0 loss 0 draw 1 unreachable_win 0 "
            "unreachable_loss 0 unreachable_draw 0 sets 1 concrete 1 "
            "bellman_residual 0 rank_residual 0 belief_cap none exhaustive 1",
        ])
        with self.assertRaisesRegex(ValueError, "lacks one exact fixed point"):
            finalizer.exact_proof_summaries(proof)

    def test_remote_output_does_not_stream_unbounded_proof(self):
        args = SimpleNamespace(
            filename="kknightghostk.uftb",
            s3_prefix="results/hidden/ordinary-ghost/v1",
            unit="ultimatefish-knight.service",
            arbitrary="/work/results/kknightghostk.ufgd",
            overlay="/work/results/kknightghostk.ufiw",
            proof_log="/work/logs/solve.log",
            source_table="/work/tablebases/kknightghostk.uftb",
            binary="/work/ultimate_ghost_ordinary_information_tablebase",
            source_bundle="/source/source.tar",
            bucket="versioned-bucket",
            region="us-west-2",
        )
        script = finalizer.remote_script(args)
        self.assertNotIn("__PROOF__", script)
        self.assertNotIn('cat "$proof"', script)
        self.assertIn("kknightghostk.proof.log", script)
        self.assertIn('zstd -T0 -19 -q -f -o "$archive"', script)

    def test_validates_restored_sidecar_payload_and_orientation(self):
        source = "a" * 64
        model = "b" * 64
        body = b"exact arbitrary payload"
        header = bytearray(1056)
        header[:8] = b"UFMG1\0\0\0"
        struct.pack_into("<I", header, 12, len(header))
        struct.pack_into("<I", header, 32, 1)
        struct.pack_into("<Q", header, 152, len(body))
        header[160:224] = source.encode()
        header[288:352] = model.encode()
        header[928:992] = hashlib.sha256(body).hexdigest().encode()
        semantics = b"fresh-maximal-public-view-v2:mage-ghost-generic"
        header[992:992 + len(semantics)] = semantics
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sidecar"
            path.write_bytes(header + body)
            finalizer.validate_arbitrary(path, source, model, True)
            with self.assertRaisesRegex(ValueError, "header binding"):
                finalizer.validate_arbitrary(path, source, model, False)
            payload = bytearray(path.read_bytes())
            payload[-1] ^= 1
            path.write_bytes(payload)
            with self.assertRaisesRegex(ValueError, "payload residual"):
                finalizer.validate_arbitrary(path, source, model, True)


if __name__ == "__main__":
    unittest.main()
