# Ultimate Fly

An unlinked public page at `/fly` on the existing Next server. It plays native
Chess Ultimate against a trained linear readout of the **full 164,587-cell
MaleCNS v1.0 graph**, with **25,563,197 directed connections**. No small-circuit
fallback exists.

Move logic runs in a native C++ server subprocess, following the analysis UI’s
request/response pattern. `/api/fly/state` accepts only one of three public presets,
a reflection flag and at most 400 legal action indices. Each request replays that
history and returns the board, legal actions and public features. Undo truncates
the history; games have no shared mutable session state. Requests are capped at
8 KiB, four concurrent referees and ten seconds per process, with cancellation
when the client disconnects. The referee has no search or arbitrary UPN import.
The browser handles display, input and animation; no WebAssembly is required.

The full neural circuit runs in a persistent native subprocess loaded lazily by
`/api/fly/choose`. That public endpoint accepts at most 256 eight-number action-feature
vectors, caps the request at 32 KiB, admits one inference at a time (429 otherwise),
and kills a stalled computation after 120 seconds. Peak circuit payload is about
196 MiB plus working arrays; the full graph is never sent to browsers.

`password-exemption.mjs` enumerates the page, its required static files, the
corresponding-source archive and precisely `/api/fly/choose` and `/api/fly/state`. Existing engine APIs,
root page, and unrelated paths remain protected. No existing page links to `/fly`.
The HTML opts out of indexing. This is not an access-control guarantee: the page
and fly endpoint are intentionally public.

## Local setup

Use Node >=22.13, C++17, and Python with NumPy/PyArrow.
From the repository root:

```sh
python3 tools/fly/download-data.py /tmp/ultimate-fly-data
python3 tools/fly/prepare-connectome.py /tmp/ultimate-fly-data networks/fly
# The generated manifest must match ui/public/fly/data/manifest.json's binary hash.
tools/fly/build-native.sh
cd ui
npm install
npm run build:fly
npm run dev
```

The existing Dockerfile builds the native referee, circuit runner, and full data
from checksum-pinned official sources. Data compilation downloads about 1.1 GiB
and requires several GiB of build memory. Runtime contains only the 196 MiB graph.
The data converter streams edge batches to avoid loading all 152 million source
rows at once. BuildKit caches checksum-verified raw downloads separately from
conversion. Each native executable has its own build stage and minimal source
inputs; changing a training or recording script does not recompile the engines.
The runtime copies only the four Python files needed by public draft search.
Generated local binaries, browser bundles, and tablebase datasets are excluded
from the build context (the published tablebase plot is retained).

No deployment has been performed. Model and mesh/atlas assets are reviewable;
generated executables, bundles, source archive and full graph are ignored.

Optional runtime overrides: `ULTIMATE_FLY_BINARY` and `ULTIMATE_FLY_DATA` are paths
to the native circuit runner and connectome.bin, respectively.
`ULTIMATE_FLY_RULES_BINARY` overrides the native referee executable. Defaults resolve relative
to the UI directory. Start the existing UI normally; a separate engine bridge is
not needed for the fly page. A missing graph/binary returns a recoverable 503.

`tools/fly/rules-server.cpp` wraps the same `rules.cpp` used during training. The
server transport change does not alter its rules, features or trained checkpoint.

## Reproduce training

```sh
c++ -Isrc/ultimate -std=c++17 -O3 -DNDEBUG -DFLY_TEACHER tools/fly/rules.cpp \
  src/ultimate/position.cpp src/ultimate/search.cpp src/ultimate/nnue.cpp \
  src/ultimate/tablebases/tablebase_probe.cpp src/ultimate/tablebases/information.cpp \
  -o /tmp/ultimate-fly-teacher
c++ -std=c++17 -O3 -shared -fPIC tools/fly/brain.cpp -o /tmp/ultimate-fly-brain.dylib
FLY_DATA=networks/fly OPENBLAS_NUM_THREADS=1 VECLIB_MAXIMUM_THREADS=1 \
  python3 tools/fly/train.py
cd ui && npm run build:fly
```

Python additionally needs SciPy for optimization. The `.dylib` output name is just
a path; on Linux use `.so` and set `FLY_BRAIN_LIBRARY`. `FLY_TEACHER` overrides the
teacher path. The inference cache in `/tmp` is a performance aid, not a trained network.
It is invalidated when the graph manifest or circuit source changes. Dataset and
code fingerprints are recorded in the report. `FLY_CACHE` overrides its path.

