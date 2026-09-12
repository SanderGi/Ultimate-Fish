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

That identity journal must also be used after calibration. The shipping client
can print `TEXURE ASSIGNED TO <piece>` either before or after the
`OnBanCharacter` method frame. In a captured Ranked trace, the exact Angel
texture arrived first; waiting for the method frame discarded it and an
overlapping-pot image fallback incorrectly reported Mage. The controller now
reads exactly one non-consuming journal addition for the phase and has no
visual ban-identity fallback. Bans remain global and permanent, as in the
native game.

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
authoritative point sequence 22 -> 7 -> 25. Ordinary models are placed before
Giants, matching the native builder's collider behavior, while each mutable
group is planned as a complete non-overlapping packing. More importantly, a
replacement no longer aborts the controller: the removed cost
and reported landing identify the displaced pending model(s), their cells are
released, the actual landing is reserved, and the missing desired roster is
re-added during the same pick window. An accidentally selected extra type is
similarly replaced in place before remaining missing models move to clear
cells. Earlier committed groups are never touched. The cumulative total and
exact pending roster must both equal the engine's desired pick before the
controller presses the icon-only green Lock checkmark.

Although `LoadBoardDraft` retains the full live Board object, the pick camera
renders the local home zone as an 8x3 grid below three rows of character pots.
It does not share the later settled 8x10 gameplay coordinates. On the reference
1080x2400 portrait layout the playable gray-cell edges are `(60,1344)` through
`(1032,1668)`, scaled with the device resolution. The cyan aura begins near
x=8, while y=1145 through 1343 still contains pots; neither region is
raycastable board space. Using that decorative/pot rectangle caused the
leftmost drop to miss, shifted files, and placed every target above the actual
board. Group placement backtracks all still-mutable pieces so
four 2x2 Giants can be packed alongside ordinary models; a failed target is
excluded only for that duplicate instance and never terminates the controller
during an active Ranked clock. Giants are a special native placement case:
`Giant.getClosestIntersection` snaps their 2x2 footprint around a grid
intersection. Recovered ARM code at `0x168d290` loads the world positions of
the four squares `(x,y)`, `(x+1,y)`, `(x+1,y+1)`, and `(x,y+1)`, sums their
three coordinates, and multiplies each by `0.25`. This happens after ArmyMove
has resolved the logical anchor: the pointer-up itself must be at the center of
that raycastable anchor cell. A live attempt to drop directly on the computed
intersection was rejected for every Giant because grid seams have no Square
collider. Giant drops use the shipping-Local-proven point 41% toward the bottom
of the requested cell. This remains within the same Square collider while
avoiding character-model interception. Multiple
Giants are dragged contiguously from the open left edge (`a,c,e,g`) rather than
maximum-clearance order (`a,g,e,c`); the latter left its final target between
two oversized 3D colliders and was rejected despite a legal logical packing.
Ordinary models are placed before Giants, matching the native Local builder;
a live Prince placed afterward otherwise intercepted and replaced a logically
disjoint Giant. Prince's far-right pot also has a large drag-model offset: a
live `b1` target physically landed on `c2`. Ranked therefore prefers the
Local-validated `f1,g1` back-rank cells for Princes before placing Giants. A
two-sided Local fixture measured both Prince drops one rank high from their
cell centers and learned the same `+52 px` correction on 111-pixel cells;
Ranked applies that native-measured 47% downward in-cell bias up front. If any
ordinary pot still reports a different public landing, Ranked now drags that
already placed model to the requested square and iteratively compensates from
the next native `ArmyMove` coordinate, exactly like the Local builder. The
planner never reserves the intended square until the physical correction is
acknowledged, so a known misdrop is not represented as uncertainty.

A fully autonomous live Ranked validation locked the exact difficult opening
group `Prince@f1,g1; Giant@a2,c2,e2,g2`. The later windows also recovered
transient landings and locked `Prince@d1,e1; Pawn@c1`, then
`Prince@h1; Checker@b1`, producing the evolved 99-point roster without timeout
or manual input. The resulting game remained synchronized and ended in an
Ultimate Fish forced-mate win.

