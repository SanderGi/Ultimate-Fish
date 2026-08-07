# Ultimate Fish search benchmark

Fixed position: orthodox initial material on the native 8x10 board represented
in UPN, Apple Silicon development machine, release build.

| Search revision | Depth | Nodes | Time | Best move |
| --- | ---: | ---: | ---: | --- |
| Initial alpha-beta + TT | 6 | 378,558 | 971 ms | `a2-a4` |
| PVS + LMR + killers/history | 6 | 22,471 | 87 ms | `e2-e4` |
| Turn-aware PVS + aspiration windows | 6 | 22,470 | 83 ms | `e2-e4` |
| Native-cost evaluation + tropism | 6 | 47,323 | 185 ms | `d2-d4` |
| Four-way TT + mate normalization | 6 | 47,288 | 193 ms | `d2-d4` |
| Current native rules + search-child legality | 6 | 17,331 | 22 ms | `a2-a4` |

The search optimizations reduced the original fixed-depth node count by more
than 94% before evaluation tuning. The native-cost rows deliberately change the
tree: material follows the recovered `Character.value` table, with native
king-distance and state terms layered on top. The current row (2026-08-06)
retains complete Ultimate King-safety and special-interaction legality while
validating each ordered pseudo-legal action on the same child that search will
visit. These are deterministic development measurements, not an Elo claim.

## Recovered Unranked loss fixture

The exact 99-vs-100 opening from the two failed Unranked games is now a fixed
performance and horizon regression. The early depth-four rows below searched
the same historical 13,414-node tree. Subsequent native-rule corrections change
the current frontier to 13,724 nodes, so only rows with identical node counts
are direct speed comparisons.

| Rule/search implementation | Depth | Nodes | Time |
| --- | ---: | ---: | ---: |
| Revalidate every already-legal search move; simulate every opponent action for check | 4 | 13,414 | 29.010 s |
| Trusted application of the generated legal frontier | 4 | 13,414 | 21.5 s |
| Generate only native attack actions plus indirect-special exceptions | 4 | 13,414 | 3.831 s |
| Preserve Mage/Fisherman/CopyCat exceptions while filtering unrelated replies | 4 | 13,414 | 1.892 s |
| Simulate only King/Bomb-chain-relevant threats | 4 | 13,414 | 1.275 s |
| Single legal frontier for terminal detection and search | 4 | 13,414 | 0.616 s median |
| Incremental bitboards with debug rebuild oracle | 4 | 13,414 | 0.547 s median |

The final optimization pass used the now-corrected 13,724-node frontier and the
259,421-node depth-seven tree throughout:

| Current-tree implementation | Depth | Nodes | Time |
| --- | ---: | ---: | ---: |
| Disposable search children plus bitboard terminal scans | 7 | 259,422 | 1.633 s |
| Cached capture flags and fixed-capacity rule scratch state | 7 | 259,422 | 1.409 s |
| One computed ordering score per move | 7 | 259,422 | 1.210 s |
| Ordered pseudo-legal actions, validated on the searched child | 7 | 259,421 | 0.505 s |

A cold ten-second release search now completes **depth 10**, visits about
6.5 million nodes, and selects `f2!f8` with the PV
`f2!f8 f3@h2 h1-g2 a8-b7 g2-h3 a10xa2 f1-f3 g9-d6 f2-a7 a10xa7`.
The previous corrected native build required 8.019 seconds merely to complete
depth seven. This is a position-specific performance result, not an Elo
estimate.

The mixed Ultimate fixture currently has stable perft counts of 44, 1,936,
and 74,045 at depths one through three. These counts include the native rule
that a deployed Penguin does not freeze anything until it moves. In the current
build, depth five searched 17,662 nodes in 10 ms and selected `g2xg9` (Sniper
shot). Forced Checker chains
and the Prince's second action complete without consuming another nominal turn
of search depth.

