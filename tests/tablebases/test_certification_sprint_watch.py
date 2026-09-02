#!/usr/bin/env python3
"""Static safety checks for the two-day certification preservation watcher."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/tablebases/ultimatefish-certification-sprint-watch-v1.sh"
PRESERVER = ROOT / "tools/tablebases/ultimatefish-preserve-information-result-v1.sh"
BERSERKER_AFTER_REBIND = (
    ROOT / "tools/tablebases/solve_ultimate_opposed_berserker_after_rebind_v17.sh")
BERSERKER_PATH = (
    ROOT / "tools/tablebases/ultimatefish-info-kberserkerkghost-after-rebind-v17.path")
I024_MANIFEST = (
    ROOT / "tools/tablebases/ultimatefish-certification-sprint-i024-v38.manifest")
I08_MANIFEST = ROOT / "tools/tablebases/certification_sprint_manifest_i08_v19.txt"
I0B_MANIFEST = (
    ROOT / "tools/tablebases/ultimatefish-certification-sprint-i0b-manifest-v2.txt")
I0B_HANDOFF = (
    ROOT / "tools/tablebases/ultimatefish-certification-sprint-i0b-cpu-handoff-v1.sh")
I0B_PHASE_HANDOFF = (
    ROOT / "tools/tablebases/ultimatefish-certification-sprint-i0b-phase-handoff-v2.sh")
I03_DRAGON_RACE = (
    ROOT / "tools/tablebases/ultimatefish-run-kghostkdragon-i03-race-v1.sh")
I03_DRAGON_SERVICE = (
    ROOT / "tools/tablebases/ultimatefish-info-kghostkdragon-i03-race-v1.service")
I03_MANIFEST = (
    ROOT / "tools/tablebases/ultimatefish-certification-sprint-i03-v6.manifest")


class CertificationSprintWatchTests(unittest.TestCase):
    def test_waits_for_non_active_unit_or_collected_unit_and_both_outputs(self) -> None:
        text = SCRIPT.read_text()
        self.assertIn('"${active}" != active', text)
        self.assertIn('"${active}" != activating', text)
        self.assertIn('"${active}" != reloading', text)
        self.assertIn('-z "${result}" || "${result}" == success', text)
        self.assertIn('systemd-run --collect may remove', text)
        self.assertIn('${result_stem}.ufiw', text)
        self.assertIn('${result_stem}.${arbitrary_extension}', text)
        self.assertIn('result_stem=${result_stem:-${stem}}', text)
        self.assertIn('arbitrary_extension=${arbitrary_extension:-ufgd}', text)

    def test_ghost_pair_profile_requires_ufgg_and_exact_result_hashes(self) -> None:
        text = PRESERVER.read_text()
        self.assertIn('ghost_pair)', text)
        self.assertIn('test "${arbitrary_extension}" = ufgg', text)
        self.assertIn("'^ghost_pair_domain_cache '", text)
        self.assertIn("'^ghost_pair_artifacts '", text)
        self.assertIn('fields.get("overlay_sha256") != digest(overlay)', text)
        self.assertIn('fields.get("arbitrary_sha256") != digest(arbitrary)', text)
        self.assertIn('header[:8] != b"UFGG1\\0\\0\\0"', text)
        self.assertIn('payload_bytes != cursor - 928', text)

    def test_never_overwrites_existing_preservation(self) -> None:
        text = SCRIPT.read_text()
        self.assertIn('if [[ ! -e "${preserve}" ]]', text)

    def test_preserver_accepts_only_inactive_success_or_fully_collected_unit(self) -> None:
        text = PRESERVER.read_text()
        self.assertIn('active=$(systemctl show', text)
        self.assertIn('result=$(systemctl show', text)
        self.assertIn('if [[ -n "${active}" || -n "${result}" ]]', text)
        self.assertIn('test "${active}" = inactive', text)
        self.assertIn('test "${result}" = success', text)

    def test_receipt_is_content_addressed_and_version_verified(self) -> None:
        text = SCRIPT.read_text()
        self.assertIn('/receipts/sha256/${receipt_sha}/', text)
        self.assertIn('--version-id "${receipt_version}"', text)
        self.assertIn('Metadata.sha256', text)
        self.assertIn('mv "${temporary}" "${receipt_put}"', text)

    def test_partial_preservation_is_quarantined_and_retried(self) -> None:
        text = SCRIPT.read_text()
        self.assertIn('quarantine_preservation', text)
        self.assertIn('missing-receipt', text)
        self.assertIn('invalid-receipt', text)
        self.assertIn('ultimate-information-preservation-receipt-v1', text)
        self.assertIn('artifact_hash_residual', text)

    def test_legacy_direct_result_layout_uses_relative_symlink(self) -> None:
        text = SCRIPT.read_text()
        self.assertIn('ln -s ../results "${root}/work/results"', text)

    def test_berserker_rebind_continuation_fails_closed(self) -> None:
        text = BERSERKER_AFTER_REBIND.read_text()
        self.assertIn(
            "berserker_ghost_opposed_rebind_v13 complete 1 residual 0", text)
        self.assertIn('sha256sum --check --strict "${rebind_manifest}"', text)
        self.assertIn('kberserkerkghost.pre-rebind-empty', text)
        self.assertIn('--workers 30', text)
        self.assertNotIn(
            "823ad1a14af45def9a1e41a3eec0040ec5d8797b51ba219c9505783c35ee90e0",
            text)

    def test_berserker_path_waits_for_rebind_manifest(self) -> None:
        text = BERSERKER_PATH.read_text()
        self.assertIn("kberserkerkghost.rebind-v13.sha256", text)
        self.assertIn(
            "Unit=ultimatefish-info-kberserkerkghost-after-rebind-v17.service",
            text)

    def test_current_preservation_manifests_bind_current_units(self) -> None:
        i024 = I024_MANIFEST.read_text()
        self.assertIn(
            "ultimatefish-info-kberserkerkghost-after-rebind-v17.service",
            i024)
        self.assertIn(
            "ultimatefish-info-kbombkghost-resume-v16.service", i024)
        self.assertIn(
            "ultimatefish-info-kghostsniperk-resume-v12.service", i024)
        self.assertNotIn(
            "ultimatefish-info-kbombkghost-resume-v9.service", i024)
        self.assertNotIn(
            "ultimatefish-info-kghostsniperk-resume-v11.service", i024)
        self.assertIn(
            "ultimatefish-info-kghostkprince-action-conditioned-v19.service",
            I08_MANIFEST.read_text())
        self.assertIn("kbombkghost-1.5b-v8", i024)
        self.assertIn("kghostkdragon-memory-v5", I0B_MANIFEST.read_text())

    def test_i0b_cpu_handoff_requires_both_terminal_outputs(self) -> None:
        text = I0B_HANDOFF.read_text()
        self.assertIn('[[ -s "${stem}.ufiw" && -s "${stem}.ufgd" ]]', text)
        self.assertIn('! systemctl is-active --quiet "${ninja_unit}"', text)
        self.assertIn('! systemctl is-active --quiet "${dragon_unit}"', text)
        self.assertIn('systemctl set-property --runtime', text)
        self.assertNotIn('systemctl start', text)

    def test_i0b_phase_handoff_never_overlaps_allocations(self) -> None:
        text = I0B_PHASE_HANDOFF.read_text()
        shrink_ninja = text.index(
            '"${ninja_unit}" AllowedCPUs=4-7')
        expand_dragon = text.index(
            '"${dragon_unit}" AllowedCPUs=0-3,8-31')
        shrink_dragon = text.index(
            '"${dragon_unit}" AllowedCPUs=0-3')
        expand_ninja = text.index(
            '"${ninja_unit}" AllowedCPUs=4-31')
        self.assertLess(shrink_ninja, expand_dragon)
        self.assertLess(shrink_dragon, expand_ninja)
        self.assertIn('trap restore_allocations EXIT INT TERM', text)
        self.assertIn('iteration 17 geometries 492960/492960', text)
        self.assertIn('iteration 18 geometries 5000/492960', text)

    def test_i03_dragon_race_is_fresh_exact_and_cpu_disjoint(self) -> None:
        runner = I03_DRAGON_RACE.read_text()
        service = I03_DRAGON_SERVICE.read_text()
        manifest = I03_MANIFEST.read_text()

        self.assertIn(
            "679efb649622592a9d5034b56ea101623d169ed1a6f6843a6fa0c223ea029030",
            runner)
        self.assertIn("--version-id tGQosk2CGRF4If4zw4gDr49iilHlYbax", runner)
        self.assertIn("--version-id Q6lITyol9ytTGZe6hjaZeIgUwil8_FTQ", runner)
        self.assertIn("--version-id jalHavk2h4QI0Cy267M3j_11brHWmWXi", runner)
        self.assertIn("--version-id DZX._9FksVOU8VTkFnvv7xc4A8pY1kyU", runner)
        self.assertEqual(3, runner.count(
            "28d3cbeba82d02611a48bf2d0a6a527d11ff4cd3049f04bf2b4b929a05ed86c6"))
        self.assertNotIn("28d3bcf48109e095", runner)
        # This exact v26 binary predates the worker-count CLI.  Its worker pool
        # is safely constrained by the service's fourteen-CPU affinity.
        self.assertNotIn("--workers", runner)
        self.assertNotIn("--resume", runner)
        self.assertIn("find \"${restore}/work/solve\"", runner)
        self.assertIn("test ! -e \"${restore}/work/results/", runner)
        self.assertIn("AllowedCPUs=2-15", service)
        self.assertIn("MemoryMax=140G", service)
        self.assertIn(
            "ultimatefish-info-kghostkdragon-i03-race-v1.service", manifest)
        self.assertIn("|14|", manifest)
        self.assertIn("kghostkdragon-memory-v5", manifest)


if __name__ == "__main__":
    unittest.main()