Jester placement timing is strategically observable. Once a player's opening
group has locked and revealed the fixed King's square, a Jester added in a
later immutable group cannot make that known King location ambiguous. The
draft policy therefore applies a prohibitive late-Jester penalty while keeping
the choice rules-legal for replaying an opponent's public draft exactly.

The coarse native 24-cell limit is not enough to prove a later group is
physically packable. In one Onyx trace, a Giant requested at `c2` legally
locked at `c1`; together with Giants at `a2,e2,g2`, that exact layout left five
single cells but no free mirror pair for the engine's proposed CopyCat. Draft
selection now uses a non-mutating `draft preview` protocol. The controller
tests each complete proposed group against the exact locked footprints, then
minimally excludes the weakest conflicting proposal and previews again before
issuing any real `draft choose` or `draft commit`. For the captured state,
excluding CopyCat changed `Prince Prince Mage CopyCat` into the packable exact
50-point group `Prince Prince Mage Knight`; the engine draft phase remains
unchanged throughout previewing.

The next live Ranked game validated the combined draft path end to end. It
used only journaled Ban identities, locked all three exact local groups, and
selected `Prince; Checker` rather than late Jesters in the final window.
Gameplay stayed synchronized and ended in an opponent-clock victory worth +21
ranking.

A later draft exposed why Giant's point-only acknowledgement is insufficient:
one locked Giant footprint differed from the controller's assumed anchor, so a
later group repeatedly targeted cells that were not actually free. The local
`OnSpawnPieceGroup` callback is now consumed after every Lock, not only for the
opponent. Its exact public coordinates replace all assumed placements before
the next window is planned. A rejected pot drag which merely selects an
existing same-type model is also left untouched when the native material total
does not increase; it is never mistaken for a new misdrop to be corrected.
The pot retry must also vary its physical pointer-up point. A live missing
Prince remained legal but the default point in each attempted cell intersected
a locked Giant's oversized model collider; a manual drag to a clear part of
the cell succeeded. Ranked now sweeps five bounded points inside the same
logical cell before excluding it, while still requiring a native material
increase and landing record. This is geometric input recovery, not an engine
position assumption.

Remote Ranked pick clocks are authoritative. One opponent took longer than the
controller's former 75-second guard while the native display still showed 72
seconds remaining. Opponent Ban/Pick waits therefore have no synthetic
deadline: they poll until the matching commit callback or native terminal
event and periodically report liveness. The captured match ultimately ended
in a native opponent-forfeit victory worth +15 ranking.

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
- Checker Kings slide along all four diagonals. The 5.731 ARMv7 method
  `SimulatedChecker.GetPieceSimulations` at `0x18163b8` sets `movementAmount`
  to 8 for type 22, versus 1 for ordinary type 21. A capture scans to the
  first visible enemy and lands exactly one square beyond it, not at an
  arbitrary square past it. Friendly occupancy or an unavailable landing
  blocks that jump. `GetAllQueuedMoves` (`0x18177d8`) and
  `HasAvailableAttack` (`0x1818c84`) also use this long range; promotion in
  `ShouldUpgrade` (`0x1819514`) changes the range immediately, including
  during a capture chain. Hidden enemy Ghosts remain apparent quiet squares
  along the move-generation ray and are not exposed as jump victims.
  These addresses were resolved from the authenticated ARMv7 package's own
  metadata and Assembly-CSharp method table on 2026-09-12; the older interaction
  audit's Checker addresses do not identify these methods in that binary.
  Existing Checker-containing tablebase payloads encode the old short-range
  rules and are rejected pending regeneration and a new compatibility marker.
- Sludge creates goop on its origin and, for a two-square move, the intervening
  square.
- Goop has no moves. When a melee character other than a bomb attacks it, goop
  kills that attacker while dying itself. Ranged and support attacks do not
  trigger this retaliation.