## Optimization experiments

The optimization suggestions were measured rather than accepted wholesale:

- An incremental selection-style move picker was rejected twice. The first
  version changed equal-score order, grew the fixed tree by 21%, and was
  slower. A later stable picker preserved the `-0.44` score and identical PV;
  three alternating deterministic depth-7 runs measured a 440 ms median versus
  434 ms for full sorting. Ultimate's searched tails are large enough that its
  repeated scans and stable rotations still lose here. Full stable ordering
  remains in negamax, with every ordering score computed once rather than in
  the comparator.
- Lambda captures do not allocate by themselves. The measurable costs were
  repeated capture classification, vector element movement, and transient
  hash/vector storage in Penguin, Minion, and blast resolution. Capture flags
  and fixed-capacity scratch arrays removed those costs without changing rules.
- Search now consumes pseudo-legal actions and validates legality after applying
  each action to its disposable child. The public `legal_moves()` API remains
  the fully filtered reference path used by perft, the UI, and conformance tests.
- Null-move pruning was rejected. Cooldowns, forced Checker/Prince actions,
  automatic Minion movement, Jester check suspension, and genuine Ultimate
  zugzwangs make an orthodox null action unsound without a much stronger set of
  variant-specific preconditions.
- Adaptive late-move reductions reduce the fixed depth-nine tree from 2,289,525
  to 1,511,309 nodes (34%) with the same `+0.16` score and PV. Against the
  compile-time conservative one-ply-LMR baseline, the adaptive build scored
  5.5/10 at both 20,000 and 50,000 nodes per move across five color-balanced
  fixtures. These samples are gates, not statistically significant Elo claims.
- A higher-budget relocation match exposed an unbounded reversible
  Fisherman/check quiescence cycle. AddressSanitizer identified a stack overflow;
  search now has a hard ply ceiling and the exact persistent-history sequence is
  a regression test.
- A sampled 30-second search showed that native royal-threat validation, not
  move-vector allocation, was the largest avoidable legality cost. Guaranteed
  attacks on an unprotected real King now return before copying and replaying
  the full child. Angel-protected Kings and indirect Bomb/Giant attacks retain
  the complete simulator path. Across nine alternating identical-tree depth-7
  runs, median time fell from 488 ms to 469 ms (3.9%); a ten-second run visited
  6,228,864 nodes while preserving the same score and PV. A reusable per-ply
  vector experiment measured 500 ms versus 499 ms and was rejected as noise.
- Royal-threat generation formerly copied the entire position before it knew
  whether any opponent action required simulation. Per-piece action generation
  is side-independent, so replies now come directly from the immutable source
  and a child is copied only for the rare Angel or indirect-knockout candidate.
  Across sixteen alternating identical-tree depth-7 runs, median time fell
  from 472 ms to 447 ms (5.4%). The ten-second fixture rose to 6,530,816 nodes
  with the same best move and principal variation.
- Incremental selection-style move picking was measured against the retained
  stable full ordering on the recovered Unranked loss fixture. Five depth-7
  runs increased the median from 457 ms to 476 ms and expanded the tree from
  271,325 to 290,422 nodes with the same score and PV. The altered equal-score
  tie order cost more alpha-beta work than it saved in sorting, so the candidate
  was rejected. Capture flags, pseudo-legal child validation, and progressively
  stronger LMR were already present; orthodox null-move pruning remains unsafe
  without a proven variant-specific pass transition because cooldowns, forced
  continuations, compulsory captures, Jester check rules, and timeout wins all
  invalidate the usual null-move assumptions.
- Native royal-threat validation now performs a conservative direct-attack
  geometry test before generating complete action lists for ordinary pieces.
  Bomb, Giant, Checker, Mage, Fisherman, and CopyCat retain the full special
  simulator. On the recovered Unranked fixture this preserved the exact
  271,325-node depth-7 tree, `-0.44` score, and PV while reducing the median of
  seven fresh-process runs from 426 ms to 348 ms (18.3%). At depth nine it
  preserved the exact 1,511,309-node `+0.16` result while reducing the median
  from 2.155 s to 1.770 s (17.9%). Focused regressions exercise every ordinary
  attack geometry, and the complete native rule suite remains green.
