# Rule recovery and conformance policy

Chess Ultimate rule fidelity is the first release gate. A mechanic is not
considered implemented merely because it resembles its in-game description.
Each mechanic must have a source-backed specification and executable fixtures
covering legal moves, state transitions, undo, captures, interactions, and edge
cases.

## Evidence order

1. Native Android 5.731 IL2CPP simulation and game-manager logic.
2. Android 5.731 type metadata, field offsets, enums, and serialized assets.
3. Current in-app English descriptions, tutorials, and UI strings.
4. Controlled observations from the running game when native intent is
   ambiguous.
5. Public descriptions only as a cross-check, never as the sole authority.

The simulation classes are especially valuable because the shipping game uses
them for bot move generation and undo. They expose the state model needed by a
search engine: cooldowns, visibility, freezing, forced Checker continuations,
multi-moves, linked pieces, explosions, automatic minion movement, and board
evaluation.

The native board is eight files by ten ranks. Each army is deployed within its
own three-rank home zone before play. Ultimate Fish uses an unsigned 128-bit
bitboard for the 80 playable squares, preserving bit-parallel occupancy and
piece-set operations without splitting the board into slower container types.

## Recovered roster in version 5.731

The serialized `Character.Type` enum contains 30 values in this order:

`king`, `jester`, `knight`, `pawn`, `queen`, `rook`, `bishop`, `berserker`,
`bomb`, `ninja`, `turtle`, `ghost`, `mage`, `goop`, `penguin`, `parasite`,
`devil`, `minion`, `sludge`, `sniper`, `prince`, `checker`, `checkerKing`,
`giant`, `copycat`, `copycatClone`, `angel`, `halo`, `fisherman`, `dragon`.

The primary native search model contains corresponding specialized classes for
Angel/Halo, Berserker, Bomb, Checker, CopyCat/clone, Devil/Undead (minion),
Dragon, Fisherman, Freeze (penguin), Ghost, Giant, Goop, King, Mage, Ninja,
Parasite, Pawn, Prince, Sludge, Sniper, and the standard sliding/leaping pieces.

## Directly verified public-facing mechanics

The current Android translation asset confirms the following high-level rules.
These summaries are not yet substitutes for native conformance cases:

- Win by knocking out the opposing real king. The jester appears as a king to
  the opponent.
- Pawn and checker promotion, pawn double-step/en-passant, and compulsory
  chained checker captures exist.
- Berserker movement radius begins at one and grows after knockouts.
- Bomb movement is up to two orthogonally or one diagonally; attacking or being
  attacked explodes every character in a one-square radius.
- Ninja moves up to three squares in any direction and passes through pieces.
- Ghost moves one square in any direction, is hidden from the opponent, reveals
  on attack or near a king/jester, and becomes hidden after moving again.
- Mage swaps with an ally. Penguin freezes adjacent characters and cannot be
  frozen. Parasite possesses a target after attacking or after a melee attack.
- Devil spawns a minion within radius two, then sleeps for one turn. Minions
  advance automatically at the start of their side's turns and disappear at
  the far edge.
- Sludge moves up to two orthogonally without attacking and leaves retaliating
  goop. Sniper moves one square sideways or shoots forward, then sleeps for one
  turn.
- Prince can make a second one-square move if its first move did not attack.
  Giant occupies a 2x2 footprint and shifts exactly two squares orthogonally. CopyCat and
  its clone mirror one another and share death.
- Angel links to an ally and creates an immobile halo at its origin. A lethal
  hit removes the angel instead and returns the host to the halo; halo death
  also removes the angel.
- Dragon combines diagonal sliding with knight leaps; only the leap jumps over
  characters. Fisherman pulls characters toward itself.
- Ranked draft uses three sequential ban/pick phases and a 100-point team
  budget. The native phase ceilings are 15, then 30 for Ivory/40 for Onyx, then
  100; drafted armies are placed within their three-rank home zones.

## Required conformance layers

- Per-piece movement and attack destinations on empty, allied, and hostile
  occupancy.
- Mandatory/queued actions and start/end-of-turn automatic effects.
- Capture replacement, area effects, linked deaths, promotion, cooldown,
  invisibility, freeze stacking, and exact undo symmetry.
- Multi-square occupancy and mirror/link invariants.
- Real-king loss, jester information hiding, check/checkmate behavior, draws,
  and insufficient-material behavior.
- Draft phase ordering, bans, budgets, team/king validation, and placement.
- Position serialization round trips for every state field needed by analysis.

Every optimized bitboard implementation must be compared against a simple
reference state machine derived from the fixtures. Perft-style hashes and move
counts will be recorded before search-strength work begins.
