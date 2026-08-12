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

async function streamHistoryAnalysis(body) {
  const response = await fetch(`${base}/analyze-history-stream`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
  });
  assert.equal(response.status, 200);
  return (await response.text()).trim().split("\n").filter(Boolean)
    .map((line) => JSON.parse(line));
}

test("bridge exposes state, mate scores, results, and continuations", async () => {
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

});

test("bridge searches every public draft window", async () => {
  const history = [];
  for (let phase = 0; phase < 12; phase += 1) {
    const result = await post("/draft-ai", {
      history, player: phase % 2 ? "black" : "white",
      depth: 1, timeLimit: 0.5,
    });
    if ([0, 1, 4, 5, 8, 9].includes(phase)) assert.equal(result.choices.length, 1);
    else assert.ok(Array.isArray(result.choices));
    history.push(...result.choices.map((piece) => `draft choose ${piece}`), "draft commit");
    assert.match(result.status, new RegExp(`^draft phase ${phase + 1}\\b`));
  }
});

test("history analysis forgets leaked Ghost cells and replays public observations", async () => {
  const ghostStart = (side, square) =>
    `${side};king,w,a1;ghost,w,${square},0,0,0,0,0,0,-1,1,-1,0;king,b,h10`;
  const c3 = await post("/history-state", {
    initialUpn: ghostStart("b", "c3"), moves: [], observer: "black",
    enemyKingKnown: false,
  });
  const f3 = await post("/history-state", {
    initialUpn: ghostStart("b", "f3"), moves: [], observer: "black",
    enemyKingKnown: false,
  });
  assert.equal(c3.beliefs, 75);
  assert.equal(f3.beliefs, 75);
  assert.equal(c3.decisionPartitions, 1);
  assert.equal(c3.ghostKnowledge.c3.known, false);
  assert.equal(c3.ghostKnowledge.c3.candidates.length, 75);
  assert.equal(f3.ghostKnowledge.f3.known, false);
  const ownGhost = await post("/history-state", {
    initialUpn: ghostStart("b", "c3"), moves: [], observer: "white",
    enemyKingKnown: false,
  });
  assert.deepEqual(ownGhost.ghostKnowledge.c3, {
    known: true, candidates: ["c3"],
  });
  const deployed = await post("/history-state", {
    initialUpn: ghostStart("b", "c3"), moves: [], observer: "black",
    enemyKingKnown: false, initialDeploymentKnown: true,
  });
  assert.equal(deployed.beliefs, 23);

  const analysisC3 = await post("/analyze-history", {
    initialUpn: ghostStart("b", "c3"), moves: [], observer: "black",
    enemyKingKnown: false, depth: 1,
  });
  const analysisF3 = await post("/analyze-history", {
    initialUpn: ghostStart("b", "f3"), moves: [], observer: "black",
    enemyKingKnown: false, depth: 1,
  });
  assert.equal(analysisC3.bestmove, analysisF3.bestmove);
  assert.equal(analysisC3.score, analysisF3.score);
  assert.equal(analysisC3.beliefs, 75);

  const computerC3 = await post("/computer-history", {
    initialUpn: ghostStart("b", "c3"), moves: [], player: "white",
    enemyKingKnown: false, depth: 1, movetime: 0,
  });
  const computerF3 = await post("/computer-history", {
    initialUpn: ghostStart("b", "f3"), moves: [], player: "white",
    enemyKingKnown: false, depth: 1, movetime: 0,
  });
  assert.equal(computerC3.engineMoves[0], computerF3.engineMoves[0]);
  assert.equal(computerC3.engine.beliefs, 75);

  const reportedMidgame = await post("/computer-history", {
    initialUpn: "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,d8,0,0,0,0,0,1,-1,1,-1,0;king,b,d10,0,0,0,0,0,1,-1,1,-1,0;ghost,w,b10,0,0,0,0,0,0,-1,1,-1,0",
    moves: [], player: "white", enemyKingKnown: false,
    depth: 1, movetime: 0,
  });
  assert.ok(["d10-c10", "d10-e10"].includes(reportedMidgame.engineMoves[0]));
  assert.equal(reportedMidgame.upn[0], "w");
  const streamed = await streamHistoryAnalysis({
    initialUpn: "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,d8,0,0,0,0,0,1,-1,1,-1,0;king,b,d10,0,0,0,0,0,1,-1,1,-1,0;ghost,w,b10,0,0,0,0,0,0,-1,1,-1,0",
    moves: [], observer: "black", enemyKingKnown: false, depth: 2,
  });
  assert.deepEqual(
    streamed.filter((event) => event.type === "iteration")
      .map((event) => event.analysis.depth),
    [1, 2],
  );
  assert.equal(streamed.at(-1).type, "result");
  assert.equal(streamed.at(-1).analysis.depth, 2);
  assert.equal(streamed.at(-1).analysis.moves.length, 2);

  const afterInvisibleMove = await post("/history-state", {
    initialUpn: ghostStart("w", "c3"), moves: ["c3-d3"],
    observer: "black", enemyKingKnown: false,
  });
  assert.ok(afterInvisibleMove.beliefs > 1);
  assert.equal(afterInvisibleMove.decisionPartitions, 1);

  const royalUpn = "b;king,w,a1;jester,w,b1;king,b,h10";
  const concealed = await post("/history-state", {
    initialUpn: royalUpn, moves: [], observer: "black",
    enemyKingKnown: false,
  });
  const revealed = await post("/history-state", {
    initialUpn: royalUpn, moves: [], observer: "black",
    enemyKingKnown: true,
  });
  assert.equal(concealed.beliefs, 2);
  assert.equal(concealed.enemyKingKnown, false);
  assert.equal(revealed.beliefs, 1);
  assert.equal(revealed.enemyKingKnown, true);
  assert.equal(concealed.jesterKnowledge.b1, false);
  assert.equal(revealed.jesterKnowledge.b1, true);

  const chronologicalRoyals =
    "b;king,w,a1;jester,w,b1;jester,w,c1;king,b,h10";
  const firstGroup = await post("/history-state", {
    initialUpn: chronologicalRoyals, moves: [], observer: "black",
    enemyKingKnown: false, enemyKingCandidates: ["a1", "b1"],
  });
  assert.equal(firstGroup.beliefs, 2);
  assert.equal(firstGroup.enemyKingKnown, false);
  assert.deepEqual(firstGroup.enemyKingCandidates, ["a1", "b1"]);
  assert.equal(firstGroup.jesterKnowledge.b1, false);
  assert.equal(firstGroup.jesterKnowledge.c1, true);
  const singletonFirstGroup = await post("/history-state", {
    initialUpn: chronologicalRoyals, moves: [], observer: "black",
    enemyKingKnown: false, enemyKingCandidates: ["a1"],
  });
  assert.equal(singletonFirstGroup.beliefs, 1);
  assert.equal(singletonFirstGroup.enemyKingKnown, true);

  const draftAmbiguityRegression =
    "b;hm=41;fm=19;ep=-;cont=0;forced=-1;epv=-1;king,w,h1,0,0,0,0,1,1,-1,1,-1,0;jester,w,a1,0,0,0,0,0,1,-1,1,-1,0;rook,w,e1,0,0,0,0,1,1,-1,1,-1,0;berserker,w,e4,0,0,0,0,1,1,-1,1,-1,0;pawn,w,a3,0,0,0,0,0,1,-1,1,-1,0;pawn,w,b3,0,0,0,0,0,1,-1,1,-1,0;pawn,w,c3,0,0,0,0,0,1,-1,1,-1,0;sniper,w,g2,0,0,0,0,1,1,-1,1,-1,0;pawn,w,f3,0,0,0,0,0,1,-1,1,-1,0;king,b,d10,0,0,0,0,0,1,-1,1,-1,0;prince,b,f5,0,0,0,0,1,1,-1,1,-1,0;prince,b,d9,0,0,0,0,1,1,-1,1,-1,0";
  const informationSearch = await post("/analyze-history", {
    initialUpn: draftAmbiguityRegression, moves: [], observer: "black",
    enemyKingKnown: false, enemyKingCandidates: ["h1", "a1"], depth: 3,
  });
  assert.equal(informationSearch.beliefs, 2);
  assert.equal(informationSearch.scoreType, "cp");
  assert.ok(informationSearch.nodes > 0);

  const bothSidesAmbiguous =
    "b;king,w,a1;jester,w,b1;king,b,h10;jester,b,g10;rook,b,e9";
  const computerBeliefs = await post("/computer-history", {
    initialUpn: bothSidesAmbiguous, moves: [], player: "white",
    enemyKingKnown: false, enemyKingCandidates: ["h10", "g10"],
    engineEnemyKingKnown: false, engineEnemyKingCandidates: ["a1", "b1"],
    depth: 1, movetime: 0,
  });
  assert.equal(computerBeliefs.engine.beliefs, 2);
  assert.deepEqual(computerBeliefs.enemyKingCandidates, ["g10", "h10"]);
});

