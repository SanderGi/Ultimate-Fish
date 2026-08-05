#!/usr/bin/env python3

import importlib.util
import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch


ROOT = Path(__file__).parents[1]
SPEC = importlib.util.spec_from_file_location(
    "ultimate_phone", ROOT / "tools/ultimate_phone.py")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


class CoordinateTests(unittest.TestCase):
    def test_unity_scene_index_round_trip(self):
        expected = {0: "a1", 9: "a10", 40: "e1", 49: "e10", 79: "h10"}
        for index, square in expected.items():
            self.assertEqual(MODULE.scene_index_to_square(index), square)
            self.assertEqual(MODULE.square_to_scene_index(square), index)

    def test_repetition_key_ignores_only_move_counters(self):
        first = "w;hm=4;fm=7;ep=-;cont=0;forced=-1;king,w,a1"
        repeated = "w;hm=12;fm=19;ep=-;cont=0;forced=-1;king,w,a1"
        different = "b;hm=12;fm=19;ep=-;cont=0;forced=-1;king,w,a1"
        self.assertEqual(
            MODULE.upn_repetition_key(first),
            MODULE.upn_repetition_key(repeated),
        )
        self.assertNotEqual(
            MODULE.upn_repetition_key(first),
            MODULE.upn_repetition_key(different),
        )

    def test_rotated_player_perspective(self):
        self.assertEqual(MODULE.rotate_square("a1"), "h10")
        self.assertEqual(MODULE.rotate_square("h10"), "a1")
        self.assertEqual(MODULE.rotate_square("c8"), "f3")

    def test_fisherman_landing_is_adjacent_on_each_ray(self):
        self.assertEqual(MODULE.adjacent_toward("f8", "f1"), "f7")
        self.assertEqual(MODULE.adjacent_toward("d4", "h8"), "e5")
        with self.assertRaises(ValueError):
            MODULE.adjacent_toward("d4", "f5")

    def test_board_pixel_centers(self):
        geometry = MODULE.BoardGeometry()
        self.assertEqual(geometry.point("a10"), (130, 667))
        self.assertEqual(geometry.point("h1"), (950, 1691))

    def test_engine_move_parser(self):
        self.assertEqual(MODULE.parse_engine_move("e1-e10"), ("e1", "e10", "-"))
        self.assertEqual(MODULE.parse_engine_move("a2@c4"), ("a2", "c4", "@"))
        self.assertEqual(MODULE.parse_engine_move("pass"), ("pass", "pass", "pass"))

    def test_move_time_is_adjustable_and_defaults_to_unlimited(self):
        self.assertEqual(MODULE.controller_movetime("unranked", None), 0)
        self.assertEqual(MODULE.controller_movetime("ranked", None), 0)
        self.assertEqual(MODULE.controller_movetime("cpu", None), 0)
        self.assertEqual(MODULE.controller_movetime("unranked", 7500), 7500)
        self.assertEqual(MODULE.controller_movetime("unranked", 0), 0)
        with self.assertRaisesRegex(ValueError, "cannot be negative"):
            MODULE.controller_movetime("unranked", -1)

    def test_builder_drag_learning_never_targets_outside_edge_cells(self):
        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.army_drag_offsets = {}
        game.verbose = False
        deployment = MODULE.DeploymentGeometry()
        horizontal = game._learn_army_drag_offset(
            "pawn", "h1", (6, 0), deployment
        )
        vertical = game._learn_army_drag_offset(
            "dragon", "g1", (6, 1), deployment
        )
        h1 = deployment.point("h1")
        g1 = deployment.point("g1")
        self.assertLessEqual(h1[0] + horizontal[0], deployment.right - 4)
        self.assertLessEqual(g1[1] + vertical[1], deployment.bottom - 4)

    def test_public_copycat_callbacks_rebuild_one_mirrored_pair(self):
        self.assertEqual(MODULE.public_probe_piece("copycat"), "copycatPair")
        self.assertEqual(MODULE.public_probe_piece("copycatClone"), "copycatPair")
        self.assertEqual(
            MODULE.normalize_copycat_probes((
                ("pawn", "a8"),
                ("copycatPair", "f8"),
                ("copycatPair", "c8"),
            )),
            [("pawn", "a8"), ("copycat", "c8"), ("copycatClone", "f8")],
        )
        with self.assertRaisesRegex(RuntimeError, "no mirrored partner"):
            MODULE.normalize_copycat_probes((("copycatPair", "c8"),))

    def test_native_giant_log_cells_match_engine_footprint_anchors(self):
        position = "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;giant,b,a8"
        event = MODULE.AppEvent("move", "giant", "b9", "a7")
        self.assertTrue(MODULE.native_move_matches(position, "a8-a6", event))
        self.assertFalse(MODULE.native_move_matches(position, "a8-c8", event))


class LogParserTests(unittest.TestCase):
    def test_builder_army_move_reports_native_piece_identity(self):
        dragon = MODULE.parse_unity_line("I/Unity: ArmyMove dragon")
        giant = MODULE.parse_unity_line("I/Unity: ArmyMove")
        self.assertEqual((dragon.kind, dragon.piece), ("army_piece", "dragon"))
        self.assertEqual((giant.kind, giant.piece), ("army_piece", "giant"))

    def test_move_and_selection(self):
        move = MODULE.parse_unity_line("I/Unity: queen(Clone) moves 40 --> 49")
        self.assertEqual(move, MODULE.AppEvent("move", "queen", "e1", "e10",
                                               "I/Unity: queen(Clone) moves 40 --> 49"))
        selected = MODULE.parse_unity_line(
            "I/Unity: checkerKing(Clone)GetAvailableMoves was called..."
        )
        self.assertEqual(selected.kind, "selected")
        self.assertEqual(selected.piece, "checkerKing")

    def test_public_bot_action_diagnostics_parse_without_guessing_target(self):
        source = MODULE.parse_unity_line("I/Unity: from c8")
        target = MODULE.parse_unity_line("I/Unity: to b9")
        piece = MODULE.parse_unity_line("I/Unity: Character devil")
        self.assertEqual((source.kind, source.source), ("bot_from", "c8"))
        self.assertEqual((target.kind, target.target), ("bot_to", "b9"))
        self.assertEqual((piece.kind, piece.piece), ("bot_piece", "devil"))

    def test_attack_name_is_not_truncated_when_clone_suffix_is_optional(self):
        cloned = MODULE.parse_unity_line("I/Unity: attacking queen(Clone)")
        promoted = MODULE.parse_unity_line("I/Unity: attacking queenprefab")
        self.assertEqual((cloned.kind, cloned.piece), ("attack", "queen"))
        self.assertEqual((promoted.kind, promoted.piece), ("attack", "queen"))

    def test_cosmetic_prefab_names_map_to_rules_piece(self):
        selected = MODULE.parse_unity_line(
            "I/Unity: sniperCowBoy(Clone)GetAvailableMoves was called..."
        )
        self.assertEqual(selected.piece, "sniper")
        move = MODULE.parse_unity_line(
            "I/Unity: sniperCowBoy(Clone) moves 22 --> 23"
        )
        self.assertEqual((move.piece, move.source, move.target),
                         ("sniper", "c3", "c4"))
        spaced = MODULE.parse_unity_line(
            "I/Unity: fisherman bear(Clone)GetAvailableMoves was called..."
        )
        self.assertEqual(spaced.piece, "fisherman")
        royal = MODULE.parse_unity_line(
            "I/Unity: kong(Clone)GetAvailableMoves was called..."
        )
        self.assertEqual(royal.piece, "king")
        generated = MODULE.parse_unity_line(
            "I/Unity: undead(Clone)GetAvailableMoves was called..."
        )
        self.assertEqual((generated.kind, generated.piece), ("selected", "minion"))
        penguin = MODULE.parse_unity_line(
            "I/Unity: freeze(Clone)GetAvailableMoves was called..."
        )
        self.assertEqual((penguin.kind, penguin.piece), ("selected", "penguin"))
        promoted = MODULE.parse_unity_line(
            "I/Unity: queenprefabGetAvailableMoves was called..."
        )
        self.assertEqual((promoted.kind, promoted.piece), ("selected", "queen"))
        promoted_move = MODULE.parse_unity_line(
            "I/Unity: queenprefab moves 69 --> 66"
        )
        self.assertEqual(
            (promoted_move.kind, promoted_move.piece,
             promoted_move.source, promoted_move.target),
            ("move", "queen", "g10", "g7"),
        )

    def test_turn_and_terminal_events(self):
        self.assertEqual(MODULE.parse_unity_line(
            "Board:LoadBoard(Boolean, Boolean, List`1)").kind, "board_loaded")
        self.assertEqual(MODULE.parse_unity_line("Board: ChangeTurn Start").kind,
                         "turn_start")
        self.assertEqual(MODULE.parse_unity_line("Board: ChangeTurn End").kind, "turn_end")
        self.assertEqual(MODULE.parse_unity_line("Character:SetUpMyDot()").kind, "dot_ready")
        self.assertEqual(MODULE.parse_unity_line("!!!CompareDragDisplacement").kind,
                         "touch_end")
        self.assertEqual(MODULE.parse_unity_line("Board:OutOfTime()").kind, "out_of_time")
        bomb_death = MODULE.parse_unity_line("I/Unity: Bomb:Die()")
        self.assertEqual((bomb_death.kind, bomb_death.piece), ("dead", "bomb"))
        self.assertEqual(MODULE.parse_unity_line("GameFound MESSAGE").kind,
                         "match_found")
        self.assertEqual(MODULE.parse_unity_line("GameOver MESSAGE").kind,
                         "game_over")
        visible = MODULE.parse_unity_line("I/Unity: Ghost:MakeVis()")
        hidden = MODULE.parse_unity_line("I/Unity: Ghost:MakeInvis()")
        self.assertEqual(visible.kind, "ghost_visible")
        self.assertIsNone(visible.source)
        self.assertEqual(hidden.kind, "ghost_hidden")
        self.assertIsNone(hidden.target)
        self.assertEqual(MODULE.parse_unity_line("OnJoinQueue MESSAGE").kind,
                         "queue_joined")
        points = MODULE.parse_unity_line(
            "GetPoints() - player1 points : 98 - player2 points : 0")
        self.assertEqual(
            (points.kind, points.source, points.target),
            ("army_points", "98", "0"),
        )
        self.assertEqual(MODULE.parse_unity_line("ShopItem:OnPointerClick(x)").kind,
                         "shop_item")
        self.assertEqual(
            MODULE.parse_unity_line("I/Unity: CHECKMATE").source, "checkmate"
        )
        self.assertEqual(
            MODULE.parse_unity_line("Board state repeated 3 times...").source,
            "draw",
        )
        self.assertIsNone(MODULE.parse_unity_line("Character:GetAvailableMoves was called"))

    def test_army_drop_coordinates_include_error_priority_logs(self):
        event = MODULE.parse_unity_line("E/Unity   (21682): 5:1 - 8:0")
        self.assertEqual(
            event,
            MODULE.AppEvent(
                "army_drop", source="5:1", target="8:0",
                raw="E/Unity   (21682): 5:1 - 8:0",
            ),
        )

    def test_public_ranked_protocol_events_discard_payloads(self):
        expected = {
            "Board:LoadBoardDraft()": "draft_board_loaded",
            "OnStartGame MESSAGE private payload": "start_game",
            "OnSpawnPieceGroup MESSAGE private placement": "draft_pick_committed",
            "OnBanCharacter MESSAGE public ban": "draft_ban_committed",
        }
        for line, kind in expected.items():
            event = MODULE.parse_unity_line(line)
            self.assertEqual(event.kind, kind)
            self.assertIsNone(event.piece)
            self.assertIsNone(event.source)
            self.assertIsNone(event.target)

        self.assertEqual(
            MODULE.parse_unity_line(
                "I/Unity: NetworkManager:OnBanCharacter(Type)"
            ).kind,
            "draft_ban_committed",
        )
        spawn = MODULE.parse_unity_line("I/Unity (15933): queen 6:1")
        self.assertEqual(
            (spawn.kind, spawn.piece, spawn.source),
            ("draft_piece_spawn", "queen", "6:1"),
        )
        hidden = MODULE.parse_unity_line("ghost 4:2")
        self.assertEqual(
            (hidden.kind, hidden.piece, hidden.source, hidden.raw),
            ("draft_piece_spawn", "ghost", None, ""),
        )
        self.assertEqual(
            MODULE.parse_unity_line("OnSanityCheck MESSAGE").kind,
            "sanity_check",
        )
        turn = MODULE.parse_unity_line("myBoard.turn != team: False")
        self.assertEqual(
            (turn.kind, turn.source),
            ("draft_turn_probe", "local"),
        )
        opponent_turn = MODULE.parse_unity_line("myBoard.turn != team: True")
        self.assertEqual(opponent_turn.source, "opponent")
        ban = MODULE.parse_unity_line("TEXURE ASSIGNED TO Giant")
        self.assertEqual(
            (ban.kind, ban.piece),
            ("draft_ban_piece", "giant"),
        )
        compatible = MODULE.parse_unity_line("TEXTURE ASSIGNED TO Queen")
        self.assertEqual(
            (compatible.kind, compatible.piece),
            ("draft_ban_piece", "queen"),
        )

    def test_structured_start_payload_is_validated_and_ignores_skin(self):
        line = (
            'I/Unity: {"type":1,"target":"OnStartGame","arguments":[{'
            '"model_Pieces":[{"type":4,"team":0,"skin":99,"x":1,'
            '"y":0,"action":2,"cd":3,"freezeCount":1,"turnMoved":7,'
            '"army":true}],"maxPoints":100}]}'
        )
        event = MODULE.parse_unity_line(line)
        self.assertEqual(event.kind, "start_game")
        self.assertEqual(
            event.payload,
            MODULE.OnlineStartState((
                MODULE.ModelPieceRecord(
                    "queen", 0, 1, 0, 2, 3, 1, 7, True
                ),
            ), 100),
        )

    def test_structured_ranked_group_payload(self):
        line = (
            'OnSpawnPieceGroup {"target":"OnSpawnPieceGroup",'
            '"arguments":[[{"type":"Ghost","team":1,"x":4,"y":8}]]}'
        )
        event = MODULE.parse_unity_line(line)
        self.assertEqual(event.kind, "draft_pick_committed")
        self.assertEqual(
            event.payload,
            (MODULE.ModelPieceRecord("ghost", 1, 4, 8),),
        )


