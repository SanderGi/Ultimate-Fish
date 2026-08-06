# Chess Ultimate 5.731 native specification ledger

This ledger records behavior recovered directly from the Android ARMv7 IL2CPP
simulation classes. It distinguishes source-backed mechanics from items still
requiring differential fixtures against the app.

## Draft

`ArmyBuilder.SetTotalMaxPoints(100, draft=true)` initializes a 100-point total,
a 15-point initial group minimum, and a 40-point draft increment.
`GameManager.GetDraftPhase` exposes twelve windows:

| Window | Side | Action | Minimum new group | Cumulative maximum |
| ---: | --- | --- | ---: | ---: |
| 0 | Ivory | Ban | — | — |
| 1 | Onyx | Ban | — | — |
| 2 | Ivory | Pick | 15 | 40 |
| 3 | Onyx | Pick | 15 | 40 |
| 4 | Ivory | Ban | — | — |
| 5 | Onyx | Ban | — | — |
| 6 | Ivory | Pick | 15 | 80 |
| 7 | Onyx | Pick | 40 | 90 |
| 8 | Ivory | Ban | — | — |
| 9 | Onyx | Ban | — | — |
| 10 | Ivory | Final pick | 0 | 100 |
| 11 | Onyx | Final pick | 0 | 100 |

The `Character.value` initializer is a 30-element integer array, in exact
`Character.Type` order:

`0, 10, 6, 3, 17, 13, 9, 15, 15, 20, 4, 15, 8, 0, 15, 15, 15, 0, 12, 17, 18, 2, 2, 1, 5, 0, 13, 0, 12, 15`.

This is now the engine/UI draft-cost table. Generated forms retain their
serialized entries but are not selectable.

Picks are deployed during each pick window, not in a separate post-draft
step. Native `GameMenu.LockinPicks` scans the live board for newly placed
characters, validates their cumulative point spend, clears their temporary
pick markers, and only then advances `GameManager.ChangeTurnDraft`. The local
UI mirrors that sequence. `LockinPicks` checks the new group's spend against
`CUR_MIN_POINTS` and the cumulative spend against the phase cap. This is
confirmed by the ARMv7/ARM64 `SetTotalMaxPoints`, `GetDraftPhase`, and
`LockinPicks` paths and by a live 27-point opening group; the following Onyx UI
displayed the corresponding "more than 15 and less than 40" range.

Every committed group is immutable and becomes public before the other player
acts. The partial opponent deployment is therefore updated after each pick
window rather than hidden until the end. Royal chronology is public evidence:
only silhouettes present in the first revealed group can be the original King;
a royal silhouette first added by a later locked group is certainly a Jester.
Invisible Ghost coordinates remain private even though their count follows
from the public cumulative material total.

The shipping Ranked scene constructs all selectable pots from character
prefabs, stably sorts them by the native `Character.value` table, and traverses
a live 8x3 top/middle/bottom layout. The verified equal-cost order is
Dragon, Ghost, Bomb, Penguin, Parasite, Devil, Berserker for the seven 15-point
pots. `OnSpawnPieceGroup` delivers each newly committed group to the app and
then advances the phase. The stock release exposes only the callback marker,
but its subsequent public `Square.Spawn` journal names each model in that newly
committed group as a `prefab x:y` record. The opening journal also redraws the
two fixed Kings. Phone automation accumulates these immutable group deltas,
reconciles roster additions against the public cumulative material log, and
records first-group royal candidates. The parser discards enemy Ghost
coordinates before journaling; only their count is inferred from material.
`OnSanityCheck` is not a reliable spawn delimiter: some live groups omit it
until the following local action, while another emitted it 357 ms before the
first delayed `Square.Spawn` coroutine. The marker is ignored as an end
delimiter; a non-empty journal needs 120 ms of spawn-log quiet, and the consumer
continues waiting until the cumulative
public cells and inferred Ghosts reconcile exactly with the announced material;
the opening candidate must also contain a royal. Ranked automation fails closed
if that public journal is absent and never falls back to tapping cells obscured
by character pots.
Either player's final group may legally stop below 100; an opponent is not
required to spend the full budget. A live autonomous Onyx draft was accepted at
90 points and played to completion. Ranked initialization therefore uses each
side's last public cumulative total from `GetPoints`, not the global 100-point
ceiling, when reconstructing the exact public roster (including hidden Ghost
count) and royal hypotheses.
`OnBanCharacter` is public. Its subsequent native
`TEXURE ASSIGNED TO <piece>` diagnostic identifies the exact newly locked pot,
so the controller applies that public ban without overlapping-pot image
differencing. After pot calibration, a harmless local-pot touch logs the native
`myBoard.turn != team` predicate. The controller binds it to the non-consuming
count of Bans already completed, because a fast remote phase zero can finish
during calibration; together they establish whether the player is Ivory or
Onyx. Repeated
`OnBanCharacter(Type)` method frames from the same callback are debounced as
one transition before the next phase can advance. Both its unique callback
count and named pot are journaled independently of queue consumers, so a remote
phase-zero Ban completed during pot calibration is replayed into the engine
before the twelve-window loop continues.

