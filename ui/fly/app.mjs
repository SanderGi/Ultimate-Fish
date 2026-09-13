import { flyScene, brainScene } from "./scenes.mjs";
import { createReferee } from "./referee.mjs";
const $ = (id) => document.getElementById(id);
const visual = {
  paused: matchMedia("(prefers-reduced-motion: reduce)").matches,
  thinking: false,
  trace: null,
  traceStart: 0,
  celebrateUntil: 0,
};
let state = null,
  selected = null,
  human = 0,
  generation = 0,
  busy = false,
  abort = null,
  lastMove = null,
  turnStarts = [];
const symbols = new Map();
const referee = createReferee();
const squareName = (n) => "abcdefgh"[n % 8] + (Math.floor(n / 8) + 1);
const title = (s) => s.charAt(0).toUpperCase() + s.slice(1);
function setMood(text) {
  const label = $("fly-mood");
  if (label) label.textContent = text;
}
function status(text) {
  $("status").textContent = text;
}
function resetBrainDisplay() {
  visual.trace = null;
  visual.thinking = false;
  $("brain-phase").textContent = "Waiting for a move";
  $("brain-key").textContent = "Dim points show measured anatomy";
  $("activity-count").textContent = "—";
  $("think-time").textContent = "—";
}
function render() {
  if (!state) return;
  const fragment = document.createDocumentFragment();
  const order = Array.from({ length: 80 }, (_, i) =>
    human
      ? Math.floor(i / 8) * 8 + 7 - (i % 8)
      : (9 - Math.floor(i / 8)) * 8 + (i % 8),
  );
  for (const square of order) {
    const p = state.pieces.find((p) => p.square === square),
      moves = state.moves.filter((m) => m.from === selected && m.to === square);
    const button = document.createElement("button");
    button.type = "button";
    button.className =
      "square" +
      ((Math.floor(square / 8) + (square % 8)) % 2 ? "" : " dark") +
      (p ? " occupied" : "") +
      (selected === square ? " selected" : "") +
      (moves.length ? " legal" : "") +
      (lastMove && (square === lastMove.from || square === lastMove.to)
        ? " last"
        : "");
    button.dataset.square = square;
    button.setAttribute(
      "aria-label",
      `${squareName(square)}${p ? `: ${p.color ? "Black" : "White"} ${p.type}${p.cooldown ? `, cooldown ${p.cooldown}` : ""}${p.frozen ? ", frozen" : ""}` : ", empty"}${moves.length ? ", legal destination" : ""}`,
    );
    button.setAttribute("aria-pressed", String(selected === square));
    if (p) {
      const ns = "http://www.w3.org/2000/svg";
      const icon = document.createElementNS(ns, "svg");
      icon.setAttribute("viewBox", "0 0 64 64");
      icon.setAttribute("class", `piece-icon ${p.color ? "black" : "white"}`);
      icon.setAttribute("aria-hidden", "true");
      const symbol = symbols.get(p.type.toLowerCase());
      if (symbol)
        for (const node of symbol.children) icon.append(node.cloneNode(true));
      button.append(icon);
      if (p.cooldown) {
        const c = document.createElement("span");
        c.className = "cooldown";
        c.textContent = p.cooldown;
        button.append(c);
      }
      if (p.frozen) {
        const f = document.createElement("span");
        f.className = "frozen";
        f.textContent = "❄";
        button.append(f);
      }
    }
    const coord = document.createElement("span");
    coord.className = "coordinate";
    coord.textContent = squareName(square);
    button.append(coord);
    button.onclick = () => onSquare(square);
    button.onkeydown = (e) => {
      const delta = { ArrowLeft: -1, ArrowRight: 1, ArrowUp: -8, ArrowDown: 8 }[
        e.key
      ];
      if (delta) {
        e.preventDefault();
        const current = order.indexOf(square);
        const next = order[Math.max(0, Math.min(79, current + delta))];
        $("board").querySelector(`[data-square="${next}"]`)?.focus();
      }
      if (e.key === "Escape") {
        selected = null;
        render();
      }
    };
    fragment.append(button);
  }
  const focusSquare = document.activeElement?.dataset?.square;
  $("board").replaceChildren(fragment);
  if (focusSquare)
    $("board").querySelector(`[data-square="${focusSquare}"]`)?.focus();
  $("move-number").textContent = `Action ${state.ply}`;
  $("human-side").textContent = human ? "Black" : "White";
  $("opponent-side").textContent = human ? "White" : "Black";
  $("turn-pill").textContent = state.terminal
    ? "Game complete"
    : state.side === human
      ? "Your turn"
      : "Fly’s turn";
  $("undo").disabled = !turnStarts.length || busy;
  if (state.terminal) {
    status(
      state.winner < 0
        ? "A draw. Time for another encounter."
        : state.winner === human
          ? "You won. The fly would like a rematch."
          : "The fly wins this one. Tiny brain, good move.",
    );
    setMood(state.winner === human ? "OUTPLAYED" : "AT REST");
  } else if (!busy)
    status(
      state.side === human
        ? state.check
          ? "Your king is in check. Choose a legal reply."
          : "Select a piece, then a highlighted square."
        : "The fly is ready to think.",
    );
  $("choices").replaceChildren();
  if (!state.terminal && state.side === human && !busy)
    for (const m of state.moves.filter((m) => m.kind === 7)) {
      const b = document.createElement("button");
      b.textContent = "Finish turn / pass";
      b.onclick = () => humanMove(m);
      $("choices").append(b);
    }
}
function onSquare(square) {
  if (!state || busy || state.terminal || state.side !== human) return;
  const options = state.moves.filter(
    (m) => m.from === selected && m.to === square,
  );
  if (options.length === 1) {
    void humanMove(options[0]);
    return;
  }
  if (options.length > 1) {
    $("choices").replaceChildren(
      ...options.map((m) => {
        const b = document.createElement("button");
        b.textContent = m.label;
        b.onclick = () => humanMove(m);
        return b;
      }),
    );
    status("Choose the action below the board.");
    return;
  }
  selected = state.pieces.some((p) => p.square === square && p.color === human)
    ? square
    : null;
  render();
  if (selected !== null) {
    const p = state.pieces.find((p) => p.square === selected);
    status(
      `${title(p.type)} on ${squareName(selected)}${p.cooldown ? ` · ${p.cooldown} cooldown` : ""}${p.frozen ? " · frozen" : ""}. ${state.moves.filter((m) => m.from === selected).length} legal actions.`,
    );
  }
}
async function apply(move) {
  state = await referee("move", [move.index]);
  lastMove = move;
  selected = null;
}
async function humanMove(move) {
  if (busy) return;
  const token = generation;
  busy = true;
  turnStarts.push(state.ply);
  try {
    await apply(move);
    if (token !== generation) return;
    busy = false;
    render();
    await flyTurn(token);
  } catch (error) {
    if (token !== generation) return;
    turnStarts.pop();
    busy = false;
    render();
    status(error.message);
  }
}
async function flyTurn(token = generation) {
  if (!state || state.terminal || state.side === human) return;
  busy = true;
  visual.thinking = true;
  $("retry").hidden = true;
  render();
  setMood("CONSIDERING");
  $("brain-phase").textContent = "Simulating every legal choice";
  $("brain-key").textContent = visual.paused
    ? "Dim points show measured anatomy"
    : "Recorded simulation · waiting";
  $("activity-count").textContent = "—";
  $("think-time").textContent = "—";
  status("The full fly circuit is considering its moves…");
  abort = new AbortController();
  try {
    const response = await fetch("/api/fly/choose", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ candidates: state.moves.map((m) => m.features) }),
      signal: abort.signal,
    });
    const result = await response.json();
    if (!response.ok)
      throw Error(result.error || "The fly could not choose a move.");
    if (token !== generation) return;
    if (!Number.isInteger(result.index) || !state.moves[result.index])
      throw Error("The fly returned an invalid action.");
    visual.trace = result.frames;
    visual.traceStart = performance.now();
    visual.thinking = false;
    $("brain-key").textContent = "Brightness = computed activity";
    $("activity-count").textContent = result.active.toLocaleString();
    $("think-time").textContent = (result.elapsedMs / 1000).toFixed(1) + "s";
    $("brain-phase").textContent = "Replaying the chosen action’s activity";
    if (!visual.paused)
      await new Promise((resolve) => setTimeout(resolve, 1400));
    if (token !== generation) return;
    await apply(state.moves[result.index]);
    if (token !== generation) return;
    visual.celebrateUntil = performance.now() + 900;
    busy = false;
    visual.thinking = false;
    setMood("AT REST");
    $("brain-phase").textContent = "Last decision · computed activity";
    render();
    if (!state.terminal && state.side !== human) await flyTurn(token);
  } catch (error) {
    if (token !== generation) return;
    busy = false;
    visual.thinking = false;
    render();
    status(error.message);
    $("retry").hidden = false;
    $("brain-phase").textContent = "Inference paused";
    $("brain-key").textContent = visual.trace
      ? "Brightness = last computed activity"
      : "Dim points show measured anatomy";
    setMood("WAITING");
  }
}
async function newGame() {
  const token = ++generation;
  abort?.abort();
  busy = true;
  selected = null;
  human = Number($("color").value);
  turnStarts = [];
  lastMove = null;
  resetBrainDisplay();
  $("retry").hidden = true;
  status("Setting up the board…");
  try {
    const next = await referee("reset", [Number($("army").value), 0]);
    if (token !== generation) return;
    state = next;
    busy = false;
    render();
    await flyTurn(token);
  } catch (error) {
    if (token !== generation) return;
    state = null;
    busy = false;
    $("board").replaceChildren();
    status(error.message);
  }
}
$("new-game").onclick = () => void newGame();
$("retry").onclick = () => void flyTurn();
$("undo").onclick = async () => {
  if (busy || !turnStarts.length) return;
  busy = true;
  const token = generation,
    target = turnStarts.at(-1);
  try {
    const next = await referee("undo", [target]);
    if (token !== generation) return;
    state = next;
    turnStarts.pop();
    lastMove = null;
    selected = null;
    resetBrainDisplay();
    busy = false;
    render();
  } catch (error) {
    if (token !== generation) return;
    busy = false;
    status(error.message);
  }
};
$("about-open").onclick = () => $("about").showModal();
$("about-close").onclick = () => $("about").close();
for (const [create, id] of [
  [flyScene, "fly-scene"],
  [brainScene, "brain-scene"],
])
  create($(id), visual).catch(() => {
    const message = $(id).querySelector(".scene-loading");
    if (message)
      message.textContent =
        "3D view unavailable. You can still play on the board.";
  });
fetch("/fly/data/readout.json")
  .then((r) => r.json())
  .then((m) => {
    $("training-summary").textContent =
      `The readout was trained on ${m.trainingPositions.toLocaleString()} positions. See the report for held-out results and controls.`;
  })
  .catch(() => {
    $("training-summary").textContent =
      "See the training report for the checkpoint’s methods and results.";
  });
fetch("/fly/pieces.svg")
  .then((r) => {
    if (!r.ok) throw Error("Piece artwork unavailable");
    return r.text();
  })
  .then((text) => {
    const doc = new DOMParser().parseFromString(text, "image/svg+xml");
    for (const symbol of doc.querySelectorAll("symbol"))
      symbols.set(symbol.id, symbol);
  })
  .catch(() => {})
  .finally(() => void newGame());