- When a parasite attacks, it dies and changes the target to its team. When an
  opposing melee character attacks a parasite, the attacker is possessed;
  ranged/support characters and bombs kill it normally. Possession changes the
  team on the existing native character object rather than replacing its type
  or identity. A possessed Jester therefore remains the same Jester: its former
  owner retains that private provenance, while an observer whose King/Jester
  assignment was ambiguous learns it from the non-terminal possession result.
  The corresponding Ghost exception is visual rather than ontological: the
  attached Parasite remains public, so both players can track that possessed
  Ghost even when the Ghost model itself fades.
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
  square in any direction while an available partner applies `(-dx,dy)` from
  its current (not necessarily reflected) square. Both targets must be legal,
  both captures resolve, and either half's death removes the other. A frozen
  or cooldown partner stays put, so the pair can cease to be file mirrors.
  Linked death is atomic with respect to Angel protection: an Angel on the
  initially struck half can save the pair, but after that half is marked dying
  the partner's death callback bypasses its own Angel. The app therefore does
  not leave a singleton CopyCat when only the partner was protected.
- Fisherman has queen-direction quiet rays. The first visible character on a
  ray can be hooked only at distance two or greater, regardless of team; it is
  pulled to the adjacent ray square while the Fisherman stays put. Hooking a
  Giant translates its anchor so the selected footprint square lands there;
  native forced-Giant movement knocks out allies and enemies in all four
  destination cells. If the adjacent landing cell contains an invisible
  Ghost, the app dispatches death independently to both the dragged character
  and the Ghost. Thus pulling a King into its own hidden Ghost knocks out both;
  an attached Angel may still intercept either individual death callback.
- Penguin/`SimulatedFreeze` steps to any adjacent apparently empty square but
  cannot attack a visible character. Its inherited invisibility branch still
  treats an enemy hidden Ghost as an available step; arriving there captures
  the Ghost through a Penguin-specific mutual knockout, so both pieces die.
  It has no aura merely from deployment: after an ordinary Penguin move, it
  freezes the adjacent non-Penguin characters until its next move or death,
  with counts stacking across Penguins. The action byte encodes that currently
  frozen direction set for serialization.
- A Ghost is revealed by attacking or by enemy King/Jester adjacency. Either
  side of the adjacency can initiate it: a Ghost stepping into royal range and
  a King or Jester stepping beside a hidden Ghost both reveal it. The latter is
  explicit in the shipping live-play path (`King.HasMovedHandler` calls
  `King.CheckRadiusForGhost`, which Jester inherits). Local pass-and-play has a
  playback bug that can fail to present that royal-initiated reveal, so Local
  fixtures are not an authority for this one interaction. A quiet Ghost move
  away from those royals makes it invisible again, but because the previously
  visible model animates its direction, both endpoints of that first fading
  move remain public. The live `Ghost.MoveTowards` coroutine reaches the
  destination before `Ghost.DoneMoving` resolves the visibility transition.
  Only a later move begun while hidden makes its new square unknown and
  expands the location belief.
- A Parasite that attacks an opposing Ghost possesses it normally, but the
  floating Parasite model does not inherit Ghost invisibility. The possessed
  Ghost is therefore permanently public to both players, including its
  original owner, and remains public after every later Ghost move. The live
  `Parasite.AttackHandler` dispatches through `TakeOverAndAttach`; the attached
  Parasite GameObject persists independently of the Ghost renderer.
- Sludge may blindly enter a square occupied by an invisible enemy Ghost. Both
  Sludge and Ghost are knocked out, the Sludge still leaves Goop on its origin,
  and the dying Sludge emits no ordinary movement callback. The public online
  action plus `ChangeTurn End` is therefore used to apply that exact move. A
  live Ranked `c5-c4` collision reduced the two material totals by 12 and 15,
  respectively, confirming that both characters died.
