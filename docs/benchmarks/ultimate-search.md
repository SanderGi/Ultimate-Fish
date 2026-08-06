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
backtracks the remaining mutable group. Offline
tests pack four Giants plus two Princes without overlap and assert all four
corrected centers. A successful live four-Giant lock remains an explicit
validation gate rather than a claimed result.

The same checkpoint won a fresh Very Hard CPU game after searching 64 public
beliefs and finding a forced mate, then won a fresh Unranked game against a
100-point Giant/Rook/Mage army. Both used iterative depth up to 20 with a
10-second move cap and the evolved Queen/Giant/Dragon saved setup.
