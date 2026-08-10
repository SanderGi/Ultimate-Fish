#!/usr/bin/env python3

import hashlib
import sys
from pathlib import Path
import struct
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

import rebind_ultimate_jester_ghost_transitions as rebind


SOURCE = "1" * 64
OLD_MODEL = "2" * 64
NEW_MODEL = "3" * 64
OBSERVATION = "4" * 64


def digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def write_prefix(directory: Path, name: str = "shard-00") -> Path:
    prefix = directory / name
    meta = b"m" * rebind.GEOMETRY_BYTES
    strata = b"s" * rebind.STRATUM_BYTES
    index = struct.pack("<2Q", 0, 4)
    blocks = b"data"
    prefix.with_suffix(".meta").write_bytes(meta)
    prefix.with_suffix(".strata").write_bytes(strata)
    prefix.with_suffix(".index").write_bytes(index)
    prefix.with_suffix(".blocks").write_bytes(blocks)
    payload = digest(meta + strata + index + blocks)
    counters = (1, 2, 2, 2, 2, 0, 3, 3, 3, 12, 8, 8, 12, 4, 1, 4)
    header = rebind.HEADER.pack(
        rebind.HEADER_MAGIC, rebind.VERSION, rebind.HEADER.size,
        0, 1, rebind.RAW_DOMAIN, 0, *counters,
        SOURCE.encode(), OLD_MODEL.encode(), OBSERVATION.encode(),
        payload.encode())
    prefix.with_suffix(".header").write_bytes(header)
    marker = rebind.MARKER.pack(
        rebind.MARKER_MAGIC, rebind.VERSION, rebind.MARKER.size, 0, 1,
        SOURCE.encode(), OLD_MODEL.encode(), OBSERVATION.encode(),
        payload.encode(), digest(header).encode())
    prefix.with_suffix(".verified").write_bytes(marker)
    return prefix


class RebindJesterGhostTransitionsTest(unittest.TestCase):
    def test_rebind_changes_only_authenticated_fields(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            destination = root / "destination"
            source.mkdir()
            prefix = write_prefix(source)
            orphan = source / "aborted.header"
            orphan.write_bytes(b"unfinished")
            before = {suffix: prefix.with_suffix(suffix).read_bytes()
                      for suffix in rebind.SUFFIXES}

            result = rebind.migrate_tree(
                source, destination, OLD_MODEL, NEW_MODEL, SOURCE,
                OBSERVATION, require_complete=False)

            self.assertEqual(result["verified_prefixes"], 1)
            self.assertEqual(result["unverified_headers_copied_unchanged"],
                             ["aborted.header"])
            self.assertEqual(orphan.read_bytes(), b"unfinished")
            self.assertEqual((destination / orphan.name).read_bytes(),
                             b"unfinished")
            rebound = destination / prefix.name
            for suffix in (".meta", ".strata", ".index", ".blocks"):
                self.assertEqual(rebound.with_suffix(suffix).read_bytes(),
                                 before[suffix])
            header = rebound.with_suffix(".header").read_bytes()
            marker = rebound.with_suffix(".verified").read_bytes()
            header_diffs = {i for i, (old, new) in
                            enumerate(zip(before[".header"], header))
                            if old != new}
            marker_diffs = {i for i, (old, new) in
                            enumerate(zip(before[".verified"], marker))
                            if old != new}
            self.assertTrue(header_diffs)
            self.assertTrue(marker_diffs)
            self.assertTrue(all(rebind.MODEL_OFFSET <= i <
                                rebind.MODEL_OFFSET + 64
                                for i in header_diffs))
            self.assertTrue(all(
                rebind.MARKER_MODEL_OFFSET <= i <
                rebind.MARKER_MODEL_OFFSET + 64 or
                rebind.MARKER_HEADER_SHA_OFFSET <= i <
                rebind.MARKER_HEADER_SHA_OFFSET + 64
                for i in marker_diffs))
            rebind.authenticate_prefix(rebound, SOURCE.encode(),
                                       NEW_MODEL.encode(),
                                       OBSERVATION.encode())

    def test_corrupt_payload_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "source"
            source.mkdir()
            prefix = write_prefix(source)
            prefix.with_suffix(".blocks").write_bytes(b"fail")
            with self.assertRaisesRegex(ValueError, "payload SHA-256"):
                rebind.migrate_tree(
                    source, Path(temporary) / "destination", OLD_MODEL,
                    NEW_MODEL, SOURCE, OBSERVATION, require_complete=False)

    def test_requires_complete_merged_certificate_by_default(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "source"
            source.mkdir()
            write_prefix(source)
            with self.assertRaisesRegex(ValueError, "complete merged"):
                rebind.migrate_tree(
                    source, Path(temporary) / "destination", OLD_MODEL,
                    NEW_MODEL, SOURCE, OBSERVATION)

    def test_existing_destination_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            destination = root / "destination"
            source.mkdir()
            destination.mkdir()
            write_prefix(source)
            with self.assertRaisesRegex(ValueError, "already exists"):
                rebind.migrate_tree(
                    source, destination, OLD_MODEL, NEW_MODEL, SOURCE,
                    OBSERVATION, require_complete=False)


if __name__ == "__main__":
    unittest.main()
