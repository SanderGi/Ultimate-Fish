import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tools/tablebases/audit_ultimate_devil_stateful_sidecar.cpp"
VERIFIER = ROOT / "tools/tablebases/verify_ultimate_devil_stateful_slice.cpp"


def key(minions=(), side: int = 0, cooldown: int = 0,
        alive: bool = True, white_king: int = 72,
        black_king: int = 79) -> int:
    squares = sorted(minions)
    rank = sum(math.comb(80, count) for count in range(len(squares)))
    rank += sum(math.comb(square, ordinal)
                for ordinal, square in enumerate(squares, 1))
    black_index = black_king - int(black_king > white_king)
    kings = white_king * 79 + black_index
    return (rank | (kings << 25) | (80 << 38) | (cooldown << 45) |
            (side << 47) | (int(alive) << 48))


class DevilStatefulCensusTests(unittest.TestCase):
    def test_fast_root_filter_matches_native_position(self):
        with tempfile.TemporaryDirectory() as raw:
            binary = Path(raw) / "verify"
            subprocess.run([
                "g++", "-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra",
                "-Wpedantic", "-Werror", "-I", str(ROOT / "tools/tablebases"),
                "-I", str(ROOT / "src/ultimate"), str(VERIFIER),
                str(ROOT / "src/ultimate/position.cpp"),
                str(ROOT / "src/ultimate/nnue.cpp"), "-o", str(binary),
            ], check=True)
            completed = subprocess.run(
                [str(binary), "10000"], check=True, capture_output=True,
                text=True)
            self.assertIn("DEVIL_SLICE_VERIFIED", completed.stdout)

    def test_counts_full_stateful_domain_and_witnesses(self):
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            binary = directory / "audit"
            subprocess.run([
                "g++", "-std=c++17", "-O2", "-pthread", "-Wall", "-Wextra",
                "-Wpedantic", "-Werror", str(SOURCE), "-o", str(binary),
            ], check=True)
            sidecar = directory / "devil-18.ufds"
            rows = sorted([
                (key(alive=False), 3, 0),
                (key(), 3, 0),
                (key((11,), side=1, cooldown=2), 1, 7),
                (key((11, 12), side=1), 2, 11),
                (key(cooldown=3, black_king=73), 3, 0),
            ])
            with sidecar.open("wb") as stream:
                stream.write(struct.pack("<8sIIQII", b"UFDSV1\0\0", 1, 18,
                                         len(rows), 10, 0))
                for logical, wdl, dtw in rows:
                    stream.write(logical.to_bytes(7, "little"))
                    stream.write(struct.pack("<BH", wdl, dtw))
            result = subprocess.run([
                str(binary), "--input", str(sidecar), "--square", "18",
                "--workers", "2",
            ], check=True, text=True, stdout=subprocess.PIPE)
            census = json.loads(result.stdout)
            self.assertEqual({"win": 1, "loss": 1, "draw": 3},
                             census["outcomes"])
            self.assertEqual(0, census["sorted_key_residual"])
            self.assertEqual(1, census["by_minion_count"][1]["outcomes"]["win"])
            self.assertEqual(1, census["by_minion_count"][2]["outcomes"]["loss"])
            self.assertEqual(
                {"win": 0, "loss": 0, "draw": 2},
                census["by_minion_count"][0][
                    "alive_outcomes_by_side_to_move"][0],
            )
            self.assertEqual(
                {"win": 0, "loss": 1, "draw": 0},
                census["by_minion_count"][2][
                    "alive_outcomes_by_side_to_move"][1],
            )
            zero_white = census["by_minion_count"][0][
                "root_filter_by_side_to_move"][0]
            self.assertEqual({"win": 0, "loss": 0, "draw": 2},
                             zero_white["total"])
            self.assertEqual({"win": 0, "loss": 0, "draw": 1},
                             zero_white["excluded"])
            self.assertEqual({"win": 0, "loss": 0, "draw": 1},
                             zero_white["display"])
            self.assertEqual(
                "stateful-reachability-admitted-minus-trivial-v1",
                census["root_filter_semantics"])
            self.assertEqual(0, census["root_filter_conservation_residual"])
            self.assertEqual(11, census["max_dtw"]["loss"])

            streamed = subprocess.run([
                str(binary), "--input", "-", "--square", "18",
                "--workers", "1",
            ], input=sidecar.read_bytes(), check=True, stdout=subprocess.PIPE)
            self.assertEqual(census, json.loads(streamed.stdout))


if __name__ == "__main__":
    unittest.main()