- An invisible enemy Ghost is rendered as an empty cell. Ordinary moves onto
  that cell remain legal and reveal a normal capture only after arrival; the
  moving character survives unless its own piece-specific callback says
  otherwise. Sliding rays also continue through the Ghost. A same-team hidden
  Ghost is private known occupancy, so its cell is unavailable to its owner,
  but the owner's sliding ray still continues through it. Pawns cannot
  deliberately capture a hidden Ghost diagonally; a forward Pawn move into it
  uses the native blind-collision path and knocks out both characters.
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
- A linked CopyCat action emits one movement callback for each available half
  before the common `ChangeTurn End` barrier; an unavailable partner emits no
  movement callback and remains stationary. Both models emit legal-dot setup
  callbacks when either is selected, so the phone controller validates their
  current UPN-linked squares rather than assuming they remain file mirrors.
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
- Current-turn check is tested after a hypothetical opponent turn-start phase:
  `Bot.GetAllAvailableMoves` calls `SimulatedBoard.ChangeTurn`, then
  `Bot.DidKingDie`, then `UndoChangeTurn`. Cooldowns decrement and eligible
  opponent Minions advance before the threat test. Thus a Minion net with no
  legal royal escape is checkmate, not stalemate. Automatic movement may also
  uncover or obstruct another character's checking ray; frozen Minions, Angel
  rescues, and simultaneous King deaths keep their ordinary simulation rules.
  The no-move result is checkmate (CheckType 2) only for an opposing win;
  otherwise it is stalemate (3). `GameManager.GetAllMoves` maps those to
  GameOverReason Checkmate (5) and Stalemate (6). Post-action legality does not
  repeat this phase: the actual move has already changed turns. The engine
  uses this distinction for terminal results, search check evasions, and move
  notation. Recovered on 2026-09-12 from the hash-matched APK documented in
  `docs/reverse-engineering.md`: ARMv7 RVAs `0x17df510`, `0x1810908`,
  `0x17e01f0`, and `0x1852bb8` respectively (these recovered addresses differ
  from the later interaction-ledger address set). Regression:
  `b;king,w,b8;king,b,a10;devil,w,a1;minion,w,a9;minion,w,b9` is Ivory mate.
  The same error occurs with a ready-next-turn Sniper in
  `b;king,w,b8;king,b,a10;sniper,w,a9,0,1;knight,w,c8` (Ivory mate).
  Published Devil/Minion and Sniper WDL/DTW predates this rule correction and
  is not trusted by the runtime; recomputation and plot repair remain separate
  work. Both runtime probes and UFTB material compatibility reject the stale
  classes, including roots that currently have no Minions or cooldown.
- Legal-move dots are private pre-decision observations. The active player may
  select each of their own characters and inspect the destinations the native
  UI offers before committing an action; the nonmoving opponent does not see
  those dots. Consequently, hidden royal identity can be disclosed without a
  move. With the active King on `e7` and enemy royal silhouettes on `e6/e5`,
  the `e6` dot is present when `e6` is the real King, but absent when `e6` is
  the Jester and the real King is on `e5`: the latter capture would land the
  active King adjacent to the real King. Information-set play must refine the
  mover's belief by the complete displayed dot frontier before choosing a
  move; it must not require one action across hypotheses whose dot frontiers
  the player can distinguish.
- The native insufficient-material table is implemented, including its special
  minor/color-bound/support combinations. With both real kings present and
  neither team sufficient, the result is a draw.
- Three occurrences of the same legal position are a draw. The history
  protocol retains the chronological set of concrete keys compatible with
  every public observation; concrete search and exact singleton handoffs use
  that history and also detect repetitions created below the root. Position
  identity ignores a non-Pawn `moved` bit unless it preserves a live
  King/Jester-and-Rook castling right; Pawn double-step and effective castling
  rights remain distinct. Decisive, history-free tablebase values are bypassed
  after a second occurrence makes DTW history-dependent. A public-information
  root repetition is accepted only when every surviving concrete successor has
  occurred twice already.

## Conformance status

Conformance is an active release gate, not a completeness claim. The current
engine has a native-code ledger, focused C++ regressions, deterministic Android
Local fixtures, and coverage-guided Local self-play, but the interaction matrix
is not yet exhausted. [conformance-matrix.md](conformance-matrix.md) records
which mechanics have all three kinds of evidence and which still require a
targeted native fixture.

The latest gap pass added the native Berserker Chebyshev leap/growth behavior,
ordinary blind captures of invisible enemy Ghosts, Mage-triggered
pawn/checker promotion, Fisherman relocation of Angel-protected hosts, dynamic
Berserker material, the 24-cell deployment limit with correct Giant/CopyCat
footprints, Onyx's color-relative Giant/Halo rescue anchor, and Sniper targeting
of the actual Giant footprint cell rather than its off-file stored anchor.
Both Giant/Sniper/Onyx-Angel rescue and nested ordered Angel rescues now have
deterministic shipping-app Local fixtures in addition to C++ regressions.