test("history-preserving analysis scores terminal worlds and observer perspective", async () => {
  const matingUpn =
    "b;hm=3;fm=2;ep=-;cont=0;forced=-1;epv=-1;king,w,a1,0,0,0,0,1,1,-1,1,-1,0;jester,w,h1,0,0,0,0,0,1,-1,1,-1,0;rook,w,d1,0,0,0,0,0,1,-1,1,-1,0;rook,w,d8,0,0,0,0,1,1,-1,1,-1,0;ninja,w,b2,0,0,0,0,0,1,-1,1,-1,0;ghost,w,g2,0,0,0,0,0,0,-1,1,-1,0;rook,w,e1,0,0,0,0,0,1,-1,1,-1,0;pawn,w,f2,0,0,0,0,0,1,-1,1,-1,0;king,b,d10,0,0,0,0,0,1,-1,1,-1,0;giant,b,e9,0,0,0,0,0,1,-1,1,-1,0;giant,b,b9,0,0,0,0,0,1,-1,1,-1,0;prince,b,g10,0,0,0,0,0,1,-1,1,-1,0;prince,b,a10,0,0,0,0,0,1,-1,1,-1,0;checker,b,h10,0,0,0,0,0,1,-1,1,-1,0;giant,b,g8,0,0,0,0,0,1,-1,1,-1,0;prince,b,a9,0,0,0,0,0,1,-1,1,-1,0;knight,b,e8,0,0,0,0,0,1,-1,1,-1,0;checker,b,c8,0,0,0,0,0,1,-1,1,-1,0;checker,b,f8,0,0,0,0,0,1,-1,1,-1,0";
  const historyMate = await post("/analyze-history", {
    initialUpn: matingUpn, moves: [], observer: "black",
    enemyKingKnown: false, enemyKingCandidates: ["a1", "h1"], depth: 1,
  });
  assert.ok(historyMate.beliefs > 1);
  assert.equal(historyMate.scoreType, "mate");
  assert.equal(historyMate.score, -1);
  assert.equal(historyMate.bestmove, null);

  const singletonForWinner = await post("/analyze-beliefs", {
    positions: [matingUpn], observer: "white", enemyKingKnown: true,
    depth: 1,
  });
  assert.equal(singletonForWinner.scoreType, "mate");
  assert.equal(singletonForWinner.score, 1);
});