An opponent timeout or forfeit can end the game during any of their six draft
windows, before a playable board exists. The result overlay is classified at
that terminal callback and returned as a completed game, allowing a multi-game
Ranked run to retain the win and requeue instead of treating it as controller
failure. A live opponent phase-six forfeit awarded +15 rating.

Local Ranked placement is likewise acknowledged rather than assumed. Each pot
drag collects the native landing coordinate, `ArmyMove` identity, and complete
cumulative `GetPoints` transition before the cell is reserved in the engine
draft. This matters for wide colliders such as CopyCat, which can land one file
inward and create its mirror there. A live Prince drop aimed at a clear `a3`
was intercepted at `c3`, despawned the pending Ghost there, and produced the
authoritative point sequence 22 -> 7 -> 25. Pending wide and high-value models
are now placed first, and every new cell is ranked by clearance. More
importantly, a replacement no longer aborts the controller: the removed cost
and reported landing identify the displaced pending model(s), their cells are
released, the actual landing is reserved, and the missing desired roster is
re-added during the same pick window. An accidentally selected extra type is
similarly replaced in place before remaining missing models move to clear
cells. Earlier committed groups are never touched. The cumulative total and
exact pending roster must both equal the engine's desired pick before the
controller presses the icon-only green Lock checkmark.

Although `LoadBoardDraft` retains the full live Board object, the pick camera
renders the local home zone as a compact 8x3 grid. It does not share the later
settled 8x10 gameplay coordinates. On the reference 1080x2400 portrait layout
the stable deployment edges are `(8,1145)` through `(912,1465)`, scaled with
the device resolution. Group placement backtracks all still-mutable pieces so
four 2x2 Giants can be packed alongside ordinary models; a failed target is
excluded only for that duplicate instance and never terminates the controller
during an active Ranked clock. Giants are a special native placement case:
`Giant.getClosestIntersection` snaps their 2x2 footprint around a grid
intersection. The controller therefore drops at the exact center intersection
of the requested four cells. Dropping at an ordinary anchor-cell center is
ambiguous and was observed to shift `c2/e2/g2` down to `c1/e1/g1`, where the
fixed King prevents a fourth Giant from fitting.

After selecting a Ban pot, the current Ranked UI exposes the actionable large
red Ban button in the lower-left character inspector. The blue top-row `BAN`
element is only the phase label, while the selected-pot red speech bubble is
also not a reliable pointer target. The controller waits for the native local
turn predicate, detects only the fixed lower-left red control, and waits for
the named native Ban callback before advancing.

`OnStartGameResponse.model_Pieces` and replay `initState` share the authoritative
`Model_Piece` schema: type, team, x/y, skin, action, cooldown, freeze count,
movement-turn state, and army flag. The phone controller validates that schema,
ignores cosmetic skins, retains exact local records, and reconstructs hidden
enemy beliefs behind a dedicated sanitizer. The stock Android 5.73 build's
logcat currently emits only `OnStartGame MESSAGE`, not the serialized response;
therefore normal CPU and Unranked automation uses the public tap probe. Ranked
uses the public post-commit spawn journal recovered above, including its final
pre-move position. The tap probe requires unanimous occupancy across three
unobscured frames, requires one consistent native piece identity across an
adaptive 3×3 hit probe, rejects stable outline spill only after all nine points
select nothing, and fails before moving on singular or conflicting evidence.
The tap-free parser
is dormant unless an operator explicitly supplies `--structured-state` with a
compatible diagnostic/replay source.

