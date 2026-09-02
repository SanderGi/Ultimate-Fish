import importlib
import sys
from pathlib import Path
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tablebases"))
import migrate_ultimate_devil_companion_v15_aws as migration  # noqa: E402
import migrate_ultimate_devil_checkpoint_to_nvme_v17_aws as nvme  # noqa: E402
import resume_ultimate_devil_companion_solver_v15_aws as resume  # noqa: E402
import resume_ultimate_devil_companion_solver_v19_aws as resume_v19  # noqa: E402
import resume_ultimate_devil_companion_solver_v20_aws as resume_v20  # noqa: E402
import resume_ultimate_devil_companion_solver_v21_aws as resume_v21  # noqa: E402
import resume_ultimate_devil_companion_solver_v22_aws as resume_v22  # noqa: E402
import resume_ultimate_devil_companion_solver_v23_aws as resume_v23  # noqa: E402


class DevilCompanionV15AwsTests(unittest.TestCase):
    def setUp(self):
        # The historical v22+ launch adapters intentionally configure the v21
        # implementation module in place. Tests import several generations in
        # one interpreter, unlike the standalone production runners, so reset
        # the shared module before each generation-specific assertion.
        importlib.reload(resume_v21)

    def test_v23_stages_batched_lookup_binary_and_stops_v22(self):
        captured = {}

        def fake_send(instance, commands, timeout):
            captured.update(instance=instance, commands=commands, timeout=timeout)
            return "ok"

        with mock.patch.object(resume_v23.v21.remote, "send", side_effect=fake_send):
            result = resume_v23.resume(
                "i-test", "bishop", False, 1, tuple(range(28)),
                20_000_000_000, 20_000_000_000, "/mnt/ultimatefish",
            )
        self.assertEqual("ok", result)
        shell = captured["commands"][0]
        self.assertIn(resume_v23.SOURCE_SHA256, shell)
        self.assertIn(resume_v23.BINARY_SHA256, shell)
        self.assertIn("systemctl stop ultimatefish-devil-companion-v22-", shell)
        self.assertIn("ultimatefish-devil-companion-v23-same-bishop-square-1", shell)

    def test_v22_stages_byte_fingerprint_binary_and_stops_v21(self):
        captured = {}

        def fake_send(instance, commands, timeout):
            captured.update(instance=instance, commands=commands, timeout=timeout)
            return "ok"

        with mock.patch.object(resume_v22.v21.remote, "send", side_effect=fake_send):
            result = resume_v22.resume(
                "i-test", "bishop", False, 2, tuple(range(24)),
                18_000_000_000, 18_000_000_000, "/mnt/ultimatefish",
            )
        self.assertEqual("ok", result)
        shell = captured["commands"][0]
        self.assertIn(resume_v22.SOURCE_SHA256, shell)
        self.assertIn(resume_v22.BINARY_SHA256, shell)
        self.assertIn("systemctl stop ultimatefish-devil-companion-v21-", shell)
        self.assertIn("ultimatefish-devil-companion-v22-same-bishop-square-2", shell)

    def test_v21_stages_six_bit_binary_with_reviewed_memory_gate(self):
        captured = {}

        def fake_send(instance, commands, timeout):
            captured.update(instance=instance, commands=commands, timeout=timeout)
            return "ok"

        with mock.patch.object(resume_v21.remote, "send", side_effect=fake_send):
            result = resume_v21.resume(
                "i-test", "bishop", False, 2, tuple(range(24)),
                18_000_000_000, 18_000_000_000, "/mnt/ultimatefish",
            )
        self.assertEqual("ok", result)
        shell = captured["commands"][0]
        self.assertIn(resume_v21.SOURCE_SHA256, shell)
        self.assertIn(resume_v21.BINARY_SHA256, shell)
        self.assertIn("systemctl stop ultimatefish-devil-companion-v20-", shell)
        self.assertIn("MemoryMax=240518168576", shell)

    def test_v20_stages_phase_advised_binary_and_stops_v19(self):
        captured = {}

        def fake_send(instance, commands, timeout):
            captured.update(instance=instance, commands=commands, timeout=timeout)
            return "ok"

        with mock.patch.object(resume_v20.remote, "send", side_effect=fake_send):
            result = resume_v20.resume(
                "i-test", "bishop", False, 0, tuple(range(1, 22)) + (31,),
                22_000_000_000, 20_000_000_000, "/mnt/ultimatefish",
            )
        self.assertEqual("ok", result)
        shell = captured["commands"][0]
        self.assertIn(resume_v20.SOURCE_SHA256, shell)
        self.assertIn(resume_v20.BINARY_SHA256, shell)
        self.assertIn("systemctl stop ultimatefish-devil-companion-v19-", shell)
        self.assertIn("ultimatefish-devil-companion-v20-same-bishop-square-0", shell)
        self.assertIn("--devil-spawned-hash-capacity 20000000000", shell)

    def test_v19_rejects_obsolete_bishop_c1_hash_envelope(self):
        with self.assertRaisesRegex(ValueError, "at least 18000000000"):
            resume_v19.resume(
                "i-test", "bishop", False, 2, tuple(range(24)),
                16_000_000_000, 8_000_000_000, "/mnt/ultimatefish",
            )

    def test_v19_uses_reviewed_bishop_c1_hash_envelope(self):
        captured = {}

        def fake_send(instance, commands, timeout):
            captured.update(instance=instance, commands=commands, timeout=timeout)
            return "ok"

        with mock.patch.object(resume_v19.remote, "send", side_effect=fake_send):
            result = resume_v19.resume(
                "i-test", "bishop", False, 2, tuple(range(24)),
                18_000_000_000, 18_000_000_000, "/mnt/ultimatefish",
            )
        self.assertEqual("ok", result)
        self.assertIn(
            "--devil-spawned-hash-capacity 18000000000",
            captured["commands"][0],
        )

    def test_v19_rejects_exhausted_bishop_b1_hash_envelope(self):
        with self.assertRaisesRegex(ValueError, "at least 20000000000"):
            resume_v19.resume(
                "i-test", "bishop", False, 1, tuple(range(28)),
                20_000_000_000, 16_000_000_000, "/mnt/ultimatefish",
            )

    def test_v21_rejects_exhausted_bishop_c1_hash_envelope(self):
        with self.assertRaisesRegex(ValueError, "at least 18000000000"):
            resume_v21.resume(
                "i-test", "bishop", False, 2, tuple(range(24)),
                18_000_000_000, 15_000_000_000, "/mnt/ultimatefish",
            )

    def test_v11_retained_path_uses_original_v8_checkpoint(self):
        source_unit, source_work, destination, unit, _ = migration.paths(
            "bishop", False, 0, "/mnt/checkpoint-migrate", "v11"
        )
        self.assertEqual(
            "ultimatefish-devil-companion-v11-same-bishop-square-0",
            source_unit,
        )
        self.assertIn("devil-companion-v8-f2103f6a", source_work)
        self.assertIn("devil-companion-v15-34aa7f01", destination)
        self.assertIn("migrate-v11-v15", unit)

    def test_v14_retained_path_and_v15_resume_names_are_consistent(self):
        volume = "/mnt/ultimatefish-devil-v11"
        source_unit, source_work, destination, _, _ = migration.paths(
            "bishop", False, 2, volume, "v14"
        )
        unit, work, _, v11_unit, v14_unit = resume.names(
            "bishop", False, 2, volume
        )
        self.assertEqual(v14_unit, source_unit)
        self.assertIn("devil-companion-v14-4044755f", source_work)
        self.assertEqual(destination, work)
        self.assertEqual(
            "ultimatefish-devil-companion-v15-same-bishop-square-2", unit
        )
        self.assertEqual(
            "ultimatefish-devil-companion-v11-same-bishop-square-2", v11_unit
        )

    def test_launch_keeps_all_fail_closed_gates_in_one_shell(self):
        captured = {}

        def fake_send(instance, commands, timeout):
            captured.update(instance=instance, commands=commands, timeout=timeout)
            return "ok"

        with mock.patch.object(migration, "stage_commands", return_value=[]), \
             mock.patch.object(migration.base, "send", side_effect=fake_send):
            result = migration.launch(
                "i-test", "bishop", False, 0, "/mnt/checkpoint-migrate",
                "v11", (1, 2, 3, 4)
            )
        self.assertEqual("ok", result)
        self.assertEqual(1, len(captured["commands"]))
        shell = captured["commands"][0]
        self.assertIn("set -euo pipefail", shell)
        self.assertIn("AllowedCPUs=1,2,3,4", shell)
        self.assertIn("MemoryMax=137438953472", shell)
        self.assertIn("--migrate-devil-checkpoint", shell)

    def test_nvme_copy_stops_source_and_authenticates_every_checkpoint_file(self):
        captured = {}

        def fake_send(instance, commands, timeout):
            captured.update(instance=instance, commands=commands, timeout=timeout)
            return "ok"

        with mock.patch.object(nvme.remote, "send", side_effect=fake_send):
            result = nvme.launch(
                "i-test", "bishop", False, 2,
                "/mnt/ultimatefish-devil-v11", "/mnt/ultimatefish", 0,
            )
        self.assertEqual("ok", result)
        self.assertEqual(1, len(captured["commands"]))
        shell = captured["commands"][0]
        self.assertIn("set -euo pipefail", shell)
        self.assertIn("systemctl stop", shell)
        self.assertIn("companion-v16-same-bishop-square-2", shell)
        self.assertIn("AllowedCPUs=0,1", shell)
        self.assertEqual(6, shell.count("sha256sum"))
        self.assertEqual(3, shell.count("source_hash_pid=$!"))
        self.assertEqual(3, shell.count("destination_hash_pid=$!"))
        self.assertEqual(3, shell.count("wait $source_hash_pid"))
        self.assertEqual(3, shell.count("wait $destination_hash_pid"))
        for suffix in nvme.SUFFIXES:
            self.assertIn(f"devil-2.{suffix}", shell)
        for suffix in nvme.GRAPH_SUFFIXES:
            self.assertIn(f"devil-2.{suffix}", shell)
        self.assertIn("nvme-copy-v17.receipt", shell)

    def test_nvme_copy_rejects_nonlocal_destination(self):
        with self.assertRaisesRegex(ValueError, "instance-local"):
            nvme.launch(
                "i-test", "bishop", False, 2,
                "/mnt/ultimatefish-devil-v11", "/mnt/checkpoint-migrate", 0,
            )

    def test_nvme_finalize_reauthenticates_with_parallel_large_reads(self):
        captured = {}

        def fake_send(instance, commands, timeout):
            captured.update(instance=instance, commands=commands, timeout=timeout)
            return "ok"

        with mock.patch.object(nvme.remote, "send", side_effect=fake_send):
            result = nvme.finalize(
                "i-test", "bishop", False, 2,
                "/mnt/ultimatefish-devil-v11", "/mnt/ultimatefish", 0,
            )
        self.assertEqual("ok", result)
        shell = captured["commands"][0]
        self.assertIn("systemctl stop", shell)
        self.assertEqual(6, shell.count("bs=16M"))
        self.assertEqual(3, shell.count("wait $source_hash_pid"))
        self.assertEqual(3, shell.count("wait $destination_hash_pid"))
        self.assertIn("cmp -s", shell)
        self.assertIn("mv -f", shell)
        self.assertIn("nvme-copy-v17.receipt.stage", shell)
        self.assertIn("AllowedCPUs=0,1", shell)


if __name__ == "__main__":
    unittest.main()