class OnlineStateSanitizerTests(unittest.TestCase):
    def test_infers_rotated_local_team_and_giant_anchor(self):
        state = MODULE.OnlineStartState((
            MODULE.ModelPieceRecord("king", 1, 7, 9),
            MODULE.ModelPieceRecord("giant", 1, 2, 8),
            MODULE.ModelPieceRecord("king", 0, 0, 0),
        ), 1)
        expected = [("king", "a1"), ("giant", "e1")]
        self.assertEqual(MODULE.infer_online_local_team(state, expected), 1)

    def test_sanitizer_masks_enemy_ghost_and_royal_identity(self):
        state = MODULE.OnlineStartState((
            MODULE.ModelPieceRecord("king", 0, 0, 0),
            MODULE.ModelPieceRecord("queen", 0, 1, 0, 2, 3, 1, 7),
            MODULE.ModelPieceRecord("king", 1, 7, 9),
            MODULE.ModelPieceRecord("jester", 1, 6, 9),
            MODULE.ModelPieceRecord("rook", 1, 5, 9, 1, 2, 3, 4),
            MODULE.ModelPieceRecord("ghost", 1, 4, 8),
        ), 38)
        own, positions = MODULE.sanitize_online_start(
            state, 0, belief_limit=8
        )
        self.assertEqual(own, [("king", "a1"), ("queen", "b1")])
        self.assertGreater(len(positions), 2)
        ghost_squares = set()
        royal_assignments = set()
        for position in positions:
            parsed = MODULE.parse_upn_pieces(position)
            queen = next(item for item in parsed if item[0:3] == (
                "queen", "w", "b1"))
            rook = next(item for item in parsed if item[0:3] == (
                "rook", "b", "f10"))
            self.assertEqual(queen[3][0:3], ["2", "3", "1"])
            self.assertEqual(rook[3][0:3], ["1", "2", "3"])
            ghosts = [item for item in parsed if item[0:2] == ("ghost", "b")]
            self.assertEqual(len(ghosts), 1)
            self.assertEqual(ghosts[0][3][5], "0")
            ghost_squares.add(ghosts[0][2])
            royal_assignments.add(tuple(
                (piece, square) for piece, color, square, _state in parsed
                if color == "b" and piece in ("king", "jester")
            ))
        self.assertGreater(len(ghost_squares), 1)
        self.assertEqual(len(royal_assignments), 2)


class EngineDraftProtocolTests(unittest.TestCase):
    def client_with_lines(self, *lines):
        client = MODULE.EngineClient.__new__(MODULE.EngineClient)
        commands = []
        responses = iter(lines)
        client.send = commands.append
        client.until = lambda _prefixes: next(responses)
        return client, commands

    def test_status_parser(self):
        client, commands = self.client_with_lines(
            "draft phase 7 player black action pick min 65 max 90 white 40 black 25")
        self.assertEqual(client.draft_status(), {
            "phase": 7, "player": "black", "action": "pick",
            "min": 65, "max": 90, "white": 40, "black": 25,
        })
        self.assertEqual(commands, ["draft status"])

    def test_auto_and_exact_public_ban(self):
        client, commands = self.client_with_lines(
            "draftauto dragon jester", "draftok", "draftok")
        self.assertEqual(client.draft_auto(), ["dragon", "jester"])
        client.draft_choose("queen")
        client.draft_commit()
        self.assertEqual(commands, [
            "draft auto", "draft choose queen", "draft commit",
        ])


