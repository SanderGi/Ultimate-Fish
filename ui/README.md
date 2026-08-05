# Ultimate Fish UI

A local-first play, native-draft, position-editing, and analysis workbench for
Ultimate Fish. It exposes the recovered 5.731 roster and exact draft costs,
uses lossless UPN for special state, and talks to the local C++ engine through a
loopback-only bridge.

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
