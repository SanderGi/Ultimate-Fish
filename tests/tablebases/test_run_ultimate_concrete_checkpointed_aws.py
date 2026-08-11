from __future__ import annotations

from pathlib import Path
import sys
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "tablebases"))
import run_ultimate_concrete_checkpointed_aws as checkpointed  # noqa: E402
import run_ultimate_concrete_tablebase_shard_aws as concrete  # noqa: E402


class CheckpointedConcreteAwsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.record = concrete.wave_inventory(0)[20]

    def test_full_command_checkpoints_complete_frontier(self) -> None:
        command = checkpointed.checkpointed_class_command(self.record)
        option = command.index("--checkpoint-every") + 1
        self.assertEqual(command[option], str(self.record["states"]))
        self.assertIn("--disk-backed", command)

    def test_dry_run_command_is_unchanged(self) -> None:
        expected = checkpointed.BASE_CLASS_COMMAND(
            self.record, dry_run=123, dry_run_begin=456)
        actual = checkpointed.checkpointed_class_command(
            self.record, dry_run=123, dry_run_begin=456)
        self.assertEqual(actual, expected)
        self.assertNotIn("--checkpoint-every", actual)

    def test_wrapper_is_outside_generator_model(self) -> None:
        self.assertNotIn(
            "tools/tablebases/run_ultimate_concrete_checkpointed_aws.py",
            concrete.MODEL_SOURCES)

    def test_main_delegates_with_checkpoint_override(self) -> None:
        with mock.patch.object(
                concrete, "generator_model_sha256",
                return_value=checkpointed.EXPECTED_MODEL), mock.patch.object(
                concrete, "inventory_sha256",
                return_value=checkpointed.EXPECTED_INVENTORY), mock.patch.object(
                concrete, "main", return_value=7) as main:
            self.assertEqual(checkpointed.main(["--wave", "0"]), 7)
        main.assert_called_once_with(["--wave", "0"])
        self.assertIs(concrete.class_command,
                      checkpointed.checkpointed_class_command)

    def test_main_rejects_wrong_model_before_runner(self) -> None:
        with mock.patch.object(concrete, "generator_model_sha256",
                               return_value="f" * 64), \
                mock.patch.object(concrete, "main") as main:
            with self.assertRaisesRegex(RuntimeError, "certified model"):
                checkpointed.main([])
        main.assert_not_called()


if __name__ == "__main__":
    unittest.main()