class RankedDraftControllerTests(unittest.TestCase):
    class FakeImage:
        width = 1080
        height = 2400

    class FakeAdb:
        def screenshot(self):
            return RankedDraftControllerTests.FakeImage()

    class FakeEvents:
        def __init__(self, responses, precompleted_bans=()):
            self.responses = iter(responses)
            self.draft_generation = 0
            self.ban_pieces = list(precompleted_bans)
            self.ban_count = len(self.ban_pieces)

        def drain(self):
            return None

        def wait(self, kinds, _timeout, predicate=None):
            response = next(self.responses)
            if response is TimeoutError:
                raise TimeoutError("scripted quiet period")
            accepted = {kinds} if isinstance(kinds, str) else set(kinds)
            if response.kind not in accepted:
                raise AssertionError(
                    f"scripted {response.kind} not accepted by {sorted(accepted)}"
                )
            if predicate is not None and not predicate(response):
                raise AssertionError("scripted event failed predicate")
            if response.kind == "draft_pick_committed":
                self.draft_generation += 1
            elif response.kind == "draft_ban_piece" and response.piece:
                self.ban_pieces.append(response.piece)
            return response

        def ranked_spawn_snapshot(self):
            return (
                self.draft_generation,
                True,
                (MODULE.AppEvent("draft_piece_spawn", "king", "0:0"),),
            )

        def ranked_bans_completed(self):
            return self.ban_count

        def ranked_ban_snapshot(self):
            return tuple(self.ban_pieces)

    class FakeEngine:
        ACTIONS = (
            "ban", "ban", "pick", "pick", "ban", "ban",
            "pick", "pick", "ban", "ban", "pick", "pick",
        )
        LOCAL_CHOICES = {
            1: ["ghost"],
            3: ["jester", "dragon"],
            5: ["parasite"],
            7: ["penguin"],
            9: ["sniper"],
            11: ["rook"],
        }

        def __init__(self):
            self.phase = 0
            self.pending = []

        def draft_new(self):
            self.phase = 0
            self.pending = []

        def draft_status(self):
            if self.phase >= 12:
                return {"phase": 12, "action": "complete"}
            return {"phase": self.phase, "action": self.ACTIONS[self.phase]}

        def draft_auto(self):
            choices = list(self.LOCAL_CHOICES[self.phase])
            self.phase += 1
            return choices

        def draft_choose(self, piece):
            self.pending.append(piece)

        def draft_commit(self):
            self.pending.clear()
            self.phase += 1

    @staticmethod
    def _event(line):
        event = MODULE.parse_unity_line(line)
        assert event is not None
        return event

    def test_local_ban_selects_requested_pot_before_fixed_control(self):
        class BanAdb:
            def __init__(self):
                self.taps = []

            @staticmethod
            def screenshot():
                return object()

            def tap_sync(self, x, y):
                self.taps.append((x, y))

        class BanEvents:
            @staticmethod
            def drain():
                return None

            @staticmethod
            def wait(_kinds, _timeout):
                return MODULE.AppEvent("draft_turn_probe", source="local")

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.adb = BanAdb()
        game.events = BanEvents()
        game.draft_pots = {"prince": (941, 1105)}
        game._pot_ban_control = lambda _image, _piece: (157, 1967)
        game._ban_ranked_piece("prince")
        self.assertEqual(game.adb.taps, [(941, 1105), (157, 1967)])

    def test_actual_ban_callbacks_drive_all_twelve_ranked_phases(self):
        ban = "I/Unity: NetworkManager:OnBanCharacter(Type)"
        pick = "I/Unity: OnSpawnPieceGroup MESSAGE"
        responses = [
            self._event(ban),
            self._event(pick),
            self._event(pick),
            self._event(ban), MODULE.AppEvent("draft_ban_piece", piece="bomb"),
            self._event(ban),
            self._event(pick),
            self._event(pick),
            self._event(ban), MODULE.AppEvent("draft_ban_piece", piece="ninja"),
            self._event(ban),
            self._event(pick),
            self._event(pick), MODULE.AppEvent("board_loaded"),
        ]
        first = [
            ("king", "a10"), ("queen", "b10"), ("king", "c10"),
        ]
        middle_group = [("pawn", "d10")]
        final_group = [
            ("rook", "e10"), ("rook", "f10"), ("rook", "g10"),
            ("rook", "h10"), ("rook", "a9"), ("turtle", "b9"),
            ("giant", "c8"), ("giant", "d8"),
            ("giant", "c9"), ("giant", "d9"),
        ]

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.adb = self.FakeAdb()
        # Captured winning trace: remote Angel phase zero completed while all
        # 24 addressable pots were still being calibrated.
        game.events = self.FakeEvents(responses, precompleted_bans=("angel",))
        game.engine = self.FakeEngine()
        game.geometry = MODULE.BoardGeometry()
        game.draft_pots = {piece: (index, index) for index, piece in enumerate(
            MODULE.POT_SORT_ORDER
        )}
        game.ranked_enemy_roster = MODULE.Counter()
        game.ranked_enemy_king_candidates = None
        game.ranked_enemy_snapshots = []
        game.ranked_local_points = 0
        game.online_local_team = None
        game.own_team = []
        game.verbose = False
        game.log = lambda _message: None
        game._ranked_is_ivory = lambda: False
        game._ban_ranked_piece = lambda _piece, _timeout=3.0: None
        game._tap_draft_control = lambda _labels, _timeout=4.0: "LOCK"
        game._place_ranked_piece = lambda _piece, square, _ivory: square
        game._ranked_committed_points = Mock(side_effect=(
            (0, 27),       # remote opening group
            (10, 27), (25, 27),  # local Jester + Dragon
            (25, 30),      # remote middle group
            (40, 30),      # local Penguin
            (40, 100),     # remote final group
            (53, 100),     # local Rook
        ))
        game.probe_enemy = Mock()

        with patch.object(MODULE, "ranked_spawn_public", side_effect=(
            first, middle_group, final_group,
        )), patch.object(MODULE.time, "sleep", return_value=None):
            team = game.run_ranked_draft()

        self.assertEqual(game.engine.phase, 12)
        self.assertEqual(game.online_local_team, 1)
        self.assertTrue(team)
        self.assertEqual(game.ranked_enemy_roster["queen"], 1)
        self.assertEqual(game.ranked_enemy_roster["jester"], 1)
        self.assertEqual(game.ranked_enemy_roster["rook"], 5)
        self.assertEqual(game.ranked_enemy_roster["giant"], 1)
        self.assertEqual(game.ranked_enemy_king_candidates, {"a10", "c10"})
        game.probe_enemy.assert_not_called()

    def test_native_turn_probe_identifies_phase_zero_owner(self):
        class ProbeAdb:
            def __init__(self):
                self.taps = []

            def tap_sync(self, x, y):
                self.taps.append((x, y))

        cases = (
            ("local", 0, True), ("opponent", 0, False),
            # Captured winning trace: the remote phase-zero Ban completed
            # during pot calibration, then the local phase-one touch was usable.
            ("local", 1, False), ("opponent", 1, True),
        )
        for source, completed_bans, expected in cases:
            game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
            game.adb = ProbeAdb()
            game.events = self.FakeEvents((
                MODULE.AppEvent(
                    "draft_turn_probe", source=source, payload=completed_bans,
                ),
            ))
            game.draft_pots = {"ninja": (123, 456)}
            self.assertEqual(game._ranked_is_ivory(), expected)
            self.assertEqual(game.adb.taps, [(123, 456)])

    def test_one_native_ban_callback_cannot_advance_two_phases(self):
        stream = MODULE.EventStream.__new__(MODULE.EventStream)
        stream.last_ban_callback_at = float("-inf")
        self.assertTrue(stream._accept_ban_callback(10.0))
        self.assertFalse(stream._accept_ban_callback(10.01))
        self.assertFalse(stream._accept_ban_callback(10.20))
        self.assertTrue(stream._accept_ban_callback(10.30))

    def test_ranked_spawn_group_completes_after_public_log_quiet(self):
        stream = MODULE.EventStream.__new__(MODULE.EventStream)
        stream.draft_spawn_lock = MODULE.threading.Lock()
        stream.draft_spawn_generation = 4
        # An early OnSanityCheck must not override the spawn quiet period.
        stream.draft_spawn_complete = True
        stream.draft_spawns = [
            MODULE.AppEvent("draft_piece_spawn", "ghost", None),
        ]
        stream.draft_spawn_updated_at = 10.0
        with patch.object(MODULE.time, "monotonic", return_value=10.05):
            _generation, complete, _spawns = stream.ranked_spawn_snapshot()
        self.assertFalse(complete)
        with patch.object(MODULE.time, "monotonic", return_value=10.13):
            generation, complete, spawns = stream.ranked_spawn_snapshot()
        self.assertEqual(generation, 4)
        self.assertTrue(complete)
        self.assertEqual(len(spawns), 1)

    def test_ranked_pick_waits_for_spawns_after_early_sanity_check(self):
        full = (
            MODULE.AppEvent("draft_piece_spawn", "mage", "3:0"),
            MODULE.AppEvent("draft_piece_spawn", "king", "0:0"),
            MODULE.AppEvent("draft_piece_spawn", "king", "7:0"),
            MODULE.AppEvent("draft_piece_spawn", "king", "7:9"),
        )

        class DelayedSpawns:
            def __init__(self):
                self.calls = 0

            def ranked_spawn_snapshot(self):
                self.calls += 1
                if self.calls <= 2:
                    return 1, True, ()
                if self.calls == 3:
                    return 1, True, full[:1]
                return 1, True, full

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.events = DelayedSpawns()
        game.engine = Mock()
        game.ranked_enemy_roster = MODULE.Counter()
        game.ranked_enemy_king_candidates = None
        game.ranked_enemy_snapshots = []
        game._ranked_committed_points = lambda _ivory: (0, 18)
        game.log = lambda _message: None

        with patch.object(MODULE.time, "sleep", return_value=None):
            choices = game._observe_ranked_opponent_pick(local_ivory=False)

        self.assertEqual(choices, ["mage", "jester"])
        self.assertGreaterEqual(game.events.calls, 4)
        self.assertEqual(
            game.ranked_enemy_roster,
            MODULE.Counter({"mage": 1, "jester": 1}),
        )
        self.assertEqual(game.ranked_enemy_king_candidates, {"a10", "h10"})


