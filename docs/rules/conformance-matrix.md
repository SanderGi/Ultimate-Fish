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
| King/Jester steps, hidden-royal check, knockout | yes | yes | fixtures/self-play | Paired fixtures distinguish protected Jester and real-King capture dots; separate fixtures cover direct and forced-collision knockout. |
| Native castling scan and moved flags | yes | yes | fixtures | Includes enemy-Rook eligibility, ownership/moved-state interactions, and the native lack of a through-check test. |
| Pawn double step, promotion, en passant | yes | yes | fixtures | Allied/enemy hidden-Ghost pass-through and hidden-Ghost en-passant destination are deterministic fixtures. |
| Queen/Rook/Bishop/Knight/Ninja/Turtle/Dragon movement | yes | yes | fixture/self-play | A deterministic Rook fixture confirms that an ordinary move may enter and capture an apparently empty enemy Ghost cell while the Rook survives. |
| Berserker leap radius, growth, dynamic material | yes | yes | fixture/self-play | A deterministic power-nine fixture covers growth, file wrapping, and the nine-rank leap to rank ten. |
| Bomb direct, collateral, chained, ranged and Angel-save effects | yes | yes | fixtures/self-play | A clustered chain fixture also drives the native insufficient-material draw; duplicate callbacks are compared as normalized effects. |
| Checker jumps, chains, promotion and hidden Ghosts | yes | yes | fixture/self-play | Deterministic forced multi-jump and far-rank promotion are in the gate. |
| Prince two-part action and hidden Ghost capture | yes | yes | self-play | Mid-continuation replay import remains fail-closed rather than reconstructed from one byte. |
| Giant footprint, ordinary and forced collision | yes | yes | fixtures/self-play | Fixtures distinguish an enemy hidden Ghost crushed by the footprint from an allied hidden Ghost that blocks translation. |
| CopyCat conditional paired movement, capture, linked death, Ghost and Angel interactions | yes | yes | fixtures/self-play | A partner-only Angel fixture confirms linked death kills both halves without creating a singleton; exact UPN links preserve asymmetric live pairs. |
| Mage swaps, Giant translation and forced promotion | yes | yes | fixture/self-play | Long Mage swaps and Giant/Angel relocation use verified native drag gestures. |
| Fisherman rays, hidden Ghosts and Giant pull | yes | yes | fixtures/self-play | Local confirms direct blind collision and the forced collision produced by pulling a King into its own hidden Ghost. |
| Penguin move-triggered stacked freeze and hidden-Ghost step | yes | yes | fixtures/self-play | Deterministic fixtures cover no deployment aura, two-layer stacking/thaw, and mutual knockout on an apparently empty Ghost cell. |
| Ghost visibility, royal reveal and blind interactions | yes | yes | fixtures/self-play | Ordinary blind capture, piece-specific mutual collisions, and forced Fisherman collision are deterministic Local fixtures; exact enemy coordinates remain outside the belief state. |
| Devil spawn, cooldown and hidden-Ghost endpoint | yes | yes | fixture/self-play | Blind spawn kills the Ghost without creating a Minion. |
| Sludge/Goop trail, blind collision and retaliation | yes | yes | fixtures/self-play | Both destination and intervening hidden-Ghost cases are deterministic fixtures. |
| Parasite possession, CopyCat ownership and Angel transfer | yes | yes | fixtures/self-play | Deterministic fixtures cover moved-state preservation, Rook ownership hooks, and possession of a Bomb without detonation; pairwise self-play broadens target types. |
| Angel/Halo linking, nested protection and forced relocation | yes | yes | fixtures/self-play | Deterministic fixtures cover nested Angel-on-Angel reparenting, ordered rescue layers, and the color-relative Onyx Giant anchor. |
| Minion automatic movement, train, freeze and far edge | yes | yes | fixture/self-play | Unity emits an early turn end before some automatic animations; automation requires the engine-predicted Minion callback and final turn barrier. |
| Insufficient material and terminal result | yes | yes | fixture | A two-stage Bomb chain leaves King+Knight versus bare King and confirms the native `DRAW / CHECKMATE NOT POSSIBLE` result. |
| Draft windows, costs, placement locking and public reveals | yes | yes | Ranked observations | Ranked remains paused while this gameplay matrix is expanded. |

## Automated Local campaign

`tools/conform_ultimate_local.py` supports deterministic fixtures and
coverage-guided self-play. Its seven 100-point profiles cover every deployable
piece pair as opponents and retain same-roster allied interactions. Coverage
is retained across games in one invocation so later seeds seek interactions
that earlier games did not reach.

The certified 2026-08-13 pass against the shipping Android 5.731 app comprises
36 deterministic Local fixtures, 284 exact observations, and 94 unique rule
contracts. It includes 233 accepted actions, 11 rejected actions, 15 native
effect assertions, 21 public board-state assertions, and four terminal
assertions. Every contract is attached through `proves` to exactly one action,
rejection, callback, state assertion, or terminal result; a fixture-level label
without an exact proof step is rejected by the manifest loader.

`tests/ultimate_local_conformance.validation.json` seals that complete native
run to a canonical SHA-256 digest of the fixture semantics. Changing an army,
step, expected effect, contract, schema, or app version invalidates the unit
test until the whole Local fixture corpus is replayed successfully and a new
certificate is recorded. A partial or single-fixture phone run is never enough
to refresh the certificate.

The seven coverage-guided profiles remain a complementary interaction fuzzer,
not a substitute for the deterministic gate. These counts are evidence of
exercised paths, not a claim that the combinatorial interaction space is
exhausted.

Before touching the phone, dry-run mode replays every fixture through the
engine and checks legality, rejection, terminal status, automatic relocation,
and net public deaths. During Local Play, every effect callback is scoped to
the exact preceding transition and board generation; delayed records from a
previous game cannot satisfy a new fixture. An accepted action proves
move-generation agreement for that path. Native death, visibility, spawn and
automatic-move callbacks are also journaled to compare effects, but their raw
multiplicity is not meaningful: the shipping app routinely emits the same
death callback several times and sometimes emits an early `ChangeTurn End`
before a compulsory Minion animation. The runner detects an engine-predicted
Minion state transition and waits for the second, settled barrier before
sending the next action. Unlabeled game-over callbacks are classified from the
public native result overlay rather than assumed to match the fixture.

Engine-only invariants such as undo identity, UPN round trips, incremental
bitboards, belief equivalence, search, and tablebase probing cannot be observed
directly through the app UI. Their C++ regressions are therefore anchored to
the same certified rule transitions but remain a separate internal layer; the
native certificate covers every app-observable contract declared by the Local
manifest, not every possible interaction of every material combination.

## Known remaining import boundary

Initial `OnStartGame`/`Replay.initState` records can be sanitized losslessly for
ordinary moved state, Pawn/Checker promotion, Berserker power, Penguin freeze
directions and Ghost visibility. A single polymorphic action byte is not enough
by itself to reconstruct an arbitrary mid-continuation Prince, linked
Angel/Halo graph, or displaced CopyCat pair without the companion records and
move history. Those non-initial imports remain fail-closed; manually supplied
UPN already represents the complete engine state.
