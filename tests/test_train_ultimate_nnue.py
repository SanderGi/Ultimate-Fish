import importlib.util
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "train_ultimate_nnue", ROOT / "tools" / "train_ultimate_nnue.py")
NNUE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(NNUE)


class UltimateNnueFeatureTests(unittest.TestCase):
    def test_orientation_and_relative_color_are_symmetric(self):
        white = "w;hm=0;fm=1;ep=-;cont=0;forced=-1;king,w,a1;rook,b,h10"
        black = "b;hm=0;fm=1;ep=-;cont=0;forced=-1;king,b,h10;rook,w,a1"
        self.assertEqual(NNUE.features(white, "w"), NNUE.features(black, "b"))

    def test_lossless_state_features_are_distinct(self):
        base = "w;hm=0;fm=1;ep=-;cont=0;forced=-1;king,w,a1;penguin,w,b2"
        stateful = (
            "b;hm=0;fm=1;ep=c3;cont=2;forced=1;king,w,a1;"
            "penguin,w,b2,129,3,2,4,1,0,0,1,-1,0"
        )
        plain = set(NNUE.features(base, "w"))
        state = set(NNUE.features(stateful, "w"))
        self.assertNotEqual(plain, state)
        self.assertGreater(len(state - plain), 8)

    def test_feature_indices_fit_serialized_input(self):
        upn = (
            "w;hm=0;fm=1;ep=h10;cont=1;forced=1;king,w,a1;"
            "angel,w,b2,255,8,8,8,1,0,2,0,0,3;halo,w,b2;rook,b,h10"
        )
        for perspective in ("w", "b"):
            active = NNUE.features(upn, perspective)
            self.assertTrue(active)
            self.assertEqual(len(active), len(set(active)))
            self.assertGreaterEqual(min(active), 0)
            self.assertLess(max(active), NNUE.INPUTS)

    def test_outcome_weight_changes_teacher_without_changing_perspective(self):
        record = {"upn": "b;king,w,a1;king,b,a10", "score_stm": -200,
                  "static_stm": -100, "result_white": 1}
        search_only = NNUE.target(record, 0.0)
        outcome_only = NNUE.target(record, 1.0)
        self.assertAlmostEqual(search_only, 0.1)
        self.assertAlmostEqual(outcome_only, 1.1)


if __name__ == "__main__":
    unittest.main()
