# Validation — 2026-09-13

- `tools/fly/build-native.sh`: native circuit and server referee compiled.
- `npm run lint`: passed.
- `npm test`: all 51 tests passed, including native replay parity, legal starts,
  repetition/undo, request isolation, stale browser response handling, model checks,
  HTTP input bounds and password-path checks. The suite reused the existing local
  UI through `ULTIMATE_FISH_UI_TEST_URL` after an initial rendering-test run hit
  its development-server lock. `FLY_TEACHER` enabled the native training oracle.
- `npm run build`: clean production build, including TypeScript and fly assets.
  Standalone file tracing excludes source/data paths explicitly supplied by the
  Dockerfile; no broad project tracing warning remains.
- Server replay parity: three 120-action deterministic rollouts compared state,
  legal actions, features, terminal status and UPN after every action, ending
  early on terminal positions. Every initial action in all three armies and both
  file reflections was applied. Independent requests restored identical starts.
  A repeated knight cycle reached threefold draw; undo restored its predecessor.
- Password-configured standalone server: `/fly` and checked required assets 200;
  `/`, `/api/engine/state`, unknown fly API children and removed worker/WASM paths
  401. Both exact fly API routes remain public.
- Complete HTTP turn: native opening state, human move, full-circuit neural choice,
  native fly move, and history truncation back to the identical opening all passed.
  Native opening response took 18.7 ms; neural inference took 5.82 s, returning
  eight activity frames, 164,587 neurons and 25,563,197 connections.
- Referee input checks: illegal histories/arbitrary UPN/invalid JSON 400;
  request above 8 KiB 413; unrelated Origin 403. Same-origin requests worked on
  the standalone server; HTTPS proxy handling has a regression test.
- The worker, generated WebAssembly files and Emscripten build stage were removed.
  The rules/feature encoder and trained checkpoint are unchanged, preserving the
  training report's historical source fingerprints.
- Flight animation revision: regenerated anatomy from pinned Flybody source,
  preserving 67 body nodes and all 272,550 source triangles. Regression checks
  verify all eight segments in each of six leg chains inherit hip movement,
  every leg and both wings change pose, transforms remain finite, and reduced
  motion hides the wing exposure copies. Source and output hashes match.
  Lint and the production build passed after the scene/controller changes.
- `git diff --check`: passed.

Final checkpoint: 973 deduplicated training positions, 232 validation positions
with all training keys excluded. On up to six sampled candidates per position,
teacher-choice accuracy was 53.0% versus 18.2% chance; cross entropy 1.290 versus
1.754. Fixed 100-iteration optimization budget, not a convergence claim.
Twelve color-balanced control games total: against random, 1 win / 5 draws /
0 losses; against silenced circuit, 2 wins / 4 draws / 0 losses. Games capped at
100 actions are counted as draws. This is not an Elo estimate or a topology
advantage study. Full records are in training-report.json.

Validation used the native compiler, Node, HTTP requests and rule/model tests.
No manual browser visual/interaction QA was performed.
The local standalone assembly and ARM64 Docker image were exercised directly. No release, deployment,
commit, online game, currency operation, or external engine integration occurred.

## Docker optimization verification

The first full Docker build exposed an out-of-memory failure while decompressing
all 152 million source edges. The converter now streams Feather record batches,
retaining only the 25,563,197 circuit edges. The output graph hash and neuron
atlas match the prior artifacts exactly. The complete ARM64 image then built
successfully in the existing 8 GiB Docker VM.

Native engine, circuit and referee compilation now have independent cache stages.
Raw source files use a BuildKit cache mount with atomic downloads and checksum
validation. The runtime includes only four draft-search Python helpers (506,979
source bytes). Compiler, NumPy and PyArrow are absent from the final runtime.
The optimized build transferred 16.70 MB of context on the first build.

The running container passed public-asset/password boundary checks, full-circuit
inference (5.746 seconds for the tested opening), native move application, and
all twelve public draft windows through the authenticated engine API. It ran as
the non-root node user. Deployment regression tests passed. No image was pushed
or deployed; all source changes remain uncommitted.