- Three further Fairy-Stockfish ideas were ablated and rejected in their
  initial and retuned forms. A per-position royal-threat cache measured 429 ms
  versus 426 ms. Quiescence TT storage changed the depth-7 result and expanded
  its tree. Adaptive deep aspiration changed the depth-9 result and expanded
  1.51 million nodes to 2.74 million because Ultimate reductions and
  same-side action bounds do not satisfy the ordinary repeated-probe
  assumptions. Countermove ordering was retuned below killers/history, but
  still expanded the depth-9 tree to 1.90 million nodes.
- The ordinary-piece royal prefilter now uses precomputed 80-square bitboard
  masks for King, Pawn, Knight, sliders, Ninja, Turtle, Sniper, and Dragon,
  while stateful Berserkers and indirect special effects retain native logic.
  It also rejects frozen and cooldown actors before action generation. The
  exact 1,511,309-node depth-9 result and PV were unchanged; seven
  fresh-process runs reduced median time from 1.770 s to 1.672 s (5.5%).
- Stockfish-style capture history was tested both globally and only from depth
  four. Both versions preserved the result but made the depth-9 fixture
  slightly slower, so neither was retained. A YBWC-like timed root split was
  also rejected: at ten seconds one and two threads both completed depth 10,
  while four workers expanded substantially more nodes without completing an
  extra iteration. Independent worker tables destroyed enough shared ordering
  information that raw throughput did not become usable depth.
- Reverse-futility pruning was tested with native check/continuation guards.
  A conservative 500 cp-per-turn margin expanded the depth-9 tree from 1.51M
  to 1.81M nodes through less useful fail-soft bounds. A 250 cp margin reduced
  it to 0.95M but lost its fixed-node match 6.5/14. Restricting the heuristic
  to depth two still changed the recovered fixture from `+0.16` to `+1.27`
  for only a 6% node reduction. All forms were rejected: Ultimate's explosive
  captures and stateful actions make static material margins too unreliable.
- Ultimate-aware static exchange evaluation (SEE) now orders and prunes only
  provably ordinary captures. It declines the entire position when any live
  piece has blast, attachment, hidden-information, forced-state, generated,
  or other non-orthodox semantics, and also declines promotion and en-passant
  edges. On the classic 8x10 fixture, SEE ordering alone reduced the depth-7
  tree from 35,074 to 34,916 nodes. Pruning negative SEE captures only in
  non-forced quiescence reduced the depth-9 tree from 426,737 to 392,240 nodes
  (8.1%) with the same `+0.20` score, move, and PV; five fresh-process medians
  improved from 406 ms to 388 ms. The pruning candidate scored 7.0/14 in a
  20,000-node color-balanced match and 7.5/14 at 20 ms per move against the
  ordering-only build. Bomb and Angel regressions verify that special semantics
  fail closed and retain the complete search.
- ProbCut was tested behind the same ordinary-SEE and native check/continuation
  guards. A conservative depth-5, 180 cp-margin version preserved the classic
  depth-10 result but reduced 2,561,912 nodes by only 0.09% and had no median
  wall-clock gain. Expanding it to depth four with a 120 cp margin increased
  the tree to 2,996,819 nodes (17.0%), slowed the search, and changed the best
  move. Neither version was retained; forcing-action probes are too sparse
  under safe Ultimate guards to repay their verification searches.