class VisionTests(unittest.TestCase):
    def test_opening_emote_detector_does_not_sum_white_piece_skins(self):
        from PIL import Image, ImageDraw

        pieces = Image.new("RGB", (1080, 2400), (40, 100, 40))
        draw = ImageDraw.Draw(pieces)
        # Several disjoint white characters exceed the former total-pixel
        # threshold but none has speech-bubble geometry.
        for x in (100, 220, 340, 460, 580, 700):
            draw.rectangle((x, 610, x + 58, 673), fill="white")
        self.assertFalse(MODULE.PhoneGame._opening_board_obscured(pieces))

        emote = pieces.copy()
        ImageDraw.Draw(emote).rounded_rectangle(
            (360, 500, 500, 670), radius=20, fill="white")
        self.assertTrue(MODULE.PhoneGame._opening_board_obscured(emote))

    @unittest.skipUnless(importlib.util.find_spec("PIL") and importlib.util.find_spec("numpy"),
                         "Pillow/numpy not installed")
    def test_outline_detector_uses_square_centers(self):
        from PIL import Image, ImageDraw

        geometry = MODULE.BoardGeometry()
        image = Image.new("RGB", (1080, 2400), (70, 120, 50))
        draw = ImageDraw.Draw(image)
        for square in ("a10", "d9", "h8"):
            x, y = geometry.point(square)
            draw.ellipse((x - 38, y - 38, x + 38, y + 38),
                         outline=(255, 65, 55), width=18)
        found = MODULE.detect_outline_squares(image, geometry)
        self.assertEqual(found, ["h8", "d9", "a10"])

    def test_outline_detector_recovers_dark_checker_body(self):
        from PIL import Image, ImageDraw

        geometry = MODULE.BoardGeometry()
        image = Image.new("RGB", (1080, 2400), (110, 112, 114))
        draw = ImageDraw.Draw(image)
        x, y = geometry.point("e9")
        draw.ellipse((x - 46, y - 46, x + 46, y + 46), fill=(35, 35, 38))
        draw.arc((x - 48, y - 48, x + 48, y + 48), 5, 85,
                 fill=(230, 65, 55), width=3)
        self.assertEqual(
            MODULE.detect_outline_squares(image, geometry), ["e9"]
        )

    @unittest.skipUnless(Path("/opt/homebrew/bin/tesseract").is_file(),
                         "Tesseract not installed")
    def test_text_locator(self):
        from PIL import Image, ImageDraw, ImageFont

        image = Image.new("RGB", (800, 500), "white")
        font = ImageFont.truetype(
            "/System/Library/Fonts/Supplemental/Arial Bold.ttf", 72)
        ImageDraw.Draw(image).text((240, 190), "ACCEPT", fill="black",
                                   font=font, stroke_width=1)
        point = MODULE.find_text_center(
            image, "ACCEPT", "/opt/homebrew/bin/tesseract")
        self.assertIsNotNone(point)
        self.assertTrue(250 <= point[0] <= 400)
        self.assertTrue(180 <= point[1] <= 260)

    @unittest.skipUnless(Path("/opt/homebrew/bin/tesseract").is_file(),
                         "Tesseract not installed")
    def test_game_over_headline_classifier(self):
        from PIL import Image, ImageDraw, ImageFont

        font = ImageFont.truetype(
            "/System/Library/Fonts/Supplemental/Arial Bold.ttf", 72)
        for headline, expected in (
            ("VICTORY", "win"), ("DEFEAT", "loss"), ("DRAW", "draw"),
            ("CHECKMATE", "checkmate"), ("KNOCK-OUT!", "knockout"),
        ):
            image = Image.new("RGB", (800, 500), (50, 175, 235))
            ImageDraw.Draw(image).text(
                (120, 25 if expected in ("checkmate", "knockout") else 220),
                headline, fill="white", font=font,
                stroke_width=3, stroke_fill=(30, 90, 140),
            )
            self.assertEqual(
                MODULE.read_game_over_result(
                    image, "/opt/homebrew/bin/tesseract"
                ),
                expected,
            )

    def test_game_over_classifier_retries_isolated_white_headline(self):
        from PIL import Image

        image = Image.new("RGB", (1080, 2400), (40, 155, 225))
        blank = Mock(stdout=b"")
        isolated = Mock(stdout=b"VICTORY\n")
        with patch.object(
            MODULE.subprocess, "run", side_effect=(blank, blank, isolated)
        ) as run:
            self.assertEqual(MODULE.read_game_over_result(image), "win")
        self.assertEqual(run.call_count, 3)

    def test_ranked_lock_checkmark_detector_uses_full_screen_coordinates(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (70, 110, 70))
        draw = ImageDraw.Draw(image)
        draw.ellipse((241, 2002, 421, 2182), fill=(115, 220, 55))
        draw.line((285, 2090, 320, 2125, 380, 2050), fill="white", width=24)
        point = MODULE.PhoneGame._draft_control(image, "LOCK")
        self.assertIsNotNone(point)
        self.assertTrue(320 <= point[0] <= 345)
        self.assertTrue(2080 <= point[1] <= 2105)

    def test_reconnect_decline_detector(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (40, 140, 210))
        ImageDraw.Draw(image).rounded_rectangle(
            (260, 1300, 815, 1450), radius=40, fill=(245, 55, 25))
        point = MODULE.PhoneGame._reconnect_decline_point(image)
        self.assertIsNotNone(point)
        self.assertTrue(520 <= point[0] <= 550)
        self.assertTrue(1360 <= point[1] <= 1390)

    def test_unlock_ack_detector(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (40, 140, 210))
        ImageDraw.Draw(image).rounded_rectangle(
            (410, 1260, 670, 1350), radius=35, fill=(135, 215, 240))
        with patch.object(MODULE, "find_text_center", return_value=(540, 1305)):
            self.assertEqual(MODULE.PhoneGame._unlock_ack_point(image), (540, 1305))

    def test_unlock_ack_detector_ignores_cyan_information_panel(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (40, 140, 210))
        ImageDraw.Draw(image).rounded_rectangle(
            (410, 1260, 670, 1350), radius=35, fill=(135, 215, 240))
        with patch.object(MODULE, "find_text_center", return_value=None):
            self.assertIsNone(MODULE.PhoneGame._unlock_ack_point(image))

    def test_login_detector(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (40, 140, 210))
        ImageDraw.Draw(image).rounded_rectangle(
            (265, 1250, 815, 1415), radius=60, fill=(135, 215, 240))
        self.assertEqual(MODULE.PhoneGame._login_point(image), (540, 1332))

    def test_game_found_accept_detector_ignores_large_play_button(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (40, 140, 210))
        draw = ImageDraw.Draw(image)
        draw.rounded_rectangle(
            (220, 1050, 850, 1240), radius=50, fill=(250, 175, 25))
        self.assertIsNone(MODULE.PhoneGame._accept_point(image))
        draw.rounded_rectangle(
            (550, 1390, 825, 1470), radius=35, fill=(250, 175, 25))
        self.assertIsNone(MODULE.PhoneGame._accept_point(image))
        draw.rounded_rectangle(
            (250, 1390, 525, 1470), radius=35, fill=(235, 55, 30))
        point = MODULE.PhoneGame._accept_point(image)
        self.assertIsNotNone(point)
        self.assertTrue(680 <= point[0] <= 695)
        self.assertTrue(1425 <= point[1] <= 1435)

    def test_army_clear_confirmation_detector(self):
        from PIL import Image, ImageDraw

        upper = Image.new("RGB", (1080, 2400), (40, 140, 210))
        ImageDraw.Draw(upper).rounded_rectangle(
            (264, 1085, 815, 1212), radius=50, fill=(235, 55, 30))
        upper_point = MODULE.PhoneGame._army_clear_confirmation_point(upper)
        self.assertIsNotNone(upper_point)
        self.assertTrue(535 <= upper_point[0] <= 545)
        self.assertTrue(1140 <= upper_point[1] <= 1155)

        image = Image.new("RGB", (1080, 2400), (40, 140, 210))
        ImageDraw.Draw(image).rounded_rectangle(
            (264, 1261, 815, 1417), radius=50, fill=(235, 55, 30))
        point = MODULE.PhoneGame._army_clear_confirmation_point(image)
        self.assertIsNotNone(point)
        self.assertTrue(535 <= point[0] <= 545)
        self.assertTrue(1335 <= point[1] <= 1345)
        # An unlocked character outline can touch the lower button rim. The
        # modal-band crop must prevent that tall board component from hiding
        # the safe Clear control.
        ImageDraw.Draw(image).rectangle(
            (530, 1410, 570, 1700), fill=(235, 55, 30))
        joined = MODULE.PhoneGame._army_clear_confirmation_point(image)
        self.assertIsNotNone(joined)
        self.assertTrue(530 <= joined[0] <= 550)
        self.assertTrue(1335 <= joined[1] <= 1360)

    def test_ranked_ban_bubble_detector_ignores_character_body(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (80, 130, 80))
        draw = ImageDraw.Draw(image)
        source = (850, 930)
        # A red character body at the pot must not be mistaken for its action.
        draw.ellipse((815, 880, 885, 970), fill=(225, 35, 25))
        draw.rounded_rectangle(
            (770, 760, 930, 850), radius=35, fill=(235, 45, 25))
        self.assertEqual(
            MODULE.PhoneGame._visual_ban_control(image, source), (850, 805)
        )

    def test_ranked_fixed_ban_button_wins_over_pot_lock_chains(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (80, 130, 80))
        draw = ImageDraw.Draw(image)
        # Blue top BAN is only a phase label and must never win detection.
        draw.rounded_rectangle(
            (560, 140, 690, 275), radius=30, fill=(78, 167, 254)
        )
        # The actual lower-left inspector control from the captured frame.
        draw.rounded_rectangle(
            (23, 1922, 292, 2012), radius=35, fill=(235, 45, 25)
        )
        # This red component reproduces the misleading chained pot selected by
        # the old pot-relative detector in the captured phase-five failure.
        draw.rounded_rectangle(
            (770, 760, 930, 850), radius=35, fill=(235, 45, 25)
        )
        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.draft_pots = {"prince": (850, 930)}

        point = game._pot_ban_control(image, "prince")

        self.assertIsNotNone(point)
        self.assertTrue(150 <= point[0] <= 165)
        self.assertTrue(1960 <= point[1] <= 1975)

    def test_connected_main_requires_profile_and_play(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (40, 140, 210))
        draw = ImageDraw.Draw(image)
        draw.rectangle((40, 120, 300, 390), fill=(120, 50, 190))
        self.assertFalse(MODULE.PhoneGame._connected_main(image))
        draw.rectangle((220, 900, 860, 1110), fill=(255, 180, 25))
        self.assertTrue(MODULE.PhoneGame._connected_main(image))
        draw.rounded_rectangle((700, 90, 980, 210), radius=35,
                               fill=(235, 55, 30))
        self.assertFalse(MODULE.PhoneGame._connected_main(image))

    def test_ranked_queue_requires_top_banner_and_cancel_control(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (55, 135, 225))
        draw = ImageDraw.Draw(image)
        draw.rounded_rectangle((130, 115, 950, 320), radius=45,
                               fill=(80, 190, 235))
        self.assertFalse(MODULE.PhoneGame._ranked_queue_visible(image))
        draw.rounded_rectangle((410, 270, 670, 345), radius=30,
                               fill=(235, 55, 30))
        self.assertTrue(MODULE.PhoneGame._ranked_queue_visible(image))

    def test_opening_emote_obscuration_detector(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (80, 125, 70))
        self.assertFalse(MODULE.PhoneGame._opening_board_obscured(image))
        ImageDraw.Draw(image).rounded_rectangle(
            (290, 530, 800, 870), radius=70, fill=(250, 250, 250)
        )
        self.assertTrue(MODULE.PhoneGame._opening_board_obscured(image))

    def test_maintenance_detector(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (40, 140, 210))
        self.assertIsNone(MODULE.PhoneGame._maintenance_ack_point(image))
        ImageDraw.Draw(image).rectangle(
            (25, 900, 1055, 1500), fill=(245, 55, 35))
        self.assertEqual(MODULE.PhoneGame._maintenance_ack_point(image),
                         (540, 1416))
        self.assertIsNone(MODULE.PhoneGame._reconnect_decline_point(image))

    def test_unlock_badges_include_scrolled_tail_and_crowned_prince(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (80, 150, 200))
        draw = ImageDraw.Draw(image)
        draw.rectangle((275, 860, 405, 945), fill=(250, 165, 35))
        draw.rectangle((245, 1570, 402, 1658), fill=(250, 165, 35))
        self.assertEqual(MODULE.PhoneGame._unlock_badges(image),
                         [(340, 902), (323, 1614)])

    @unittest.skipUnless(importlib.util.find_spec("numpy"),
                         "numpy not installed")
    def test_public_ban_change_isolated_to_one_pot(self):
        from PIL import Image, ImageDraw

        before = Image.new("RGB", (500, 300), (70, 130, 80))
        after = before.copy()
        pots = {"queen": (100, 150), "ninja": (250, 150), "ghost": (400, 150)}
        draw = ImageDraw.Draw(after)
        draw.rectangle((205, 105, 295, 195), fill=(235, 50, 35))
        piece, scores = MODULE.changed_pot(before, after, pots, 45)
        self.assertEqual(piece, "ninja")
        self.assertGreater(scores["ninja"], scores["queen"])

    @unittest.skipUnless(importlib.util.find_spec("PIL") and importlib.util.find_spec("numpy"),
                         "Pillow/numpy not installed")
    def test_outline_consensus_rejects_one_frame_animation_noise(self):
        from PIL import Image, ImageDraw

        geometry = MODULE.BoardGeometry()
        frames = []
        for index in range(3):
            image = Image.new("RGB", (1080, 2400), (80, 130, 80))
            draw = ImageDraw.Draw(image)
            for square in ("a8", "b9"):
                x, y = geometry.point(square)
                draw.rectangle((x - 18, y - 18, x + 18, y + 18),
                               fill=(240, 35, 25))
            if index == 0:
                x, y = geometry.point("c10")
                draw.rectangle((x - 18, y - 18, x + 18, y + 18),
                               fill=(240, 35, 25))
            frames.append(image)

        self.assertEqual(
            MODULE.consensus_outline_squares(frames, geometry),
            ["a8", "b9"],
        )

    @unittest.skipUnless(importlib.util.find_spec("PIL") and importlib.util.find_spec("numpy"),
                         "Pillow/numpy not installed")
    def test_enemy_probe_fails_closed_on_conflicting_colliders(self):
        from PIL import Image, ImageDraw

        geometry = MODULE.BoardGeometry()
        image = Image.new("RGB", (1080, 2400), (80, 130, 80))
        x, y = geometry.point("a8")
        ImageDraw.Draw(image).rectangle(
            (x - 18, y - 18, x + 18, y + 18), fill=(240, 35, 25))

        class FakeAdb:
            def screenshot(self):
                return image

            def tap(self, _x, _y):
                pass

        class FakeEvents:
            def __init__(self):
                self.pieces = iter((
                    "queen", "queen", "pawn", "pawn", "pawn",
                    "queen", "pawn", "queen", "pawn",
                ))

            def drain(self):
                pass

            def wait(self, _kinds, _timeout):
                return MODULE.AppEvent("selected", next(self.pieces))

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.geometry = geometry
        game.adb = FakeAdb()
        game.events = FakeEvents()
        game.verbose = False
        game.log = lambda _message: None
        with self.assertRaisesRegex(RuntimeError, "conflicting collider hits"):
            game.probe_enemy(image, ("a8",))

    @unittest.skipUnless(importlib.util.find_spec("PIL") and importlib.util.find_spec("numpy"),
                         "Pillow/numpy not installed")
    def test_enemy_probe_accepts_repeated_native_identity(self):
        from PIL import Image

        image = Image.new("RGB", (1080, 2400), (80, 130, 80))

        class FakeAdb:
            def tap(self, _x, _y):
                pass

        class FakeEvents:
            def drain(self):
                pass

            def wait(self, _kinds, _timeout):
                return MODULE.AppEvent("selected", "pawn")

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.geometry = MODULE.BoardGeometry()
        game.adb = FakeAdb()
        game.events = FakeEvents()
        game.verbose = False
        game.log = lambda _message: None
        self.assertEqual(game.probe_enemy(image, ("a8",)), [("pawn", "a8")])

    @unittest.skipUnless(importlib.util.find_spec("PIL") and importlib.util.find_spec("numpy"),
                         "Pillow/numpy not installed")
    def test_enemy_probe_rejects_stable_outline_with_zero_native_hits(self):
        from PIL import Image

        image = Image.new("RGB", (1080, 2400), (80, 130, 80))

        class FakeAdb:
            def tap(self, _x, _y):
                pass

        class FakeEvents:
            def drain(self):
                pass

            def wait(self, _kinds, _timeout):
                raise TimeoutError

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.geometry = MODULE.BoardGeometry()
        game.adb = FakeAdb()
        game.events = FakeEvents()
        game.verbose = False
        game.log = lambda _message: None
        with self.assertRaisesRegex(RuntimeError, "no native-confirmed"):
            game.probe_enemy(image, ("h9",))

    @unittest.skipUnless(importlib.util.find_spec("PIL") and importlib.util.find_spec("numpy"),
                         "Pillow/numpy not installed")
    def test_pot_detector_matches_recovered_builder_capture(self):
        capture = Path("/private/tmp/ultimate_builder_live.png")
        if not capture.is_file():
            self.skipTest("live calibration capture is unavailable")
        from PIL import Image

        centers = MODULE.detect_pot_centers(Image.open(capture))
        self.assertEqual(len(centers), 20)
        self.assertTrue(any(abs(x - 244) < 5 and abs(y - 965) < 6
                            for x, y in centers))
        self.assertTrue(any(abs(x - 861) < 5 and abs(y - 1254) < 6
                            for x, y in centers))

    @unittest.skipUnless(importlib.util.find_spec("PIL") and importlib.util.find_spec("numpy"),
                         "Pillow/numpy not installed")
    def test_pot_detector_splits_character_pixels_joining_adjacent_rows(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (70, 130, 80))
        draw = ImageDraw.Draw(image)
        xs = [120 + 120 * column for column in range(8)]
        rows = (920, 1060, 1200)
        for y in rows:
            for x in xs:
                draw.rectangle((x - 40, y, x + 40, y + 80), fill=(225, 110, 30))
        # A character model can make the top and middle pots one connected
        # orange component.  Row-first segmentation must still recover both.
        draw.rectangle((xs[2] - 5, 1000, xs[2] + 5, 1060), fill=(225, 110, 30))
        centers = MODULE.detect_pot_centers(image)
        self.assertEqual(len(centers), 24)
        self.assertEqual(len([point for point in centers if abs(point[0] - xs[2]) < 5]), 3)

    def test_full_ranked_pot_layout_follows_live_column_major_traversal(self):
        class FakeImage:
            height = 2400

        top = [(100 + index * 120, 950) for index in range(8)]
        middle = [(100 + index * 120, 1100) for index in range(8)]
        bottom = [(100 + index * 125, 1250) for index in range(8)]
        original = MODULE.detect_pot_centers
        MODULE.detect_pot_centers = lambda _image: top + middle + bottom
        try:
            mapped = MODULE.map_ranked_pots(FakeImage())
        finally:
            MODULE.detect_pot_centers = original
        self.assertEqual(mapped["giant"], top[0])
        self.assertEqual(mapped["checker"], middle[0])
        self.assertEqual(mapped["pawn"], bottom[0])
        self.assertEqual(mapped["bomb"], bottom[4])
        self.assertEqual(mapped["ghost"], top[5])
        self.assertEqual(mapped["prince"], middle[7])
        self.assertEqual(mapped["ninja"], bottom[7])

    @unittest.skipUnless(importlib.util.find_spec("PIL") and importlib.util.find_spec("numpy"),
                         "Pillow/numpy not installed")
    def test_builder_giant_detector_maps_visual_mesh_to_footprint_anchor(self):
        from PIL import Image, ImageDraw

        geometry = MODULE.DeploymentGeometry()
        for square in ("b1", "e2"):
            image = Image.new("RGB", (1080, 2400), (45, 90, 55))
            draw = ImageDraw.Draw(image)
            file_index = ord(square[0]) - ord("a")
            rank = int(square[1])
            center_x = geometry.left + (file_index + 1.0) * geometry.cell_width
            logical_center_y = geometry.top + (3 - rank) * geometry.cell_height
            mesh_center_y = logical_center_y - geometry.cell_height * 0.24
            draw.rectangle((
                round(center_x - geometry.cell_width * 0.82),
                round(mesh_center_y - geometry.cell_height * 0.55),
                round(center_x + geometry.cell_width * 0.82),
                round(mesh_center_y + geometry.cell_height * 0.55),
            ), fill=(220, 90, 180))
            self.assertEqual(MODULE.detect_builder_giant_anchor(image), square)

    @unittest.skipUnless(importlib.util.find_spec("PIL") and importlib.util.find_spec("numpy"),
                         "Pillow/numpy not installed")
    def test_pre_ready_verification_rejects_wrong_giant_anchor(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (45, 90, 55))
        draw = ImageDraw.Draw(image)
        geometry = MODULE.DeploymentGeometry()
        center_x = geometry.left + 2.0 * geometry.cell_width
        logical_center_y = geometry.top + geometry.cell_height  # b2
        mesh_center_y = logical_center_y - geometry.cell_height * 0.24
        draw.rectangle((
            round(center_x - geometry.cell_width * 0.82),
            round(mesh_center_y - geometry.cell_height * 0.55),
            round(center_x + geometry.cell_width * 0.82),
            round(mesh_center_y + geometry.cell_height * 0.55),
        ), fill=(220, 90, 180))
        team = (("king", "a1"), ("giant", "b1"), ("ghost", "d1"))
        with self.assertRaisesRegex(
            MODULE.ArmyPlacementRetry, "expected b1, detected b2"
        ):
            MODULE.verify_builder_placement(
                image, team, MODULE.Counter({("ghost", "d1"): 1})
            )

    @unittest.skipUnless(importlib.util.find_spec("PIL") and importlib.util.find_spec("numpy"),
                         "Pillow/numpy not installed")
    def test_pre_ready_verification_accepts_two_adjacent_giants(self):
        from PIL import Image, ImageDraw

        image = Image.new("RGB", (1080, 2400), (45, 90, 55))
        draw = ImageDraw.Draw(image)
        geometry = MODULE.DeploymentGeometry()
        for square in ("c1", "e1"):
            file_index = ord(square[0]) - ord("a")
            center_x = geometry.left + (file_index + 1.0) * geometry.cell_width
            logical_center_y = geometry.top + 2 * geometry.cell_height
            mesh_center_y = logical_center_y - geometry.cell_height * 0.24
            draw.rectangle((
                round(center_x - geometry.cell_width * 0.82),
                round(mesh_center_y - geometry.cell_height * 0.55),
                round(center_x + geometry.cell_width * 0.82),
                round(mesh_center_y + geometry.cell_height * 0.55),
            ), fill=(220, 90, 180))
        team = (("king", "a1"), ("giant", "c1"), ("giant", "e1"))
        MODULE.verify_builder_placement(image, team, MODULE.Counter())
        self.assertEqual(
            MODULE.detect_builder_giant_anchors(image, 2), ["c1", "e1"]
        )


