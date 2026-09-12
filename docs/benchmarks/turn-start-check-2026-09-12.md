# Native turn-start check correction — 2026-09-12

## Result

The recovered 5.731 game tests current-turn check after a hypothetical
`ChangeTurn`. This advances opposing Minions and decrements cooldowns. The
engine previously used its post-action threat predicate without this phase,
misclassifying Minion mates (and ready-next-turn Sniper mates) as stalemate.

`Position::in_check()` now separates current-turn classification from
post-action legality, which must not advance Minions twice. Terminal results,
concrete/factored/enumerated search, quiescence evasions, and `+`/`#` notation
use the corrected timing. See the source-backed entry in
[`native-spec.md`](../rules/native-spec.md).

Published Devil/Minion and Sniper results have stale terminal seeds. Probing
those material classes now falls back to search, including Minion-free Devil
roots, surviving Minions after Devil capture, and currently ready Snipers.
UFTB compatibility also rejects Devil/Sniper in either material slot. No
tablebase payload, certificate, plot data, or cloud resource was changed or
recomputed. An invalid O(1) tablebase answer is deliberately no longer used;
the timing comparison below measures the search engine with tablebases off
on both sides, not the speed of that incorrect lookup.

## Performance

Baseline: clean revision `a8393fd60130ca80db6230fc96ff9c3ac3012027`, compiled
from a separate source snapshot before editing. Both binaries use Apple clang
17.0.0 (`clang-1700.6.4.2`), `-std=c++17 -O3 -DNDEBUG`, on local arm64 macOS.
No test/build processes ran alongside the final benchmark. Nine serial AB/BA
pairs per fixture/mode follow one discarded warmup pair. Each search starts
with a fresh process/TT; fixtures and ordering are deterministic (no random
seed). Search-reported elapsed time excludes process startup and PV display;
raw records also retain total process wall time.

Run:

```sh
python3 tools/benchmark_ultimate_turn_start.py /path/to/baseline /path/to/candidate
```

### Fixed 1,000,000-node searches

| Fixture | Baseline median ms | Corrected median ms | Median paired throughput change |
| --- | ---: | ---: | ---: |
| Classic | 1363 | 1189 | +14.0% |
| Ultimate / Jesters | 628 | 629 | +0.8% |
| Devils and Minions | 519 | 436 | +18.3% |
| Sniper cooldowns | 419 | 385 | +8.4% |
| Recovered mixed-piece opening | 1244 | 1185 | +4.2% |

All fixed-node pairs returned identical scores, PVs, and node counts. The
Jester fixture is effectively flat: its separate medians differ by 1 ms and
its paired throughput ratio is slightly positive. Do not interpret that
sub-percent difference as a strength or reliable speed claim.

### Fixed depth 8

| Fixture | Baseline median ms | Corrected median ms |
| --- | ---: | ---: |
| Classic | 107 | 90 |
| Ultimate / Jesters | 435 | 434 |
| Devils and Minions | 77 | 65 |
| Sniper cooldowns | 9 | 8 |
| Recovered mixed-piece opening | 790 | 756 |

Minion and mixed-piece searches visit slightly different numbers of nodes
at fixed depth because the check predicate changes the search tree. Raw
nodes, scores, PVs, timings, binary hashes and summaries are retained in
[`turn-start-check-2026-09-12.json`](turn-start-check-2026-09-12.json).
The short cooldown fixed-depth result has millisecond-resolution limits;
the million-node measurement is the useful throughput comparison.

Early versions of this fix showed a 5–7% mixed-piece slowdown. The final
implementation removes the unnecessary Bomb-array scans using bitboard
connected-component traversal, delays attack-vector allocation, skips
non-attacking types, avoids irrelevant cooldown simulations, and retains
conservative quiet King/Devil/Minion and inline Jester fast paths. Winner
queries also stop at the first legal action instead of generating all moves.

These runs show no material performance regression on the tested suite, not
a guarantee for every position or machine. No Elo claim or evaluation tuning
is involved; the unchanged engine is not a rule-correct match baseline for
the newly fixed mate positions.

## Validation

- `make -C src ultimate-rules-test`: passed, including new turn-start cases.
- `make -C src ultimate-information-test`: passed.
- Tablebase generator translation unit: warning-enabled syntax check passed;
  no tablebase solve was run.
- Depth-three perft matches baseline for all fixtures: 8,900; 74,045; 1,862;
  5,707; 42,870 respectively.
- Regression cases cover both colors, mate and ordinary check, genuine
  stalemate, cooldown 1 versus 2, freeze, Minion trains, edge disappearance,
  ray discovery/blocking, Angel rescue, simultaneous King deaths, Jester/Ghost
  exceptions, notation, concrete/belief mate scoring, and state/undo integrity.
- `git diff --check`: passed.
- Full `make -C src ultimate-test` was attempted. Twenty external-payload
  fixture assertions fail because required installed tables are unavailable.
  An independently built unchanged revision reproduces those same failures,
  plus two old Sniper-probe expectations now intentionally replaced by stale-
  codec rejection checks (22 baseline failures). These are not presented as
  a passing full external-tablebase integration run.

The unrelated untracked Checker repair plan was left untouched.