- TT singular extensions are retained only when every live character and state
  is supported by the ordinary exchange model and the TT action itself is a
  normal, non-promotion move. The exclusion search cannot probe or store the
  excluded node, and an extension adds a nominal ply only when the action
  hands over the turn; Checker and Prince same-side continuations already keep
  full turn depth. This leaves the recovered special-interaction depth-9 tree
  exactly unchanged at 1,511,309 nodes, `+0.16`, and the same PV. On the
  classic fixture it reduced depth ten from 2,561,912 nodes / 2.49 s to
  1,665,145 nodes / 1.59 s (35% and 36%) while completing selectively deeper
  forcing lines. The guarded build scored 21.0/42 in the 20 ms color-balanced
  wall-clock gate.
- Forward futility and late-move pruning are likewise limited to ordinary,
  non-PV, non-check, non-continuation nodes. Forward futility alone saved 1.2%
  of the classic depth-10 tree with the same result. The stronger depth-1/2
  late-quiet cutoff, combined with futility, reduced 1,665,145 nodes to
  1,195,914 (28.2%) and about 1.80 s to 1.54 s. The recovered Ultimate fixture
  remained exactly unchanged at 1,511,309 nodes and the same score/PV. The
  combined candidate scored 7.0/14 at 20,000 nodes and 21.0/42 at 20 ms per
  move against the unpruned build.
- A 65,536-entry continuation history keyed by the previous character type,
  action kind, destination, same-side-continuation bit, and current action was
  tested and rejected. It expanded the classic depth-10 tree from 1,195,914 to
  1,346,313 nodes (12.6%) and the recovered Ultimate tree from 1,511,309 to
  1,633,178 (8.1%), changing both horizon lines. At current Ultimate depths,
  splitting the proven piece/destination history into this sparse context
  loses more ordering signal than it adds.
- A 16,384-entry correction history keyed by the complete Ultimate position
  hash and side was also rejected. It expanded the classic depth-10 tree from
  1,195,914 to 1,471,957 nodes and slowed it from 1.54 s to 2.45 s, while the
  recovered fixture's small 1.4% node reduction did not offset hash/update
  overhead (1.73 s to 1.97 s). The learned residuals destabilized shallow
  pruning bounds more than they corrected the handcrafted evaluator.
- Incremental Zobrist hashing was prototyped with cached contributions for all
  character fields plus side, continuation, forced actor, en-passant state,
  timeout winner, and attachment order. A full 96-slot refresh slowed the
  current classic fixture from 1.54 s to about 1.83 s and the recovered
  Ultimate fixture from 1.73 s to about 2.63 s because it ran after every
  pseudo-child. A parent-delta version deferred work to legal searched edges,
  but the independent recomputation oracle caught stale state when callers
  retained the public mutable `PieceState&` analysis handle. Both versions
  were rejected: incremental hashing must follow a compact-state refactor that
  routes every mutation through tracked setters, otherwise TT correctness
  cannot be guaranteed.
- Packing the four `PieceState` booleans into bitfields reduced each record
  from 16 to 12 bytes and shrank both the 2,656-byte `Position` and reference
  `Undo` by 384 bytes. It did not improve the identical-tree classic median
  (1.529 s versus 1.528 s) and slowed the recovered fixture median from
  1.722 s to 1.753 s through bitfield access overhead. The layout was reverted;
  a useful compact make/unmake design must eliminate full copies rather than
  only making them 14% smaller.
- Shared-TT Lazy SMP was tested with race-free atomic four-way clusters and
  helper-specific equal-score ordering. The atomic TT preserved single-thread
  throughput and results. At ten seconds, 1/2/4 workers searched about
  8.3M/16.4M/29.6M aggregate nodes, but every configuration completed only
  depth 10 with the identical score and PV. Staggering helpers over odd depths
  reduced duplication but still did not finish depth 11. This version was
  rejected: unlike the earlier independent-table split, shared bounds scale
  raw work, but the remaining same-tree duplication yields no usable depth.
