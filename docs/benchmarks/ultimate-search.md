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

The search optimizations reduced the original fixed-depth node count by more
than 94% before evaluation tuning. The latest row deliberately changes the
tree: material now follows the recovered `Character.value` table, with native
king-distance and state terms layered on top. These are deterministic
development measurements, not an Elo claim.

The mixed Ultimate fixture currently has stable perft counts of 44, 1,276,
and 34,928 at depths one through three. At depth five the tuned engine searched
8,893 nodes in 28 ms and selected `g2xg9` (Sniper shot). Forced Checker chains
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
