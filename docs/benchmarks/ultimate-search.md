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
| Current full native-legality build | 6 | 47,272 | 1.079 s | `d2-d4` |

The search optimizations reduced the original fixed-depth node count by more
than 94% before evaluation tuning. The native-cost rows deliberately change the
tree: material follows the recovered `Character.value` table, with native
king-distance and state terms layered on top. The current build spends more
time per node than the earlier orthodox-only checkpoints because it performs
the complete Ultimate King-safety and special-interaction legality checks.
These are deterministic development measurements, not an Elo claim.

## Recovered Unranked loss fixture

The exact 99-vs-100 opening from the two failed Unranked games is now a fixed
performance and horizon regression. Every depth-four row below searched the
same 13,414 nodes, returned `f2!f8`, and scored the position `-0.37`; the time
changes therefore measure eliminated duplicate rule work rather than a pruned
or altered tree.

| Rule/search implementation | Depth | Nodes | Time |
| --- | ---: | ---: | ---: |
| Revalidate every already-legal search move; simulate every opponent action for check | 4 | 13,414 | 29.010 s |
| Trusted application of the generated legal frontier | 4 | 13,414 | 21.5 s |
| Generate only native attack actions plus indirect-special exceptions | 4 | 13,414 | 3.831 s |
| Preserve Mage/Fisherman/CopyCat exceptions while filtering unrelated replies | 4 | 13,414 | 1.892 s |
| Simulate only King/Bomb-chain-relevant threats | 4 | 13,414 | 1.275 s |
| Single legal frontier for terminal detection and search | 4 | 13,414 | 0.616 s median |
| Incremental bitboards with debug rebuild oracle | 4 | 13,414 | 0.547 s median |

Before this pass, a ten-second live search could complete only depth three in
the fixture. The corrected release build completes depth seven in 8.019 s
(195,792 nodes) and selects `f2!f8`; the deeper score is `+3.63`, exposing the
large horizon error that drove the earlier losing play. The same benchmark run
completed depth four in 549 ms. This is a position-specific performance result,
not an Elo estimate.

The mixed Ultimate fixture currently has stable perft counts of 44, 1,936,
and 74,084 at depths one through three. These counts include the native rule
that a deployed Penguin does not freeze anything until it moves. In the current
build, depth five searched 17,637 nodes in 585 ms and selected `g2xg9` (Sniper
shot). Forced Checker chains
and the Prince's second action complete without consuming another nominal turn
of search depth.

Run `tools/benchmark_ultimate.sh` for fixed perft/search measurements. Run
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