Ranked reuses the draft board for gameplay and does **not** call
`Board.LoadBoard` after phase eleven. The final `OnSpawnPieceGroup` is the
public reveal barrier; the existing board then runs its roughly ten-second
intro before accepting gameplay input. Phone automation returns directly to
perspective calibration with a 15-second Ranked reveal allowance. Its separate
gameplay journal remains non-consuming during that wait, so a quick Ivory
opening is replayed into an Onyx engine position rather than discarded while
waiting for a nonexistent load callback. `Board.LoadBoardDraft` starts a new
journal generation just like ordinary `Board.LoadBoard`; otherwise a duplicate
terminal event from the preceding Ranked result can make a new Onyx game appear
finished and cause the following app restart to forfeit its still-live board.
Local post-draft verification accepts either two exact native selection
acknowledgements or one exact acknowledgement corroborated by that cell's blue
ownership outline. The latter is required for duplicate Jesters observed live;
one correct callback was emitted for each occupied cell before the turn state
suppressed further selection, even though both placements were valid.

## Board, deployment, and analysis state

`SimulatedBoard` stores an 8x10 array. Native army save/load traverses an 8x3
home zone for each side. Each pick window therefore consumes its finite new
roster additions into ranks 1-3 for Ivory and ranks 8-10 for Onyx. A Giant
anchor must leave its entire 2x2 footprint inside that zone; placing a CopyCat
also requires its mirrored clone square to be free.

Ultimate Position Notation (UPN) records every search-relevant field: side to
move, half/full move counters, en-passant square and exact victim, forced
continuation, visibility, cooldown, action, freeze, power, moved state,
links/hosts, and Angel attachment order. Serialization compacts live piece IDs
and remaps every relationship so captures cannot corrupt a round trip.

## Source-backed interactions implemented

- A bomb's capture or death invokes its radius-one explosion and removes the
  bomb and every character in that area. Ordinary attackers land inside the
  blast and die; a distant Sniper remains on its origin and survives. Bombs
  caught in the area trigger their own chained radius-one explosions.
- Ninja destinations are generated independently of intervening occupancy for
  each configured direction and distance.
- A prince may capture with its first one-square move. An empty first step is
  generated as the first half of a queued second one-square move, which may be
  quiet or capturing; only the completed pair ends the side's turn.
- If a Checker has a jump, that Checker offers jumps instead of quiet steps,
  but it does not suppress moves by the side's other characters. Once the
  first jump is chosen, further jumps by that Checker are forced. Checkers
  promote at the far rank.
- Sludge creates goop on its origin and, for a two-square move, the intervening
  square.
- Goop has no moves. When a melee character other than a bomb attacks it, goop
  kills that attacker while dying itself. Ranged and support attacks do not
  trigger this retaliation.
- When a parasite attacks, it dies and changes the target to its team. When an
  opposing melee character attacks a parasite, the attacker is possessed;
  ranged/support characters and bombs kill it normally.
- Angel attachment removes the angel from board occupancy, creates a halo at
  its origin, and links the host/angel/halo state. A lethal host hit consumes
  the angel and returns the host to the halo. Halo death also removes the
  angel. An Angel may protect another Angel; when that protected Angel later
  attaches to a host, its dependent Angel is reparented to the same host and
  the rescue layers are consumed in attachment order. For an Onyx Giant, the
  native rescue path subtracts one file and rank from the Halo coordinate
  before clamping the canonical lower-left 2x2 anchor.
- Cooldowns decrement on every side change. Setting the native base value of
  three therefore produces one skipped owner turn.
- A Giant occupies `(x,y)`, `(x+1,y)`, `(x,y+1)`, and `(x+1,y+1)`. It moves
  exactly two coordinates orthogonally and knocks out every enemy character in
  the destination footprint; any friendly character in that footprint blocks
  the move. The four destination cells are resolved in native sequence: if an
  Angel rescues a struck enemy onto another one of those cells, the Giant
  strikes it again. Generic movers occupy their destination before dispatching
  the victim's death callback. Consequently, if an Angel-rescued Giant's new
  footprint covers that destination, the rescued Giant knocks out the
  attacker rather than sharing or surrendering the square.
- A Mage selecting a Giant footprint tile translates the Giant so that tile
  lands on the Mage's origin. Native forced-Giant resolution then knocks out
  every other character in the translated footprint, including the Mage when
  its selected destination overlaps that footprint.