class OpeningSynchronizationTests(unittest.TestCase):
    def test_gameplay_journal_survives_queue_drains_and_replays(self):
        import queue
        import threading

        stream = MODULE.EventStream.__new__(MODULE.EventStream)
        stream.events = queue.Queue()
        stream.gameplay_lock = threading.Lock()
        stream.gameplay_generation = 3
        stream.gameplay_events = [
            MODULE.AppEvent("move", "sludge", "a10", "c10"),
            MODULE.AppEvent("turn_end"),
        ]
        stream.events.put(MODULE.AppEvent("selected", "king"))
        self.assertTrue(stream.opening_action_started())
        stream.drain()
        stream.replay_gameplay_journal()
        self.assertEqual(stream.events.get_nowait().kind, "move")
        self.assertEqual(stream.events.get_nowait().kind, "turn_end")

    def test_ivory_does_not_wait_for_an_opponent_opening(self):
        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.perspective_flipped = False
        game.await_onyx_opening(timeout=0.01)

    @patch.object(MODULE.time, "sleep", return_value=None)
    def test_onyx_requires_action_then_turn_completion(self, _sleep):
        class FakeEvents:
            @staticmethod
            def gameplay_snapshot():
                return 1, (
                    MODULE.AppEvent("turn_end"),  # initial board activation
                    MODULE.AppEvent("move", "sludge", "a10", "c10"),
                    MODULE.AppEvent("turn_end"),
                )

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.perspective_flipped = True
        game.events = FakeEvents()
        game.log = lambda _message: None
        opening = game.await_onyx_opening(timeout=0.01)
        self.assertEqual([event.kind for event in opening], ["move", "turn_end"])
        self.assertEqual(opening[0].source, "h1")
        self.assertEqual(opening[0].target, "f1")

    @patch.object(MODULE.time, "sleep", return_value=None)
    def test_onyx_opening_terminal_event_returns_public_result(self, _sleep):
        class FakeEvents:
            @staticmethod
            def gameplay_snapshot():
                return 1, (MODULE.AppEvent("game_over"),)

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.perspective_flipped = True
        game.events = FakeEvents()
        game.log = lambda _message: None
        game.classify_game_over = (
            lambda _timeout=4.0, _decisive_result=None: "defeat"
        )
        outcome = game.await_onyx_opening(timeout=0.01)
        self.assertEqual(outcome, MODULE.OpeningTerminal("defeat", "game_over"))

    @patch.object(MODULE.time, "sleep", return_value=None)
    def test_onyx_opening_timeout_without_opponent_action_is_a_win(self, _sleep):
        class FakeEvents:
            @staticmethod
            def gameplay_snapshot():
                return 1, (MODULE.AppEvent("game_over"),)

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.perspective_flipped = True
        game.events = FakeEvents()
        game.log = lambda _message: None
        game.classify_game_over = (
            lambda _timeout=4.0, _decisive_result=None: "unknown"
        )
        outcome = game.await_onyx_opening(timeout=0.01)
        self.assertEqual(outcome, MODULE.OpeningTerminal("win", "game_over"))

    @patch.object(MODULE.time, "sleep", return_value=None)
    def test_onyx_opening_game_over_after_public_action_is_a_loss(self, _sleep):
        class FakeEvents:
            @staticmethod
            def gameplay_snapshot():
                return 1, (
                    MODULE.AppEvent("move", "queen", "a10", "a1"),
                    MODULE.AppEvent("game_over"),
                )

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.perspective_flipped = True
        game.events = FakeEvents()
        game.log = lambda _message: None
        game.classify_game_over = (
            lambda _timeout=4.0, _decisive_result=None: "unknown"
        )
        outcome = game.await_onyx_opening(timeout=0.01)
        self.assertEqual(outcome, MODULE.OpeningTerminal("loss", "game_over"))

    @patch.object(MODULE.time, "sleep", return_value=None)
    def test_onyx_bomb_opening_ignores_early_network_turn(self, _sleep):
        class FakeEvents:
            @staticmethod
            def gameplay_snapshot():
                return 1, (
                    MODULE.AppEvent("move", "bomb", "a10", "a7"),
                    MODULE.AppEvent("turn_end", raw="early network turn"),
                    MODULE.AppEvent("dead", "bomb"),
                    MODULE.AppEvent("turn_end", raw="settled native turn"),
                )

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.perspective_flipped = True
        game.events = FakeEvents()
        game.log = lambda _message: None
        opening = game.await_onyx_opening(timeout=0.01)
        self.assertEqual(opening[-1].raw, "settled native turn")

    def test_rewind_ordinary_opening_move(self):
        current = (("king", "a10"), ("queen", "c7"))
        variants = MODULE.rewind_public_enemy_opening(
            current, (MODULE.AppEvent("move", "queen", "c10", "c7"),)
        )
        self.assertEqual(
            {tuple(variant) for variant in variants},
            {(('king', 'a10'), ('queen', 'c10'))},
        )

    def test_rewind_removes_sludge_products(self):
        current = (
            ("king", "a10"), ("goop", "b8"), ("sludge", "c8"),
            ("goop", "d8"),
        )
        variants = MODULE.rewind_public_enemy_opening(
            current, (MODULE.AppEvent("move", "sludge", "c10", "c8"),)
        )
        self.assertEqual(
            {tuple(variant) for variant in variants},
            {(('king', 'a10'), ('sludge', 'c10'))},
        )

    def test_rewind_does_not_retain_private_ghost_coordinate(self):
        variants = MODULE.rewind_public_enemy_opening(
            (("king", "a10"), ("ghost", "c7")),
            (MODULE.AppEvent("move", "ghost", "c10", "c7"),),
        )
        self.assertEqual(variants, [[("king", "a10")]])

    def test_rewind_copycat_pair_and_mage_swap(self):
        copycat = MODULE.rewind_public_enemy_opening(
            (("king", "a10"), ("copycat", "b9"),
             ("copycatClone", "g9")),
            (MODULE.AppEvent("move", "copycat", "b10", "b9"),),
        )
        self.assertEqual(
            {tuple(variant) for variant in copycat},
            {(('king', 'a10'), ('copycat', 'b10'),
              ('copycatClone', 'g10'))},
        )

        mage = MODULE.rewind_public_enemy_opening(
            (("king", "a10"), ("rook", "b10"), ("mage", "c10")),
            (MODULE.AppEvent("move", "mage", "b10", "b10"),),
        )
        self.assertEqual(
            {tuple(variant) for variant in mage},
            {(('king', 'a10'), ('mage', 'b10'), ('rook', 'c10'))},
        )

    def test_rewind_castle_restores_both_royal_and_rook(self):
        exact = MODULE.rewind_public_enemy_opening(
            (("king", "f10"), ("rook", "e10")),
            (
                MODULE.AppEvent("move", "king", "d10", "f10"),
                MODULE.AppEvent("move", "rook", "h10", "e10"),
            ),
        )
        self.assertEqual(
            {tuple(variant) for variant in exact},
            {(('king', 'd10'), ('rook', 'h10'))},
        )

        # Some builds/replay paths omit the automatic Rook's animation
        # callback. Retain both native deployment origins that can produce the
        # observed castle instead of inventing one private coordinate.
        inferred = MODULE.rewind_public_enemy_opening(
            (("king", "f10"), ("rook", "e10")),
            (MODULE.AppEvent("move", "jester", "d10", "f10"),),
        )
        self.assertEqual(
            {tuple(variant) for variant in inferred},
            {
                (('king', 'd10'), ('rook', 'g10')),
                (('king', 'd10'), ('rook', 'h10')),
            },
        )


