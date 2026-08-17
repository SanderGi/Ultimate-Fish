import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "tablebases"
sys.path.insert(0, str(TOOLS))


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


plot = load("ultimate_tablebase_radius_plot", TOOLS / "plot_ultimate_tablebases.py")
audit = load(
    "ultimate_berserker_radius_audit",
    TOOLS / "run_ultimate_berserker_radius_audit_aws.py",
)
prepare = load(
    "ultimate_berserker_radius_prepare",
    TOOLS / "prepare_berserker_radius_audit.py",
)


class BerserkerRadiusPlotTests(unittest.TestCase):
    def test_radius_rows_extend_berserker_without_renaming_existing_labels(self):
        self.assertEqual("Berserker", plot.PIECE_LABELS["berserker"])
        self.assertEqual(
            ("Berserker (radius 1)", "Berserker (radius 2)",
             "Berserker (radius 3)"),
            tuple(plot.PIECE_LABELS[row] for row in plot.BERSERKER_RADIUS_ROWS),
        )

    def test_radius_summary_preserves_exact_start_order_and_row_view(self):
        document = {
            "schema": 1,
            "files": {
                "kberserkerkninja.uftb": {
                    "radii": {
                        "1": {
                            "first_starts": {
                                "wins": 2_499_580,
                                "losses": 6_238_816,
                                "draws": 7_147_320,
                            },
                            "second_starts": {
                                "wins": 9_866_198,
                                "losses": 5_680,
                                "draws": 3_847_982,
                            },
                        }
                    }
                }
            },
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "radii.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            radii = plot.read_berserker_radii(path)
        catalog = plot.OutcomeCatalog(
            plot.read_summary(ROOT / "tablebases" / "README.md"), radii)
        cell = catalog.opposed_row("berserker_radius_1", "ninja")
        self.assertEqual("mixed", cell.kind)
        self.assertEqual(
            plot.WDL(2_499_580, 6_238_816, 7_147_320), cell.first)
        # The second raw cell is Ninja-to-move.  The plot is row-relative,
        # so Ninja's losses become Berserker's wins and vice versa.
        self.assertEqual(
            plot.WDL(5_680, 9_866_198, 3_847_982), cell.second)

    def test_certified_radius_summary_covers_every_distinguished_berserker(self):
        path = ROOT / "tablebases" / "berserker-radius-summary.json"
        document = json.loads(path.read_text(encoding="utf-8"))
        radii = plot.read_berserker_radii(path)

        self.assertEqual(36, len(document["files"]))
        self.assertEqual(105, len(radii))
        self.assertEqual(
            {
                "excluded": True,
                "reason": (
                    "same-team identical Berserkers are exchange-folded and "
                    "have no distinguished row piece"
                ),
            },
            document["files"]["kberserkerberserkerk.uftb"],
        )
        for record in document["files"].values():
            if record.get("excluded"):
                continue
            self.assertEqual({"1", "2", "3"}, set(record["radii"]))

        self.assertEqual(
            plot.WDL(10_407_744, 19_570, 171_582),
            radii[("kberserkerkninja.uftb", 3)].first_starts,
        )
        self.assertEqual(
            plot.WDL(3_650_750, 8_946_358, 1_122_752),
            radii[("kberserkerkninja.uftb", 3)].second_starts,
        )

    def test_identical_berserker_intersections_keep_domain_semantics(self):
        catalog = plot.OutcomeCatalog(
            plot.read_summary(ROOT / "tablebases" / "README.md"), {})
        self.assertEqual(
            "unknown",
            catalog.together_row("berserker_radius_1", "berserker").kind,
        )
        self.assertEqual(
            "computing",
            catalog.opposed_row("berserker_radius_1", "berserker").kind,
        )

    def test_concrete_parser_subtracts_unreachable_per_substate(self):
        text = "\n".join((
            "reachability_primary_substate substate 0 side 0 unknown 0 win 3 loss 5 draw 7",
            "reachability_primary_substate_total substate 0 side 0 unknown 0 win 13 loss 25 draw 37",
            "reachability_primary_substate substate 0 side 1 unknown 0 win 11 loss 13 draw 17",
            "reachability_primary_substate_total substate 0 side 1 unknown 0 win 31 loss 43 draw 57",
        ))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "audit.txt"
            path.write_text(text, encoding="utf-8")
            rows = audit.parse_concrete(path, "primary")
        self.assertEqual(([0, 10, 20, 30], [0, 20, 30, 40]), rows[0])

    def test_archive_manifest_entry_authenticates_uncompressed_payload(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "ARCHIVE.MANIFEST").write_text(
                "schema-name\nabc123 42 tablebases/kberserkercheckerk.uftb\n",
                encoding="utf-8",
            )
            self.assertEqual(
                ("abc123", 42),
                audit.archive_manifest_entry(
                    root, "tablebases/kberserkercheckerk.uftb"),
            )

    def test_legacy_abbreviated_s3_binding_uses_archive_not_sidecar_version(self):
        storage = (
            "S3 `bbbfcb3d…` / `ziGJ3OQj8DQorqRiy9uVx07S5uk5Bial`; "
            "output sha256:" + "a" * 64 + "; bound reachability sha256:" +
            "b" * 64 + " VersionId wrong-sidecar-version")
        self.assertEqual(
            "ziGJ3OQj8DQorqRiy9uVx07S5uk5Bial",
            prepare.artifact_version(storage),
        )

    def test_single_berserker_binds_archive_and_extracted_payload(self):
        record = prepare.build(ROOT / "tablebases" / "README.md")["files"][
            "kberserkerk.uftb"]
        self.assertEqual(
            "e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263",
            record["expected_sha256"],
        )
        self.assertEqual(
            "f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1",
            record["expected_payload_sha256"],
        )

    def test_information_parser_vectorizes_flags_without_changing_semantics(self):
        # Jester primary, Berserker secondary, two geometries per side.
        flags = bytearray(40)
        flags[0], flags[10] = 1, 4       # White mover: win, draw.
        flags[1], flags[11] = 2, 0       # White mover: loss, unreachable.
        flags[20], flags[30] = 2, 4      # Black mover: win, draw.
        flags[21], flags[31] = 1, 0      # Black mover: loss, unreachable.
        payload = (
            b"UFIW1\0\0\0" + struct.pack("<IIIIII", 1, 2, audit.BERSERKER,
                                           0, len(flags), 10) + flags)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.ufiw"
            path.write_bytes(payload)
            rows = audit.parse_information(path, "secondary")
        self.assertEqual(([0, 1, 0, 1], [0, 1, 0, 1]), rows[0])
        self.assertEqual(([0, 0, 1, 0], [0, 0, 1, 0]), rows[1])


if __name__ == "__main__":
    unittest.main()
