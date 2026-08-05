import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { fileURLToPath } from "node:url";
import path from "node:path";

const uiDirectory = path.dirname(fileURLToPath(import.meta.url));
const engineBinary = process.env.ULTIMATE_FISH_BINARY
  ? path.resolve(process.env.ULTIMATE_FISH_BINARY)
  : path.resolve(uiDirectory, "../src/ultimatefish");
const port = Number(process.env.ULTIMATE_FISH_PORT ?? 3001);

function runEngine(commands, signal) {
  return new Promise((resolve, reject) => {
    const child = spawn(engineBinary, [], { stdio: ["pipe", "pipe", "pipe"] });
    let stdout = "";
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
    child.stdout.on("data", (chunk) => { stdout += chunk; });
    child.stderr.on("data", (chunk) => { stderr += chunk; });
    child.on("error", (error) => finish(reject, error));
    child.on("close", (code) => {
      if (code !== 0) {
        finish(reject, new Error(stderr.trim() || `engine exited with status ${code}`));
        return;
      }
      finish(resolve, stdout.split(/\r?\n/).filter(Boolean));
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

async function analyze(upn, requestedDepth, requestedTime, signal) {
  const depth = Math.max(1, Math.min(16, Number(requestedDepth) || 6));
  const moveTime = Math.max(0, Math.min(120_000, Number(requestedTime) || 0));
  const go = moveTime ? `go depth ${depth} movetime ${moveTime}` : `go depth ${depth}`;
  const lines = await runEngine([`position upn ${upn}`, "d", go], signal);
  const result = parseState(lines);
  const info = lines.find((line) => line.startsWith("info depth ")) ?? "";
  const best = lines.find((line) => line.startsWith("bestmove "))?.slice(9) ?? null;
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
    engine = await analyze(current, requestedDepth, requestedTime, signal);
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
  if (request.method !== "POST" || !["/state", "/analyze", "/move", "/play", "/computer", "/draft-ai"].includes(request.url)) {
    send(response, 404, { error: "Not found" });
    return;
  }
  try {
    let raw = "";
    for await (const chunk of request)
      raw += chunk;
    if (raw.length > 100_000)
      throw new Error("Request is too large");
    const body = JSON.parse(raw || "{}");
    if (request.url === "/draft-ai") {
      send(response, 200, await draftAuto(body.history, cancellation.signal));
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
    send(response, 400, { error: error instanceof Error ? error.message : "Engine request failed" });
  }
});

server.listen(port, "127.0.0.1", () => {
  console.log(`Ultimate Fish engine bridge listening on http://127.0.0.1:${port}`);
  console.log(`Engine: ${engineBinary}`);
});