class ControllerActionTests(unittest.TestCase):
    def test_builder_pot_identity_swaps_mislabeled_equal_cost_slots(self):
        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.army_pot_slots = {}
        messages = []
        game.log = messages.append

        self.assertTrue(game._learn_army_pot_identity("dragon", "berserker"))
        self.assertEqual(game.army_pot_slots["dragon"], "berserker")
        self.assertEqual(game.army_pot_slots["berserker"], "dragon")
        self.assertFalse(game._learn_army_pot_identity("dragon", "dragon"))
        self.assertIn("learned native pot swap", messages[0])

    def test_cpu_army_misdrop_restarts_with_learned_calibration(self):
        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        attempts = []
        messages = []

        def start_once():
            attempts.append(None)
            if len(attempts) == 1:
                raise MODULE.ArmyPlacementRetry("colliding drop")

        game._start_very_hard_cpu_once = start_once
        game.log = messages.append
        game.configure_army = True
        game.start_very_hard_cpu()

        self.assertEqual(len(attempts), 2)
        self.assertFalse(game.configure_army)
        self.assertIn("restarting a clean corrected CPU build", messages[0])

    def test_terminal_event_during_search_prevents_board_tap(self):
        class FakeAdb:
            def __init__(self):
                self.taps = []

            def tap_square(self, _geometry, square):
                self.taps.append(square)

        class FakeEvents:
            @staticmethod
            def drain():
                return MODULE.AppEvent("game_over")

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.geometry = MODULE.BoardGeometry()
        game.adb = FakeAdb()
        game.events = FakeEvents()
        event = game.execute("b5-e8")
        self.assertEqual(event.kind, "game_over")
        self.assertEqual(game.adb.taps, [])

    def test_bomb_action_waits_past_early_network_turn(self):
        class FakeAdb:
            def tap_square(self, _geometry, _square):
                pass

        class FakeEvents:
            def __init__(self):
                self.pending = [
                    MODULE.AppEvent("selected", "queen"),
                    MODULE.AppEvent("touch_end"),
                    MODULE.AppEvent("move", "queen", "a1", "a2"),
                    MODULE.AppEvent("turn_end", raw="early network turn"),
                    MODULE.AppEvent("dead", "bomb"),
                    MODULE.AppEvent("turn_end", raw="post-explosion turn"),
                ]

            def drain(self):
                pass

            def wait(self, kinds, _timeout, predicate=None):
                accepted = {kinds} if isinstance(kinds, str) else set(kinds)
                while self.pending:
                    event = self.pending.pop(0)
                    if (event.kind in accepted and
                            (predicate is None or predicate(event))):
                        return event
                raise TimeoutError

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.geometry = MODULE.BoardGeometry()
        game.adb = FakeAdb()
        game.events = FakeEvents()
        game.perspective_flipped = False
        event = game.execute("a1-a2", expect_bomb_resolution=True)
        self.assertEqual((event.kind, event.source, event.target),
                         ("move", "a1", "a2"))
        self.assertEqual(game.events.pending, [])

    def test_castle_waits_through_the_companion_rook_animation(self):
        class FakeAdb:
            def tap_square(self, _geometry, _square):
                pass

        class FakeEvents:
            def __init__(self):
                self.pending = [
                    MODULE.AppEvent("selected", "king"),
                    MODULE.AppEvent("touch_end"),
                    MODULE.AppEvent("move", "king", "d1", "f1"),
                    MODULE.AppEvent("move", "rook", "h1", "e1"),
                    MODULE.AppEvent("turn_end"),
                ]

            @staticmethod
            def drain():
                return None

            def wait(self, kinds, _timeout, predicate=None):
                accepted = {kinds} if isinstance(kinds, str) else set(kinds)
                while self.pending:
                    event = self.pending.pop(0)
                    if (event.kind in accepted and
                            (predicate is None or predicate(event))):
                        return event
                raise TimeoutError

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.geometry = MODULE.BoardGeometry()
        game.adb = FakeAdb()
        game.events = FakeEvents()
        game.perspective_flipped = False
        game.beliefs = type("Beliefs", (), {"positions": [
            "w;king,w,d1;rook,w,h1;king,b,a10"
        ]})()
        event = game.execute("d1-f1")
        self.assertEqual((event.kind, event.source, event.target),
                         ("move", "d1", "f1"))
        self.assertEqual(game.events.pending, [])

    def test_mage_swap_uses_drag_and_turn_completion(self):
        class FakeAdb:
            def __init__(self):
                self.drags = []

            def drag_sync(self, source, target, duration):
                self.drags.append((source, target, duration))

        class FakeEvents:
            def drain(self):
                pass

            @staticmethod
            def wait(kinds, timeout):
                self.assertIn("turn_end", kinds)
                self.assertGreater(timeout, 0.0)
                self.assertLessEqual(timeout, 8.0)
                return MODULE.AppEvent("turn_end")

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.geometry = MODULE.BoardGeometry()
        game.adb = FakeAdb()
        game.events = FakeEvents()
        event = game.execute("b1~a1")
        self.assertEqual((event.kind, event.source, event.target),
                         ("move", "b1", "a1"))
        self.assertEqual(
            game.adb.drags,
            [(game.geometry.point("b1"),
              game.geometry.drag_destination("b1", "a1"), 320)],
        )

    def test_fisherman_hook_uses_the_same_guarded_drag_path(self):
        class FakeAdb:
            def __init__(self):
                self.drags = []

            def drag_sync(self, source, target, duration):
                self.drags.append((source, target, duration))

        class FakeEvents:
            def drain(self):
                pass

            @staticmethod
            def wait(_kinds, _timeout):
                return MODULE.AppEvent("turn_end")

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.geometry = MODULE.BoardGeometry()
        game.adb = FakeAdb()
        game.events = FakeEvents()
        event = game.execute("f8!f1")
        self.assertEqual((event.kind, event.source, event.target),
                         ("move", "f8", "f1"))
        self.assertEqual(len(game.adb.drags), 1)

    def test_fisherman_retries_native_source_to_source_drag(self):
        class FakeAdb:
            def __init__(self):
                self.drags = []

            def drag_sync(self, source, target, duration):
                self.drags.append((source, target, duration))

        class FakeEvents:
            def __init__(self):
                self.pending = [
                    MODULE.AppEvent("army_drop", source="2:7", target="2:7"),
                    MODULE.AppEvent("army_drop", source="2:7", target="2:9"),
                    MODULE.AppEvent("turn_end"),
                ]

            @staticmethod
            def drain():
                pass

            def wait(self, _kinds, _timeout):
                return self.pending.pop(0)

        game = MODULE.PhoneGame.__new__(MODULE.PhoneGame)
        game.geometry = MODULE.BoardGeometry()
        game.adb = FakeAdb()
        game.events = FakeEvents()
        game.verbose = False
        event = game.execute("f3!f1")
        self.assertEqual(event.kind, "move")
        self.assertEqual(len(game.adb.drags), 2)
        self.assertNotEqual(game.adb.drags[0][1], game.geometry.point("f1"))