- Constructing a CopyCat creates its clone at `(7-x,y)`. Either half moves one
  square in any direction while the available partner applies `(-dx,dy)`.
  Both targets must be legal, both captures resolve, and either half's death
  removes the other. A stunned partner does not mirror that move.
- Fisherman has queen-direction quiet rays. The first visible character on a
  ray can be hooked only at distance two or greater, regardless of team; it is
  pulled to the adjacent ray square while the Fisherman stays put. Hooking a
  Giant translates its anchor so the selected footprint square lands there;
  native forced-Giant movement knocks out allies and enemies in all four
  destination cells.
- Penguin/`SimulatedFreeze` steps to any adjacent empty square but cannot
  attack. It has no aura merely from deployment: after the Penguin moves, it
  freezes the adjacent non-Penguin characters until its next move or death,
  with counts stacking across Penguins. The action byte encodes that currently
  frozen direction set for serialization.
- A Ghost is revealed by attacking or by enemy King/Jester adjacency. A quiet
  Ghost move away from those royals makes it invisible again.
- Sludge may blindly enter a square occupied by an invisible enemy Ghost. Both
  Sludge and Ghost are knocked out, the Sludge still leaves Goop on its origin,
  and the dying Sludge emits no ordinary movement callback. The public online
  action plus `ChangeTurn End` is therefore used to apply that exact move. A
  live Ranked `c5-c4` collision reduced the two material totals by 12 and 15,
  respectively, confirming that both characters died.
- An invisible Ghost makes its square unavailable while ordinary sliding rays
  continue through it, even for a same-team ray. Pawns cannot deliberately capture it
  diagonally; a forward pawn move into it uses the native blind-collision path
  and knocks out both characters.
- A Sniper's forward shot also passes through invisible Ghosts to the first
  visible character. Its one-square sideways action is quiet-only; an occupied
  lateral square produces no legal Dot. Only a forward shot starts the reload
  cooldown. When the first target is a Giant, the shot targets the actual
  footprint cell encountered on the Sniper's file, not the Giant model's
  potentially off-file stored anchor.
- When a Sniper shoots a Bomb, the Bomb death path can invoke the Sniper's
  stay-put movement callback again just after `ChangeTurn End`. That delayed
  duplicate has no new `Bot.RecordAiMove` diagnostic and no legal shot target
  on the opponent turn; the phone controller discards only this exact stale
  combination so it cannot be mistaken for the opponent's action.
- A linked CopyCat action emits one movement callback for each half before the
  common `ChangeTurn End` barrier. The phone controller consumes through that
  barrier so the companion callback cannot be mistaken for an opponent move.
- A Mage can choose any of a Giant's four footprint tiles as its swap target
  when the translated Giant remains in bounds. The Mage lands on the selected
  tile; the Giant is forcibly translated to the Mage and knocks out either
  team's characters in its new footprint.
- A Mage swap preserves the displaced target's movement-turn state. It does
  not count as that piece taking a turn: an unmoved Pawn moved from `h9` to
  `a3`, for example, can still double-step to `a1` and promote.
- An unmoved pawn can double-step from any valid deployment rank when both
  forward squares are available. Promotion occurs on rank 10 for Ivory and
  rank 1 for Onyx. En-passant stores and removes the exact pawn victim.
- King and Jester both use `SimulatedKing` castling. An unmoved royal scans
  horizontally to the first occupied square; when it is an unmoved Rook at
  least three files away, the royal may move two files toward it and the Rook
  lands on the square beside the royal. The Rook need not share the royal's
  team. With an enemy-owned Rook, the live Character path visibly relocates
  both pieces and advances `board.turn`, but never hands `playerTeam` to the
  opponent. That opponent cannot act and deterministically loses when their
  action clock expires, so Ultimate Fish scores the move as a terminal forced-
  timeout win. Castling does not test check on the starting or crossed square.
- While a side still owns a Jester, either royal silhouette may remain in
  check. The opponent has not proved which silhouette is the real King, so
  ordinary real-King check filtering is suspended until that Jester is gone;
  capturing the actual King still ends the game immediately. This was
  confirmed in Ranked when a public linked CopyCat move was accepted while its
  owner's real King remained attacked. Check filtering resumes in the first
  resulting position without a living Jester.
- The native insufficient-material table is implemented, including its special
  minor/color-bound/support combinations. With both real kings present and
  neither team sufficient, the result is a draw.