- Reference make/unmake now stores only the 96 character records and scalar
  turn state; board ownership and type/occupancy bitboards are derived and are
  rebuilt on restore. This reduces `Undo` from 2,656 to 1,572 bytes (40.8%)
  while the full native rule and undo oracle remains green. Hot search retains
  disposable child positions: its prior benchmark showed that even compact
  restoration work cannot beat one optimized 2,656-byte child copy on these
  broad trees, while the smaller transactional state benefits analysis and
  conformance callers that require actual undo.
- Clock-aware time management accepts UCI `wtime`, `btime`, increments,
  `movestogo`, and configurable `Move Overhead`. It derives separate soft and
  hard budgets, predicts the next iteration from the last iteration cost, and
  scales the soft stop by PV stability and score volatility. No clock still
  means unlimited search, and explicit `movetime` remains an exact hard cap.
  On the recovered fixture, 10 seconds remaining used 280 ms for depth 6;
  60 seconds plus a 2-second increment used 5.025 s for depth 10; explicit
  `movetime 1000` stopped at 1,000 ms after depth 8.
- The combined retained search changes scored 21.0/42 against the pre-pass
  `0b52b815` checkpoint at both 20,000 nodes and 20 ms per move across the
  seven color-balanced fixture families. All 206 Python controller/vision
  tests, the native C++ rule/undo suite, and the fixed perft/search benchmark
  passed in the final audit.

## Deadline-aware draft search

Ranked Ban and Pick now share the same engine-backed adversarial macro search.
Ban uses a wider policy-ordered frontier with short per-leaf searches; Pick
uses deeper leaves. Both are anytime searches with independent deadlines, and
an incompletely searched opponent-reply frontier cannot displace a fully
searched action. `tools/benchmark_ultimate_draft_search.py` reproduces the
live limits. On the 2026-08-06 Apple Silicon development machine it measured:

| Window | Choice | Engine leaves | Elapsed |
| --- | --- | ---: | ---: |
| Opening Ban | Penguin | 9 | 1.509 s |
| Opening 40-point Pick | Queen, Queen, Checker x3 | 15 | 1.525 s |
| Ban after both opening rosters reveal | Jester | 15 | 1.558 s |

Ban deliberately stops at its 1.5-second analysis slice and reserves the rest
of the native clock for pot selection and log-confirmed commit. Pick finished
its complete configured frontier well inside the six-second search slice.

## Exact Ultimate tablebases

`src/ultimate/tablebase.cpp` performs complete retrograde analysis over closed
three-character state classes and checkpoints its frontier to disk. It then
reconstructs every legal successor and Bellman-verifies WDL and distance to
win before writing a packed database. Four useful moved-state classes are
bundled: Queen, Rook, Ninja, and Dragon versus a bare King. Together they cover
3,943,680 exact states and 52,230,152 in-class edges. Generation plus complete
verification took between 8.1 and 10.2 seconds per decisive class on the
development machine.

Search probes exact child states before quiescence, maps either attacker color
into the canonical database, returns draws as zero, and maps WDL/DTW to
mate-distance scores. In the bundled `a1` King, `b1` attacker versus `h10`
King fixture, heuristic depth-two Rook search reported `+4.89`; exact probing
reports mate in 12 at depth one. Replacing the Rook with a Queen reports mate
in 9, demonstrating that DTW selects the shorter conversion. Unmoved pieces,
special state, and unrepresented material decline the probe rather than
silently using an inexact result.
Normal middlegames reject the three-character probe from the incremental
occupancy bitboard before scanning piece records, keeping exact-table lookup
off the hot path until only three occupied cells remain.

## Ultimate-specific NNUE experiment

The surrounding Fairy-Stockfish NNUE cannot represent Ultimate directly: it
assumes 64 squares, orthodox/Fairy piece planes, and Fairy's `Position` state.
The experimental Ultimate evaluator instead uses 7,164 sparse features for
oriented 8x10 locations, all 30 native/generated types, moved/visibility,
cooldown, freeze, Berserker power, action bits, links/hosts, forced actors,
continuations, en passant, and side to move. A 32-neuron accumulator learns a
residual over the native handcrafted score and serializes to a strictly
versioned 458,784-byte `.ufnn` file.

