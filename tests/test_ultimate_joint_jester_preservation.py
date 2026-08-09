import importlib.util
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def load(name: str, relative: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PACKAGE = load(
    "joint_jester_preservation_package",
    "tools/package_ultimate_joint_jester_preservation_aws.py",
)
RUNNER = load(
    "joint_jester_preservation_runner",
    "tools/run_ultimate_joint_jester_preservation_aws.py",
)


class JointJesterPreservationTest(unittest.TestCase):
    def test_bundle_is_deterministic_and_authenticated(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / "first.tar"
            second = root / "second.tar"
            first_manifest = PACKAGE.build_bundle(first)
            second_manifest = PACKAGE.build_bundle(second)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertEqual(PACKAGE.verify_bundle(first), PACKAGE.verify_bundle(second))
            self.assertEqual(
                first_manifest["capture_model_sha256"],
                second_manifest["capture_model_sha256"],
            )
            self.assertEqual(first_manifest["default_mode"], "measure-only")

    def test_compact_archive_round_trip_is_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            alpha = root / "alpha"
            beta = root / "beta"
            alpha.write_bytes(b"alpha" * 101)
            beta.write_bytes(bytes(range(256)))
            files = {"raw/a": alpha, "proof/b": beta}
            first = root / "first.tar.zst"
            second = root / "second.tar.zst"
            first_sha = RUNNER.write_zstd_archive(
                first, files, RUNNER.RAW_ARCHIVE_SCHEMA
            )
            second_sha = RUNNER.write_zstd_archive(
                second, files, RUNNER.RAW_ARCHIVE_SCHEMA
            )
            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertEqual(first_sha, second_sha)
            restored = RUNNER.restore_zstd_archive(
                first, root / "restore", RUNNER.RAW_ARCHIVE_SCHEMA
            )
            self.assertEqual(restored["raw/a"].read_bytes(), alpha.read_bytes())
            self.assertEqual(restored["proof/b"].read_bytes(), beta.read_bytes())

    def test_measurement_and_overlay_contracts_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "measure.log"
            log.write_text(
                "joint_capture_measurement nodes 1 white_variables 4 "
                "black_variables 4 white_tokens 7 black_tokens 8 "
                "scratch_bytes 9 resident_bytes 10 raw_bytes 11 "
                "sidecar_upper_bytes 12 solve_launched 0\n"
            )
            fields = RUNNER.parse_measurement(log)
            self.assertEqual(fields["resident_bytes"], 10)
            log.write_text(log.read_text().replace("solve_launched 0", "solve_launched 1"))
            with self.assertRaisesRegex(RuntimeError, "malformed"):
                RUNNER.parse_measurement(log)

            overlay = root / "lower.ufiw"
            overlay.write_bytes(
                b"UFIW2\0\0\0" + bytes(24)
                + bytes.fromhex("11" * 32).hex().encode()
                + bytes.fromhex("22" * 32).hex().encode()
            )
            self.assertEqual(
                RUNNER.overlay_binding(overlay), ("11" * 32, "22" * 32)
            )
            overlay.write_bytes(b"bad")
            with self.assertRaisesRegex(RuntimeError, "invalid UFIW2"):
                RUNNER.overlay_binding(overlay)

    def test_s3_keys_are_content_addressable_and_scoped(self) -> None:
        bucket, key, uri = RUNNER.s3_object(
            "s3://private-proof/prefix",
            "results/sha256/abcd/result.tar.zst",
        )
        self.assertEqual(bucket, "private-proof")
        self.assertEqual(key, "prefix/results/sha256/abcd/result.tar.zst")
        self.assertEqual(uri, "s3://" + bucket + "/" + key)
        with self.assertRaisesRegex(RuntimeError, "s3://"):
            RUNNER.s3_object("https://example.invalid", "object")


if __name__ == "__main__":
    unittest.main()
