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

async function readAnalysisStream(response, onEvent) {
  assert.equal(response.status, 200);
  assert.ok(response.body);
  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  const events = [];
  let pending = "";
  const consume = (line) => {
    if (!line.trim()) return;
    const event = JSON.parse(line);
    events.push(event);
    onEvent?.(event);
  };
  while (true) {
    const { value, done } = await reader.read();
    pending += decoder.decode(value, { stream: !done });
    const lines = pending.split(/\r?\n/);
    pending = lines.pop() ?? "";
    lines.forEach(consume);
    if (done) break;
  }
  consume(pending);
  return events;
}

async function streamAnalysis(body, onEvent) {
  const response = await fetch(`${base}/analyze-stream`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
  });
  return readAnalysisStream(response, onEvent);
}

async function streamHistoryAnalysis(body, onEvent, signal) {
  const response = await fetch(`${base}/analyze-history-stream`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
    signal,
  });
  return readAnalysisStream(response, onEvent);
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
  assert.equal(longTablebase.score, -143);
  assert.equal(longTablebase.depth, 1);
  assert.equal(longTablebase.pv.length, 16);
  assert.equal(longTablebase.pvNotation.length, 16);
  assert.equal(longTablebase.publicPvNotation.length, 16);

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

test("concrete analysis scores are positive for Ivory", async () => {
  const onyxMateUpn = "b;king,w,b7;king,b,a10;queen,b,b9";
  const onyxAnalysis = await post("/analyze", {
    upn: onyxMateUpn, depth: 3,
  });
  assert.equal(onyxAnalysis.scoreType, "mate");
  assert.equal(onyxAnalysis.score, -1);
  assert.equal(onyxAnalysis.bestmove, "b9-b7");
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

  const incrementallyAdvanced = await post("/history-state", {
    initialUpn: ghostStart("b", "c3"),
    moves: ["h10-g10", "c3-d4", "g10-f10"],
    observer: "black", enemyKingKnown: false,
  });
  assert.equal(incrementallyAdvanced.upn[0], "w");
  assert.equal(incrementallyAdvanced.ghostKnowledge.d4.known, false);
  assert.ok(incrementallyAdvanced.ghostKnowledge.d4.candidates.includes("d4"));

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
  assert.equal(analysisC3.pvNotation[0], "Kg10");
  assert.equal(analysisC3.publicPvNotation[0], "Kg10");

  const replay = await post("/replay", {
    initialUpn: ghostStart("b", "c3"), moves: ["h10-g10", "c3-d4"],
  });
  assert.equal(replay.upn[0], "b");
  assert.deepEqual(replay.history.map((record) => ({
    color: record.color,
    move: record.move,
    notation: record.notation,
    publicNotation: record.publicNotation,
    moveNumber: record.moveNumber,
  })), [
    { color: "black", move: "h10-g10", notation: "Kg10",
      publicNotation: "Kg10", moveNumber: 1 },
    { color: "white", move: "c3-d4", notation: "GHd4",
      publicNotation: "GH", moveNumber: 2 },
  ]);
  assert.deepEqual(replay.history[1].history, ["h10-g10", "c3-d4"]);

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

  const selectedViewerRegression =
    "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,c1,0,0,0,0,0,1,-1,1,-1,0;jester,w,e1,0,0,0,0,0,1,-1,1,-1,0;king,b,d10,0,0,0,0,0,1,-1,1,-1,0";
  const ivoryViewer = await post("/analyze-history", {
    initialUpn: selectedViewerRegression, moves: [], observer: "white",
    enemyKingKnown: false, depth: 1,
  });
  const onyxViewer = await post("/analyze-history", {
    initialUpn: selectedViewerRegression, moves: [], observer: "black",
    enemyKingKnown: false, depth: 1,
  });
  assert.equal(ivoryViewer.beliefs, 1);
  assert.equal(onyxViewer.beliefs, 2);

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
  assert.equal(computerBeliefs.knowledgePending, true);
  assert.ok(Array.isArray(computerBeliefs.moves));
  assert.equal(computerBeliefs.result, "ongoing");
  const hydratedComputerBeliefs = await post("/history-state", {
    initialUpn: bothSidesAmbiguous,
    moves: computerBeliefs.engineMoves,
    observer: "white", enemyKingKnown: false,
    enemyKingCandidates: ["h10", "g10"],
  });
  assert.deepEqual(
    hydratedComputerBeliefs.enemyKingCandidates, ["g10", "h10"]);
});

