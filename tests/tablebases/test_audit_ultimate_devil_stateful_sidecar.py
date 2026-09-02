import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tools/tablebases/audit_ultimate_devil_stateful_sidecar.cpp"


def key(rank: int, side: int = 0) -> int:
    return rank | (80 << 38) | (side << 47) | (1 << 48)


class DevilStatefulCensusTests(unittest.TestCase):
    def test_counts_full_stateful_domain_and_witnesses(self):
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            binary = directory / "audit"
            subprocess.run([
                "g++", "-std=c++17", "-O2", "-pthread", "-Wall", "-Wextra",
                "-Wpedantic", "-Werror", str(SOURCE), "-o", str(binary),
            ], check=True)
            sidecar = directory / "devil-18.ufds"
            # Ranks 0, 1..80, and 81..3240 encode zero, one, and two Minions.
            rows = [
                (key(0), 3, 0),
                (key(1), 1, 7),
                (key(81, 1), 2, 11),
            ]
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
            self.assertEqual({"win": 1, "loss": 1, "draw": 1},
                             census["outcomes"])
            self.assertEqual(0, census["sorted_key_residual"])
            self.assertEqual(1, census["by_minion_count"][1]["outcomes"]["win"])
            self.assertEqual(1, census["by_minion_count"][2]["outcomes"]["loss"])
            self.assertEqual(11, census["max_dtw"]["loss"])


if __name__ == "__main__":
    unittest.main()