test("bridge keeps raw actions while exposing chess-style public notation", async () => {
  const captureUpn = "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,a1;queen,w,h9;king,b,a8;pawn,b,h10";
  const capture = await post("/move", { upn: captureUpn, move: "h9-h10" });
  assert.equal(capture.notation, "Qxh10");
  assert.equal(capture.publicNotation, "Qxh10");

  const jesterUpn = "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,h1;jester,w,c2;king,b,h10";
  const jester = await post("/move", { upn: jesterUpn, move: "c2-d3" });
  assert.equal(jester.notation, "Jd3");
  assert.equal(jester.publicNotation, "Kd3");

  const ghostUpn = "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,a1;ghost,w,c3,0,0,0,0,0,0;king,b,h10";
  const ghost = await post("/move", { upn: ghostUpn, move: "c3-d4" });
  assert.equal(ghost.notation, "GHd4");
  assert.equal(ghost.publicNotation, "GH");

  const analysis = await post("/analyze", { upn: captureUpn, depth: 1 });
  assert.equal(analysis.pvNotation.length, analysis.pv.length);
  assert.equal(analysis.publicPvNotation.length, analysis.pv.length);
  assert.ok(analysis.pvNotation.every((move) => !/[-~@!&]/.test(move)));
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

  const conservative = await post("/analyze-beliefs", {
    positions: worlds, observer: "white", enemyKingKnown: false, depth: 1,
  });
  assert.equal(conservative.beliefs, worlds.length);
  assert.ok(conservative.decisionPartitions > 1);
  assert.equal(conservative.decisionMode, "merged-conservative");
  assert.equal(conservative.beliefMode, "history-preserving");
  assert.ok(conservative.bestmove);

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
