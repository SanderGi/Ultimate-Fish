import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { fileURLToPath } from "node:url";
import path from "node:path";
import {
  MAX_ENGINE_SEARCH_DEPTH,
  normalizedSearchDepth,
} from "./engine-settings.mjs";

const uiDirectory = path.dirname(fileURLToPath(import.meta.url));
const engineBinary = process.env.ULTIMATE_FISH_BINARY
  ? path.resolve(process.env.ULTIMATE_FISH_BINARY)
  : path.resolve(uiDirectory, "../src/ultimatefish");
const pythonBinary = process.env.ULTIMATE_FISH_PYTHON ?? "python3";
const draftSearchScript = path.resolve(
  uiDirectory, "../tools/search_ultimate_public_draft.py",
);
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

function parsePieceKnowledge(lines) {
  const ghostKnowledge = {};
  const jesterKnowledge = {};
  for (const line of lines) {
    const ghost = line.match(
      /^knowledge ghost ([a-h](?:10|[1-9])) ([01]) (\S+)$/,
    );
    if (ghost) {
      ghostKnowledge[ghost[1]] = {
        known: ghost[2] === "1",
        candidates: ghost[3] === "-" ? [] : ghost[3].split(","),
      };
      continue;
    }
    const jester = line.match(
      /^knowledge jester ([a-h](?:10|[1-9])) ([01])$/,
    );
    if (jester) jesterKnowledge[jester[1]] = jester[2] === "1";
  }
  return { ghostKnowledge, jesterKnowledge };
}

async function state(upn, signal) {
  const lines = await runEngine([`position upn ${upn}`, "d"], signal);
  if (lines.some((line) => line.startsWith("info string invalid upn")))
    throw new Error(lines.find((line) => line.startsWith("info string invalid upn")));
  return parseState(lines);
}

async function applyMove(upn, move, signal) {
  const lines = await runEngine(
    [`position upn ${upn}`, `notation ${move}`, `move ${move}`, "d"],
    signal,
  );
  if (lines.includes("illegalmove"))
    throw new Error(`Illegal move: ${move}`);
  const positionLine = lines.find((line) => line.startsWith("position "));
  if (!positionLine)
    throw new Error("Engine did not return a resulting position");
  const notationLine = lines.find((line) => line.startsWith("notation ")) ?? "";
  const notation = notationLine.match(/^notation (\S+) public (\S+)$/);
  return {
    ...parseState(lines),
    // The move response is authoritative here. `parseState` normally gets the
    // same UPN from the trailing `d`, but retaining it makes this robust to an
    // engine build that reports only `position` after applying a move.
    upn: positionLine.slice(9),
    notation: notation?.[1] ?? move,
    publicNotation: notation?.[2] ?? notation?.[1] ?? move,
  };
}

async function replayPosition(initialUpn, moves, signal) {
  // Reuse the strict public-history input validation without constructing its
  // exponentially larger belief set. This path follows only the concrete
  // recorded game and is intended for immediate UI hydration.
  historyCommands(initialUpn, moves, "white", false, false);
  const commands = [`position upn ${initialUpn}`];
  for (const move of moves)
    commands.push(`notation ${move}`, `move ${move}`);
  commands.push("d");
  const lines = await runEngine(commands, signal);
  const invalid = lines.find((line) =>
    line.startsWith("info string invalid upn"));
  if (invalid) throw new Error(invalid);
  if (lines.includes("illegalmove"))
    throw new Error("Replayable belief contains an illegal concrete move");
  const positions = lines
    .filter((line) => line.startsWith("position "))
    .map((line) => line.slice(9));
  const notations = lines
    .filter((line) => line.startsWith("notation "))
    .map((line) => line.match(/^notation (\S+) public (\S+)$/));
  if (positions.length !== moves.length || notations.length !== moves.length ||
      notations.some((notation) => !notation))
    throw new Error("Engine did not return a complete concrete replay");
  let before = initialUpn;
  const history = moves.map((move, index) => {
    const fullmove = Number(before.match(/(?:^|;)fm=(\d+)/)?.[1] ?? 1);
    const notation = notations[index];
    const record = {
      color: before[0] === "b" ? "black" : "white",
      move,
      notation: notation[1],
      publicNotation: notation[2],
      upn: positions[index],
      moveNumber: fullmove,
      history: moves.slice(0, index + 1),
    };
    before = positions[index];
    return record;
  });
  return { ...parseState(lines), history };
}

