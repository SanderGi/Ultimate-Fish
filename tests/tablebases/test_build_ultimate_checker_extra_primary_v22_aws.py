import importlib.util
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "build_ultimate_checker_extra_primary_v22_aws",
    TOOLS / "build_ultimate_checker_extra_primary_v22_aws.py",
)
assert SPEC and SPEC.loader
build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build)


class CheckerExtraPrimaryBuildTests(unittest.TestCase):
    def test_physical_oracle_order_is_part_of_build_contract(self):
        self.assertEqual(
            build.build_base.PRIMARY_DEFINE,
            "-DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY",
        )
        self.assertIn("checker-extra-primary", build.build_base.SEMANTICS)
        self.assertEqual(build.build_base.GENERATION, "v22")
        self.assertIn(
            "-DULTIMATE_GHOST_EXTRA_IS_CHECKER",
            build.build_base.EXTRA_DEFINES,
        )


if __name__ == "__main__":
    unittest.main()
