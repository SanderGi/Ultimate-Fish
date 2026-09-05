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
- `npm run dev`: start the local workbench
- `npm run lint`: run static UI checks
- `npm test`: verify the local UI and engine bridge

## Password-protected container

The repository-level `Dockerfile` builds the native engine, the standalone
Next server, the loopback engine bridge, the draft helper, and the bundled
tablebases into one image. The public server listens on port `8080`; the engine
bridge remains accessible only inside the container.

The image also publishes the canonical tablebase outcome plot at
`/ultimate-tablebase-grid.svg`. The Docker build copies it directly from
`tablebases/ultimate-tablebase-grid.svg`, so the deployed asset cannot drift
from the repository plot. It is covered by the same password protection as the
rest of the server.

Production fails closed unless `UI_PASSWORD` is set. HTTP Basic authentication
uses the username `ultimatefish` by default; set `UI_USERNAME` to override it.
For Fly.io, configure both values as app secrets rather than build arguments:

```bash
fly secrets set UI_PASSWORD='replace-me'
# Optional:
fly secrets set UI_USERNAME='alex'
```

Local development remains unprotected when `UI_PASSWORD` is absent. Setting it
before `npm run dev` enables the same password prompt locally.