function parseAnalysis(lines, infoLine) {
  const result = parseState(lines);
  const info = infoLine ?? [...lines].reverse().find((line) => line.startsWith("info depth ")) ?? "";
  const best = [...lines].reverse().find((line) => line.startsWith("bestmove "))?.slice(9) ?? null;
  const match = info.match(/^info depth (\d+) score (cp|mate) (-?\d+) nodes (\d+) time (\d+) pv(?: (.*))?$/);
  const display = [...lines].reverse().find((line) => line === "displaypv" || line.startsWith("displaypv "));
  const publicDisplay = [...lines].reverse().find((line) => line === "publicpv" || line.startsWith("publicpv "));
  const rawScore = match ? Number(match[3]) : 0;
  const ivoryScore = result.upn?.[0] === "b" ? -rawScore : rawScore;
  return {
    ...result,
    bestmove: best === "(none)" ? null : best,
    depth: match ? Number(match[1]) : 0,
    scoreType: match?.[2] ?? "cp",
    // Engine search is negamax and reports from the side-to-move perspective.
    // The bridge contract is stable: positive always favors Ivory.
    score: ivoryScore,
    nodes: match ? Number(match[4]) : 0,
    time: match ? Number(match[5]) : 0,
    pv: match?.[6]?.split(" ").filter(Boolean) ?? [],
    pvNotation: display?.slice(9).trim().split(/\s+/).filter(Boolean) ?? [],
    publicPvNotation: publicDisplay?.slice(8).trim().split(/\s+/).filter(Boolean) ?? [],
  };
}

