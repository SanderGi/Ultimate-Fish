import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import path from "node:path";
import test, { after, before } from "node:test";

const base = process.env.ULTIMATE_FISH_TEST_URL ?? "http://127.0.0.1:3011";
const uiDirectory = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
let bridge;

before(async () => {
  if (process.env.ULTIMATE_FISH_TEST_URL) return;
  bridge = spawn(process.execPath, ["engine-server.mjs"], {
    cwd: uiDirectory,
    env: { ...process.env, ULTIMATE_FISH_PORT: "3011" },
    stdio: ["ignore", "pipe", "pipe"],
  });
  let output = "";
  bridge.stdout.on("data", (chunk) => { output += chunk; });
  bridge.stderr.on("data", (chunk) => { output += chunk; });
  for (let attempt = 0; attempt < 100; attempt += 1) {
    if (bridge.exitCode !== null)
      throw new Error(`Engine bridge exited before becoming ready:\n${output}`);
    try {
      const response = await fetch(`${base}/health`);
      if (response.ok) return;
    } catch {
      // The listener may not be bound yet.
    }
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  bridge.kill("SIGTERM");
  throw new Error(`Timed out waiting for the engine bridge:\n${output}`);
});

after(async () => {
  if (!bridge || bridge.exitCode !== null) return;
  const closed = new Promise((resolve) => bridge.once("close", resolve));
  bridge.kill("SIGTERM");
  await closed;
});

async function post(endpoint, body) {
  const response = await fetch(`${base}${endpoint}`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
  });
  const result = await response.json();
  assert.equal(response.status, 200, result.error ?? `${endpoint} failed`);
  return result;
}

async function streamAnalysis(body) {
  const response = await fetch(`${base}/analyze-stream`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
  });
  assert.equal(response.status, 200);
  return (await response.text()).trim().split("\n").filter(Boolean).map((line) => JSON.parse(line));
}

test("bridge exposes state, mate scores, results, continuations, and the complete AI draft", async () => {
  const health = await fetch(`${base}/health`).then((response) => response.json());
  assert.equal(health.ok, true);

  const mateUpn = "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,a1;rook,w,b2;king,b,b4";
  const state = await post("/state", { upn: mateUpn });
  assert.equal(state.result, "ongoing");
  assert.deepEqual(state.material, { white: 13, black: 0 });
  assert.ok(state.moves.includes("b2-b4"));

  const analysis = await post("/analyze", { upn: mateUpn, depth: 3 });
  assert.equal(analysis.scoreType, "mate");
  assert.equal(analysis.score, 1);
  assert.equal(analysis.bestmove, "b2-b4");

  const longTablebaseUpn = "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;win=-;king,w,c2,0,0,0,0,1,1,-1,1,-1,0;king,b,a1,0,0,0,0,1,1,-1,1,-1,0;bishop,w,h1,0,0,0,0,1,1,-1,1,-1,0;dragon,b,c1,0,0,0,0,1,1,-1,1,-1,0";
  const longTablebase = await post("/analyze", { upn: longTablebaseUpn, depth: 16 });
  assert.equal(longTablebase.scoreType, "mate");
  assert.equal(longTablebase.score, 143);
  assert.equal(longTablebase.depth, 1);

  const captured = await post("/move", { upn: mateUpn, move: "b2-b4" });
  assert.equal(captured.result, "white");
  assert.equal(captured.resultReason, "king-captured");

  const princeUpn = "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,a1;prince,w,c3;king,b,h10;pawn,b,e3";
  const firstAction = await post("/play", {
    upn: princeUpn, move: "c3-d3", player: "white", depth: 2, movetime: 0,
  });
  assert.equal(firstAction.upn[0], "w");
  assert.deepEqual(firstAction.engineMoves, []);
  assert.ok(firstAction.moves.includes("d3-e3"));

  const secondAction = await post("/play", {
    upn: firstAction.upn, move: "d3-e3", player: "white", depth: 2, movetime: 0,
  });
  assert.equal(secondAction.upn[0], "w");
  assert.ok(secondAction.engineMoves.length >= 1);

  const history = [];
  for (let phase = 0; phase < 12; phase += 1) {
    const result = await post("/draft-ai", { history });
    if ([0, 1, 4, 5, 8, 9].includes(phase)) assert.equal(result.choices.length, 1);
    else assert.ok(Array.isArray(result.choices));
    history.push(...result.choices.map((piece) => `draft choose ${piece}`), "draft commit");
    assert.match(result.status, new RegExp(`^draft phase ${phase + 1}\\b`));
  }
});