The first expanded corpus contains 12,977 positions from 200 exploratory games
at 50,000 teacher nodes per position across seven fixture families. Complete
games, rather than adjacent positions, form a 20% validation split. Its best
checkpoint reduced held-out residual MAE from 302.54 to 299.28 cp and quantized
within 1 cp of the float model. A fresh C++ process matched Python integer
inference on 100/100 records, and incremental search matched the full-refresh
reference exactly at depth five on all seven fixture families.

This prediction improvement did **not** establish playing strength. The final
candidate drew all 14 color-balanced games at 30,000 nodes per move. At 100 ms
per move it scored 7.0/14, with one win and one loss in the same color pair and
all other games drawn. A 50/50 outcome/search target also drew 14/14. Even with
incremental accumulators, a one-million-node fixture took a 1.303 s median
versus 1.120 s handcrafted (16.3% overhead). Consequently NNUE remains opt-in;
these tools and correctness gates are retained, but the neutral pilot is not
presented as an Elo gain or enabled by default.

Run `tools/benchmark_ultimate.sh` for fixed perft/search measurements, or set
`ULTIMATE_LONG_BENCHMARK=1` to include the cold ten-second horizon check. Run
`tools/selfplay_ultimate.py CANDIDATE BASELINE` to alternate colors between two
locally built engine revisions. The runner caps every move by a deterministic
node budget, rejects protocol/state errors instead of hanging, and can select a
comma-separated fixture subset. Each game gets fresh engine processes, and the
harness has a threefold-repetition cutoff, so TT/history carryover and long
cycles cannot bias color pairs. The four-way TT scored a neutral 4.0/8 against
its immediately preceding compatible build at 10,000 nodes per move. The
current native-cost evaluation build scored 3.5/4 against the pre-tuning rules
build at 20,000 nodes per move on the rule-compatible mixed/classic fixtures.
The final five-fixture identical-binary control scored exactly 5.0/10; its new
relocation pair exercises Mage/Giant/Fisherman state transitions.
These small samples are regression signals, not statistically significant Elo
estimates.

## Live Unranked smoke test

After the rule and controller fixes, a two-game Unranked smoke test against a
varied 100-point opposing army finished with one Ultimate Fish win and one
threefold-repetition draw, with no controller desynchronization. The draw was
classified from the exact position history instead of unreliable result-screen
OCR. This is useful end-to-end validation, but the sample is far too small to
claim reliable dominance or an Elo gain.

## 2026-08-06 live and setup validation

The optimized engine won a complete Very Hard CPU game with the deliberately
varied `Jester/Ninja/Queen/Sniper/Prince/Mage/Knight/Turtle` test roster. The
controller installed and verified all nine deployment cells, reconstructed all
nine CPU pieces, executed repeated Prince continuations correctly, and reached
depths 11–15 within the ten-second cap before recognizing the public victory
screen.

The faster engine then continued the adversarial setup league from generations
9 through 12 at 5,000 nodes per move and reranked four finalists at 20,000
nodes. The winning deployment in
[`evolved-army-optimized-2026-08-06.json`](evolved-army-optimized-2026-08-06.json)
scored 20W-13D-3L. It won two further supervised Very Hard CPU games: a forced
mate in three and an immediate King capture. The app's online service was under
maintenance during the first validation, so Unranked and Ranked were not
sampled; the CPU controller correctly fell back to the explicitly detected
**Play Offline** control without invoking Google login. A later online attempt
reached the version endpoint but rejected the restored account with HTTP 401;
Google Play Games then failed twice. The controller now reports that state
immediately instead of repeating navigation for its full online timeout.