Teacher: iterative-deepening Ultimate Fish, depth <=3, 512 nodes per turn,
tablebases disabled. Training: 24 games, seeds 1000–1023, 48-action cap, 30% seeded
random exploration. Up to six legal actions per position: teacher choice plus
five sampled alternatives. Each candidate's full-circuit motor outputs are
memoized by its exact public feature vector, an exact optimization of a stateless
encoder, not a reduced circuit. All training and validation position keys are
deduplicated; validation excludes every training key (6 games, seeds
100000–100005). The validation metric ranks sampled candidates, not every legal
move, and must not be described as full-frontier playing accuracy.

Readout: standardize 2,129 motor activities (scale floor .005), fit a scalar linear
score with teacher-choice cross entropy and .02 L2 penalty. L-BFGS has a fixed
100-iteration budget; the report records whether it converged or hit that limit.
Only these 2,129 weights train. There is no observation bypass into the score,
handwritten tactical override, runtime search, or engine hint.

Features: tanh(material-point change / 8), public destination capture exposure,
normalized advancement, file centrality, gives check, mover's resulting cooldown,
mover's draft value, and capture flag. Except the signed first feature, these
are in [0,1]. This deliberately simple encoder discards considerable chess detail.
It is an experimental, weak opponent; no biological fidelity or Elo claim is made.

Controls: 6 color-balanced games each versus seeded random actions and the same
readout with circuit activity silenced (all action scores tie, first legal move).
Three rosters × both colors, mirrored setup orientations, seeds 200000–200002,
100-action cap. Capped games are counted as draws, not proven game-theoretic draws.
These small samples are regression signals. The report includes per-game action
counts, timing, outcomes, teacher nodes, seeds and validation measurements.

## Rules and presentation

Three fixed mirrored public armies: classic pieces; a Ninja/Mage/Prince/Sniper
army; and a Dragon/Fisherman/Penguin/Goop army. Hidden Ghosts and Jesters are not
included, so no private facts are supplied to the policy. This does not implement
Ranked drafting, private placement or an arbitrary army editor. Both sides use
the same roster. Native cooldowns, automatic promotions, special moves, check,
terminal adjudication and same-side continuations are preserved. Threefold
repetition and a 400-action exhibition draw cap are enforced by the wrapper.

The anatomical mesh is Flybody, with the complete 67-body joint hierarchy and
all 272,550 source triangles. Six connected leg chains tuck into the source
model’s flight posture. Wing sweeps, small leg adjustments and hover/bank motion
are authored animations. Translucent wing samples prevent display-rate aliasing.
There is no platform. Reduced motion shows a still, wings-open flight pose. The neuron point cloud uses 139,741 real soma positions.
All circuit cells are computed; only a fixed 1/8 index sample is sent in eight
activity frames for efficient animation, interpolated between frames. Every
measured soma remains visible as a dim point at rest. During computation,
prerecorded native full-circuit telemetry provides the waiting animation, then
the current result takes over. Reproduce the 21 clips (three armies, deterministic
legal actions at eight-action intervals) with `python3 tools/fly/record-activity.py`
after building the native executables and graph. The recording includes its
inputs and graph/runtime/output hashes. Playback uses 140 ms per simulation step,
interpolates between frames, and loops only after all 21 responses. It adds no
procedural waves or fabricated spikes and is labeled as recorded, not live. Replay brightness depicts
dimensionless absolute rate activity, not spikes. Paused/reduced motion leaves play usable.
The game continues without WebGL if the 3D views are unavailable.

Credits and exact modeling assumptions: [public notice](../../ui/public/fly/data/NOTICE.md).
Rebuild anatomy independently of training (NumPy only):

```sh
git clone https://github.com/TuragaLab/flybody.git /tmp/ultimate-fly-anatomy
git -C /tmp/ultimate-fly-anatomy checkout d015e9bfe441bd90ae431bac24c55cb74bdbce26
python3 tools/fly/prepare-anatomy.py /tmp/ultimate-fly-anatomy ui/public/fly/data
```

The metadata pins source-file and binary hashes. Meshes and joint transforms come
directly from that Flybody revision. The original prototype used the body
conversion from [fly-connectome-template by Mert Cobanov](https://github.com/cobanov/fly-connectome-template);
its attribution remains in the public notices.
