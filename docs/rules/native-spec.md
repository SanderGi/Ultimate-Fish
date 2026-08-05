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
`OnBanCharacter` is public. Its subsequent native
`TEXURE ASSIGNED TO <piece>` diagnostic identifies the exact newly locked pot,
so the controller applies that public ban without overlapping-pot image
differencing. After pot calibration, a harmless local-pot touch logs the native
`myBoard.turn != team` predicate. The controller binds it to the non-consuming
count of Bans already completed, because a fast remote phase zero can finish
during calibration; together they establish whether the player is Ivory or
Onyx. The Ban bubble remains only a compatibility fallback. Repeated
`OnBanCharacter(Type)` method frames from the same callback are debounced as
one transition before the next phase can advance. Both its unique callback
count and named pot are journaled independently of queue consumers, so a remote
phase-zero Ban completed during pot calibration is replayed into the engine
before the twelve-window loop continues.

Local Ranked placement is likewise acknowledged rather than assumed. Each pot
drag must produce the native `ArmyMove` coordinate and an increased cumulative
`GetPoints` total before the cell is reserved in the engine draft. This matters
for wide colliders such as CopyCat, which can land one file inward and create
its mirror there, or can be despawned when that measured footprint collides.
The committed total must equal the engine's expected roster cost before the
controller presses the icon-only green Lock checkmark.
After selecting a Ban pot, the current Ranked UI uses a fixed blue top-row Ban
button. Pot-relative saturated-red detection is not authoritative because lock
chains on previously drafted or banned pieces have the same geometry. The
controller therefore detects the fixed blue component and waits for the native
named Ban callback before advancing.

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
  angel.
- Cooldowns decrement on every side change. Setting the native base value of
  three therefore produces one skipped owner turn.
- A Giant occupies `(x,y)`, `(x+1,y)`, `(x,y+1)`, and `(x+1,y+1)`. It moves
  exactly two coordinates orthogonally and knocks out every enemy character in
  the destination footprint; any friendly character in that footprint blocks
  the move.
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
- An invisible Ghost makes its square unavailable while ordinary sliding rays
  continue through it, even for a same-team ray. Pawns cannot deliberately capture it
  diagonally; a forward pawn move into it uses the native blind-collision path
  and knocks out both characters.
- A Sniper's forward shot also passes through invisible Ghosts to the first
  visible character. Its one-square sideways action can attack an enemy; only
  a forward shot starts the reload cooldown.
- A Mage can choose any of a Giant's four footprint tiles as its swap target
  when the translated Giant remains in bounds. The Mage lands on the selected
  tile; the Giant is forcibly translated to the Mage and knocks out either
  team's characters in its new footprint.
- An unmoved pawn can double-step from any valid deployment rank when both
  forward squares are available. Promotion occurs on rank 10 for Ivory and
  rank 1 for Onyx. En-passant stores and removes the exact pawn victim.
- The native insufficient-material table is implemented, including its special
  minor/color-bound/support combinations. With both real kings present and
  neither team sufficient, the result is a draw.

## Conformance status

Every rule branch recovered from the 5.731 simulation classes is now represented
in the engine and reference suite. The final gap pass added the native
Berserker Chebyshev leap/growth behavior, ordinary-move exclusion for invisible
Ghost squares, Mage-triggered pawn/checker promotion, Fisherman relocation of
Angel-protected hosts, dynamic Berserker material, and the 24-cell deployment
limit with correct Giant/CopyCat footprints.

Hidden information follows the shipping simulator rather than a chess-style
determinization: the position retains an invisible Ghost's coordinate, while
move generation applies the native blind-square, ray pass-through, pawn
collision, royal reveal, and Sniper/Fisherman exceptions. Play mode additionally
conceals enemy Ghost rendering, inspector data, capture-ring styling, invisible
Ghost move coordinates, and enemy Jester identity. Analysis mode deliberately
shows that state with a dashed translucent treatment.

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
Prince first steps followed by quiet or capturing second steps, Bomb removal of
both combatants before the radius-one blast, adjacent Giant footprint tiling,
Mage stay-put swap diagnostics, cosmetic prefab aliases, concealed/revealed
Ghost transitions, and King/Jester ambiguity. A duplicate-heavy six-Rook army
also demonstrated the shipping counter's bounded dynamic adjustment: its
static visible-plus-Ghost values total 102 while the public counter reads 100.
The phone controller reconciles only the uniquely determined Ghost count within
that observed two-point adjustment and rejects larger mismatches.

There is no known source-level or live differential rules gap in the recovered
5.731 ledger.