class BeliefConstructionTests(unittest.TestCase):
    def test_bomb_resolution_is_derived_from_engine_transition(self):
        before = (
            "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
            "king,w,a1;king,b,h10;queen,w,d4;bomb,b,d5"
        )
        after = (
            "b;hm=1;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
            "king,w,a1;king,b,h10"
        )

        class FakeEngine:
            @staticmethod
            def legal_moves(position):
                return ["d4-d5"] if position == before else []

            @staticmethod
            def apply(position, move):
                self.assertEqual((position, move), (before, "d4-d5"))
                return after

        beliefs = MODULE.BeliefSet(FakeEngine(), (before,))
        self.assertTrue(beliefs.move_causes_bomb_detonation("d4-d5"))
        self.assertTrue(beliefs.observation_causes_bomb_detonation(
            MODULE.AppEvent("move", "queen", "d4", "d5")
        ))

    def test_special_target_identities_include_giant_footprint(self):
        position = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
            "king,w,a1;king,b,h10;fisherman,b,c8;giant,w,f4"
        )

        class FakeEngine:
            @staticmethod
            def legal_moves(_position):
                return ["c8!f5"]

        beliefs = MODULE.BeliefSet(FakeEngine(), (position,))
        self.assertEqual(
            beliefs.special_target_identities("fisherman", "c8", "!"),
            {"f5": {"giant"}},
        )

    def test_public_special_diagnostic_must_match_piece_source_and_legality(self):
        candidates = ["g2", "h2"]
        diagnostic = MODULE.AppEvent(
            "bot_action", "devil", "f3", "g2"
        )
        self.assertEqual(
            MODULE.PhoneGame.diagnostic_special_target(
                "devil", "f3", candidates, diagnostic
            ),
            "g2",
        )
        self.assertIsNone(MODULE.PhoneGame.diagnostic_special_target(
            "mage", "f3", candidates, diagnostic
        ))
        self.assertIsNone(MODULE.PhoneGame.diagnostic_special_target(
            "devil", "f3", ["h2"], diagnostic
        ))

    def test_belief_search_is_delegated_to_native_information_set_engine(self):
        class LegalEngine:
            calls = []

            def legal_moves(self, position):
                return {
                    "p1": ["a1-a2", "b1-b2"],
                    "p2": ["a1-a2", "c1-c2"],
                }[position]

            def search_beliefs(self, positions, depth, nodes, movetime, draw_moves=()):
                self.calls.append(
                    (tuple(positions), depth, nodes, movetime, tuple(draw_moves))
                )
                return "a1-a2", 40, (
                    "info depth 5 score cp 40 beliefs 2 beliefworst 40"
                )

        engine = LegalEngine()
        beliefs = MODULE.BeliefSet(engine, ("p1", "p2"))
        move, info = beliefs.choose(5, 1000, 250)
        self.assertEqual(move, "a1-a2")
        self.assertEqual(
            engine.calls,
            [(("p1", "p2"), 5, 1000, 250, ())],
        )
        self.assertIn("beliefworst 40", info)

    def test_public_threefold_roots_are_scored_by_native_belief_search(self):
        class LegalEngine:
            calls = []

            @staticmethod
            def legal_moves(_position):
                return ["a1-a2", "b1-b2"]

            def search_beliefs(self, positions, depth, nodes, movetime, draw_moves=()):
                del positions, depth, nodes, movetime
                self.calls.append(tuple(draw_moves))
                return "b1-b2", 0, "info depth 4 score cp 0"

        engine = LegalEngine()
        beliefs = MODULE.BeliefSet(engine, ("p1", "p2"))
        move, _info = beliefs.choose(4, draw_moves=("b1-b2",))
        self.assertEqual(move, "b1-b2")
        self.assertEqual(engine.calls, [("b1-b2",)])

    def test_controller_rejects_native_move_not_common_to_all_beliefs(self):
        class LegalEngine:
            @staticmethod
            def legal_moves(position):
                return ["a1-a2", "b1-b2"] if position == "p1" else ["a1-a2"]

            @staticmethod
            def search_beliefs(_positions, _depth, _nodes, _movetime, _draw_moves=()):
                return "b1-b2", 100, "info depth 5 score cp 100"

        beliefs = MODULE.BeliefSet(LegalEngine(), ("p1", "p2"))
        with self.assertRaisesRegex(RuntimeError, "not legal in every belief"):
            beliefs.choose(5)

    def test_public_continuation_collapses_ambiguous_royal_identity(self):
        class RoyalEngine:
            @staticmethod
            def legal_moves(position):
                return [] if position == "captured-real-king" else ["a1-a2"]

            @staticmethod
            def apply(position, _move):
                return ("captured-real-king" if position == "king-on-target"
                        else "captured-jester")

        beliefs = MODULE.BeliefSet(
            RoyalEngine(), ("king-on-target", "jester-on-target"))
        beliefs.apply_known("a1-a2")
        self.assertEqual(
            beliefs.positions, ["captured-jester", "captured-real-king"])
        beliefs.observe_continuation()
        self.assertEqual(beliefs.positions, ["captured-jester"])

    def test_royal_move_prefab_does_not_reveal_king_jester_identity(self):
        king_at_source = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
            "king,w,a1;king,b,e9;jester,b,d10"
        )
        jester_at_source = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
            "king,w,a1;jester,b,e9;king,b,d10"
        )

        class RoyalEngine:
            @staticmethod
            def legal_moves(_position):
                return ["e9-e10"]

            @staticmethod
            def apply(position, _move):
                return position.replace("b;", "w;", 1).replace(
                    ",b,e9", ",b,e10"
                )

        beliefs = MODULE.BeliefSet(
            RoyalEngine(), (king_at_source, jester_at_source)
        )
        beliefs.observe_move(
            MODULE.AppEvent("move", "king", "e9", "e10"), False
        )
        self.assertEqual(len(beliefs.positions), 2)
        self.assertTrue(any("king,b,e10" in upn for upn in beliefs.positions))
        self.assertTrue(any("jester,b,e10" in upn for upn in beliefs.positions))

    def test_bomb_capture_survives_missing_attacker_move_callback(self):
        before = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
            "king,w,a1;bomb,w,g1;king,b,h10;rook,b,g8"
        )
        after_capture = (
            "w;hm=0;fm=2;ep=-;cont=0;forced=-1;epv=-1;"
            "king,w,a1;king,b,h10"
        )

        class BombEngine:
            @staticmethod
            def legal_moves(position):
                if position == before:
                    return ["g8-g1", "h10-h9"]
                return ["a1-a2"]

            @staticmethod
            def apply(position, move):
                if position == before and move == "g8-g1":
                    return after_capture
                return position

        beliefs = MODULE.BeliefSet(BombEngine(), (before,))
        self.assertEqual(beliefs.observe_unlogged_bomb_capture(), ["g8-g1"])
        self.assertEqual(beliefs.positions, [after_capture])

    def test_hidden_ghost_move_ignores_private_release_coordinates(self):
        first = (
            "b;king,w,a1;king,b,h10;"
            "ghost,b,c3,0,0,0,0,0,0,-1,1,-1,0"
        )
        second = (
            "b;king,w,a1;king,b,h10;"
            "ghost,b,d3,0,0,0,0,0,0,-1,1,-1,0"
        )

        class FakeEngine:
            @staticmethod
            def legal_moves(position):
                return (["c3-b2", "c3-d2"] if ";ghost,b,c3," in position
                        else ["d3-c2", "d3-e2"])

            @staticmethod
            def apply(position, move):
                source, target, _separator = MODULE.parse_engine_move(move)
                return position.replace(
                    f"ghost,b,{source}", f"ghost,b,{target}"
                ).replace("b;", "w;", 1)

        beliefs = MODULE.BeliefSet(FakeEngine(), (first, second))
        beliefs.observe_move(
            MODULE.AppEvent("move", "ghost", "c3", "b2"), True)
        self.assertEqual(len(beliefs.positions), 4)
        self.assertTrue(any(";ghost,b,d2," in upn for upn in beliefs.positions))
        self.assertTrue(any(";ghost,b,e2," in upn for upn in beliefs.positions))

    def test_hidden_ghost_move_excludes_unreported_capture_worlds(self):
        before = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
            "king,w,a1;queen,w,d2;king,b,h10;"
            "ghost,b,c3,0,0,0,0,0,0,-1,1,-1,0"
        )
        quiet = before.replace("ghost,b,c3", "ghost,b,b2").replace(
            "b;", "w;", 1
        )
        capture = (
            "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
            "king,w,a1;king,b,h10;"
            "ghost,b,d2,0,0,0,0,1,1,-1,1,-1,0"
        )

        class FakeEngine:
            @staticmethod
            def legal_moves(_position):
                return ["c3-b2", "c3-d2"]

            @staticmethod
            def apply(_position, move):
                return quiet if move == "c3-b2" else capture

        beliefs = MODULE.BeliefSet(FakeEngine(), (before,))
        beliefs.observe_move(
            MODULE.AppEvent("move", "ghost", "c3", "b2"), True
        )
        self.assertEqual(beliefs.positions, [quiet])
        self.assertIn("queen,w,d2", beliefs.positions[0])

    def test_visible_ghost_uses_public_destination_not_private_origin(self):
        first = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
            "king,w,a1;king,b,h10;ghost,b,c3,0,0,0,0,0,0,-1,1,-1,0"
        )
        second = (
            "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
            "king,w,a1;king,b,h10;ghost,b,d3,0,0,0,0,0,0,-1,1,-1,0"
        )

        class FakeEngine:
            @staticmethod
            def legal_moves(position):
                return (["c3-b2", "c3-d2"] if ";ghost,b,c3," in position
                        else ["d3-c2", "d3-e2"])

            @staticmethod
            def apply(position, move):
                source, target, _separator = MODULE.parse_engine_move(move)
                return position.replace(
                    f"ghost,b,{source}", f"ghost,b,{target}"
                ).replace(
                    ",0,0,0,0,0,0,-1,1,-1,0",
                    ",0,0,0,0,1,1,-1,1,-1,0",
                ).replace("b;", "w;", 1)

        beliefs = MODULE.BeliefSet(FakeEngine(), (first, second))
        beliefs.observe_visible_ghost_destination("b2")
        self.assertEqual(len(beliefs.positions), 1)
        self.assertIn(";ghost,b,b2,", beliefs.positions[0])
        self.assertNotIn(";ghost,b,c3,", beliefs.positions[0])

    def test_adjacent_giant_footprints_are_not_merged(self):
        cells = tuple(
            f"{file}{rank}"
            for rank in (8, 9) for file in ("c", "d", "e", "f")
        )
        self.assertEqual(MODULE.giant_anchors(cells), ["c8", "e8"])

    def test_unique_three_cell_giant_footprint_is_completed(self):
        self.assertEqual(MODULE.giant_anchors(("c8", "d8", "c9")), ["c8"])

    def test_ambiguous_sparse_giant_footprint_is_rejected(self):
        with self.assertRaises(RuntimeError):
            MODULE.giant_anchors(("c8", "d8"))

    def test_royals_are_ambiguous_and_ghost_count_comes_from_public_total(self):
        own = (("king", "a1"),)
        enemy = (("king", "a10"), ("king", "b10"), ("rook", "c10"))
        # Visible cost 23 (Jester 10 + Rook 13); a hidden Ghost raises it to 38.
        positions = MODULE.initial_beliefs(own, enemy, 38, limit=16)
        self.assertGreater(len(positions), 2)
        for upn in positions:
            self.assertEqual(upn.count(";king,b,"), 1)
            self.assertEqual(upn.count(";jester,b,"), 1)
            self.assertEqual(upn.count(";ghost,b,"), 1)
            self.assertIn(",0,0,0,0,0,0,-1,1,-1,0", upn)

    def test_ranked_spawn_journal_recovers_captured_onyx_public_group(self):
        spawns = (
            MODULE.AppEvent("draft_piece_spawn", "queen", "6:1"),
            MODULE.AppEvent("draft_piece_spawn", "king", "0:0"),
            MODULE.AppEvent("draft_piece_spawn", "king", "7:0"),
            # Local Onyx's fixed King rotates to a1 and is filtered out.
            MODULE.AppEvent("draft_piece_spawn", "king", "7:9"),
            # The hidden coordinate was discarded at parse time.
            MODULE.AppEvent("draft_piece_spawn", "ghost", None),
        )
        public = MODULE.ranked_spawn_public(spawns, local_ivory=False)
        self.assertEqual(
            public,
            [("queen", "b9"), ("king", "a10"), ("king", "h10")],
        )
        self.assertEqual(
            MODULE.ranked_public_roster(public, 27),
            MODULE.Counter({"queen": 1, "jester": 1}),
        )

    def test_ranked_spawn_journal_filters_local_ivory_without_rotation(self):
        spawns = (
            # Local Ivory's fixed King is outside the opponent home ranks.
            MODULE.AppEvent("draft_piece_spawn", "king", "0:0"),
            MODULE.AppEvent("draft_piece_spawn", "queen", "6:8"),
            MODULE.AppEvent("draft_piece_spawn", "king", "7:9"),
        )
        self.assertEqual(
            MODULE.ranked_spawn_public(spawns, local_ivory=True),
            [("queen", "g9"), ("king", "h10")],
        )

    def test_captured_winning_ranked_trace_accumulates_group_deltas(self):
        def group(*records):
            return MODULE.ranked_spawn_public(tuple(
                MODULE.AppEvent("draft_piece_spawn", piece, source)
                for piece, source in records
            ), local_ivory=False)

        opening = group(
            ("pawn", "7:2"), ("knight", "4:1"),
            ("bishop", "6:0"), ("queen", "7:0"),
            ("king", "0:0"), ("king", "7:9"),
        )
        middle = group(
            ("checker", "1:0"), ("turtle", "1:2"),
            ("turtle", "0:2"), ("berserker", "0:1"),
            ("parasite", "1:1"),
        )
        final = group(
            ("pawn", "6:2"), ("bishop", "2:0"), ("rook", "2:2"),
        )
        cumulative = opening + middle + final
        self.assertEqual(
            MODULE.ranked_public_roster(cumulative, 100),
            MODULE.Counter({
                "pawn": 2, "knight": 1, "bishop": 2, "queen": 1,
                "checker": 1, "turtle": 2, "berserker": 1,
                "parasite": 1, "rook": 1,
            }),
        )
        self.assertEqual(
            {square for piece, square in opening if piece == "king"},
            {"h10"},
        )

    def test_hidden_only_ranked_group_is_a_valid_empty_public_delta(self):
        self.assertEqual(
            MODULE.ranked_spawn_public((
                MODULE.AppEvent("draft_piece_spawn", "ghost", None),
            ), local_ivory=True),
            [],
        )

    def test_ranked_public_roster_uses_material_only_for_hidden_ghost_count(self):
        public = (
            ("king", "a10"), ("queen", "b10"), ("king", "c10"),
            ("copycat", "d9"), ("copycatClone", "e9"),
            ("giant", "f8"), ("giant", "g8"),
            ("giant", "f9"), ("giant", "g9"),
        )
        # Queen 17 + Jester 10 + CopyCat 5 + Giant 1 + hidden Ghost 15.
        roster = MODULE.ranked_public_roster(public, 48)
        self.assertEqual(roster, MODULE.Counter({
            "queen": 1, "jester": 1, "copycat": 1,
            "giant": 1, "ghost": 1,
        }))
        adjusted = MODULE.ranked_public_roster(public, 47)
        self.assertEqual(adjusted["ghost"], 1)

    def test_ranked_first_pick_chronology_excludes_late_jester_from_king(self):
        positions = MODULE.initial_beliefs(
            (("king", "a1"),),
            (("king", "a10"), ("king", "b10")),
            10,
            limit=8,
            enemy_king_candidates={"a10"},
        )
        self.assertTrue(positions)
        self.assertTrue(all(";king,b,a10" in position for position in positions))
        self.assertTrue(all(";jester,b,b10" in position for position in positions))
        self.assertTrue(all(";king,b,b10" not in position for position in positions))

    def test_material_mismatch_is_rejected(self):
        with self.assertRaises(RuntimeError):
            MODULE.initial_beliefs((("king", "a1"),),
                                   (("king", "a10"), ("rook", "b10")), 18)

    def test_post_move_sludge_goop_keeps_original_public_material(self):
        # Onyx initialization observes the current board after Ivory has moved.
        # Goop is a public zero-point product; the still-visible Sludge retains
        # its twelve draft points in the unchanged army counter.
        enemy = (
            (("king", "d8"), ("king", "e8"), ("king", "f8"),
             ("king", "d9"), ("king", "e9"), ("king", "f9"),
             ("parasite", "g8"), ("berserker", "g9"),
             ("goop", "e10"), ("goop", "f10"), ("sludge", "g10"))
        )
        positions = MODULE.initial_beliefs(
            (("king", "a1"),), enemy, 92, limit=64)
        self.assertTrue(positions)
        self.assertTrue(all(";sludge,b,g10" in position for position in positions))
        self.assertTrue(all(position.count(";goop,b,") == 2
                            for position in positions))
        self.assertTrue(all(";ghost,b," not in position for position in positions))

    def test_duplicate_heavy_dynamic_material_reconciles_hidden_ghost(self):
        enemy = (("king", "b10"), ("bishop", "h9"),
                 ("rook", "a9"), ("rook", "b9"), ("rook", "c9"),
                 ("rook", "e10"), ("rook", "f10"), ("rook", "g10"))
        positions = MODULE.initial_beliefs(
            (("king", "a1"),), enemy, 100, limit=8)
        self.assertTrue(positions)
        self.assertTrue(all(position.count(";ghost,b,") == 1
                            for position in positions))

    def test_ranked_deployment_reserves_multicell_footprints(self):
        deployment = MODULE.DraftDeployment()
        giant = deployment.place("giant")
        giant_cells = deployment._giant_cells(giant)
        copycat = deployment.place("copycat")
        self.assertFalse(giant_cells & {copycat, deployment._mirror(copycat)})
        self.assertEqual(len(deployment.occupied), 7)  # King + Giant 4 + pair 2.

    def test_ranked_deployment_reserves_measured_copycat_anchor(self):
        deployment = MODULE.DraftDeployment()
        requested = deployment.propose("copycat")
        self.assertEqual(requested, "h2")
        # Captured Onyx drag landed one file toward the center. Reserve the
        # native coordinate and its true mirror, not the requested pixel cell.
        deployment.reserve("copycat", "g2")
        self.assertTrue({"b2", "g2"} <= deployment.occupied)
        self.assertEqual(deployment.team[-1], ("copycat", "g2"))

    def test_ranked_reference_roster_has_no_overlap(self):
        deployment = MODULE.DraftDeployment()
        pieces = ("ghost", "dragon", "jester", "bomb", "berserker",
                  "parasite", "penguin")
        for piece in pieces:
            deployment.place(piece)
        self.assertEqual(len(deployment.occupied), 1 + len(pieces))
        self.assertEqual(sum(MODULE.PIECE_COST[p] for p, _ in deployment.team), 100)


if __name__ == "__main__":
    unittest.main()
