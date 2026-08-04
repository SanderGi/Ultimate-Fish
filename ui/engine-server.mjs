import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { fileURLToPath } from "node:url";
import path from "node:path";

const uiDirectory = path.dirname(fileURLToPath(import.meta.url));
const engineBinary = process.env.ULTIMATE_FISH_BINARY
  ? path.resolve(process.env.ULTIMATE_FISH_BINARY)
  : path.resolve(uiDirectory, "../src/ultimatefish");
const port = Number(process.env.ULTIMATE_FISH_PORT ?? 3001);

function runEngine(commands) {
  return new Promise((resolve, reject) => {
    const child = spawn(engineBinary, [], { stdio: ["pipe", "pipe", "pipe"] });
    let stdout = "";
    let stderr = "";
    child.stdout.setEncoding("utf8");
    child.stderr.setEncoding("utf8");
    child.stdout.on("data", (chunk) => { stdout += chunk; });
    child.stderr.on("data", (chunk) => { stderr += chunk; });
    child.on("error", reject);
    child.on("close", (code) => {
      if (code !== 0) {
        reject(new Error(stderr.trim() || `engine exited with status ${code}`));
        return;
      }
      resolve(stdout.split(/\r?\n/).filter(Boolean));
    });
    child.stdin.end(`${commands.join("\n")}\nquit\n`);
  });
}

function parseState(lines) {
  const upnLine = [...lines].reverse().find((line) => line.startsWith("upn ") || line.startsWith("position "));
  const movesLine = [...lines].reverse().find((line) => line === "moves" || line.startsWith("moves "));
  return {
    upn: upnLine?.replace(/^(?:upn|position) /, "") ?? null,
    moves: movesLine ? movesLine.slice(5).trim().split(/\s+/).filter(Boolean) : [],
  };
}

async function state(upn) {
  const lines = await runEngine([`position upn ${upn}`, "d"]);
  if (lines.some((line) => line.startsWith("info string invalid upn")))
    throw new Error(lines.find((line) => line.startsWith("info string invalid upn")));
  return parseState(lines);
}

async function applyMove(upn, move) {
  const lines = await runEngine([`position upn ${upn}`, `move ${move}`]);
  if (lines.includes("illegalmove"))
    throw new Error(`Illegal move: ${move}`);
  const positionLine = lines.find((line) => line.startsWith("position "));
  if (!positionLine)
    throw new Error("Engine did not return a resulting position");
  return positionLine.slice(9);
}

async function analyze(upn, requestedDepth, requestedTime) {
  const depth = Math.max(1, Math.min(16, Number(requestedDepth) || 6));
  const moveTime = Math.max(0, Math.min(120_000, Number(requestedTime) || 0));
  const go = moveTime ? `go depth ${depth} movetime ${moveTime}` : `go depth ${depth}`;
  const lines = await runEngine([`position upn ${upn}`, "d", go]);
  const result = parseState(lines);
  const info = lines.find((line) => line.startsWith("info depth ")) ?? "";
  const best = lines.find((line) => line.startsWith("bestmove "))?.slice(9) ?? null;
  const match = info.match(/^info depth (\d+) score cp (-?\d+) nodes (\d+) time (\d+) pv(?: (.*))?$/);
  return {
    ...result,
    bestmove: best === "(none)" ? null : best,
    depth: match ? Number(match[1]) : 0,
    score: match ? Number(match[2]) : 0,
    nodes: match ? Number(match[3]) : 0,
    time: match ? Number(match[4]) : 0,
    pv: match?.[5]?.split(" ").filter(Boolean) ?? [],
  };
}

function send(response, status, body) {
  response.writeHead(status, {
    "access-control-allow-origin": "http://localhost:3000",
    "access-control-allow-headers": "content-type",
    "access-control-allow-methods": "GET,POST,OPTIONS",
    "content-type": "application/json; charset=utf-8",
  });
  response.end(JSON.stringify(body));
}

const server = createServer(async (request, response) => {
  if (request.method === "OPTIONS") {
    send(response, 204, {});
    return;
  }
  if (request.method === "GET" && request.url === "/health") {
    send(response, 200, { ok: true, engineBinary });
    return;
  }
  if (request.method !== "POST" || !["/state", "/analyze", "/move", "/play"].includes(request.url)) {
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
    if (typeof body.upn !== "string" || body.upn.length > 20_000)
      throw new Error("A valid UPN string is required");

    if (request.url === "/state") {
      send(response, 200, await state(body.upn));
      return;
    }
    if (request.url === "/analyze") {
      send(response, 200, await analyze(body.upn, body.depth, body.movetime));
      return;
    }
    if (request.url === "/move") {
      const upn = await applyMove(body.upn, String(body.move ?? ""));
      send(response, 200, await state(upn));
      return;
    }

    const afterHuman = await applyMove(body.upn, String(body.move ?? ""));
    const engine = await analyze(afterHuman, body.depth, body.movetime);
    const finalUpn = engine.bestmove ? await applyMove(afterHuman, engine.bestmove) : afterHuman;
    const finalState = await state(finalUpn);
    send(response, 200, { ...finalState, engine });
  } catch (error) {
    send(response, 400, { error: error instanceof Error ? error.message : "Engine request failed" });
  }
});

server.listen(port, "127.0.0.1", () => {
  console.log(`Ultimate Fish engine bridge listening on http://127.0.0.1:${port}`);
  console.log(`Engine: ${engineBinary}`);
});
