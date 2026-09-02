import pathlib
import re
import sys
import unittest
from unittest import mock


ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/tablebases/ultimatefish-migrate-c1-gp3-to-nvme-v30.sh"
sys.path.insert(0, str(ROOT / "tools/tablebases"))
import resume_ultimate_devil_companion_solver_v30_aws as resume  # noqa: E402


class MigrateC1Gp3ToNvmeTest(unittest.TestCase):
    def test_xfs_label_is_valid_and_device_is_identity_gated(self):
        source = SCRIPT.read_text()
        match = re.search(r"^mkfs\.xfs -f -L (\S+) \"\$\{device\}\"$", source, re.MULTILINE)
        self.assertIsNotNone(match)
        self.assertLessEqual(len(match.group(1)), 12)
        self.assertIn('lsblk -dn -o SERIAL "${device}"', source)
        self.assertIn('lsblk -bdn -o SIZE "${device}"', source)
        self.assertIn('findmnt -n -o SOURCE --target /mnt/ultimatefish-devil-v11', source)
        self.assertIn('systemctl stop "${unit}" 2>/dev/null || true', source)
        self.assertIn('systemctl is-active "${unit}"', source)
        self.assertNotIn("|| +", source)

    def test_v30_resume_uses_nvme_checkpoint_and_resized_memory_gate(self):
        captured = {}

        def fake_send(instance, commands, timeout):
            captured.update(instance=instance, commands=commands, timeout=timeout)
            return "ok"

        remote = resume.impl.impl.base.v21.remote
        with mock.patch.object(remote, "send", side_effect=fake_send):
            result = resume.impl.impl.resume(
                "i-test", "bishop", 2, tuple(range(48)),
                40_000_000_000, 40_000_000_000)
        self.assertEqual("ok", result)
        shell = captured["commands"][0]
        self.assertIn("/mnt/ultimatefish-devil-fast", shell)
        self.assertIn(
            "install -d -m 0755 "
            "/mnt/ultimatefish-devil-fast/devil-companion-v30/logs", shell)
        self.assertIn(
            "ultimatefish-devil-companion-v30-same-bishop-square-2", shell)
        self.assertIn("MemoryMax=498216206336", shell)
        self.assertIn("--devil-spawned-limit 40000000000", shell)
        self.assertIn("--devil-spawned-hash-capacity 40000000000", shell)

    def test_v31_b1_resume_uses_raid_and_40_billion_gates(self):
        import resume_ultimate_devil_companion_solver_v31_aws as resume_b1

        captured = {}

        def fake_send(instance, commands, timeout):
            captured.update(instance=instance, commands=commands, timeout=timeout)
            return "ok"

        remote = resume_b1.impl.impl.base.v21.remote
        with mock.patch.object(remote, "send", side_effect=fake_send):
            result = resume_b1.impl.impl.resume(
                "i-test", "bishop", 1, tuple(range(48)),
                40_000_000_000, 40_000_000_000)
        self.assertEqual("ok", result)
        shell = captured["commands"][0]
        self.assertIn("/mnt/ultimatefish/devil-companion-v29", shell)
        self.assertIn(
            "ultimatefish-devil-companion-v31-same-bishop-square-1", shell)
        self.assertIn("MemoryMax=498216206336", shell)
        self.assertIn("--devil-spawned-limit 40000000000", shell)
        self.assertIn("--devil-spawned-hash-capacity 40000000000", shell)


if __name__ == "__main__":
    unittest.main()
