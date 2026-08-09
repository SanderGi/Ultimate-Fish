import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
SPEC = importlib.util.spec_from_file_location(
    "tb_plan", ROOT / "tools" / "plan_ultimate_tablebases.py")
assert SPEC and SPEC.loader
tb = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = tb
SPEC.loader.exec_module(tb)
SUMMARY_SPEC = importlib.util.spec_from_file_location(
    "tb_summary", ROOT / "tools" / "summarize_ultimate_tablebases.py")
assert SUMMARY_SPEC and SUMMARY_SPEC.loader
summary = importlib.util.module_from_spec(SUMMARY_SPEC)
sys.modules[SUMMARY_SPEC.name] = summary
SUMMARY_SPEC.loader.exec_module(summary)
SHARD_SPEC = importlib.util.spec_from_file_location(
    "tb_shards", ROOT / "tools" / "ultimate_tablebase_shards.py")
assert SHARD_SPEC and SHARD_SPEC.loader
shards = importlib.util.module_from_spec(SHARD_SPEC)
sys.modules[SHARD_SPEC.name] = shards
SHARD_SPEC.loader.exec_module(shards)
README_SPEC = importlib.util.spec_from_file_location(
    "tb_readme", ROOT / "tools" / "update_ultimate_tablebase_readme.py")
assert README_SPEC and README_SPEC.loader
readme = importlib.util.module_from_spec(README_SPEC)
sys.modules[README_SPEC.name] = readme
README_SPEC.loader.exec_module(readme)