## Ranked draft coevolution

A twelve-generation, 16-policy coevolution run exercised every native Ranked
ban and immutable pick window. Each generation played a complete 240-game
color-balanced league at 5,000 nodes per move; six finalists were reranked at
30,000 nodes. This campaign also acted as a rules fuzzer and exposed two
previously unrepresented Angel/Giant call-order cases. Both now have focused
C++ regressions and round-trip clean state transitions.

The final policies converged on Penguin/Prince/Giant cores and drew all ten
high-budget finalist games. The leading policy's deployed Ivory roster against
the baseline is `Penguin, Penguin, Bishop, Giant, Prince, Prince, Pawn, Giant,
Parasite, CopyCat`; it bans `Rook, Ninja, Sniper, Penguin, Fisherman, Prince`
across the six alternating ban turns. A direct 30,000-node color pair against
the existing production draft policy was also 1.0/2.0, so the artifact is
retained as an experimental result rather than promoted on a false Elo claim:
[`evolved-draft-optimized-2026-08-06.json`](evolved-draft-optimized-2026-08-06.json).

A four-generation continuation then used that finalist as the production
baseline and re-evaluated twelve policies at 5,000 nodes per move. Its leading
finalist scored 4W-4D-0L in the 30,000-node final. A separate seven-policy
30,000-node pool against the previous six finalists produced 7W-5D-0L (79.2%);
the prior shipping policy scored 0W-10D-2L (41.7%). The promoted policy uses
adaptive pick, repeat, opponent-denial, and self-preservation weights and is
stored in
[`evolved-draft-continuation-2026-08-06.json`](evolved-draft-continuation-2026-08-06.json).
It intentionally stops at 99 and 89 points in its symmetric deterministic
fixture when those rosters outrank filler under the native 100-point ceiling.

The promoted roster's four-Giant opening exposed that Ranked's pick camera
projects the local deployment zone into a compact 8x3 grid rather than the
settled gameplay geometry. A subsequent live run placed only three of four
Giants and exposed that the cyan aura and bottom pot rows were mistaken for the
playable grid: the leftmost target missed, files were shifted, and y targets sat
above the actual board. A later native-confirmed intersection experiment
established that Giant does average four Square transforms, but only after
ArmyMove raycasts an ordinary anchor cell; direct grid-seam drops were all
rejected in a live match. The controller now uses the measured gray-cell edges
`(60,1344)` through `(1032,1668)`, targets raycastable cell centers, and
backtracks the remaining mutable group. Offline tests pack four Giants plus two
Princes without overlap and assert all four corrected centers. A shipping-app
Local profile then built and visually
verified `a2,c2,e2,g2` for both players, launched the position, and accepted a
Giant move in differential play. The subsequent live Ranked run locked both
Princes and all four Giants with the Local-proven contiguous left-to-right
order, closing the placement validation gate.

The 2026-08-06 CPU startup regression also established that an opponent army
need not spend the full 100-point allowance. A live Very Hard opponent reported
98 native points; the controller retained that exact total, reconciled nine
public deployment cells plus only the arithmetically possible hidden Ghost
hypotheses, and Ultimate Fish won by checkmate at a 10-second move budget.
Material mismatches now trigger a coordinate-confirmed native grid scan rather
than filling the opponent roster toward 100 or treating missed public pieces as
uncertainty.

The pending Ranked placement gate subsequently passed in a fully autonomous
Silver IV match. Ultimate Fish acted first, completed all twelve ban/pick
windows, and locked the evolved 99-point roster without intervention:

```text
king@a1; prince@d1,e1,f1,g1,h1; giant@a2,c2,e2,g2; pawn@c1; checker@b1
```

The difficult opening group placed both Princes and all four Giants exactly;
later transient Prince/Checker collisions were corrected inside the active pick
window rather than abandoning the match. The opponent's public 100-point spawn
journal produced one exact hypothesis. Search reached depths 8–11 at the
10-second cap, converted an approximately +8 score, and executed a forced mate
in three for the first fully autonomous Ranked win at this checkpoint.

