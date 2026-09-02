#!/usr/bin/env python3
"""Tests for the fail-closed Penguin parallel candidate build."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class PenguinParallelBuildTest(unittest.TestCase):
    def test_build_contract_is_ghost_primary_and_immutable(self) -> None:
        text = (ROOT / "tools/tablebases/"
                "build_ultimate_penguin_parallel_v22_aws.py").read_text()
        self.assertIn('build_base.PRIMARY_DEFINE = ""', text)
        self.assertIn('build_base.PIECES["penguin"] = "Penguin"', text)
        self.assertIn(
            "bcfda23c2a00580c1e4ecd08faf70447a1ae8ffe144f95bc36a074ecefbe562e",
            text)
        self.assertIn("ULTIMATE_GHOST_EXTRA_SUBSTATES=8", text)
        self.assertIn("ULTIMATE_GHOST_ORDINARY_LOWER_SUBSTATES=4", text)
        self.assertIn("parallel-v23-candidate", text)

    def test_fresh_certifier_preserves_serial_run_and_binds_sources(self) -> None:
        wrapper = (ROOT / "tools/tablebases/"
                   "ultimatefish-certify-kghostkpenguin-parallel-v24.sh").read_text()
        service = (ROOT / "tools/tablebases/"
                   "ultimatefish-info-kghostkpenguin-parallel-v24.service").read_text()
        self.assertNotIn("systemctl stop", wrapper)
        self.assertIn("--parallelism 48 --solve-workers 32", wrapper)
        self.assertIn("--prebuilt-model-sha256", wrapper)
        self.assertIn(
            "c073b92a1243ad3027b85bf539528f115b64a1b9fc9a72cc3c9dd8d2ed2cf0b9",
            wrapper)
        self.assertIn("AllowedCPUs=16-63", service)
        self.assertIn("MemoryMax=429496729600", service)

    def test_worker_options_rebuild_has_immutable_two_step_lineage(self) -> None:
        cli_package = (ROOT / "tools/tablebases/"
                       "package_ultimate_penguin_worker_cli_v27.py").read_text()
        options_package = (ROOT / "tools/tablebases/"
                           "package_ultimate_penguin_worker_options_v28.py").read_text()
        build = (ROOT / "tools/tablebases/"
                 "build_ultimate_penguin_parallel_v28_aws.py").read_text()
        self.assertIn("--workers", cli_package)
        self.assertIn("resumeConverged", cli_package)
        self.assertIn(
            "eb8138a84cdf58a292f1eec2639c0d786500e70e6f4ed43297fab536c5373805",
            options_package)
        self.assertIn("std::uint32_t workers = 1", options_package)
        self.assertIn(
            "2f48ca98b640546617aed314e7c7d4da34333dd904bafeddefbd6e6477c0c19c",
            build)
        self.assertIn('build_base.GENERATION = "v28"', build)

    def test_solve_only_resume_reuses_only_fully_authenticated_graph(self) -> None:
        wrapper = (ROOT / "tools/tablebases/"
                   "ultimatefish-resume-kghostkpenguin-worker-options-v29.sh"
                   ).read_text()
        self.assertIn("--solve-existing", wrapper)
        self.assertIn("--solve-workers 32", wrapper)
        self.assertIn("shard_regeneration_certificates 64", wrapper)
        self.assertIn("gap_residual 0", wrapper)
        self.assertIn("conservation_residual 0", wrapper)
        self.assertIn("states 607326720 remap_residual 0 count_residual 0", wrapper)
        self.assertNotIn("--resume-transitions", wrapper)


if __name__ == "__main__":
    unittest.main()
