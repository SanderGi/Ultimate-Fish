# Ultimate Fish UI

A local-first play, native-draft, position-editing, and analysis workbench for
Ultimate Fish. It exposes the recovered 5.731 roster and exact draft costs,
uses lossless UPN for special state, and talks to the local C++ engine through a
loopback-only bridge.

Play and ordinary analysis are public-information modes. The bridge sends the
engine one private starting UPN plus the selected move-history prefix; the
engine reconstructs the observer's complete public belief, including private
legal-dot observations, without selecting the leaked concrete Ghost world.
Selecting an earlier history node reconstructs knowledge at that node. Drafted
games additionally preserve the fact that their initial Ghosts are still in a
deployment home zone and carry the exact first-pick royal candidate set into
play; later-group Jesters are known while first-group silhouettes can remain
ambiguous. The draft
depth control sets the engine depth used by the shared adversarial ban/pick
search. Analysis always draws Ghosts and uses the selected player side to choose
both whether an unrevealed one is translucent and which player's public belief
is analyzed. Side to move independently controls which color acts next.

“Copy current belief” writes Ultimate Belief JSON v1. It stores the private
initial UPN, the selected move-history prefix, whether that initial position
came from a draft, the current viewer, and separate `enemyKingKnown` plus
`enemyKingCandidates` inputs for both observers. Loading it replays the history
through the engine, so Ghost and King/Jester knowledge are reconstructed rather
than serialized as one viewer's expanded world list.

## Prerequisites

- The `src/ultimatefish` engine (`make -C ../src ultimatefish`)
- Node.js `>=22.13.0`

## Quick start

```bash
npm install
npm run engine
```

In a second terminal:

```bash
npm run dev
```

Open `http://localhost:3000`. The bridge listens only on
`http://127.0.0.1:3001` and expects the engine at `../src/ultimatefish`. Set
`ULTIMATE_FISH_BINARY=/absolute/path/to/ultimatefish` to use another build.

## Useful commands

- `npm run engine`: start the loopback engine bridge
- `npm run dev`: start the local-only workbench
- `npm run lint`: run static UI checks
- `npm test`: verify the local UI and engine bridge

The workbench intentionally has no production deployment target, authentication,
database, analytics, or cloud service. It is designed to be cloned and run on
the same computer as the engine.