test("belief analysis scores are positive for Ivory", async () => {
  const matingUpn =
    "b;hm=3;fm=2;ep=-;cont=0;forced=-1;epv=-1;king,w,a1,0,0,0,0,1,1,-1,1,-1,0;jester,w,h1,0,0,0,0,0,1,-1,1,-1,0;rook,w,d1,0,0,0,0,0,1,-1,1,-1,0;rook,w,d8,0,0,0,0,1,1,-1,1,-1,0;ninja,w,b2,0,0,0,0,0,1,-1,1,-1,0;ghost,w,g2,0,0,0,0,0,0,-1,1,-1,0;rook,w,e1,0,0,0,0,0,1,-1,1,-1,0;pawn,w,f2,0,0,0,0,0,1,-1,1,-1,0;king,b,d10,0,0,0,0,0,1,-1,1,-1,0;giant,b,e9,0,0,0,0,0,1,-1,1,-1,0;giant,b,b9,0,0,0,0,0,1,-1,1,-1,0;prince,b,g10,0,0,0,0,0,1,-1,1,-1,0;prince,b,a10,0,0,0,0,0,1,-1,1,-1,0;checker,b,h10,0,0,0,0,0,1,-1,1,-1,0;giant,b,g8,0,0,0,0,0,1,-1,1,-1,0;prince,b,a9,0,0,0,0,0,1,-1,1,-1,0;knight,b,e8,0,0,0,0,0,1,-1,1,-1,0;checker,b,c8,0,0,0,0,0,1,-1,1,-1,0;checker,b,f8,0,0,0,0,0,1,-1,1,-1,0";
  const historyMate = await post("/analyze-history", {
    initialUpn: matingUpn, moves: [], observer: "black",
    enemyKingKnown: false, enemyKingCandidates: ["a1", "h1"], depth: 1,
  });
  assert.ok(historyMate.beliefs > 1);
  assert.equal(historyMate.scoreType, "mate");
  assert.equal(historyMate.score, 1);
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

  const singletonBelief = await post("/analyze-beliefs", {
    positions: [captureUpn], observer: "white", enemyKingKnown: true,
    depth: 3,
  });
  assert.ok(singletonBelief.pv.length > 1);
  assert.equal(singletonBelief.pvNotation.length, singletonBelief.pv.length);
  assert.equal(
    singletonBelief.publicPvNotation.length,
    singletonBelief.pv.length,
  );
});