class TablebasePlanTests(unittest.TestCase):
    def test_horizontal_symmetry_counts(self):
        self.assertEqual(tb.placement_states(1), 492_960)
        self.assertEqual(tb.placement_states(2), 37_957_920)
        self.assertEqual(tb.placement_states(2, identical_pair=True), 18_978_960)

    def test_all_decisive_single_material_is_planned(self):
        planned = {record["class"] for record in tb.inventory()
                   if record["phase"] == "kings+1"}
        expected = {f"K{piece.name}vK" for piece in tb.PIECES if piece.decisive}
        self.assertEqual(planned, expected)
        self.assertNotIn("KbishopvK", planned)
        self.assertNotIn("KknightvK", planned)

    def test_stateless_pair_material_filter(self):
        planned = {record["class"] for record in tb.inventory()
                   if record["phase"] == "kings+2-stateless"}
        stateless = [piece for piece in tb.PIECES if piece.stateless]
        possible = len(stateless) * (len(stateless) + 1)
        self.assertEqual(possible, 182)
        self.assertEqual(len(planned), 160)
        self.assertEqual(possible - len(planned), 22)
        self.assertIn("KknightturtlevK", planned)
        self.assertIn("KbishopmagevK", planned)
        self.assertIn("KbishopbishopvK", planned)
        self.assertNotIn("KmagefishermanvK", planned)
        self.assertNotIn("KknightvKturtle", planned)

    def test_fisherman_parasite_catalog_preserves_header_owner_order(self):
        records = {record["filename"]: record for record in tb.inventory()}
        record = records["kfishermankparasite.uftb"]
        self.assertEqual(record["class"], "KfishermanvKparasite")
        self.assertEqual(record["primary"], "fisherman")
        self.assertEqual(record["secondary"], "parasite")
        self.assertNotIn("kparasitekfisherman.uftb", records)

    def test_every_planned_file_is_sharded_below_github_limit(self):
        for record in tb.inventory():
            per_shard = (record["packed_bytes"] + record["shards"] - 1) // record["shards"]
            self.assertLess(per_shard, tb.GITHUB_FILE_LIMIT)

    def test_stateful_inventory_has_only_the_approved_narrow_overrun(self):
        records = tb.inventory()
        total = sum(record["packed_bytes"] for record in records)
        self.assertGreater(total, tb.DEFAULT_BUDGET)
        self.assertLessEqual(total, tb.AUTHORIZED_BUDGET)
        admitted = [record for record in records
                    if record["phase"] == "kings+2-stateful"]
        self.assertEqual(len(admitted), 29)
        represented = {str(record[side]) for record in admitted
                       for side in ("primary", "secondary")}
        self.assertTrue({"pawn", "berserker", "ghost", "penguin", "sniper",
                         "prince", "checker"}.issubset(represented))
        self.assertTrue(represented.isdisjoint(
            {"devil", "sludge", "copycat", "angel"}))
        requested = {record["filename"] for record in records
                     if record["phase"] == "kings+2-requested"}
        self.assertEqual(requested, {
            "kcopycatkbishop.uftb", "kdragonkpenguin.uftb"})

    def test_compound_copycat_indexes_one_anchor_not_both_halves(self):
        records = {record["filename"]: record for record in tb.inventory()}
        copycat = records["kcopycatkbishop.uftb"]
        self.assertEqual(copycat["states"], 75_915_840)
        self.assertEqual(copycat["states"], tb.compound_copycat_pair_states())

    def test_stateful_candidate_closures_are_explicit(self):
        candidates = tb.stateful_candidates()
        self.assertTrue(candidates)
        for record in candidates:
            pieces = {str(record["primary"]), str(record["secondary"])}
            self.assertFalse(pieces & {"devil", "sludge", "copycat", "angel"})

    def test_krk_side_split_and_illegal_annotation(self):
        totals, illegal = summary.summary(ROOT / "tablebases" / "krk.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "361,648 (131,312) / 0 / 0")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "0 (41,808) / 414,300 / 36,852")

    def test_prince_continuation_losses_are_annotated_as_illegal(self):
        totals, illegal = summary.summary(ROOT / "tablebases" / "kprincek.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "867,040 (80,344) / 0 (38,536) / 0")
        self.assertEqual(illegal[0], [0, 80_344, 38_536, 0])
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "0 (41,808) / 414,344 (492,944) / 36,808 (16)")

    def test_forced_checker_substate_requires_its_owners_turn(self):
        self.assertEqual(summary.continuation_mismatches(8, 21, 0, 4, 0), set())
        self.assertEqual(summary.continuation_mismatches(8, 21, 0, 4, 1), {1, 3})
        self.assertEqual(summary.continuation_mismatches(8, 21, 1, 4, 0), {1, 3})
        self.assertEqual(summary.continuation_mismatches(8, 21, 1, 4, 1), set())

    def test_forced_checker_requires_an_available_continued_jump(self):
        totals, illegal = summary.summary(
            ROOT / "tablebases" / "kbombcheckerk.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "29,074,749 (9,725,859) / 0 (3,286,402) / "
                         "0 (33,828,830)")

    def test_sniper_pre_turn_change_cooldown_is_unreachable(self):
        totals, illegal = summary.summary(ROOT / "tablebases" / "ksniperk.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "5,014 (194,552) / 0 / 872,222 (900,052)")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "0 (167,232) / 1,210 (1,066) / 901,094 (901,238)")

    def test_penguin_wins_with_impossible_aura_turns_are_unreachable(self):
        totals, illegal = summary.summary(ROOT / "tablebases" / "kpenguink.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "384 (48,064) / 192 / 489,112 (448,168)")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "796 (43,568) / 1,760 / 527,180 (412,616)")

    def test_native_stateful_audit_catches_promotion_and_hidden_ghost_states(self):
        totals, illegal = summary.summary(ROOT / "tablebases" / "kpawnk.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "576,806 (101,696) / 0 / 217,258 (90,160)")
        totals, illegal = summary.summary(ROOT / "tablebases" / "kghostk.uftb")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "0 (83,616) / 865,512 / 36,792")

    def test_invalid_giant_footprints_are_unreachable_draw_sentinels(self):
        totals, illegal = summary.summary(ROOT / "tablebases" / "kgiantk.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "3,300 (82,476) / 0 / 273,324 (133,860)")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "0 (30,868) / 1,460 / 326,772 (133,860)")
        self.assertEqual(summary.giant_invalid_placements(37_957_920, 1),
                         5_692_260)
        self.assertEqual(summary.giant_invalid_placements(18_978_960, 2),
                         5_024_148)
        self.assertEqual(summary.giant_invalid_placements(37_957_920, 2),
                         10_048_296)

    def test_folded_giant_anchor_payloads_fail_closed_until_regenerated(self):
        catalog = summary.giant_codec_catalog()
        self.assertEqual(len(catalog), 28)
        statuses = [record["status"] for record in catalog.values()]
        verified = statuses.count("verified-anchor-v2")
        stale = statuses.count("stale-anchor-v1-requires-regeneration")
        self.assertGreaterEqual(verified, 2)
        self.assertEqual(verified + stale, 28)
        self.assertTrue(set(statuses) <= {
            "verified-anchor-v2", "stale-anchor-v1-requires-regeneration"})
        for filename, record in catalog.items():
            if record["status"] != "verified-anchor-v2":
                continue
            certificate = record["generation_certificate"]
            self.assertEqual(certificate["bellman_residual"], 0)
            self.assertEqual(
                certificate["win"] + certificate["loss"] +
                certificate["draw"], certificate["states"], filename)
            packaging = record["packaging_certificate"]
            self.assertEqual(packaging["format_version"], 7)
            self.assertEqual(
                packaging["giant_anchor_tag"], "0x32474e4149474655")
        for filename in ("kjestergiantk.uftb", "kjesterkgiant.uftb"):
            record = catalog[filename]
            summary.require_current_giant_codec(
                ROOT / "tablebases" / filename, record["current_sha256"])
        if stale:
            stale_filename = next(
                filename for filename, record in catalog.items()
                if record["status"] == "stale-anchor-v1-requires-regeneration")
            with self.assertRaisesRegex(ValueError, "stale Giant anchor-v1"):
                summary.require_current_giant_codec(
                    ROOT / "tablebases" / stale_filename)
        # K+Giant-v-K retains both horizontal orientations and never used the
        # folded four-model transform.
        summary.require_current_giant_codec(
            ROOT / "tablebases" / "kgiantk.uftb")

    def test_jester_adjacent_king_wins_are_legal(self):
        totals, illegal = summary.summary(ROOT / "tablebases" / "kjesterk.uftb")
        self.assertEqual(illegal[1], [0, 0, 0, 0])
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "412,616 (80,344) / 0 / 0")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "41,808 / 414,344 / 36,808")

    def test_bomb_chain_predecessor_safety_is_outcome_independent(self):
        totals, illegal = summary.summary(ROOT / "tablebases" / "kbombbombk.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "6,651,352 (2,838,050) / 0 (78) / 0")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "0 (804,804) / 8,582,770 (70,112) / 31,794")

    def test_penguin_pairings_keep_causal_turn_artifacts_parenthesized(self):
        totals, illegal = summary.summary(
            ROOT / "tablebases" / "kbombpenguink.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "17,706,778 (5,565,108) / 7,182 (918) / "
                         "53,716 (14,624,218)")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "27,840 (1,799,520) / 19,930,438 (71,756) / "
                         "1,608,118 (14,520,248)")

        totals, illegal = summary.summary(
            ROOT / "tablebases" / "kbombkpenguin.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "6,593,680 (4,340,536) / 406,090 (6,876) / "
                         "12,086,542 (14,524,196)")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "1,559,586 (1,826,880) / 3,972,334 (31,726) / "
                         "15,915,376 (14,652,018)")

    def test_jester_ghost_audit_is_directional_by_jester_owner(self):
        totals, illegal = summary.summary(
            ROOT / "tablebases" / "kjesterghostk.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "31,771,432 (6,186,488) / 0 / 0")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "3,219,216 / 34,734,808 / 3,896")

        totals, illegal = summary.summary(
            ROOT / "tablebases" / "kjesterkghost.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "4,720,184 (6,186,488) / 1,380,558 / 25,670,690")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "13,279,972 / 987,932 / 23,690,016")

    def test_requested_overrun_tables_keep_audited_state_counts(self):
        totals, illegal = summary.summary(
            ROOT / "tablebases" / "kcopycatkbishop.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "5,967,160 (8,221,984) / 0 / "
                         "22,327,336 (1,441,440)")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "0 (7,021,104) / 36,168 / "
                         "29,459,208 (1,441,440)")

        totals, illegal = summary.summary(
            ROOT / "tablebases" / "kdragonkpenguin.uftb")
        self.assertEqual(summary.cell(totals[0], illegal[0]),
                         "6,096,392 (5,578,528) / 112,646 / "
                         "11,651,218 (14,519,136)")
        self.assertEqual(summary.cell(totals[1], illegal[1]),
                         "461,796 (1,813,288) / 931,680 (240) / "
                         "20,125,620 (14,625,296)")

    def test_every_generated_table_has_a_native_reachability_audit(self):
        catalog = summary.reachability_catalog()
        for record in tb.inventory():
            path = ROOT / "tablebases" / str(record["filename"])
            if path.exists():
                self.assertIn(path.name, catalog)

    def test_regular_git_shards_round_trip_and_verify(self):
        payload = bytes(range(251)) * 17
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.uftb"
            path.write_bytes(payload)
            outputs = shards.split(path, limit=333)
            self.assertGreater(len(outputs), 2)
            self.assertLess(path.stat().st_size, len(payload))
            self.assertTrue(all(part.stat().st_size <= 333 for part in outputs[1:]))
            self.assertEqual(shards.read_logical(path), payload)
            import hashlib
            self.assertEqual(shards.logical_sha256(path),
                             hashlib.sha256(payload).hexdigest())

    def test_stateful_side_summary_removes_substate_dimension(self):
        full = tb.placement_states(2)
        identical = tb.placement_states(2, identical_pair=True)
        for placement in (0, 1, 123_456, full - 1):
            expected = summary.kings_for(placement, full, 1)
            for substate in range(4):
                self.assertEqual(
                    summary.kings_for(placement * 4 + substate, full * 4, 4),
                    expected)
        for placement in (0, 1, 123_456, identical - 1):
            expected = summary.kings_for(placement, identical, 1)
            for substate in range(2):
                self.assertEqual(
                    summary.kings_for(placement * 2 + substate, identical * 2, 2),
                    expected)

    def test_incremental_readme_rows_are_keyed_by_logical_filename(self):
        first = "| `krk.uftb` | King+Rook vs King | 1 | 2 | 3 | `abc` |"
        second = "| `kqk.uftb` | King+Queen vs King | 4 | 5 | 6 | `def` |"
        text = "\n".join((readme.START, "| header |", first, second,
                           readme.END))
        self.assertEqual(readme.cached_rows(text), {
            "krk.uftb": first,
            "kqk.uftb": second,
        })


if __name__ == "__main__":
    unittest.main()
