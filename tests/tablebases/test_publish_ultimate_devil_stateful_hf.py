import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "tablebases" / "publish_ultimate_devil_stateful_hf.py"
SPEC = importlib.util.spec_from_file_location("publish_devil_hf", SCRIPT)
assert SPEC and SPEC.loader
publisher = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(publisher)


class PublishUltimateDevilStatefulHfTests(unittest.TestCase):
    def test_range_download_reuses_complete_local_extent_for_revalidation(self):
        partition = {
            "square": 0,
            "states": 2,
            "sidecar_version_id": "version-1",
            "sidecar_sha256": "unused-by-download",
        }
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "kdevilk-a1.ufds"
            destination.write_bytes(bytes(publisher.HEADER.size + 20))
            with mock.patch.object(publisher.subprocess, "check_output") as head:
                publisher.download(partition, destination, workers=2)
            head.assert_not_called()

    def test_range_download_replaces_stale_partial_and_marker(self):
        partition = {
            "square": 0,
            "states": 2,
            "sidecar_version_id": "version-1",
            "sidecar_sha256": "unused-by-download",
        }
        payload = bytes(range(publisher.HEADER.size + 20))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            destination = root / "kdevilk-a1.ufds"
            partial = destination.with_suffix(".ufds.part")
            partial.write_bytes(b"stale")
            ranges = root / ".kdevilk-a1.ufds.ranges"
            ranges.mkdir()
            (ranges / "000000.done").touch()

            def fake_run(arguments, **kwargs):
                self.assertEqual("get-object", arguments[2])
                Path(arguments[-1]).write_bytes(payload)
                return mock.Mock(returncode=0)

            head = json.dumps({
                "ContentLength": len(payload),
                "VersionId": "version-1",
            })
            with mock.patch.object(publisher.subprocess, "check_output",
                                   return_value=head), \
                    mock.patch.object(publisher.subprocess, "run",
                                      side_effect=fake_run):
                publisher.download(partition, destination, workers=2)

            self.assertEqual(payload, destination.read_bytes())
            self.assertFalse(partial.exists())
            self.assertFalse(ranges.exists())


if __name__ == "__main__":
    unittest.main()