async function analyze(
  upn,
  requestedDepth,
  requestedTime,
  signal,
  maximumDepth = MAX_ENGINE_SEARCH_DEPTH,
  onIteration,
) {
  const depth = normalizedSearchDepth(requestedDepth, 6, maximumDepth);
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

function parseBeliefAnalysis(lines, observer, enemyKingKnown) {
  const info = [...lines].reverse().find((line) => line.startsWith("info depth ")) ?? "";
  const display = [...lines].reverse()
    .find((line) => line === "displaypv" || line.startsWith("displaypv "));
  const publicDisplay = [...lines].reverse()
    .find((line) => line === "publicpv" || line.startsWith("publicpv "));
  const searchPath = [...lines].reverse()
    .find((line) => line.startsWith("info string searchpath "))
    ?.slice("info string searchpath ".length) ?? null;
  const match = info.match(
    /^info depth (\d+) score (cp|mate) (-?\d+) nodes (\d+) time (\d+) beliefs (\d+) deepbeliefs (\d+) common (\d+) candidates (\d+) beliefmode (\S+) historyplies (\d+) decisionmode (\S+) decisionpartitions (\d+) beliefworst (-?\d+) beliefmean (-?\d+) pv(?: (.*))?$/,
  );
  if (!match) throw new Error("Engine did not return belief analysis metadata");
  const best = [...lines].reverse().find((line) => line.startsWith("bestmove "))?.slice(9) ?? null;
  return {
    ...parseState(lines),
    ...parsePieceKnowledge(lines),
    bestmove: best === "(none)" ? null : best,
    depth: Number(match[1]),
    scoreType: match[2],
    // Information search reports from its observer's perspective. Normalize
    // that distinct engine convention to the same Ivory-relative UI contract.
    score: observer === "black" ? -Number(match[3]) : Number(match[3]),
    nodes: Number(match[4]),
    time: Number(match[5]),
    beliefs: Number(match[6]),
    deepBeliefs: Number(match[7]),
    commonMoves: Number(match[8]),
    candidates: Number(match[9]),
    searchPath,
    beliefMode: match[10],
    historyPreservingPlies: Number(match[11]),
    decisionMode: match[12],
    decisionPartitions: Number(match[13]),
    worstScore: Number(match[14]),
    meanScore: Number(match[15]),
    // Later plies can name one representative action from an opponent's
    // indistinguishable observation bucket. Only the observer's robust root
    // action is safe to expose as a concrete clickable line.
    pv: match[16]?.split(" ").filter(Boolean).slice(0, 1) ?? [],
    pvNotation: display?.slice(9).trim().split(/\s+/).filter(Boolean)
      .slice(0, 1) ?? [],
    publicPvNotation: publicDisplay?.slice(8).trim().split(/\s+/)
      .filter(Boolean).slice(0, 1) ?? [],
    observer,
    enemyKingKnown,
  };
}

async function beliefState(positions, observer, enemyKingKnown, legalMarkers, signal) {
  const lines = await runEngine([
    ...beliefCommands(positions, observer, enemyKingKnown, legalMarkers),
    "belief count",
    `belief knowledge ${positions[0]}`,
  ], signal);
  const error = beliefError(lines);
  if (error) throw new Error(error);
  const count = [...lines].reverse().find((line) => line.startsWith("beliefcount ")) ?? "";
  const match = count.match(/^beliefcount (\d+) observer (white|black) enemykingknown ([01]) mode (\S+)$/);
  if (!match) throw new Error("Engine did not return exact belief metadata");
  return {
    ...parsePieceKnowledge(lines),
    beliefs: Number(match[1]),
    observer: match[2],
    enemyKingKnown: match[3] === "1",
    mode: match[4],
  };
}

async function analyzeBeliefs(positions, observer, enemyKingKnown,
                              legalMarkers, requestedDepth, requestedTime, signal) {
  const depth = normalizedSearchDepth(requestedDepth);
  const moveTime = Math.max(0, Math.min(120_000, Number(requestedTime) || 0));
  const decisionMode = legalMarkers === undefined ? " conservative" : "";
  const go = moveTime
    ? `belief go${decisionMode} depth ${depth} movetime ${moveTime}`
    : `belief go${decisionMode} depth ${depth}`;
  const lines = await runEngine([
    ...beliefCommands(positions, observer, enemyKingKnown, legalMarkers),
    `belief knowledge ${positions[0]}`,
    go,
  ], signal);
  const error = beliefError(lines);
  if (error) throw new Error(error);
  return parseBeliefAnalysis(lines, observer, enemyKingKnown);
}

function historyCommands(initialUpn, moves, observer, enemyKingKnown,
                         initialDeploymentKnown, enemyKingCandidates) {
  if (typeof initialUpn !== "string" || initialUpn.length > 20_000 ||
      /[\r\n]/.test(initialUpn))
    throw new Error("A valid initial UPN string is required");
  if (!Array.isArray(moves) || moves.length > 2_000 ||
      moves.some((move) => typeof move !== "string" || move.length > 80 ||
        /[\r\n]/.test(move)))
    throw new Error("History moves must be a bounded string array");
  if (observer !== "white" && observer !== "black")
    throw new Error("History observer must be white or black");
  if (typeof enemyKingKnown !== "boolean")
    throw new Error("enemyKingKnown must be a boolean");
  if (typeof initialDeploymentKnown !== "boolean")
    throw new Error("initialDeploymentKnown must be a boolean");
  let candidateToken = "";
  if (enemyKingCandidates !== undefined && enemyKingCandidates !== null) {
    if (!Array.isArray(enemyKingCandidates) || enemyKingCandidates.length === 0 ||
        enemyKingCandidates.length > 96 ||
        enemyKingCandidates.some((square) => typeof square !== "string" ||
          !/^[a-h](?:10|[1-9])$/.test(square)) ||
        new Set(enemyKingCandidates).size !== enemyKingCandidates.length)
      throw new Error("enemyKingCandidates must be distinct board squares");
    candidateToken = ` kc=${enemyKingCandidates.join(",")}`;
  }
  return [
    `history start ${observer} ${enemyKingKnown ? 1 : 0} ${initialDeploymentKnown ? 1 : 0}${candidateToken} ${initialUpn}`,
    ...moves.map((move) => `history move ${move}`),
  ];
}

function historyError(lines) {
  return lines.find((line) => line.startsWith("info string invalid public history"));
}

function parseHistoryMetadata(lines) {
  const count = [...lines].reverse().find((line) => line.startsWith("historycount ")) ?? "";
  const match = count.match(
    /^historycount (\d+) observer (white|black) enemykingknown ([01]) decisionpartitions (\d+) royalknown ([01]) kingcandidates (\S+)$/,
  );
  if (!match) throw new Error("Engine did not return public-history metadata");
  return {
    ...parsePieceKnowledge(lines),
    beliefs: Number(match[1]),
    observer: match[2],
    disclosureEnemyKingKnown: match[3] === "1",
    decisionPartitions: Number(match[4]),
    enemyKingKnown: match[5] === "1",
    enemyKingCandidates: match[6] === "-" ? [] : match[6].split(","),
  };
}

const historyStateCache = new Map();
const incrementalHistorySessions = new Map();

class IncrementalHistorySession {
  constructor(initialUpn, observer, enemyKingKnown, initialDeploymentKnown,
              enemyKingCandidates) {
    this.initialUpn = initialUpn;
    this.observer = observer;
    this.enemyKingKnown = enemyKingKnown;
    this.initialDeploymentKnown = initialDeploymentKnown;
    this.enemyKingCandidates = enemyKingCandidates;
    this.moves = [];
    this.queue = Promise.resolve();
    this.child = null;
    this.pending = "";
    this.active = null;
    this.preparedMoves = null;
  }

  stop(error = new Error("Incremental history session stopped")) {
    const active = this.active;
    this.active = null;
    active?.reject(error);
    this.child?.kill("SIGTERM");
    this.child = null;
  }

  start() {
    this.stop();
    this.pending = "";
    const child = spawn(engineBinary, [], { stdio: ["pipe", "pipe", "pipe"] });
    this.child = child;
    child.stdout.setEncoding("utf8");
    child.stderr.setEncoding("utf8");
    child.stdout.on("data", (chunk) => {
      this.pending += chunk;
      const complete = this.pending.split(/\r?\n/);
      this.pending = complete.pop() ?? "";
      for (const line of complete) {
        if (line === "readyok") {
          const active = this.active;
          this.active = null;
          active?.resolve(active.lines);
        } else if (line) {
          this.active?.lines.push(line);
          this.active?.onLine?.(line, this.active.lines);
        }
      }
    });
    let stderr = "";
    child.stderr.on("data", (chunk) => { stderr += chunk; });
    const fail = (error) => {
      if (child !== this.child) return;
      this.child = null;
      const active = this.active;
      this.active = null;
      active?.reject(error instanceof Error ? error : new Error(
        stderr.trim() || "incremental history engine exited"));
    };
    child.on("error", fail);
    child.on("close", (code) => {
      fail(new Error(
        stderr.trim() || `engine exited with status ${code ?? "unknown"}`));
    });
  }

  transact(commands, signal, onLine) {
    if (!this.child) this.start();
    return new Promise((resolve, reject) => {
      const abort = () => {
        const error = new Error("Engine request cancelled");
        error.name = "AbortError";
        this.stop(error);
      };
      const cleanup = () => signal?.removeEventListener("abort", abort);
      this.active = {
        lines: [], onLine,
        resolve: (lines) => { cleanup(); resolve(lines); },
        reject: (error) => { cleanup(); reject(error); },
      };
      if (signal?.aborted) {
        abort();
        return;
      }
      signal?.addEventListener("abort", abort, { once: true });
      this.child.stdin.write(`${commands.join("\n")}\nisready\n`);
    });
  }

  execute(requestedMoves, tailCommands, signal, onLine) {
    const operation = this.queue.then(async () => {
      historyCommands(
        this.initialUpn, requestedMoves, this.observer, this.enemyKingKnown,
        this.initialDeploymentKnown, this.enemyKingCandidates,
      );
      const extendsCurrent = this.moves.length <= requestedMoves.length &&
        this.moves.every((move, index) => requestedMoves[index] === move);
      const commands = [];
      if (!extendsCurrent || !this.child) {
        this.start();
        commands.push(...historyCommands(
          this.initialUpn, [], this.observer, this.enemyKingKnown,
          this.initialDeploymentKnown, this.enemyKingCandidates,
        ));
        this.moves = [];
      }
      for (const move of requestedMoves.slice(this.moves.length))
        commands.push(`history move ${move}`);
      commands.push(...tailCommands);
      const lines = await this.transact(commands, signal, onLine);
      const error = historyError(lines);
      if (error) throw new Error(error);
      if (this.moves.length !== requestedMoves.length ||
          this.moves.some((move, index) => requestedMoves[index] !== move))
        this.preparedMoves = null;
      this.moves = [...requestedMoves];
      return lines;
    });
    this.queue = operation.catch(() => undefined);
    return operation;
  }

  async state(requestedMoves) {
    const lines = await this.execute(
      requestedMoves, ["history count", "history knowledge", "history d"]);
    return {
        ...parseState(lines),
        ...parseHistoryMetadata(lines),
        decisionMode: "exact-cell",
        beliefMode: "history-preserving",
    };
  }

  search(requestedMoves, command, signal, onLine) {
    return this.execute(requestedMoves, [command], signal, onLine);
  }

  prepare(requestedMoves) {
    const key = JSON.stringify(requestedMoves);
    const operation = this.queue.then(async () => {
      if (this.preparedMoves === key ||
          this.moves.length !== requestedMoves.length ||
          this.moves.some((move, index) => requestedMoves[index] !== move))
        return;
      const lines = await this.transact(["history prepare"]);
      const error = historyError(lines);
      if (error) throw new Error(error);
      this.preparedMoves = key;
    });
    this.queue = operation.catch(() => undefined);
    return operation;
  }
}

function historySession(initialUpn, observer, enemyKingKnown,
                        initialDeploymentKnown, enemyKingCandidates) {
  const sessionKey = JSON.stringify([
    initialUpn, observer, enemyKingKnown, initialDeploymentKnown,
    enemyKingCandidates ?? null,
  ]);
  let session = incrementalHistorySessions.get(sessionKey);
  if (!session) {
    session = new IncrementalHistorySession(
      initialUpn, observer, enemyKingKnown, initialDeploymentKnown,
      enemyKingCandidates,
    );
    incrementalHistorySessions.set(sessionKey, session);
    if (incrementalHistorySessions.size > 8) {
      const oldest = incrementalHistorySessions.keys().next().value;
      if (oldest && oldest !== sessionKey) {
        incrementalHistorySessions.get(oldest)?.stop();
        incrementalHistorySessions.delete(oldest);
      }
    }
  }
  return session;
}

function incrementalHistoryState(initialUpn, moves, observer, enemyKingKnown,
                                 initialDeploymentKnown, enemyKingCandidates) {
  return historySession(
    initialUpn, observer, enemyKingKnown, initialDeploymentKnown,
    enemyKingCandidates,
  ).state(moves);
}

async function cachedHistoryState(initialUpn, moves, observer, enemyKingKnown,
                                  initialDeploymentKnown, enemyKingCandidates) {
  const key = JSON.stringify([
    initialUpn, moves, observer, enemyKingKnown, initialDeploymentKnown,
    enemyKingCandidates ?? null,
  ]);
  let pending = historyStateCache.get(key);
  if (!pending) {
    pending = incrementalHistoryState(
      initialUpn, moves, observer, enemyKingKnown, initialDeploymentKnown,
      enemyKingCandidates,
    );
    historyStateCache.set(key, pending);
    if (historyStateCache.size > 24) {
      const oldest = historyStateCache.keys().next().value;
      if (oldest !== key) historyStateCache.delete(oldest);
    }
    pending.catch(() => historyStateCache.delete(key));
  }
  return pending;
}

function prepareHistoryIfOpponentMoves(
  state, initialUpn, moves, observer, enemyKingKnown,
  initialDeploymentKnown, enemyKingCandidates,
) {
  const observerCode = observer === "white" ? "w" : "b";
  if (!state.upn || state.upn[0] === observerCode || state.result !== "ongoing")
    return;
  // Prepared partitions retain successor worlds. Bound speculative memory on
  // extreme information sets; those still use the exact on-demand path.
  if (!Number.isFinite(state.beliefs) || state.beliefs > 20_000)
    return;
  void historySession(
    initialUpn, observer, enemyKingKnown, initialDeploymentKnown,
    enemyKingCandidates,
  ).prepare(moves).catch(() => undefined);
}

async function analyzeHistory(initialUpn, moves, observer, enemyKingKnown,
                              initialDeploymentKnown, enemyKingCandidates,
                              requestedDepth,
                              requestedTime, signal, onIteration) {
  const depth = normalizedSearchDepth(requestedDepth);
  const moveTime = Math.max(0, Math.min(120_000, Number(requestedTime) || 0));
  const baseGo = moveTime
    ? `history go depth ${depth} movetime ${moveTime}`
    : `history go depth ${depth}`;
  const go = onIteration ? `${baseGo} stream` : baseGo;
  // Build or advance the information set once, then search that exact in-memory
  // state in the same persistent engine. Spawning a fresh process here used to
  // replay the entire history around every nominally short search.
  const state = await cachedHistoryState(
    initialUpn, moves, observer, enemyKingKnown, initialDeploymentKnown,
    enemyKingCandidates,
  );
  const session = historySession(
    initialUpn, observer, enemyKingKnown, initialDeploymentKnown,
    enemyKingCandidates,
  );
  const lines = await session.search(moves, go, signal, (line, currentLines) => {
    if (line.startsWith("bestmove "))
      onIteration?.({
        ...parseBeliefAnalysis(currentLines, observer, enemyKingKnown),
        ...state,
      });
  });
  const error = historyError(lines);
  if (error) throw new Error(error);
  return {
    ...parseBeliefAnalysis(lines, observer, enemyKingKnown),
    ...state,
  };
}

async function draftAuto(history, player, requestedDepth, requestedTimeLimit, signal) {
  if (!Array.isArray(history) || history.length > 160 ||
      history.some((command) => typeof command !== "string" ||
        !/^(?:draft choose [A-Za-z]+|draft commit)$/.test(command)))
    throw new Error("Invalid draft history");
  if (player !== "white" && player !== "black")
    throw new Error("Draft player must be white or black");
  const depth = normalizedSearchDepth(requestedDepth, 4, 16);
  const timeLimit = Math.max(
    0.5, Math.min(20, Number(requestedTimeLimit) || 6),
  );
  return new Promise((resolve, reject) => {
    const child = spawn(pythonBinary, [draftSearchScript], {
      stdio: ["pipe", "pipe", "pipe"],
    });
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
      const error = new Error("Draft search cancelled");
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
      let result;
      try { result = JSON.parse(stdout); }
      catch { result = null; }
      if (code !== 0 || !result || result.error) {
        finish(reject, new Error(
          (result?.error ?? stderr.trim()) ||
          `draft search exited with status ${code}`,
        ));
        return;
      }
      finish(resolve, result);
    });
    child.stdin.end(JSON.stringify({
      history, player, depth, timeLimit, engine: engineBinary,
    }));
  });
}