Hidden information follows the shipping simulator rather than a chess-style
determinization: the position retains an invisible Ghost's coordinate, while
move generation applies the native apparent-empty capture, ray pass-through, pawn
collision, royal reveal, and Sniper/Fisherman exceptions. Play mode additionally
conceals enemy Ghost rendering, inspector data, capture-ring styling, invisible
Ghost move coordinates, and enemy Jester identity. Analysis always draws every
Ghost, using the top-right player-side selector—not the side to move—to make a
Ghost translucent when that player would not see it. Analysis reconstructs the
selected player's belief independently of the side to move; a private legal-dot
observation enters that belief only when the selected player is the mover. The
fixed King and every Jester in the first simultaneously revealed Ranked pick group remain
indistinguishable royal silhouettes. Jesters first appearing in later groups
are public Jesters. Draft, analysis, and play retain that exact first-group
candidate set instead of reducing it to a known/unknown boolean.

The lossless `visible` flag is relative to each Ghost's opponent, not the local
UI. A player's own deployed Ghost is visible on that player's screen but begins
invisible in the engine state because enemy rays must pass through it. The
separate `parasiteTracked` bit records the permanent, public floating-Parasite
exception. This distinction is essential when an enemy Sniper has the real
King as its first visible target.

Engine play represents public state as an information set of those concrete
positions. At the start of a decision it must first partition that set by the
active player's private legal-dot observation, then intersects action notation
only within the observed partition. The native `belief clear` / `belief add
<upn>` / `belief go` protocol audits every resulting common root at depth two
across the complete set, preserving immediate hidden-Ghost recaptures even
outside the deep sample, and then deep-searches a robust shortlist across up to
eight beliefs in parallel. The sample begins evenly spaced, and each root's
worst shallow belief replaces one representative when necessary. Root utility
is maximin, with complete-set shallow safety and mean score as deterministic
tie-breaks. The phone controller
only supplies public observations and verifies the returned root; it contains
no piece-value, Ghost-adjacency, plurality, or repetition move override.

The `history start` / `history move` / `history go` protocol reconstructs that
information set from one authoritative initial UPN and an action journal. The
initial concrete Ghost cells and pre-reveal King/Jester assignment are treated
as private data. An arbitrary imported snapshot expands the Ghost over every
publicly possible board cell; a root explicitly known to be freshly deployed
uses only its home zone. The engine then applies the mover's initial legal-dot
frontier and uses each concrete journal action only to identify the public
transition and next private-dot observation that actually occurred. UI play,
UI history analysis, and controller draft-leaf evaluation share this
reconstruction path.

A public continuing-turn event removes royal hypotheses in which the captured
silhouette was the real King. A quiet move begun by a visible Ghost applies its
exact animated source and destination, leaves a singleton location belief even
after the model fades, and only its following move begun while hidden expands
over legal endpoints. A public attack applies its exact revealed source and
destination, and `MakeVis` uses only the newly visible destination while
rehydrating every adjacent legal hidden origin. A Parasite-tracked Ghost never
enters the hidden branch. Private release-log coordinates participate only when
the same coordinates were publicly animated; otherwise they remain excluded.

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

The recovered 5.731 source ledger covers all 28 `SimulatedPiece` subclasses
and all 118 piece-specific override methods. Its normalized override map has
SHA-256
`0b5b0d7278018604849d79f31c89366ae2ec0e2ded27c1f1d88cc8e174539b6e`;
`tools/audit_ultimate_native_interactions.py` checks that fingerprint and 51
semantic method anchors against the external IL2CPP dump. The checked-in
`tests/ultimate_native_interaction_audit.json` maps every native simulator and
all 30 engine piece names into 19 semantic families, each tied to reference
tests and exact Local contracts. This closes the source-inventory gap without
claiming that a finite interaction corpus exhausts every reachable position.
