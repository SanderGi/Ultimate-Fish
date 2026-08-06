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

A cold ten-second release search now completes **depth 10**, visits 5,902,464
nodes, and selects `f2!f8` with the PV
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

- An incremental selection-style move picker was slower on the fixed Ultimate
  tree and increased the searched tree by 21% after changing equal-score move
  order. Full stable ordering remains in negamax, but every ordering score is
  now computed once instead of repeatedly inside the comparator.
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