async function computerTurn(upn, player, requestedDepth, requestedTime, signal) {
  if (player !== "white" && player !== "black")
    throw new Error("Player side must be white or black");
  const playerCode = player === "white" ? "w" : "b";
  let current = upn;
  let currentState = await state(current, signal);
  let engine = null;
  const engineMoves = [];
  const engineNotations = [];
  const publicEngineNotations = [];
  for (let action = 0; action < 16 && current[0] !== playerCode && currentState.result === "ongoing"; ++action) {
    engine = await analyze(current, requestedDepth, requestedTime, signal, 16);
    if (!engine.bestmove) break;
    engineMoves.push(engine.bestmove);
    const applied = await applyMove(current, engine.bestmove, signal);
    engineNotations.push(applied.notation);
    publicEngineNotations.push(applied.publicNotation);
    current = applied.upn;
    currentState = applied;
  }
  return { ...currentState, engine, engineMoves, engineNotations, publicEngineNotations };
}

async function computerHistory(
  initialUpn, moves, player, playerEnemyKingKnown,
  initialDeploymentKnown, playerEnemyKingCandidates,
  engineEnemyKingKnown, engineEnemyKingCandidates, requestedDepth,
                               requestedTime, signal) {
  if (player !== "white" && player !== "black")
    throw new Error("Player side must be white or black");
  const playerCode = player === "white" ? "w" : "b";
  const observer = player === "white" ? "black" : "white";
  const engineKnown = typeof engineEnemyKingKnown === "boolean"
    ? engineEnemyKingKnown : playerEnemyKingKnown;
  const engineCandidates = engineEnemyKingCandidates ??
    playerEnemyKingCandidates;
  const history = [...moves];
  let currentState = await cachedHistoryState(
    initialUpn, history, observer, engineKnown,
    initialDeploymentKnown, engineCandidates);
  let engine = null;
  const engineMoves = [];
  const engineNotations = [];
  const publicEngineNotations = [];
  const engineUpns = [];
  let concreteState = currentState;
  for (let action = 0;
       action < 16 && concreteState.upn?.[0] !== playerCode &&
         concreteState.result === "ongoing";
       ++action) {
    engine = await analyzeHistory(
      initialUpn, history, observer, engineKnown,
      initialDeploymentKnown, engineCandidates, requestedDepth,
      requestedTime, signal);
    if (!engine.bestmove || !currentState.upn) break;
    engineMoves.push(engine.bestmove);
    const applied = await applyMove(
      currentState.upn, engine.bestmove, signal);
    engineNotations.push(applied.notation);
    publicEngineNotations.push(applied.publicNotation);
    engineUpns.push(applied.upn);
    concreteState = applied;
    history.push(engine.bestmove);
    // Continuations require the exact next engine information set before a
    // second action can be selected. Once the turn passes back to the player,
    // the concrete public move is ready and belief hydration can finish in the
    // background instead of delaying its board animation.
    if (concreteState.upn?.[0] !== playerCode &&
        concreteState.result === "ongoing")
      currentState = await cachedHistoryState(
        initialUpn, history, observer, engineKnown,
        initialDeploymentKnown, engineCandidates);
  }
  if (engine && publicEngineNotations.length)
    engine = {
      ...engine,
      publicPvNotation: [publicEngineNotations.at(-1)],
    };
  if (engineMoves.length) {
    void cachedHistoryState(
      initialUpn, history, observer, engineKnown,
      initialDeploymentKnown, engineCandidates).catch(() => undefined);
    void cachedHistoryState(
      initialUpn, history, player, playerEnemyKingKnown,
      initialDeploymentKnown, playerEnemyKingCandidates).catch(() => undefined);
    return {
      ...concreteState, engine, engineMoves, engineNotations,
      publicEngineNotations, engineUpns, knowledgePending: true,
    };
  }
  // No engine action was needed, so retain the exact player metadata contract.
  const playerState = await cachedHistoryState(
    initialUpn, history, player, playerEnemyKingKnown,
    initialDeploymentKnown, playerEnemyKingCandidates);
  return {
    ...playerState, engine, engineMoves, engineNotations,
    publicEngineNotations, engineUpns,
  };
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
  if (request.method !== "POST" || !["/state", "/analyze", "/analyze-stream", "/move", "/replay", "/play", "/computer", "/draft-ai", "/belief-state", "/analyze-beliefs", "/history-state", "/analyze-history", "/analyze-history-stream", "/computer-history"].includes(request.url)) {
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
      send(response, 200, await draftAuto(
        body.history, body.player, body.depth, body.timeLimit,
        cancellation.signal));
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
    if (request.url === "/history-state") {
      const prefetch = body.prefetch;
      const validPrefetch = prefetch &&
        (prefetch.observer === "white" || prefetch.observer === "black") &&
        typeof prefetch.enemyKingKnown === "boolean";
      // Start the opposite observer from this same browser request. The
      // visible board was already hydrated by /replay, so both information
      // sets can use the idle interval without exposing a duplicate fetch.
      const prefetchedState = validPrefetch ? cachedHistoryState(
        body.initialUpn, body.moves, prefetch.observer,
        prefetch.enemyKingKnown, body.initialDeploymentKnown ?? false,
        prefetch.enemyKingCandidates,
      ) : null;
      const result = await cachedHistoryState(
        body.initialUpn, body.moves, body.observer, body.enemyKingKnown,
        body.initialDeploymentKnown ?? false, body.enemyKingCandidates);
      send(response, 200, result);
      prepareHistoryIfOpponentMoves(
        result, body.initialUpn, body.moves, body.observer,
        body.enemyKingKnown, body.initialDeploymentKnown ?? false,
        body.enemyKingCandidates,
      );
      if (prefetchedState)
        void prefetchedState.then((prefetched) => prepareHistoryIfOpponentMoves(
          prefetched, body.initialUpn, body.moves, prefetch.observer,
          prefetch.enemyKingKnown, body.initialDeploymentKnown ?? false,
          prefetch.enemyKingCandidates,
        )).catch(() => undefined);
      return;
    }
    if (request.url === "/replay") {
      send(response, 200, await replayPosition(
        body.initialUpn, body.moves, cancellation.signal));
      return;
    }
    if (request.url === "/analyze-history-stream") {
      response.writeHead(200, {
        "access-control-allow-origin": "*",
        "access-control-allow-headers": "content-type",
        "content-type": "application/x-ndjson; charset=utf-8",
        "cache-control": "no-store",
      });
      const result = await analyzeHistory(
        body.initialUpn, body.moves, body.observer, body.enemyKingKnown,
        body.initialDeploymentKnown ?? false, body.enemyKingCandidates,
        body.depth, body.movetime,
        cancellation.signal,
        (iteration) => response.write(`${JSON.stringify({ type: "iteration", analysis: iteration })}\n`),
      );
      response.end(`${JSON.stringify({ type: "result", analysis: result })}\n`);
      return;
    }
    if (request.url === "/analyze-history") {
      send(response, 200, await analyzeHistory(
        body.initialUpn, body.moves, body.observer, body.enemyKingKnown,
        body.initialDeploymentKnown ?? false, body.enemyKingCandidates,
        body.depth, body.movetime,
        cancellation.signal));
      return;
    }
    if (request.url === "/computer-history") {
      send(response, 200, await computerHistory(
        body.initialUpn, body.moves, body.player, body.enemyKingKnown,
        body.initialDeploymentKnown ?? false, body.enemyKingCandidates,
        body.engineEnemyKingKnown, body.engineEnemyKingCandidates,
        body.depth, body.movetime,
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
      const applied = await applyMove(body.upn, String(body.move ?? ""), cancellation.signal);
      send(response, 200, applied);
      return;
    }

    if (request.url === "/computer") {
      send(response, 200, await computerTurn(body.upn, body.player, body.depth, body.movetime, cancellation.signal));
      return;
    }

    const appliedHuman = await applyMove(body.upn, String(body.move ?? ""), cancellation.signal);
    const afterHuman = appliedHuman.upn;
    const afterState = appliedHuman;
    const playerCode = body.player === "black" ? "b" : "w";
    if (afterState.result !== "ongoing" || afterHuman[0] === playerCode) {
      send(response, 200, { ...afterState, notation: appliedHuman.notation,
        publicNotation: appliedHuman.publicNotation, engine: null, engineMoves: [],
        engineNotations: [], publicEngineNotations: [] });
      return;
    }
    send(response, 200, { ...await computerTurn(afterHuman, body.player, body.depth, body.movetime, cancellation.signal),
      notation: appliedHuman.notation, publicNotation: appliedHuman.publicNotation });
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

function stopIncrementalHistorySessions() {
  for (const session of incrementalHistorySessions.values()) session.stop();
  incrementalHistorySessions.clear();
}

process.once("exit", stopIncrementalHistorySessions);
for (const signal of ["SIGINT", "SIGTERM"])
  process.once(signal, () => {
    stopIncrementalHistorySessions();
    server.close(() => process.exit(0));
  });
