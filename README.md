<p align="center">
  <img src="ui/public/ultimate-fish-logo.png" alt="Ultimate Fish logo" width="200">
</p>

# Ultimate Fish

Ultimate Fish is a standalone engine for the rules of Chess Ultimate 5.731. It
includes the native draft, the complete 8×10 character roster, lossless custom
positions, hidden-information search, a local macOS play/analysis workbench,
and deterministic Android automation used for conformance and strength tests.

The engine began as a Fairy-Stockfish fork, but Ultimate Fish has its own
protocol, rule implementation, search, tests, controller, and development
roadmap. Fairy-Stockfish and Stockfish remain credited in [`AUTHORS`](AUTHORS),
and the project is distributed under GPLv3 in [`Copying.txt`](Copying.txt).

## Build and test

Prerequisites:

- Apple Clang or another C++17 compiler
- GNU Make
- Python 3.10 or newer for the controller/evolution tools
- Node.js 22.13 or newer for the optional UI

From the repository root:

```bash
make -C src ultimatefish
make -C src ultimate-test
python3 tests/test_ultimate_phone.py
python3 tests/test_evolve_ultimate_army.py
tools/benchmark_ultimate.sh
```

`src/ultimatefish` is a self-contained command-line engine. It accepts lossless
Ultimate Position Notation (UPN), `position upn ...`, `go depth N`, `go nodes
N`, `go movetime N`, `moves`, `perft N`, and the native draft commands exposed
by `help`.

## Local workbench

The UI is deliberately local-only. It has no accounts, authentication,
database, telemetry, cloud deployment, or production service. Build the engine,
install the UI packages, and run the bridge and development server in separate
terminals:

```bash
cd ui && npm install
cd ui && npm run engine
cd ui && npm run dev
```

Open `http://localhost:3000`. The bridge binds only to `127.0.0.1:3001` and
launches `../src/ultimatefish`. Set `ULTIMATE_FISH_BINARY` to use another engine
binary. The workbench supports native drafting, play against Ultimate Fish,
position editing, legal-move previews, move history, and continuously refreshed
analysis.

<p align="center">
  <img src="engine-ui.png" alt="Ultimate Fish local workbench" style="max-height: 60vh">
</p>

## Rules and notation

Rule fidelity has priority over playing strength. Recovered behavior is recorded
as a native conformance fixture before it is optimized. The current specification
and reverse-engineering trail are in:

- [`docs/rules/native-spec.md`](docs/rules/native-spec.md)
- [`docs/rules/README.md`](docs/rules/README.md)
- [`docs/reverse-engineering.md`](docs/reverse-engineering.md)
- [`docs/benchmarks/ultimate-search.md`](docs/benchmarks/ultimate-search.md)

Special move notation uses `~` for Mage swaps, `@` for Devil spawns, `x` for
Sniper shots, `!` for Fisherman pulls, and `&` for Angel links. UPN preserves
every search-relevant state field, including continuations, cooldowns, freeze,
visibility, attachments, Giant footprints, and the exact en-passant victim.

## Setup evolution and engine matches

The setup optimizer evolves both roster and placement and evaluates candidates
through alternating-color Ultimate Fish games:

```bash
python3 tools/evolve_ultimate_army.py --population 32 --generations 20 \
  --nodes 10000 --final-nodes 50000 --workers 4
```

Use a fixed seed for reproducibility. `tools/selfplay_ultimate.py` compares two
engine binaries on a color-balanced fixture suite, while
`tools/benchmark_ultimate.sh` provides fixed performance and perft checks.
Re-rank saved finalists at a larger budget with
`python3 tools/validate_ultimate_armies.py RESULTS.json --nodes 10000`.

The reproducible 2026-08-05 league checkpoint is stored in
[`docs/benchmarks/evolved-army-2026-08-05.json`](docs/benchmarks/evolved-army-2026-08-05.json),
with deeper common-pool validation in
[`docs/benchmarks/evolved-army-validation-2026-08-05.json`](docs/benchmarks/evolved-army-validation-2026-08-05.json).
An adversarial follow-up added a live five-Sniper/off-corner-King roster to the
coevolution league; its full checkpoint is
[`docs/benchmarks/evolved-army-adversarial-2026-08-05.json`](docs/benchmarks/evolved-army-adversarial-2026-08-05.json).
The selected finalist scored 17W-10D-1L, then beat the earlier live setup in a
deeper common pool (11W-3D-0L versus 9W-4D-1L). It subsequently won three of
three newly supervised Very Hard CPU games by forced checkmate:

```text
king,a1;queen,h3;queen,b3;queen,c3;queen,b2;pawn,a2;pawn,h2;
giant,e1;giant,c1;pawn,g2;pawn,h1;pawn,b1;dragon,g1
```

## Android conformance and live validation

[`tools/ultimate_phone.py`](tools/ultimate_phone.py) controls a USB-debuggable
Android device without an LLM in the move loop. It uses the local engine for
decisions, native lifecycle logs for synchronization, and a conservative public
board probe where structured state is unavailable. Hidden enemy Ghost locations
and King/Jester identities are sanitized into public-information belief sets.

Examples:

```bash
python3 tools/ultimate_phone.py cpu --device SERIAL --depth 7 --games 3
python3 tools/ultimate_phone.py unranked --device SERIAL --depth 7
python3 tools/ultimate_phone.py ranked --device SERIAL --depth 8
```

Online modes should be run only with authorization for the account and game
mode being tested. The optional unlock path can be constrained to earned
character keys and never needs gems or money.

## Project layout

- `src/ultimate/`: native Ultimate rules, drafting, search, and protocol
- `tests/ultimate_rules.cpp`: C++ conformance and search regressions
- `tools/`: benchmarks, self-play, setup evolution, and Android controller
- `ui/`: local Next.js workbench and loopback engine bridge
- `docs/`: recovered rules, provenance, and benchmark records

Ultimate Fish is an independent community project and is not affiliated with
the Chess Ultimate app or its developers.
