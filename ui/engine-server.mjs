import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { fileURLToPath } from "node:url";
import path from "node:path";

const uiDirectory = path.dirname(fileURLToPath(import.meta.url));
const engineBinary = process.env.ULTIMATE_FISH_BINARY
  ? path.resolve(process.env.ULTIMATE_FISH_BINARY)
  : path.resolve(uiDirectory, "../src/ultimatefish");
const port = Number(process.env.ULTIMATE_FISH_PORT ?? 3001);

function runEngine(commands, signal, onLine) {
  return new Promise((resolve, reject) => {
    const child = spawn(engineBinary, [], { stdio: ["pipe", "pipe", "pipe"] });
    const lines = [];
    let pending = "";
    let stderr = "";
    let settled = false;
    const finish = (callback, value) => {
      if (settled) return;
      settled = true;
      signal?.removeEventListener("abort", abort);
      callback(value);
    };
    const abort = () => {
      child.kill("SIGTERM");
      const error = new Error("Engine request cancelled");
      error.name = "AbortError";
      finish(reject, error);
    };
    if (signal?.aborted) { abort(); return; }
    signal?.addEventListener("abort", abort, { once: true });
    child.stdout.setEncoding("utf8");
    child.stderr.setEncoding("utf8");
    child.stdout.on("data", (chunk) => {
      pending += chunk;
      const complete = pending.split(/\r?\n/);
      pending = complete.pop() ?? "";
      for (const line of complete) {
        if (!line) continue;
        lines.push(line);
        onLine?.(line, lines);
      }
    });
    child.stderr.on("data", (chunk) => { stderr += chunk; });
    child.on("error", (error) => finish(reject, error));
    child.on("close", (code) => {
      if (pending) {
        lines.push(pending);
        onLine?.(pending, lines);
      }
      if (code !== 0) {
        finish(reject, new Error(stderr.trim() || `engine exited with status ${code}`));
        return;
      }
      finish(resolve, lines);
    });
    child.stdin.end(`${commands.join("\n")}\nquit\n`);
  });
}

function parseState(lines) {
  const upnLine = [...lines].reverse().find((line) => line.startsWith("upn ") || line.startsWith("position "));
  const movesLine = [...lines].reverse().find((line) => line === "moves" || line.startsWith("moves "));
  const materialLine = [...lines].reverse().find((line) => line.startsWith("material ")) ?? "";
  const material = materialLine.match(/^material white (\d+) black (\d+)$/);
  const resultLine = [...lines].reverse().find((line) => line.startsWith("result ")) ?? "result ongoing";
  const result = resultLine.match(/^result (ongoing|white|black|draw)(?: (.*))?$/);
  return {
    upn: upnLine?.replace(/^(?:upn|position) /, "") ?? null,
    moves: movesLine ? movesLine.slice(5).trim().split(/\s+/).filter(Boolean) : [],
    material: { white: material ? Number(material[1]) : 0, black: material ? Number(material[2]) : 0 },
    result: result?.[1] ?? "ongoing",
    resultReason: result?.[2] ?? null,
  };
}

async function state(upn, signal) {
  const lines = await runEngine([`position upn ${upn}`, "d"], signal);
  if (lines.some((line) => line.startsWith("info string invalid upn")))
    throw new Error(lines.find((line) => line.startsWith("info string invalid upn")));
  return parseState(lines);
}

async function applyMove(upn, move, signal) {
  const lines = await runEngine([`position upn ${upn}`, `move ${move}`], signal);
  if (lines.includes("illegalmove"))
    throw new Error(`Illegal move: ${move}`);
  const positionLine = lines.find((line) => line.startsWith("position "));
  if (!positionLine)
    throw new Error("Engine did not return a resulting position");
  return positionLine.slice(9);
}

function parseAnalysis(lines, infoLine) {
  const result = parseState(lines);
  const info = infoLine ?? [...lines].reverse().find((line) => line.startsWith("info depth ")) ?? "";
  const best = [...lines].reverse().find((line) => line.startsWith("bestmove "))?.slice(9) ?? null;
  const match = info.match(/^info depth (\d+) score (cp|mate) (-?\d+) nodes (\d+) time (\d+) pv(?: (.*))?$/);
  return {
    ...result,
    bestmove: best === "(none)" ? null : best,
    depth: match ? Number(match[1]) : 0,
    scoreType: match?.[2] ?? "cp",
    score: match ? Number(match[3]) : 0,
    nodes: match ? Number(match[4]) : 0,
    time: match ? Number(match[5]) : 0,
    pv: match?.[6]?.split(" ").filter(Boolean) ?? [],
  };
}

async function analyze(upn, requestedDepth, requestedTime, signal, maximumDepth = 50, onIteration) {
  const depth = Math.max(1, Math.min(maximumDepth, Number(requestedDepth) || 6));
  const moveTime = Math.max(0, Math.min(120_000, Number(requestedTime) || 0));
  const baseGo = moveTime ? `go depth ${depth} movetime ${moveTime}` : `go depth ${depth}`;
  const go = onIteration ? `${baseGo} stream` : baseGo;
  const lines = await runEngine([`position upn ${upn}`, "d", go], signal, (line, currentLines) => {
    if (line.startsWith("info depth ")) onIteration?.(parseAnalysis(currentLines, line));
  });
  return parseAnalysis(lines);
}

