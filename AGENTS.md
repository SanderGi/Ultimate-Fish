# Ultimate Fish Development Guide

## Scope

Ultimate Fish is a standalone Chess Ultimate 5.731 engine and local workbench.
It is derived from Fairy-Stockfish, but new work in this repository targets the
native Ultimate rules, search, drafting, setup optimization, UI, and Android
validation pipeline. Do not describe it as a generic Fairy-Stockfish variant or
direct new features toward `variants.ini`.

## Priorities

1. Match the shipping game's rules exactly.
2. Preserve public-information boundaries for Ghosts, King/Jester ambiguity,
   and private Ranked placement.
3. Improve strength only against a rule-correct baseline, using reproducible
   benchmarks and color-balanced matches.
4. Keep the UI and engine bridge local-only. Do not add authentication,
   databases, analytics, cloud bindings, deployment scaffolding, or remote
   services without an explicit request.

## Architecture

- `src/ultimate/position.{h,cpp}`: 8×10 bitboards, pieces, legal actions, state,
  UPN, terminal detection, and reversible move application.
- `src/ultimate/search.{h,cpp}`: evaluation, iterative deepening, alpha-beta/PVS,
  transposition table, move ordering, and multi-belief robust search.
- `src/ultimate/draft.{h,cpp}`: the recovered twelve-window draft.
- `src/ultimate/main.cpp`: the local text protocol used by tools and the UI.
- `tests/ultimate_rules.cpp`: native conformance, perft, undo, and search tests.
- `tools/ultimate_phone.py`: deterministic Android controller and state tracker.
- `tools/conform_ultimate_local.py`: manifest-driven shipping-app Local
  fixtures and coverage-guided pass-and-play differential testing.
- `tools/evolve_ultimate_army.py`: roster plus placement evolution through
  engine-versus-engine games.
- `tools/validate_ultimate_armies.py`: deeper common-pool re-ranking of evolved
  finalists.
- `ui/`: local Next.js development server and loopback-only engine bridge.

The inherited Fairy-Stockfish sources remain as the licensed baseline and may
be reused when their optimized primitives fit Ultimate semantics. Ultimate's
custom rules live under `src/ultimate/`; do not force them into the upstream
variant configuration model.

## Build and validation

Run commands from the repository root unless noted otherwise:

```bash
make -C src ultimatefish
make -C src ultimate-test
make -C src ultimate-information-test
python3 tests/test_ultimate_phone.py
python3 tests/test_conform_ultimate_local.py
python3 tests/test_evolve_ultimate_army.py
python3 -m py_compile tools/ultimate_phone.py tools/conform_ultimate_local.py tools/evolve_ultimate_army.py
tools/benchmark_ultimate.sh
git diff --check
```

For UI changes:

```bash
cd ui
npm install
npm run lint
npm test
```

The UI test starts a temporary local Next development server; there is no
production build or deployment target.

## Rule changes

- Treat native app behavior or recovered simulation code as authoritative.
- Add the smallest position that reproduces a discovered interaction to
  `tests/ultimate_rules.cpp` or `tests/test_ultimate_phone.py`.
- Record confirmed behavior in `docs/rules/native-spec.md`.
- Keep move application fully reversible and ensure UPN round trips preserve
  every field that can affect legal moves, evaluation, or transposition keys.
- Update bitboards incrementally in release paths. Debug/test rebuilds may be
  used as invariant oracles.
- Belief branching is for genuine hidden information, never for compensating
  for a missed public move or known rule bug.

## Search and strength changes

- Benchmark before and after on fixed UPN fixtures.
- Use deterministic node limits for engine-versus-engine comparisons and swap
  colors/setup orientation.
- Report nodes, elapsed time, result distribution, and seeds. Small samples are
  regression signals, not Elo claims.
- Do not add hardcoded move overrides or fixture-specific evaluation bonuses.
- Preserve iterative deepening so the controller can return the last completed
  depth when a live clock limit expires.

## Android controller

- Keep the decision loop deterministic and engine-driven; an LLM must not be
  required after launch.
- Prefer native lifecycle logs for synchronization and public board probing for
  state recovery. Accessibility APIs are not part of the supported path.
- Validate only the local army in the setup editor. Once play starts, reconcile
  public captures and moves instead of comparing against the pregame enemy side.
- Never expose exact invisible enemy Ghost coordinates, unrevealed Ranked
  placement, or concrete enemy King/Jester identity to search.
- Spending currency, unlocking characters, and entering online modes require
  explicit user authorization for that run.

## Repository hygiene

- Preserve unrelated user changes in a dirty worktree.
- Use focused commits and include the relevant tests in the commit message or
  handoff.
- Do not commit APK/IPA bundles, replay dumps containing account data, generated
  binaries, `node_modules`, `.next`, device screenshots, or controller logs.
- Keep the Fairy-Stockfish and Stockfish attribution and GPLv3 license intact.