test("aborting auto-analysis leaves the bridge responsive", async () => {
  const analysisUpn = "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,e1;jester,w,d1;ninja,w,b2;penguin,w,c2;devil,w,f2;sniper,w,g2;checker,w,h2;sludge,w,a2;king,b,e10;jester,b,d10;ninja,b,b9;penguin,b,c9;devil,b,f9;sniper,b,g9;checker,b,h9;sludge,b,a9";
  const streamed = await streamAnalysis({ upn: analysisUpn, depth: 3 });
  const iterations = streamed.filter((event) => event.type === "iteration");
  assert.deepEqual(iterations.map((event) => event.analysis.depth), [1, 2, 3]);
  assert.ok(iterations.every((event) => event.analysis.pv.length > 0));
  assert.equal(streamed.at(-1).type, "result");
  assert.equal(streamed.at(-1).analysis.depth, 3);

  const controller = new AbortController();
  const pending = fetch(`${base}/analyze`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ upn: analysisUpn, depth: 16 }),
    signal: controller.signal,
  });
  setTimeout(() => controller.abort(), 5);
  await assert.rejects(pending, { name: "AbortError" });
  const health = await fetch(`${base}/health`).then((response) => response.json());
  assert.equal(health.ok, true);
});

test("bridge retains uncapped beliefs and analyzes one exact decision cell", async () => {
  const squares = [];
  for (let rank = 1; rank <= 10; rank += 1)
    for (const file of "abcdefgh") squares.push(`${file}${rank}`);
  const occupied = new Set(["a1", "a2", "b1", "b2", "c1", "h10"]);
  const worlds = squares.filter((square) => !occupied.has(square)).map((square) =>
    `w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;win=-;king,w,a1,0,0,0,0,1,1,-1,1,-1,0;rook,w,c1,0,0,0,0,1,1,-1,1,-1,0;king,b,h10,0,0,0,0,1,1,-1,1,-1,0;ghost,b,${square},0,0,0,0,0,0,-1,1,-1,0`,
  );
  assert.ok(worlds.length > 64);
  const retained = await post("/belief-state", {
    positions: worlds, observer: "white", enemyKingKnown: false,
  });
  assert.equal(retained.beliefs, worlds.length);
  assert.equal(retained.mode, "exact-uncapped");

  const concrete = await post("/state", { upn: worlds[0] });
  const legalMarkers = [...new Set(concrete.moves.map((move) => {
    if (move === "pass") return "pass";
    const match = move.match(/^([a-h](?:10|[1-9]))[-~@x!&]([a-h](?:10|[1-9]))$/);
    assert.ok(match, `unexpected move spelling ${move}`);
    return `${match[1]}>${match[2]}`;
  }))].sort();
  const observed = await post("/belief-state", {
    positions: worlds, observer: "white", enemyKingKnown: false,
    legalMarkers,
  });
  assert.ok(observed.beliefs > 0 && observed.beliefs < worlds.length);
  const exact = await post("/analyze-beliefs", {
    positions: worlds, observer: "white", enemyKingKnown: false,
    legalMarkers, depth: 1,
  });
  assert.equal(exact.beliefs, observed.beliefs);
  assert.equal(exact.decisionPartitions, 1);
  assert.equal(exact.decisionMode, "exact-cell");
  assert.equal(exact.beliefMode, "history-preserving");
  assert.equal(exact.historyPreservingPlies, 1);
  assert.ok(exact.bestmove);

  const singleton = await post("/analyze-beliefs", {
    positions: [worlds[0]], observer: "white", enemyKingKnown: false,
    depth: 1,
  });
  assert.equal(singleton.beliefs, 1);
  assert.equal(singleton.decisionPartitions, 1);
  assert.equal(singleton.decisionMode, "merged-conservative");
  assert.equal(singleton.beliefMode, "history-preserving");
  assert.equal(singleton.historyPreservingPlies, 1);
  assert.ok(singleton.bestmove);
});