## Conformance status

Conformance is an active release gate, not a completeness claim. The current
engine has a native-code ledger, focused C++ regressions, deterministic Android
Local fixtures, and coverage-guided Local self-play, but the interaction matrix
is not yet exhausted. [conformance-matrix.md](conformance-matrix.md) records
which mechanics have all three kinds of evidence and which still require a
targeted native fixture.

The latest gap pass added the native Berserker Chebyshev leap/growth behavior,
ordinary-move exclusion for invisible Ghost squares, Mage-triggered
pawn/checker promotion, Fisherman relocation of Angel-protected hosts, dynamic
Berserker material, the 24-cell deployment limit with correct Giant/CopyCat
footprints, Onyx's color-relative Giant/Halo rescue anchor, and Sniper targeting
of the actual Giant footprint cell rather than its off-file stored anchor.
Both Giant/Sniper/Onyx-Angel rescue and nested ordered Angel rescues now have
deterministic shipping-app Local fixtures in addition to C++ regressions.

Hidden information follows the shipping simulator rather than a chess-style
determinization: the position retains an invisible Ghost's coordinate, while
move generation applies the native blind-square, ray pass-through, pawn
collision, royal reveal, and Sniper/Fisherman exceptions. Play mode additionally
conceals enemy Ghost rendering, inspector data, capture-ring styling, invisible
Ghost move coordinates, and enemy Jester identity. Analysis mode deliberately
shows that state with a dashed translucent treatment.

The lossless `visible` flag is relative to each Ghost's opponent, not the local
UI. A player's own deployed Ghost is visible on that player's screen but begins
invisible in the engine state because enemy rays must pass through it. This is
essential when an enemy Sniper has the real King as its first visible target.

Engine play represents public state as an information set of those concrete
positions. The native `belief clear` / `belief add <upn>` / `belief go` protocol
intersects legal action notation across every retained belief. It audits every
common root at depth two across the complete set, preserving immediate hidden-
Ghost recaptures even outside the deep sample, and then deep-searches a robust
shortlist across up to eight beliefs in parallel. The sample begins evenly
spaced, and each root's worst shallow belief replaces one representative when
necessary. Root utility is maximin, with complete-set shallow safety and mean
score as deterministic tie-breaks. The phone controller
only supplies public observations and verifies the returned root; it contains
no piece-value, Ghost-adjacency, plurality, or repetition move override.

A public continuing-turn event removes royal hypotheses in which the captured
silhouette was the real King. A quiet invisible Ghost move expands over all
legal hidden endpoints, a public attack applies its exact revealed source and
destination, and `MakeVis` uses only the newly visible destination while
rehydrating every adjacent legal hidden origin. Private release-log coordinates
never participate in those transitions.

The executable fixtures cover movement, queued actions, automatic turn effects,
promotion, cooldown, freeze stacking, visibility, linked death/relocation,
multi-square and mirrored occupancy, area effects, possession/retaliation,
en-passant, victory/draw outcomes, UPN round trips, every ranked draft window,
AI draft completion, and deployment capacity. Address/Undefined sanitizers pass
the same suite.

Live Android differential play additionally confirmed unconditional quiet
Prince first steps followed by quiet or capturing second steps. A first step
made while checked is legal only when at least one forced second step resolves
that check. The same runs confirmed Bomb removal of
both combatants before the radius-one blast, adjacent Giant footprint tiling,
Mage stay-put swap diagnostics, cosmetic prefab aliases, concealed/revealed
Ghost transitions, and King/Jester ambiguity. A duplicate-heavy six-Rook army
also demonstrated the shipping counter's bounded dynamic adjustment: its
static visible-plus-Ghost values total 102 while the public counter reads 100.
The phone controller reconciles only the uniquely determined Ghost count within
that observed two-point adjustment and rejects larger mismatches.

For live CPU and Unranked initialization, the app's public `GetPoints()`
diagnostic is authoritative over the animated three-tile counter. The
controller retains the maximum player totals observed after the current
`Board.LoadBoard` and freezes them when the first action begins, so partial
spawn totals, later captures, and Berserker growth do not replace the starting
roster value. Pixel OCR is used only when that native public record is
unavailable.

No source-level divergence is currently known in the recovered 5.731 ledger,
but conformance remains open until the pending matrix fixtures and broader
interaction campaigns are complete.
