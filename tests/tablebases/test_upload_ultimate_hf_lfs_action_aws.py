import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools" / "tablebases" / "upload_ultimate_hf_lfs_action_aws.py"
SPEC = importlib.util.spec_from_file_location("upload_hf_lfs_action", SCRIPT)
assert SPEC and SPEC.loader
worker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(worker)


class UploadUltimateHfLfsActionAwsTests(unittest.TestCase):
    def test_multipart_orders_etags_and_completes_exact_oid(self):
        action = {
            "actions": {"upload": {
                "href": "https://complete.invalid",
                "header": {
                    "chunk_size": "4",
                    "1": "https://part-1.invalid",
                    "2": "https://part-2.invalid",
                    "3": "https://part-3.invalid",
                },
            }},
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source.ufds"
            source.write_bytes(b"0123456789")

            def fake_put(url, payload_path, header_path):
                number = int(url.split("-")[-1].split(".")[0])
                expected = (b"0123", b"4567", b"89")[number - 1]
                self.assertEqual(expected, payload_path.read_bytes())
                return f'"etag-{number}"'

            with mock.patch.object(worker, "curl_put", side_effect=fake_put), \
                    mock.patch.object(worker.subprocess, "run") as completed:
                worker.upload_multipart(
                    source, action, "f" * 64, workers=2, temporary=root)

            payload = completed.call_args.kwargs["input"]
            self.assertIn(b'"oid": "' + b"f" * 64 + b'"', payload)
            self.assertLess(payload.index(b'etag-1'), payload.index(b'etag-2'))
            self.assertLess(payload.index(b'etag-2'), payload.index(b'etag-3'))


if __name__ == "__main__":
    unittest.main()
