from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "tablebases"))
import build_ultimate_concrete_frontier_resume_manifest as builder  # noqa: E402


class ConcreteFrontierResumeManifestSubstateTest(unittest.TestCase):
    @staticmethod
    def record(primary: str, secondary: str, opposing: bool) -> dict[str, object]:
        return {
            "primary": primary,
            "secondary": secondary,
            "opposing": opposing,
        }

    def test_same_team_angel_includes_companion_attachment_mode(self) -> None:
        self.assertEqual(
            builder.exact_substates(
                self.record("berserker", "angel", False)),
            30,
        )

    def test_opposed_angel_has_only_own_king_attachment_mode(self) -> None:
        self.assertEqual(
            builder.exact_substates(
                self.record("berserker", "angel", True)),
            20,
        )

    def test_ordinary_pair_keeps_product_factor(self) -> None:
        self.assertEqual(
            builder.exact_substates(
                self.record("berserker", "prince", True)),
            20,
        )


if __name__ == "__main__":
    unittest.main()
