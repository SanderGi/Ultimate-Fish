#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class HiddenParallelAwsTests(unittest.TestCase):
    def test_plan_is_exact_measure_first_and_resource_bounded(self) -> None:
        plan = json.loads(
            (ROOT / "tools/ultimate_hidden_parallel_aws.json").read_text())
        self.assertEqual("ultimate-hidden-parallel-aws-v2", plan["schema"])
        self.assertEqual("abe24c3a", plan["canonical_commit"])
        self.assertEqual(4, len(plan["jobs"]))
        self.assertEqual(4, len({job["instance_id"] for job in plan["jobs"]}))
        for job in plan["jobs"]:
            self.assertIn(job["bundle_sha256"], job["bundle_key"])
            self.assertTrue(job["bundle_version_id"])
            unit_path = ROOT / job["unit"]
            unit = unit_path.read_text()
            self.assertIn("Type=oneshot", unit)
            self.assertRegex(unit, r"CPUQuota=(1600|2900)%")
            self.assertIn("MemoryHigh=28G", unit)
            self.assertIn("MemoryMax=32G", unit)
            self.assertIn("Nice=10", unit)
            self.assertIn("IOSchedulingClass=idle", unit)
            self.assertIn("PrivateTmp=true", unit)
            self.assertIn("ExecStartPre=/usr/bin/mkdir -p ", unit)
            self.assertIn("Environment=TMPDIR=", unit)
            self.assertNotIn("--full", unit)
            runner = ("tools/run_ultimate_ghost_parasite_aws.py"
                      if "parasite" in job["id"] else
                      "tools/run_ultimate_ghost_bomb_aws.py")
            digest = hashlib.sha256((ROOT / runner).read_bytes()).hexdigest()
            self.assertEqual(job["runner_sha256"], digest)


if __name__ == "__main__":
    unittest.main()
