#!/usr/bin/env python3
"""Static safety checks for the retained opposed Sniper/Ghost continuation."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
WRAPPER = ROOT / "tools/tablebases/ultimatefish-resume-kghostksniper-after-stop-v4.sh"


class ResumeGhostSniperV4Tests(unittest.TestCase):
    def test_is_solve_only_and_binds_retained_graph(self) -> None:
        text = WRAPPER.read_text()
        self.assertIn("--solve-existing", text)
        self.assertNotIn("--resume-transitions", text)
        self.assertNotIn("--compile-transitions", text)
        self.assertIn(
            "181d1fee675c3f93231961f9ac31c36531d8b9329111e0a76020968c21df4600",
            text,
        )

    def test_binds_both_installed_executables_and_inputs(self) -> None:
        text = WRAPPER.read_text()
        self.assertGreaterEqual(
            text.count(
                "42e719d0723c0faaa45a705bc54561ca2e66406f23dff95a4999f54e3adc1775"
            ),
            3,
        )
        self.assertIn(
            "c9541415922355346dddf284541edb67982b0250316549043bcfbe23efa79e78",
            text,
        )
        self.assertIn(
            "472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb",
            text,
        )

    def test_preserves_existing_work_tree(self) -> None:
        text = WRAPPER.read_text()
        self.assertIn("rejected: legacy solve-existing", text)
        self.assertIn("exit 1", text)
        self.assertNotIn('test ! -e "$work"', text)
        self.assertNotIn("rm -", text)


if __name__ == "__main__":
    unittest.main()