function beliefCommands(positions, observer, enemyKingKnown, legalMarkers) {
  if (!Array.isArray(positions) || positions.length === 0 ||
      positions.some((upn) => typeof upn !== "string" || upn.length > 20_000) ||
      positions.reduce((total, upn) => total + upn.length, 0) > 4_000_000)
    throw new Error("A nonempty belief-position array under 4 MB is required");
  if (observer !== "white" && observer !== "black")
    throw new Error("Belief observer must be white or black");
  if (typeof enemyKingKnown !== "boolean")
    throw new Error("enemyKingKnown must be a boolean");
  let observe = [];
  if (legalMarkers !== undefined) {
    if (!Array.isArray(legalMarkers) || legalMarkers.length > 512 ||
        legalMarkers.some((marker) => typeof marker !== "string" ||
          !/^(?:pass|[a-h](?:10|[1-9])>[a-h](?:10|[1-9]))$/.test(marker)))
      throw new Error("legalMarkers must contain source>destination or pass markers");
    const exact = [...new Set(legalMarkers)].sort();
    observe = [`belief observe${exact.length ? ` ${exact.join(" ")}` : ""}`];
  }
  return [
    "belief clear",
    `belief observer ${observer} ${enemyKingKnown ? 1 : 0}`,
    ...positions.map((upn) => `belief add ${upn}`),
    ...observe,
  ];
}

function beliefError(lines) {
  return lines.find((line) => line.startsWith("info string invalid belief"));
}

async function beliefState(positions, observer, enemyKingKnown, legalMarkers, signal) {
  const lines = await runEngine([
    ...beliefCommands(positions, observer, enemyKingKnown, legalMarkers),
    "belief count",
  ], signal);
  const error = beliefError(lines);
  if (error) throw new Error(error);
  const count = [...lines].reverse().find((line) => line.startsWith("beliefcount ")) ?? "";
  const match = count.match(/^beliefcount (\d+) observer (white|black) enemykingknown ([01]) mode (\S+)$/);
  if (!match) throw new Error("Engine did not return exact belief metadata");
  return {
    beliefs: Number(match[1]),
    observer: match[2],
    enemyKingKnown: match[3] === "1",
    mode: match[4],
  };
}

async function analyzeBeliefs(positions, observer, enemyKingKnown,
                              legalMarkers, requestedDepth, requestedTime, signal) {
  const depth = Math.max(1, Math.min(16, Number(requestedDepth) || 4));
  const moveTime = Math.max(0, Math.min(120_000, Number(requestedTime) || 0));
  const go = moveTime
    ? `belief go depth ${depth} movetime ${moveTime}`
    : `belief go depth ${depth}`;
  const lines = await runEngine([
    ...beliefCommands(positions, observer, enemyKingKnown, legalMarkers), go,
  ], signal);
  const error = beliefError(lines);
  if (error) throw new Error(error);
  const info = [...lines].reverse().find((line) => line.startsWith("info depth ")) ?? "";
  const match = info.match(
    /^info depth (\d+) score (cp|mate) (-?\d+) nodes (\d+) time (\d+) beliefs (\d+) deepbeliefs (\d+) common (\d+) candidates (\d+) beliefmode (\S+) historyplies (\d+) decisionmode (\S+) decisionpartitions (\d+) beliefworst (-?\d+) beliefmean (-?\d+) pv(?: (.*))?$/,
  );
  if (!match) throw new Error("Engine did not return belief analysis metadata");
  const best = [...lines].reverse().find((line) => line.startsWith("bestmove "))?.slice(9) ?? null;
  return {
    bestmove: best === "(none)" ? null : best,
    depth: Number(match[1]),
    scoreType: match[2],
    score: Number(match[3]),
    nodes: Number(match[4]),
    time: Number(match[5]),
    beliefs: Number(match[6]),
    deepBeliefs: Number(match[7]),
    commonMoves: Number(match[8]),
    candidates: Number(match[9]),
    beliefMode: match[10],
    historyPreservingPlies: Number(match[11]),
    decisionMode: match[12],
    decisionPartitions: Number(match[13]),
    worstScore: Number(match[14]),
    meanScore: Number(match[15]),
    pv: match[16]?.split(" ").filter(Boolean) ?? [],
    observer,
    enemyKingKnown,
  };
}

async function draftAuto(history, signal) {
  if (!Array.isArray(history) || history.length > 160 ||
      history.some((command) => typeof command !== "string" ||
        !/^(?:draft choose [A-Za-z]+|draft commit)$/.test(command)))
    throw new Error("Invalid draft history");
  const lines = await runEngine(["draft new", ...history, "draft auto", "draft status"], signal);
  const error = lines.find((line) => line.startsWith("info string draft error "));
  if (error) throw new Error(error.slice(24));
  const auto = lines.find((line) => line === "draftauto" || line.startsWith("draftauto "));
  const status = [...lines].reverse().find((line) => line.startsWith("draft phase ")) ?? "";
  return {
    choices: auto ? auto.slice(9).trim().split(/\s+/).filter(Boolean) : [],
    status,
  };
}