test("opponent PV conditions hidden-Ghost worlds without exposing its location", async () => {
  const files = [..."abcdefgh"];
  const backRank = [
    "rook", "knight", "bishop", "queen",
    "king", "bishop", "knight", "rook",
  ];
  const pieces = [];
  for (const [index, file] of files.entries()) {
    pieces.push(
      `${backRank[index]},b,${file}10`,
      `pawn,b,${file}9`,
      `pawn,w,${file}2`,
      `${backRank[index]},w,${file}1`,
    );
  }
  pieces.push(
    "ghost,w,b3,0,0,0,0,0,0,-1,1,-1,0",
    "ghost,b,f8,0,0,0,0,0,0,-1,1,-1,0",
  );
  const initialUpn =
    `w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;${pieces.join(";")}`;
  const analysis = await post("/analyze-history", {
    clientId: "opponent-pv-hidden-ghost",
    initialUpn,
    moves: ["a2-a4"],
    observer: "white",
    enemyKingKnown: false,
    initialDeploymentKnown: false,
    depth: 2,
  });
  assert.equal(analysis.beliefs, 46);
  assert.deepEqual(analysis.pv, ["a9-a7", "b2-b4"]);
  assert.deepEqual(analysis.publicPvNotation, ["a7", "b4"]);
  assert.ok(analysis.pv.every((move) => !move.startsWith("f8-")));

  const hiddenGhostAnalysis = await post("/analyze-history", {
    clientId: "opponent-pv-hidden-ghost-later",
    initialUpn,
    moves: [
      "a2-a4", "a9-a7", "b2-b4", "b9-b7",
      "c2-c4", "c9-c7", "g2-g4",
    ],
    observer: "white",
    enemyKingKnown: false,
    initialDeploymentKnown: false,
    depth: 2,
  });
  assert.ok(
    hiddenGhostAnalysis.pv[0]?.startsWith("f8-"),
    JSON.stringify(hiddenGhostAnalysis),
  );
  assert.match(hiddenGhostAnalysis.pvNotation[0], /^GH[a-h](?:10|[1-9])$/);
  assert.equal(hiddenGhostAnalysis.publicPvNotation[0], "GH");
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

test("restarting history analysis cannot consume stale engine output", async () => {
  const initialUpn =
    "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,e1;queen,w,d1;pawn,w,a2;king,b,e10;queen,b,d10;pawn,b,a9;ghost,b,f8,0,0,0,0,0,0,-1,1,-1,0";
  const body = {
    clientId: "restart-race",
    initialUpn,
    moves: ["a2-a4"],
    observer: "white",
    enemyKingKnown: false,
    depth: 50,
    movetime: 600,
  };
  const controller = new AbortController();
  const interrupted = streamHistoryAnalysis(body, undefined, controller.signal);
  setTimeout(() => controller.abort(), 20);
  await assert.rejects(interrupted, { name: "AbortError" });

  const restarted = await streamHistoryAnalysis({
    ...body,
    depth: 2,
    movetime: 0,
  });
  assert.equal(restarted.at(-1)?.type, "result");
  assert.equal(restarted.at(-1)?.analysis.depth, 2);
  assert.equal(bridge?.exitCode ?? null, null);
});

test("same-position tabs search on independent engine sessions", async () => {
  const analysisUpn =
    "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,e1;jester,w,d1;ninja,w,b2;penguin,w,c2;devil,w,f2;sniper,w,g2;checker,w,h2;sludge,w,a2;king,b,e10;jester,b,d10;ninja,b,b9;penguin,b,c9;devil,b,f9;sniper,b,g9;checker,b,h9;sludge,b,a9";
  const timeline = [];
  const search = (clientId) => streamHistoryAnalysis({
    clientId,
    initialUpn: analysisUpn,
    moves: [],
    observer: "white",
    enemyKingKnown: true,
    depth: 50,
    movetime: 600,
  }, (event) => timeline.push({ clientId, type: event.type }));

  await Promise.all([search("parallel-tab-a"), search("parallel-tab-b")]);

  const firstResult = timeline.findIndex((event) => event.type === "result");
  assert.ok(firstResult > 0, "both streams should report search progress");
  const clientsMakingProgress = new Set(
    timeline.slice(0, firstResult)
      .filter((event) => event.type === "iteration")
      .map((event) => event.clientId),
  );
  assert.deepEqual(
    [...clientsMakingProgress].sort(),
    ["parallel-tab-a", "parallel-tab-b"],
    "one tab must not wait for the other tab's result before it starts",
  );
});

test("bridge retains uncapped beliefs without false Ghost legal-dot partitions", async () => {
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
  assert.equal(conservative.decisionPartitions, 1);
  assert.equal(conservative.decisionMode, "merged-conservative");
  assert.equal(conservative.beliefMode, "history-preserving");
  assert.equal(conservative.searchPath, "correlated-tuples");
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
  assert.equal(observed.beliefs, worlds.length);
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
