# Chess Ultimate 5.731 conformance matrix

This matrix is the release gate for Ultimate Fish rules work. A rule is not
called fully verified merely because the engine has an implementation.

- **Native** means the behavior was recovered from the Android ARMv7 IL2CPP
  simulation path.
- **Reference** means a deterministic C++ regression covers the resulting
  position, including undo/UPN state where relevant.
- **Local** means the shipping 5.731 app accepted or rejected the corresponding
  action in offline pass-and-play, with native callbacks used as the barrier.

The proprietary decompilation stays outside the repository. This document
records behavior and public test evidence only.

| Area | Native | Reference | Local | Current note |
| --- | :---: | :---: | :---: | --- |
| King/Jester steps, hidden-royal check, knockout | yes | yes | self-play | Ranked observations also confirm Jester check suspension and real-King knockout. |
| Native castling scan and moved flags | yes | yes | pending fixture | Includes enemy Rook eligibility and no through-check test. |
| Pawn double step, promotion, en passant | yes | yes | fixtures | Allied/enemy hidden-Ghost pass-through and hidden-Ghost en-passant destination are deterministic fixtures. |
| Queen/Rook/Bishop/Knight/Ninja/Turtle/Dragon movement | yes | yes | self-play | Pairwise mirror profiles now put every deployable type opposite every other type. |
| Berserker leap radius, growth, dynamic material | yes | yes | partial self-play | Power above level one and ten-rank vertical reach still need a long native capture fixture. |
| Bomb direct, collateral, chained, ranged and Angel-save effects | yes | yes | self-play | Bomb callbacks are duplicated by the app, so state deltas—not raw callback counts—are the oracle. |
| Checker jumps, chains, promotion and hidden Ghosts | yes | yes | self-play | A deterministic multi-jump/promotion Local fixture remains desirable. |
| Prince two-part action and hidden Ghost capture | yes | yes | self-play | Mid-continuation replay import remains fail-closed rather than reconstructed from one byte. |
| Giant footprint, ordinary and forced collision | yes | yes | self-play | Onyx Angel rescue has a distinguishing-cell fixture in the gate. |
| CopyCat independent-half movement, capture, linked death, Ghost and Angel interactions | yes | yes | fixture/self-play | Native gameplay proved file mirroring is construction-only; the app's duplicate death callbacks are journaled but deduplicated semantically. |
| Mage swaps, Giant translation and forced promotion | yes | yes | self-play | Mage, Fisherman and Angel use native drag gestures in Local automation. |
| Fisherman rays, hidden Ghosts and Giant pull | yes | yes | self-play | Both allied/enemy hooks and a blind hidden-Ghost collision have run in Local. |
| Penguin move-triggered stacked freeze | yes | yes | self-play | Deployment creates no aura; deterministic multi-Penguin stacking remains pending Local. |
| Ghost visibility, royal reveal and blind interactions | yes | yes | fixtures/self-play | Exact enemy coordinates remain outside the online belief state. |
| Devil spawn, cooldown and hidden-Ghost endpoint | yes | yes | fixture/self-play | Blind spawn kills the Ghost without creating a Minion. |
| Sludge/Goop trail, blind collision and retaliation | yes | yes | fixtures/self-play | Both destination and intervening hidden-Ghost cases are deterministic fixtures. |
| Parasite possession, CopyCat ownership and Angel transfer | yes | yes | self-play | Pairwise mirrors broaden the native target-type coverage; exact all-type possession remains ongoing. |
| Angel/Halo linking, nested protection and forced relocation | yes | yes | fixtures/self-play | Deterministic fixtures cover nested Angel-on-Angel reparenting, ordered rescue layers, and the color-relative Onyx Giant anchor. |
| Minion automatic movement, train, freeze and far edge | yes | yes | fixture/self-play | Unity emits an early turn end before some automatic animations; automation requires the engine-predicted Minion callback and final turn barrier. |
| Insufficient material and terminal result | yes | yes | pending fixture | The native material table is implemented; Local terminal-label cases need a compact fixture set. |
| Draft windows, costs, placement locking and public reveals | yes | yes | Ranked observations | Ranked remains paused while this gameplay matrix is expanded. |

## Automated Local campaign

`tools/conform_ultimate_local.py` supports deterministic fixtures and
coverage-guided self-play. Its six 100-point profiles are a pairwise set: the
three disjoint rosters play every cross-roster matchup and each roster also
plays its mirror. Consequently every deployable character type can occur as an
opponent of every other deployable type, while same-roster allied interactions
are present too. Coverage is retained across games in one invocation so later
seeds seek interactions that earlier games did not reach.

The current recorded pass comprises eleven deterministic Local fixtures and
162 successfully matched coverage-guided plies across all six profiles. The
mirror half includes a 40-ply callback game, a 20-ply hidden/callback game, and
a 30-ply ranged/support game. These counts are evidence of exercised paths,
not a claim that the combinatorial interaction space is exhausted.

An accepted action proves move-generation agreement for that path. Native
death, visibility, spawn and automatic-move callbacks are also journaled to
compare effects, but their raw multiplicity is not meaningful: the shipping
app routinely emits the same death callback several times and sometimes
emits an early `ChangeTurn End` before a compulsory Minion animation. The
runner detects an engine-predicted Minion state transition and waits for the
second, settled barrier before sending the next action.

## Known remaining import boundary

Initial `OnStartGame`/`Replay.initState` records can be sanitized losslessly for
ordinary moved state, Pawn/Checker promotion, Berserker power, Penguin freeze
directions and Ghost visibility. A single polymorphic action byte is not enough
by itself to reconstruct an arbitrary mid-continuation Prince, linked
Angel/Halo graph, or displaced CopyCat pair without the companion records and
move history. Those non-initial imports remain fail-closed; manually supplied
UPN already represents the complete engine state.