async function computerTurn(upn, player, requestedDepth, requestedTime, signal) {
  if (player !== "white" && player !== "black")
    throw new Error("Player side must be white or black");
  const playerCode = player === "white" ? "w" : "b";
  let current = upn;
  let currentState = await state(current, signal);
  let engine = null;
  const engineMoves = [];
  for (let action = 0; action < 16 && current[0] !== playerCode && currentState.result === "ongoing"; ++action) {
    engine = await analyze(current, requestedDepth, requestedTime, signal, 16);
    if (!engine.bestmove) break;
    engineMoves.push(engine.bestmove);
    current = await applyMove(current, engine.bestmove, signal);
    currentState = await state(current, signal);
  }
  return { ...currentState, engine, engineMoves };
}

function send(response, status, body) {
  response.writeHead(status, {
    "access-control-allow-origin": "*",
    "access-control-allow-headers": "content-type",
    "access-control-allow-methods": "GET,POST,OPTIONS",
    "content-type": "application/json; charset=utf-8",
  });
  response.end(JSON.stringify(body));
}

const server = createServer(async (request, response) => {
  const cancellation = new AbortController();
  request.on("aborted", () => cancellation.abort());
  response.on("close", () => { if (!response.writableEnded) cancellation.abort(); });
  if (request.method === "OPTIONS") {
    send(response, 204, {});
    return;
  }
  if (request.method === "GET" && request.url === "/health") {
    send(response, 200, { ok: true, engineBinary });
    return;
  }
  if (request.method !== "POST" || !["/state", "/analyze", "/analyze-stream", "/move", "/play", "/computer", "/draft-ai", "/belief-state", "/analyze-beliefs"].includes(request.url)) {
    send(response, 404, { error: "Not found" });
    return;
  }
  try {
    let raw = "";
    for await (const chunk of request)
      raw += chunk;
    if (raw.length > 5_000_000)
      throw new Error("Request is too large");
    const body = JSON.parse(raw || "{}");
    if (request.url === "/draft-ai") {
      send(response, 200, await draftAuto(body.history, cancellation.signal));
      return;
    }
    if (request.url === "/belief-state") {
      send(response, 200, await beliefState(
        body.positions, body.observer, body.enemyKingKnown,
        body.legalMarkers, cancellation.signal));
      return;
    }
    if (request.url === "/analyze-beliefs") {
      send(response, 200, await analyzeBeliefs(
        body.positions, body.observer, body.enemyKingKnown,
        body.legalMarkers, body.depth, body.movetime,
        cancellation.signal));
      return;
    }
    if (typeof body.upn !== "string" || body.upn.length > 20_000)
      throw new Error("A valid UPN string is required");

    if (request.url === "/state") {
      send(response, 200, await state(body.upn, cancellation.signal));
      return;
    }
    if (request.url === "/analyze") {
      send(response, 200, await analyze(body.upn, body.depth, body.movetime, cancellation.signal));
      return;
    }
    if (request.url === "/analyze-stream") {
      response.writeHead(200, {
        "access-control-allow-origin": "*",
        "access-control-allow-headers": "content-type",
        "content-type": "application/x-ndjson; charset=utf-8",
        "cache-control": "no-store",
      });
      const result = await analyze(body.upn, body.depth, body.movetime, cancellation.signal, 50,
        (iteration) => response.write(`${JSON.stringify({ type: "iteration", analysis: iteration })}\n`));
      response.end(`${JSON.stringify({ type: "result", analysis: result })}\n`);
      return;
    }
    if (request.url === "/move") {
      const upn = await applyMove(body.upn, String(body.move ?? ""), cancellation.signal);
      send(response, 200, await state(upn, cancellation.signal));
      return;
    }

    if (request.url === "/computer") {
      send(response, 200, await computerTurn(body.upn, body.player, body.depth, body.movetime, cancellation.signal));
      return;
    }

    const afterHuman = await applyMove(body.upn, String(body.move ?? ""), cancellation.signal);
    const afterState = await state(afterHuman, cancellation.signal);
    const playerCode = body.player === "black" ? "b" : "w";
    if (afterState.result !== "ongoing" || afterHuman[0] === playerCode) {
      send(response, 200, { ...afterState, engine: null, engineMoves: [] });
      return;
    }
    send(response, 200, await computerTurn(afterHuman, body.player, body.depth, body.movetime, cancellation.signal));
  } catch (error) {
    if (error instanceof Error && error.name === "AbortError") return;
    if (response.headersSent) {
      if (!response.writableEnded)
        response.end(`${JSON.stringify({ type: "error", error: error instanceof Error ? error.message : "Engine request failed" })}\n`);
      return;
    }
    send(response, 400, { error: error instanceof Error ? error.message : "Engine request failed" });
  }
});

server.listen(port, "127.0.0.1", () => {
  console.log(`Ultimate Fish engine bridge listening on http://127.0.0.1:${port}`);
  console.log(`Engine: ${engineBinary}`);
});
