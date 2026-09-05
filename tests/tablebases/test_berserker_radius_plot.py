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
level_audit = load(
    "ultimate_berserker_level_audit",
    TOOLS / "audit_ultimate_berserker_levels.py",
)
prepare = load(
    "ultimate_berserker_radius_prepare",
    TOOLS / "prepare_berserker_radius_audit.py",
)
merge_audits = load(
    "ultimate_berserker_radius_merge",
    TOOLS / "merge_ultimate_berserker_radius_audits.py",
)
import_radius_trivial = load(
    "ultimate_berserker_radius_information_trivial_import",
    TOOLS / "import_ultimate_berserker_radius_information_trivial.py",
)


class BerserkerRadiusPlotTests(unittest.TestCase):
    def test_shard_merge_requires_exact_disjoint_coverage(self):
        common = {
            "schema": 2,
            "description": "radius",
            "semantics": "reachability-admitted-minus-trivial-v3",
            "radius_to_power_substate": {"1": 0, "2": 1, "3": 2},
            "audit_binary_sha256": "a" * 64,
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "manifest.json"
            first = root / "first.json"
            second = root / "second.json"
            manifest.write_text(
                json.dumps({"files": {"a.uftb": {}, "b.uftb": {}}}),
                encoding="utf-8",
            )
            first.write_text(
                json.dumps(common | {"files": {"a.uftb": {"radii": {}}}}),
                encoding="utf-8",
            )
            second.write_text(
                json.dumps(common | {"files": {"b.uftb": {"radii": {}}}}),
                encoding="utf-8",
            )
            merged = merge_audits.merge(manifest, [first, second])
            self.assertEqual({"a.uftb", "b.uftb"}, set(merged["files"]))

            second.write_text(
                json.dumps(common | {"files": {"a.uftb": {"radii": {}}}}),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "duplicate radius record"):
                merge_audits.merge(manifest, [first, second])

            first.write_text(
                json.dumps(
                    common
                    | {"files": {"a.uftb": {"excluded": True}}}
                ),
                encoding="utf-8",
            )
            second.write_text(
                json.dumps(common | {"files": {"b.uftb": {"radii": {}}}}),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                ValueError, "exclusion-orientation mismatch"
            ):
                merge_audits.merge(manifest, [first, second])

    def test_universal_forced_outcomes_have_distinct_cell_kinds(self):
        win = plot.classify(
            plot.parse_wdl("100 / 4 [4] / 0"),
            plot.parse_wdl("80 / 9 [9] / 0"),
            allow_loss=True,
        )
        self.assertEqual("win", win.kind)
        self.assertEqual("Win", plot.cell_text(win))
        self.assertEqual("#5FAF32", plot.COLORS[win.kind])

        loss = plot.classify(
            plot.parse_wdl("0 / 100 / 3 [3]"),
            plot.parse_wdl("0 / 80 / 7 [7]"),
            allow_loss=True,
        )
        self.assertEqual("loss", loss.kind)
        self.assertEqual("Loss", plot.cell_text(loss))
        self.assertEqual("#D15B3B", plot.COLORS[loss.kind])

        self.assertEqual(
            "win_star",
            plot.classify(plot.WDL(10, 0, 0), plot.WDL(9, 0, 1), True).kind,
        )
        self.assertEqual(
            "loss_star",
            plot.classify(plot.WDL(0, 9, 1), plot.WDL(0, 10, 0), True).kind,
        )

    def test_trivial_subtraction_drives_cell_kind_and_display(self):
        # Raw counts would be mixed because both starts contain a loss.  Once
        # the bracketed trivial losses are removed, the same row is a forced
        # win tier.  This guards both the printed percentages and, crucially,
        # the color selected from Cell.kind.
        first = plot.parse_wdl("990 / 10 [10] / 0")
        second = plot.parse_wdl("900 / 10 [10] / 90")
        cell = plot.classify(first, second, allow_loss=False)
        self.assertEqual(plot.WDL(990, 0, 0), first)
        self.assertEqual(plot.WDL(900, 0, 90), second)
        self.assertEqual("win_star", cell.kind)
        self.assertEqual("Win*", plot.cell_text(cell))
        self.assertEqual("#91C655", plot.COLORS[cell.kind])

    def test_cell_text_collapses_identical_display_percentages(self):
        cell = plot.Cell(
            "mixed",
            plot.WDL(99, 0, 1),
            plot.WDL(990, 0, 10),
        )
        self.assertEqual("W 99%\nL 0%\nD 1%", plot.cell_text(cell))

        different = plot.Cell(
            "mixed",
            plot.WDL(99, 0, 1),
            plot.WDL(980, 0, 20),
        )
        self.assertEqual("W 99–98%\nL 0–0%\nD 1–2%", plot.cell_text(different))

    def test_radius_rows_extend_berserker_without_renaming_existing_labels(self):
        self.assertEqual("Berserker", plot.PIECE_LABELS["berserker"])
        self.assertEqual(
            tuple(
                f"Berserker (radius {radius}{'+' if radius == 10 else ''})"
                for radius in range(1, 11)
            ),
            tuple(plot.PIECE_LABELS[row] for row in plot.BERSERKER_RADIUS_ROWS),
        )

    def test_radius_summary_preserves_exact_start_order_and_row_view(self):
        document = {
            "schema": 3,
            "semantics": "reachability-admitted-minus-trivial-v3",
            "radius_to_power_substate": {
                str(radius): radius - 1 for radius in range(1, 11)
            },
            "saturated_radius": 10,
            "files": {
                "kberserkerkninja.uftb": {
                    "radii": {
                        str(radius): {
                            "power_substate": radius - 1,
                            "first_starts": {
                                "total": {"wins": 2_500_001, "losses": 6_240_002,
                                          "draws": 7_150_003},
                                "excluded": {"wins": 1, "losses": 2, "draws": 3},
                                "admitted": {"wins": 2_500_000, "losses": 6_240_000,
                                             "draws": 7_150_000},
                                "trivial": {"wins": 420, "losses": 1_184,
                                            "draws": 2_680},
                                "display": {"wins": 2_499_580, "losses": 6_238_816,
                                            "draws": 7_147_320},
                            },
                            "second_starts": {
                                "total": {"wins": 9_870_001, "losses": 6_002,
                                          "draws": 3_850_003},
                                "excluded": {"wins": 1, "losses": 2, "draws": 3},
                                "admitted": {"wins": 9_870_000, "losses": 6_000,
                                             "draws": 3_850_000},
                                "trivial": {"wins": 3_802, "losses": 320,
                                            "draws": 2_018},
                                "display": {"wins": 9_866_198, "losses": 5_680,
                                            "draws": 3_847_982},
                            },
                        } for radius in range(1, 11)
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
        ledger = plot.read_summary(ROOT / "tablebases" / "README.md")

        self.assertEqual(3, document["schema"])
        self.assertEqual(
            {str(radius): radius - 1 for radius in range(1, 11)},
            document["radius_to_power_substate"],
        )
        self.assertEqual(10, document["saturated_radius"])
        self.assertEqual(43, len(document["files"]))
        self.assertEqual(420, len(radii))
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
        for record_name, record in document["files"].items():
            if record.get("excluded"):
                continue
            self.assertEqual(
                {str(radius) for radius in range(1, 11)},
                set(record["radii"]),
            )
            expected = ledger[record_name]
            for key, wanted in (
                ("first_starts", expected.first_starts),
                ("second_starts", expected.second_starts),
            ):
                self.assertEqual(
                    {"wins": wanted.wins, "losses": wanted.losses,
                     "draws": wanted.draws},
                    {
                        field: sum(
                            record["radii"][str(radius)][key]["display"][field]
                            for radius in range(1, 11)
                        )
                        for field in ("wins", "losses", "draws")
                    },
                    (record_name, key),
                )

        self.assertIn(("kberserkerangelk.uftb", 1), radii)
        self.assertIn(("kberserkerkberserker.uftb", 3), radii)
        for record in document["files"].values():
            for radius in record.get("radii", {}).values():
                for key in ("first_starts", "second_starts"):
                    side = radius[key]
                    for field in ("wins", "losses", "draws"):
                        self.assertEqual(
                            side["display"][field],
                            side["admitted"][field] - side["trivial"][field],
                        )

    def test_prince_radius_cells_use_only_exact_turn_boundaries(self):
        path = ROOT / "tablebases" / "berserker-radius-summary.json"
        document = json.loads(path.read_text(encoding="utf-8"))
        same = document["files"]["kberserkerprincek.uftb"]
        opposed = document["files"]["kberserkerkprince.uftb"]
        self.assertEqual(
            "turn-boundary-continuation-none", same["reporting_scope"]
        )
        self.assertEqual(
            "turn-boundary-continuation-none", opposed["reporting_scope"]
        )
        self.assertEqual(
            [14_519_136, 12_338_172, 9_715_656],
            [same["radii"][str(radius)]["first_starts"]["display"]["wins"]
             for radius in (1, 2, 3)],
        )
        self.assertEqual(
            [12_526_552, 6_603_544, 4_017_722],
            [opposed["radii"][str(radius)]["second_starts"]["display"]["wins"]
             for radius in (1, 2, 3)],
        )

        catalog = plot.OutcomeCatalog(
            plot.read_summary(ROOT / "tablebases" / "README.md"),
            plot.read_berserker_radii(path),
        )
        for radius in range(1, 9):
            self.assertEqual(
                "win",
                catalog.together_row(
                    f"berserker_radius_{radius}", "prince"
                ).kind,
            )
        for radius in (9, 10):
            self.assertEqual(
                "unknown",
                catalog.together_row(
                    f"berserker_radius_{radius}", "prince"
                ).kind,
            )

    def test_prince_radius_audit_requests_turn_boundary_scope(self):
        record = {
            "primary": "berserker",
            "secondary": "prince",
            "opposing": False,
        }
        command = audit.concrete_command(
            Path("/audit"),
            record,
            Path("table.uftb"),
            4,
        )
        self.assertIn("--audit-turn-boundary-reachability", command)
        self.assertNotIn("--audit-reachability", command)
        self.assertFalse(audit.reusable_audit(
            "reachability_primary_substate substate 0 side 0 unknown 0 "
            "win 1 loss 0 draw 0\n",
            record,
        ))
        self.assertTrue(audit.reusable_audit(
            "reachability_scope turn_boundary\n"
            "reachability_primary_substate substate 0 side 0 unknown 0 "
            "win 1 loss 0 draw 0\n",
            record,
        ))

    def test_identical_berserker_intersections_repeat_same_team_outcome(self):
        catalog = plot.OutcomeCatalog(
            plot.read_summary(ROOT / "tablebases" / "README.md"), {})
        for radius in range(1, 11):
            self.assertEqual(
                "win",
                catalog.together_row(
                    f"berserker_radius_{radius}", "berserker"
                ).kind,
            )
        self.assertEqual(
            "unknown",
            catalog.opposed_row("berserker_radius_1", "berserker").kind,
        )

    def test_level_audit_parser_preserves_all_ten_concrete_substates(self):
        lines = []
        for substate in range(10):
            for side in range(2):
                total = [0, 100 + substate + side, 20, 30]
                excluded = [0, 10, 2, 3]
                trivial = [0, 5, 1, 2]
                for suffix, values in (("_total", total), ("", excluded),
                                       ("_trivial", trivial)):
                    lines.append(
                        f"reachability_primary_substate{suffix} "
                        f"substate {substate} side {side} unknown {values[0]} "
                        f"win {values[1]} loss {values[2]} draw {values[3]}"
                    )
        radii = level_audit.parse_counts(
            "\n".join(lines),
            {"primary": "berserker", "secondary": "rook"},
            False,
        )
        self.assertEqual({str(radius) for radius in range(1, 11)}, set(radii))
        self.assertEqual(9, radii["10"]["power_substate"])
        self.assertEqual(
            {"wins": 94, "losses": 17, "draws": 25},
            radii["10"]["first_starts"]["display"],
        )

    def test_level_audit_information_parser_uses_berserker_slot(self):
        lines = []
        # Jester has one tablebase substate, Berserker has ten.
        for combined in range(10):
            for side in range(2):
                for kind, values in (
                    ("admitted", [0, combined + 10, 4, 3]),
                    ("excluded", [0, 2, 1, 1]),
                    ("trivial", [0, 1, 1, 1]),
                ):
                    lines.append(
                        f"information_reachability_substate_{kind} "
                        f"substate {combined} side {side} unknown {values[0]} "
                        f"win {values[1]} loss {values[2]} draw {values[3]}"
                    )
        radii = level_audit.parse_counts(
            "\n".join(lines),
            {"primary": "jester", "secondary": "berserker"},
            True,
        )
        self.assertEqual(
            {"wins": 18, "losses": 3, "draws": 2},
            radii["10"]["second_starts"]["display"],
        )

    def test_concrete_parser_subtracts_unreachable_per_substate(self):
        text = "\n".join((
            "reachability_primary_substate substate 0 side 0 unknown 0 win 3 loss 5 draw 7",
            "reachability_primary_substate_total substate 0 side 0 unknown 0 win 13 loss 25 draw 37",
            "reachability_primary_substate_trivial substate 0 side 0 unknown 0 win 2 loss 3 draw 5",
            "reachability_primary_substate substate 0 side 1 unknown 0 win 11 loss 13 draw 17",
            "reachability_primary_substate_total substate 0 side 1 unknown 0 win 31 loss 43 draw 57",
            "reachability_primary_substate_trivial substate 0 side 1 unknown 0 win 3 loss 4 draw 7",
        ))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "audit.txt"
            path.write_text(text, encoding="utf-8")
            rows = audit.parse_concrete(path, "primary")
        self.assertEqual(
            {"admitted": ([0, 10, 20, 30], [0, 20, 30, 40]),
             "trivial": ([0, 2, 3, 5], [0, 3, 4, 7]),
             "display": ([0, 8, 17, 25], [0, 17, 26, 33])},
            rows[0],
        )

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
        manifest = prepare.build(ROOT / "tablebases" / "README.md")
        record = manifest["files"][
            "kberserkerk.uftb"]
        self.assertEqual(
            "e296deebc6d0f4ef2c53e14eab86fd1c2bbc9954a6051ffca5ce7b9623b08263",
            record["expected_sha256"],
        )
        self.assertEqual(
            "f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1",
            record["expected_payload_sha256"],
        )
        self.assertTrue(
            manifest["files"]["kberserkerberserkerk.uftb"]["excluded"]
        )
        self.assertFalse(
            manifest["files"]["kberserkerkberserker.uftb"]["excluded"]
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
        self.assertEqual(([0, 1, 0, 1], [0, 1, 0, 1]),
                         rows[0]["display"])
        self.assertEqual(([0, 0, 1, 0], [0, 0, 1, 0]),
                         rows[1]["display"])

    def test_information_trivial_import_subtracts_authenticated_substates(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            summary_path = root / "summary.json"
            receipt_path = root / "sample.receipt.json"
            sidecar_path = root / "sample.information-trivial-v2.txt"
            summary_path.write_text(json.dumps({
                "files": {"sample.uftb": {
                    "overlay_sha256": "b" * 64,
                    "trivial_semantics": "not-audited-information-overlay",
                    "radii": {str(radius): {
                        "power_substate": radius - 1,
                        "first_starts": {
                            "admitted": {"wins": 10, "losses": 2, "draws": 3},
                            "trivial": {"wins": 0, "losses": 0, "draws": 0},
                            "display": {"wins": 10, "losses": 2, "draws": 3},
                        },
                        "second_starts": {
                            "admitted": {"wins": 4, "losses": 8, "draws": 3},
                            "trivial": {"wins": 0, "losses": 0, "draws": 0},
                            "display": {"wins": 4, "losses": 8, "draws": 3},
                        },
                    } for radius in (1, 2, 3)},
                }},
            }), encoding="utf-8")
            sidecar_path.write_text(
                "information_reachability_binding source_sha256 " + "a" * 64 +
                " model_sha256 " + "c" * 64 + "\n" +
                "information_trivial_overlay_sha256 " + "b" * 64 + "\n" +
                "information_trivial_binary_sha256 " + "d" * 64 + "\n" +
                "information_trivial_source_bundle_sha256 " + "e" * 64 + "\n",
                encoding="utf-8")
            detail = {}
            totals = {str(side): {
                bucket: {kind: 0 for kind in import_radius_trivial.KINDS}
                for bucket in ("admitted", "excluded", "trivial")}
                for side in range(2)}
            for substate in range(10):
                detail[str(substate)] = {}
                for side in range(2):
                    admitted = ({"unknown": 0, "win": 10, "loss": 2, "draw": 3}
                                if side == 0 else
                                {"unknown": 0, "win": 4, "loss": 8, "draw": 3})
                    trivial = {"unknown": 0, "win": 0, "loss": 2, "draw": 3}
                    values = {
                        "admitted": admitted,
                        "excluded": {kind: 0 for kind in import_radius_trivial.KINDS},
                        "trivial": trivial,
                    }
                    detail[str(substate)][str(side)] = values
                    for bucket, counts in values.items():
                        for kind, count in counts.items():
                            totals[str(side)][bucket][kind] += count
            receipt_path.write_text(json.dumps({
                "schema": "ultimate-information-trivial-receipt-v2",
                "filename": "sample.uftb",
                "substates": 10,
                "source_sha256": "a" * 64,
                "overlay_sha256": "b" * 64,
                "model_sha256": "c" * 64,
                "binary_sha256": "d" * 64,
                "source_bundle_sha256": "e" * 64,
                "sidecar_sha256": import_radius_trivial.sha256(sidecar_path),
                "s3_key": "sidecar-key",
                "s3_version_id": "sidecar-version",
                "counts": totals,
                "substate_counts": detail,
            }), encoding="utf-8")
            import_radius_trivial.import_receipt(
                summary_path, receipt_path, sidecar_path,
                "receipt-key", "receipt-version")
            record = json.loads(summary_path.read_text())["files"]["sample.uftb"]
            self.assertEqual("authenticated-per-substate-v3",
                             record["trivial_semantics"])
            for radius in record["radii"].values():
                self.assertEqual(
                    {"wins": 10, "losses": 0, "draws": 0},
                    radius["first_starts"]["display"])
                self.assertEqual(
                    {"wins": 4, "losses": 6, "draws": 0},
                    radius["second_starts"]["display"])


if __name__ == "__main__":
    unittest.main()