The strengthened Local profile exactly matches the difficult opening group:
two Princes at `f1,g1` followed by Giants at `a2,c2,e2,g2`. Both native builders
visually verified all six models. Their adaptive correction independently
measured each Prince center drop one rank high and corrected it with `+52 px`;
the Ranked controller now applies the equivalent 47%-cell bias before its first
timed drag.

The same checkpoint won a fresh Very Hard CPU game after searching 64 public
beliefs and finding a forced mate, then won a fresh Unranked game against a
100-point Giant/Rook/Mage army. Both used iterative depth up to 20 with a
10-second move cap and the evolved Queen/Giant/Dragon saved setup.

A subsequent Onyx Ranked repeat confirmed that the post-Lock local spawn
journal corrected the exact opening group (`Prince@f1,g1` and Giants at
`a2,c2,e2,g2`). The controller then stopped on its obsolete 75-second
opponent-pick timeout even though the native clock still showed 72 seconds.
The opponent later forfeited and the account received +15 ranking, but no
engine move was searched, so this is recorded as a controller/protocol result
rather than additional strength evidence. Remote draft waits now follow the
native commit/terminal events without a shorter local deadline. The same trace
also motivated a five-point in-cell drop sweep so a locked Giant collider
cannot indefinitely intercept a legal later Prince placement.

The next Ivory repeat directly validated that placement recovery: an `e1`
Prince was obstructed at all five first-pass target points, the controller
remained in-phase, replanned, and locked the exact middle group without manual
input. The final opponent group then exposed a ban-observation race rather
than a draft-rule difference. Native logs showed the real earlier ban was
Angel, but its texture line preceded the phase-complete frame and was discarded
by the consuming wait; a visual fallback mislabeled an overlapping pot as
Mage, causing the engine to reject the opponent's later legal Mage. The game
timed out for -5 ranking before search and is excluded from strength evidence.
Ban identity now comes exclusively from the non-consuming native texture
journal. This run also showed the old policy spending its final group on two
Jesters after the King square was already revealed; late Jesters now receive a
prohibitive policy penalty pending the next coevolution run.

The following Onyx validation read the opponent's public bans exactly as
Berserker and Ghost, confirming the journal-order repair. Its opening group
also demonstrated exact local reconciliation when one Giant locked at `c1`
instead of the requested `c2`. That legal footprint made the already-committed
middle CopyCat geometrically impossible alongside the other three Giants; the
controller correctly refused to invent a placement but had no way to revise
the engine group, and the native draft timeout cost +0 rating. This is excluded
from strength evidence. The engine protocol now supports non-mutating draft
previews with candidate exclusions, and the controller validates full
backtracked placement before committing. Replaying the captured draft yields
the original infeasible `Prince Prince Mage CopyCat`, then the feasible
same-cost `Prince Prince Mage Knight` when the weakest conflicting CopyCat is
excluded.

The immediate post-preview validation completed all twelve phases as Ivory
without intervention. Exact bans were Rook, Ghost, Jester, Sniper, Bishop, and
Devil; the final local group was `Prince@c1; Checker@h1`, confirming that no
late Jester was purchased. Several obstructed target pixels were recovered and
the exact 99-point roster locked. Gameplay initialized from two legitimate
first-group royal hypotheses, remained synchronized through Prince
continuations and Queen incursions, and searched roughly 11–82 million nodes
per 10-second request at depths 9–12. Ultimate Fish held the worst belief near
+3 pawns until the opponent ran out of time. The public overlay recorded
**Victory, +21 ranking** (and the 25/25 knockout-character milestone), so the
corrected pipeline's Ranked strength record advances to **2 wins, 0 draws,
0 losses**; controller-only abandoned games remain excluded.
