# Chess Ultimate 5.731 native specification ledger

This ledger records behavior recovered directly from the Android ARMv7 IL2CPP
simulation classes. It distinguishes source-backed mechanics from items still
requiring differential fixtures against the app.

## Draft

`ArmyBuilder` initializes a 100-point total, a 15-point initial/minimum window,
and a 15-point increment. `GameManager.GetDraftPhase` exposes twelve windows:

| Window | Side | Action | Cumulative ceiling |
| ---: | --- | --- | ---: |
| 0 | Ivory | Ban | — |
| 1 | Onyx | Ban | — |
| 2 | Ivory | Pick | 15 |
| 3 | Onyx | Pick | 15 |
| 4 | Ivory | Ban | — |
| 5 | Onyx | Ban | — |
| 6 | Ivory | Pick | 30 |
| 7 | Onyx | Pick | 40 |
| 8 | Ivory | Ban | — |
| 9 | Onyx | Ban | — |
| 10 | Ivory | Final pick | 100 |
| 11 | Onyx | Final pick | 100 |

The `Character.value` initializer is a 30-element integer array, in exact
`Character.Type` order:

`0, 10, 6, 3, 17, 13, 9, 15, 15, 20, 4, 15, 8, 0, 15, 15, 15, 0, 12, 17, 18, 2, 2, 1, 5, 0, 13, 0, 12, 15`.

This is now the engine/UI draft-cost table. Generated forms retain their
serialized entries but are not selectable.

## Board, deployment, and analysis state

`SimulatedBoard` stores an 8x10 array. Native army save/load traverses an 8x3
home zone for each side. The UI deployment step therefore consumes the finite
drafted roster into ranks 1-3 for Ivory and ranks 8-10 for Onyx. A Giant anchor
must leave its entire 2x2 footprint inside that zone; placing a CopyCat also
requires its mirrored clone square to be free.

Ultimate Position Notation (UPN) records every search-relevant field: side to
move, half/full move counters, en-passant square and exact victim, forced
continuation, visibility, cooldown, action, freeze, power, moved state,
links/hosts, and Angel attachment order. Serialization compacts live piece IDs
and remaps every relationship so captures cannot corrupt a round trip.

## Source-backed interactions implemented

- A bomb's capture or death invokes its radius-one explosion and removes the
  bomb, the other combatant, and every adjacent character.
- Ninja destinations are generated independently of intervening occupancy for
  each configured direction and distance.
- A prince may capture with its first one-square move. An empty first step is
  only generated as the first half of a queued second attack.
- Checker jumps are forced, can queue further jumps, and promote at the far
  rank.
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
- Penguin/`SimulatedFreeze` freezes all eight adjacent non-Penguin characters,
  with counts stacking across Penguins. The action byte encodes the currently
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
- An unmoved pawn can double-step from any valid deployment rank when both
  forward squares are available. Promotion occurs on rank 10 for Ivory and
  rank 1 for Onyx. En-passant stores and removes the exact pawn victim.
- The native insufficient-material table is implemented, including its special
  minor/color-bound/support combinations. With both real kings present and
  neither team sufficient, the result is a draw.

## Open conformance work

Information-set search for hidden Ghosts, Fisherman interactions with attached
Angels, the remaining deployment validation branches, and the remaining
cross-piece interaction matrix still need extracted fixtures before the ruleset
can be called 100% complete.
