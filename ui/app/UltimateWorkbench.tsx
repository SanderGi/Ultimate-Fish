"use client";

import { useMemo, useState } from "react";

type Color = "white" | "black";
type View = "play" | "analysis" | "draft";
type DraftAction = "ban" | "pick";

type PieceId =
  | "king" | "jester" | "knight" | "pawn" | "queen" | "rook" | "bishop"
  | "berserker" | "bomb" | "ninja" | "turtle" | "ghost" | "mage" | "goop"
  | "penguin" | "parasite" | "devil" | "minion" | "sludge" | "sniper"
  | "prince" | "checker" | "checkerKing" | "giant" | "copycat"
  | "copycatClone" | "angel" | "halo" | "fisherman" | "dragon";

type BoardPiece = {
  id: PieceId;
  color: Color;
  moved?: boolean;
  cooldown?: number;
  visible?: boolean;
  action?: number;
  link?: string;
};

type RosterPiece = {
  id: PieceId;
  name: string;
  mark: string;
  family: "Classic" | "Melee" | "Ranged" | "Support" | "Linked";
  summary: string;
};

const roster: RosterPiece[] = [
  { id: "king", name: "King", mark: "K", family: "Classic", summary: "Royal; one square in any direction." },
  { id: "jester", name: "Jester", mark: "J", family: "Support", summary: "Appears to the opponent as a king." },
  { id: "queen", name: "Queen", mark: "Q", family: "Classic", summary: "Slides in every direction." },
  { id: "rook", name: "Rook", mark: "R", family: "Classic", summary: "Slides orthogonally." },
  { id: "bishop", name: "Bishop", mark: "B", family: "Classic", summary: "Slides diagonally." },
  { id: "knight", name: "Knight", mark: "N", family: "Classic", summary: "Leaps in an L shape." },
  { id: "pawn", name: "Pawn", mark: "P", family: "Classic", summary: "Advances, captures diagonally, and promotes." },
  { id: "checker", name: "Checker", mark: "C", family: "Ranged", summary: "Compulsory jumping captures can chain." },
  { id: "checkerKing", name: "Checker King", mark: "CK", family: "Ranged", summary: "A promoted checker that also moves backward." },
  { id: "berserker", name: "Berserker", mark: "BZ", family: "Melee", summary: "Its movement radius grows after knockouts." },
  { id: "bomb", name: "Bomb", mark: "BO", family: "Melee", summary: "A lethal hit explodes the surrounding radius." },
  { id: "ninja", name: "Ninja", mark: "NJ", family: "Melee", summary: "Moves up to three squares through characters." },
  { id: "turtle", name: "Turtle", mark: "T", family: "Melee", summary: "Moves one square orthogonally." },
  { id: "parasite", name: "Parasite", mark: "PA", family: "Melee", summary: "Possesses pieces through combat." },
  { id: "giant", name: "Giant", mark: "GI", family: "Ranged", summary: "Occupies a 2×2 footprint and moves one footprint orthogonally." },
  { id: "dragon", name: "Dragon", mark: "DR", family: "Melee", summary: "Diagonal slider plus knight leap." },
  { id: "ghost", name: "Ghost", mark: "GH", family: "Support", summary: "Hidden until attacking or near a royal decoy." },
  { id: "mage", name: "Mage", mark: "M", family: "Support", summary: "Swaps locations with an ally." },
  { id: "penguin", name: "Penguin", mark: "PG", family: "Support", summary: "Maintains a freeze around itself." },
  { id: "devil", name: "Devil", mark: "DV", family: "Support", summary: "Spawns advancing minions, then sleeps." },
  { id: "minion", name: "Minion", mark: "MN", family: "Support", summary: "Advances automatically at turn start." },
  { id: "sludge", name: "Sludge", mark: "SL", family: "Support", summary: "Leaves retaliating goop behind." },
  { id: "goop", name: "Goop", mark: "GO", family: "Support", summary: "Retaliates against melee attackers." },
  { id: "prince", name: "Prince", mark: "PR", family: "Melee", summary: "Can move twice if its first move does not attack." },
  { id: "sniper", name: "Sniper", mark: "SN", family: "Ranged", summary: "Shoots forward, then reloads for a turn." },
  { id: "fisherman", name: "Fisherman", mark: "FI", family: "Support", summary: "Slides on empty rays or pulls the first distant character adjacent." },
  { id: "copycat", name: "CopyCat", mark: "CC", family: "Linked", summary: "Moves with a mirrored clone; both share death." },
  { id: "copycatClone", name: "CopyCat Clone", mark: "C′", family: "Linked", summary: "The mirrored half of CopyCat." },
  { id: "angel", name: "Angel", mark: "A", family: "Linked", summary: "Saves a linked ally and returns it to a halo." },
  { id: "halo", name: "Halo", mark: "H", family: "Linked", summary: "Immobile return point linked to an angel." },
];

const pieceById = new Map(roster.map((piece) => [piece.id, piece]));
const files = ["a", "b", "c", "d", "e", "f", "g", "h"];
const draftCosts: Record<PieceId, number> = {
  king: 0, jester: 10, knight: 6, pawn: 3, queen: 17, rook: 13, bishop: 9,
  berserker: 15, bomb: 15, ninja: 20, turtle: 4, ghost: 15, mage: 8, goop: 0,
  penguin: 15, parasite: 15, devil: 15, minion: 0, sludge: 12, sniper: 17,
  prince: 18, checker: 2, checkerKing: 2, giant: 1, copycat: 5, copycatClone: 0,
  angel: 13, halo: 0, fisherman: 12, dragon: 15,
};

const draftWindows: Array<{ player: Color; action: DraftAction; min: number; max: number }> = [
  { player: "white", action: "ban", min: 0, max: 0 },
  { player: "black", action: "ban", min: 0, max: 0 },
  { player: "white", action: "pick", min: 15, max: 15 },
  { player: "black", action: "pick", min: 15, max: 15 },
  { player: "white", action: "ban", min: 0, max: 0 },
  { player: "black", action: "ban", min: 0, max: 0 },
  { player: "white", action: "pick", min: 15, max: 30 },
  { player: "black", action: "pick", min: 15, max: 40 },
  { player: "white", action: "ban", min: 0, max: 0 },
  { player: "black", action: "ban", min: 0, max: 0 },
  { player: "white", action: "pick", min: 0, max: 100 },
  { player: "black", action: "pick", min: 0, max: 100 },
];

const generatedDraftPieces = new Set<PieceId>([
  "king", "goop", "minion", "checkerKing", "copycatClone", "halo",
]);

function pointsFor(team: PieceId[]): number {
  return team.reduce((total, piece) => total + draftCosts[piece], 0);
}

function removeLast<T>(items: T[], value: T): T[] {
  const index = items.lastIndexOf(value);
  return index < 0 ? items : [...items.slice(0, index), ...items.slice(index + 1)];
}

type EngineAnalysis = {
  bestmove: string | null;
  depth: number;
  score: number;
  nodes: number;
  time: number;
  pv: string[];
  moves: string[];
  upn: string | null;
};

function emptyBoard(): Array<BoardPiece | null> {
  return Array.from({ length: 80 }, () => null);
}

function classicBoard(): Array<BoardPiece | null> {
  const board = emptyBoard();
  const back: PieceId[] = ["rook", "knight", "bishop", "queen", "king", "bishop", "knight", "rook"];
  back.forEach((id, file) => {
    board[file] = { id, color: "black" };
    board[8 + file] = { id: "pawn", color: "black" };
    board[64 + file] = { id: "pawn", color: "white" };
    board[72 + file] = { id, color: "white" };
  });
  return board;
}

function squareName(index: number): string {
  return `${files[index % 8]}${10 - Math.floor(index / 8)}`;
}

function positionUpn(board: Array<BoardPiece | null>, turn: Color): string {
  const pieces = board.flatMap((piece, index) => {
    if (!piece) return [];
    return [`${piece.id},${piece.color === "white" ? "w" : "b"},${squareName(index)},${piece.action ?? 0},${piece.cooldown ?? 0},0,0,${piece.moved ? 1 : 0},${piece.visible === false ? 0 : 1},-1,1,-1,0`];
  });
  return `${turn === "white" ? "w" : "b"};hm=0;fm=1;ep=-;cont=0;forced=-1;${pieces.join(";")}`;
}

function boardFromUpn(upn: string): { board: Array<BoardPiece | null>; turn: Color } {
  const fields = upn.split(";");
  if (fields[0] !== "w" && fields[0] !== "b")
    throw new Error("UPN must begin with w or b.");
  const board = emptyBoard();
  for (const field of fields.slice(1)) {
    if (field.includes("=") || !field.includes(",")) continue;
    const values = field.split(",");
    const [id, color, square] = values;
    if (!pieceById.has(id as PieceId) || (color !== "w" && color !== "b") ||
        !/^[a-h](?:10|[1-9])$/.test(square))
      throw new Error(`Invalid UPN character entry: ${field}`);
    if (values[10] === "0") continue;
    const file = files.indexOf(square[0]);
    const rank = Number(square.slice(1));
    board[(10 - rank) * 8 + file] = {
      id: id as PieceId,
      color: color === "w" ? "white" : "black",
      action: Number(values[3] ?? 0),
      cooldown: Number(values[4] ?? 0),
      moved: values[7] === "1",
      visible: values[8] !== "0",
      link: values[9],
    };
  }
  return { board, turn: fields[0] === "w" ? "white" : "black" };
}

function PieceToken({ piece }: { piece: BoardPiece }) {
  const item = pieceById.get(piece.id);
  const classicGlyphs: Partial<Record<PieceId, [string, string]>> = {
    king: ["♔", "♚"], queen: ["♕", "♛"], rook: ["♖", "♜"],
    bishop: ["♗", "♝"], knight: ["♘", "♞"], pawn: ["♙", "♟"],
  };
  const glyph = classicGlyphs[piece.id]?.[piece.color === "white" ? 0 : 1] ?? item?.mark ?? "?";
  return (
    <span className={`board-piece ${piece.color} ${glyph.length > 1 ? "compact" : ""}`} title={`${piece.color} ${item?.name}`}>
      {glyph}
    </span>
  );
}

export function UltimateWorkbench() {
  const [view, setView] = useState<View>("analysis");
  const [board, setBoard] = useState(classicBoard);
  const [turn, setTurn] = useState<Color>("white");
  const [flipped, setFlipped] = useState(false);
  const [editing, setEditing] = useState(false);
  const [tool, setTool] = useState<PieceId | "erase">("king");
  const [toolColor, setToolColor] = useState<Color>("white");
  const [selected, setSelected] = useState<number | null>(null);
  const [query, setQuery] = useState("");
  const [draftPhase, setDraftPhase] = useState(0);
  const [draftTeams, setDraftTeams] = useState<Record<Color, PieceId[]>>({ white: ["king"], black: ["king"] });
  const [draftPending, setDraftPending] = useState<PieceId[]>([]);
  const [banned, setBanned] = useState<PieceId[]>([]);
  const [deploymentPool, setDeploymentPool] = useState<Record<Color, PieceId[]> | null>(null);
  const [deploymentMessage, setDeploymentMessage] = useState("");
  const [positionOpen, setPositionOpen] = useState(false);
  const [positionText, setPositionText] = useState("");
  const [positionMessage, setPositionMessage] = useState("");
  const [depth, setDepth] = useState(6);
  const [upnOverride, setUpnOverride] = useState<string | null>(null);
  const [engineStatus, setEngineStatus] = useState<"offline" | "thinking" | "ready" | "error">("offline");
  const [engineMessage, setEngineMessage] = useState("Start the local engine bridge, then analyze.");
  const [analysis, setAnalysis] = useState<EngineAnalysis | null>(null);
  const [legalMoves, setLegalMoves] = useState<string[]>([]);

  const upn = useMemo(() => upnOverride ?? positionUpn(board, turn), [board, turn, upnOverride]);
  const draftWindow = draftWindows[draftPhase] ?? null;
  const draftTeam = draftWindow ? draftTeams[draftWindow.player] : draftTeams.white;
  const draftPoints = pointsFor(draftTeam);
  const orderedSquares = useMemo(
    () => Array.from({ length: 80 }, (_, index) => flipped ? 79 - index : index),
    [flipped],
  );
  const filteredRoster = useMemo(() => {
    const normalized = query.trim().toLowerCase();
    return roster.filter((piece) => !normalized || `${piece.name} ${piece.family} ${piece.summary}`.toLowerCase().includes(normalized));
  }, [query]);

  function handleSquare(index: number) {
    if (editing) {
      if (deploymentPool) {
        const occupant = board[index];
        if (tool === "erase") {
          if (!occupant) return;
          const returnedId = occupant.id === "copycatClone" ? "copycat" : occupant.id;
          setBoard((current) => {
            const next = [...current];
            next[index] = null;
            if (returnedId === "copycat") {
              const mirror = Math.floor(index / 8) * 8 + (7 - index % 8);
              if (next[mirror]?.id === (occupant.id === "copycat" ? "copycatClone" : "copycat"))
                next[mirror] = null;
            }
            return next;
          });
          setDeploymentPool((pool) => pool && ({
            ...pool,
            [occupant.color]: [...pool[occupant.color], returnedId],
          }));
          setDeploymentMessage(`${pieceById.get(returnedId)?.name} returned to the ${occupant.color === "white" ? "Ivory" : "Onyx"} pool.`);
          return;
        }
        const rank = Number(squareName(index).slice(1));
        const inHomeZone = toolColor === "white" ? rank <= 3 : rank >= 8;
        if (!inHomeZone) {
          setDeploymentMessage(`${toolColor === "white" ? "Ivory" : "Onyx"} deploys only on ranks ${toolColor === "white" ? "1–3" : "8–10"}.`);
          return;
        }
        if (occupant || !deploymentPool[toolColor].includes(tool)) {
          setDeploymentMessage(occupant ? "Choose an empty deployment square." : "No drafted copy of that character remains.");
          return;
        }
        if (tool === "giant" && (index % 8 === 7 || (toolColor === "white" ? rank === 3 : rank === 10))) {
          setDeploymentMessage("A Giant's 2×2 footprint must fit completely inside its three-rank home zone.");
          return;
        }
        const copycatMirror = Math.floor(index / 8) * 8 + (7 - index % 8);
        if (tool === "copycat" && board[copycatMirror]) {
          setDeploymentMessage("CopyCat needs its file-mirrored clone square to be empty.");
          return;
        }
        setUpnOverride(null);
        setAnalysis(null);
        setLegalMoves([]);
        setBoard((current) => {
          const next = [...current];
          next[index] = { id: tool, color: toolColor };
          if (tool === "copycat")
            next[copycatMirror] = { id: "copycatClone", color: toolColor };
          return next;
        });
        setDeploymentPool((pool) => pool && ({
          ...pool,
          [toolColor]: removeLast(pool[toolColor], tool),
        }));
        setDeploymentMessage(`${pieceById.get(tool)?.name} deployed on ${squareName(index)}.`);
        return;
      }
      setUpnOverride(null);
      setAnalysis(null);
      setLegalMoves([]);
      setBoard((current) => {
        const next = [...current];
        next[index] = tool === "erase" ? null : { id: tool, color: toolColor };
        return next;
      });
      return;
    }
    if (view === "play" && selected !== null && selected !== index) {
      const from = squareName(selected);
      const to = squareName(index);
      const move = legalMoves.find((candidate) => {
        const match = candidate.match(/^([a-h](?:10|[1-9]))[-~@x!&]([a-h](?:10|[1-9]))$/);
        return match?.[1] === from && match?.[2] === to;
      });
      if (move) {
        void playMove(move);
        return;
      }
    }
    setSelected((current) => current === index ? null : index);
  }

  function loadPosition() {
    try {
      if (/^[wb];/.test(positionText.trim())) {
        const loaded = boardFromUpn(positionText.trim());
        setBoard(loaded.board);
        setTurn(loaded.turn);
        setUpnOverride(positionText.trim());
        setPositionMessage("Lossless UPN loaded.");
        setEditing(false);
        setAnalysis(null);
        setLegalMoves([]);
        return;
      }
      const parsed = JSON.parse(positionText) as {
        format?: string;
        version?: number;
        turn?: Color;
        pieces?: Array<BoardPiece & { square: string }>;
      };
      if (parsed.format !== "ultimate-position" || parsed.version !== 1 || !Array.isArray(parsed.pieces)) {
        throw new Error("Expected Ultimate Position JSON version 1.");
      }
      const next = emptyBoard();
      for (const piece of parsed.pieces) {
        const file = files.indexOf(piece.square?.[0]);
        const rank = Number(piece.square?.slice(1));
        if (file < 0 || rank < 1 || rank > 10 || !pieceById.has(piece.id) || !["white", "black"].includes(piece.color)) {
          throw new Error(`Invalid piece entry at ${piece.square ?? "unknown square"}.`);
        }
        next[(10 - rank) * 8 + file] = { ...piece, square: undefined } as BoardPiece;
      }
      setBoard(next);
      setTurn(parsed.turn === "black" ? "black" : "white");
      setUpnOverride(null);
      setPositionMessage("Position loaded.");
      setEditing(true);
    } catch (error) {
      setPositionMessage(error instanceof Error ? error.message : "Could not load position.");
    }
  }

  function openPositionEditor() {
    setPositionText(upn);
    setPositionMessage("");
    setPositionOpen(true);
  }

  function chooseDraft(id: PieceId) {
    if (!draftWindow || generatedDraftPieces.has(id) || banned.includes(id)) return;
    if (draftWindow.action === "ban") {
      setDraftPending([id]);
      return;
    }
    if (draftPoints + draftCosts[id] > draftWindow.max) return;
    setDraftTeams((teams) => ({ ...teams, [draftWindow.player]: [...teams[draftWindow.player], id] }));
    setDraftPending((items) => [...items, id]);
  }

  function unchooseDraft(id: PieceId) {
    if (!draftWindow || !draftPending.includes(id)) return;
    setDraftPending((items) => removeLast(items, id));
    if (draftWindow.action === "pick")
      setDraftTeams((teams) => ({
        ...teams,
        [draftWindow.player]: removeLast(teams[draftWindow.player], id),
      }));
  }

  function commitDraftWindow() {
    if (!draftWindow) return;
    if (draftWindow.action === "ban") {
      if (draftPending.length !== 1) return;
      setBanned((items) => [...items, draftPending[0]]);
    } else if (draftPoints < draftWindow.min || draftPoints > draftWindow.max) {
      return;
    }
    setDraftPending([]);
    setDraftPhase((phase) => phase + 1);
  }

  function resetDraft() {
    setDraftPhase(0);
    setDraftTeams({ white: ["king"], black: ["king"] });
    setDraftPending([]);
    setBanned([]);
    setDeploymentPool(null);
    setDeploymentMessage("");
  }

  function beginDeployment() {
    setBoard(emptyBoard());
    setTurn("white");
    setUpnOverride(null);
    setAnalysis(null);
    setLegalMoves([]);
    setDeploymentPool({ white: [...draftTeams.white], black: [...draftTeams.black] });
    setDeploymentMessage("Place every drafted character inside its side's three-rank home zone.");
    setToolColor("white");
    setTool(draftTeams.white[0] ?? "king");
    setEditing(true);
    setView("analysis");
  }

  function finishDeployment() {
    if (!deploymentPool || deploymentPool.white.length || deploymentPool.black.length) return;
    setDeploymentPool(null);
    setDeploymentMessage("Deployment complete. The position is ready for play or analysis.");
    setEditing(false);
    setView("play");
  }

  async function engineRequest(endpoint: string, payload: Record<string, unknown>) {
    const response = await fetch(`http://127.0.0.1:3001${endpoint}`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(payload),
    });
    const result = await response.json() as EngineAnalysis & { error?: string; engine?: EngineAnalysis };
    if (!response.ok) throw new Error(result.error ?? "Engine request failed");
    return result;
  }

  async function startAnalysis() {
    setEngineStatus("thinking");
    setEngineMessage("Searching…");
    try {
      const result = await engineRequest("/analyze", { upn, depth });
      setAnalysis(result);
      setLegalMoves(result.moves);
      setEngineStatus("ready");
      setEngineMessage(`${result.nodes.toLocaleString()} nodes in ${result.time} ms`);
    } catch (error) {
      setEngineStatus("error");
      setEngineMessage(error instanceof Error ? error.message : "Could not reach the engine bridge.");
    }
  }

  async function playMove(move: string) {
    setEngineStatus("thinking");
    setEngineMessage(`Playing ${move}, then thinking…`);
    setSelected(null);
    try {
      const result = await engineRequest("/play", { upn, move, depth, movetime: 1500 });
      if (!result.upn) throw new Error("Engine returned no position.");
      const loaded = boardFromUpn(result.upn);
      setBoard(loaded.board);
      setTurn(loaded.turn);
      setUpnOverride(result.upn);
      setLegalMoves(result.moves);
      setAnalysis(result.engine ?? result);
      setEngineStatus("ready");
      setEngineMessage(result.engine?.bestmove ? `Ultimate Fish played ${result.engine.bestmove}` : "Game over");
    } catch (error) {
      setEngineStatus("error");
      setEngineMessage(error instanceof Error ? error.message : "Move failed.");
    }
  }

  const selectedPiece = selected === null ? null : board[selected];

  return (
    <main className="workbench">
      <header className="topbar">
        <div className="brand-lockup">
          <div className="brand-mark">UF</div>
          <div>
            <p className="eyebrow">FAIRY-STOCKFISH LAB</p>
            <h1>Ultimate Fish</h1>
          </div>
          <span className="build-badge">5.731 rules</span>
        </div>
        <nav className="view-tabs" aria-label="Workbench view">
          {(["play", "analysis", "draft"] as View[]).map((item) => (
            <button key={item} className={view === item ? "active" : ""} onClick={() => setView(item)}>
              {item === "analysis" ? "Analyze" : item[0].toUpperCase() + item.slice(1)}
            </button>
          ))}
        </nav>
        <div className="engine-state" title={engineMessage}>
          <span className={`status-dot ${engineStatus === "ready" ? "green" : engineStatus === "error" ? "red" : "amber"}`} />
          <div><strong>{engineStatus === "thinking" ? "Thinking" : engineStatus === "ready" ? "Ultimate Fish ready" : "Local engine"}</strong><small>{engineMessage}</small></div>
        </div>
      </header>

      <section className="workspace-grid">
        <aside className="left-rail panel">
          <div className="panel-heading">
            <div><p className="eyebrow">POSITION</p><h2>Board tools</h2></div>
            <button className={`icon-button ${editing ? "active" : ""}`} disabled={Boolean(deploymentPool)} onClick={() => setEditing(!editing)} title="Toggle position editor">✎</button>
          </div>

          <div className="segmented">
            <button className={toolColor === "white" ? "active" : ""} onClick={() => setToolColor("white")}>Ivory</button>
            <button className={toolColor === "black" ? "active" : ""} onClick={() => setToolColor("black")}>Onyx</button>
          </div>

          <label className="search-field">
            <span>⌕</span>
            <input value={query} onChange={(event) => setQuery(event.target.value)} placeholder="Find a character" />
          </label>

          <div className="piece-palette" aria-label="Character palette">
            <button className={`palette-item erase ${tool === "erase" ? "selected" : ""}`} onClick={() => { setTool("erase"); setEditing(true); }}>
              <span>×</span><small>Erase</small>
            </button>
            {filteredRoster.map((piece) => (
              <button key={piece.id} disabled={Boolean(deploymentPool && !deploymentPool[toolColor].includes(piece.id))} className={`palette-item ${tool === piece.id ? "selected" : ""}`} onClick={() => { setTool(piece.id); setEditing(true); }} title={piece.summary}>
                <span>{piece.mark}</span><small>{piece.name}</small>
              </button>
            ))}
          </div>
          <p className="rail-note">{deploymentPool ? `${deploymentMessage} Remaining: Ivory ${deploymentPool.white.length}, Onyx ${deploymentPool.black.length}.` : editing ? "Editing is on. Choose a character, then click a square." : "Turn editing on to place or erase characters."}</p>
        </aside>

        <section className="board-stage">
          <div className="board-meta">
            <div className="player-card opponent"><span className="player-token">B</span><div><small>ONYX</small><strong>Opponent</strong></div><span className="clock">10:00</span></div>
            <div className="board-actions">
              <button disabled={Boolean(deploymentPool)} onClick={() => { setBoard(classicBoard()); setUpnOverride(null); setAnalysis(null); setLegalMoves([]); }}>Reset</button>
              <button disabled={Boolean(deploymentPool)} onClick={() => { setBoard(emptyBoard()); setUpnOverride(null); setAnalysis(null); setLegalMoves([]); }}>Clear</button>
              <button onClick={() => setFlipped(!flipped)}>Flip ↻</button>
              <button onClick={openPositionEditor}>Position code</button>
              {deploymentPool && <button className="finish-deployment" disabled={Boolean(deploymentPool.white.length || deploymentPool.black.length)} onClick={finishDeployment}>Finish deployment</button>}
            </div>
          </div>

          <div className="board-shell">
            <div className="chessboard" role="grid" aria-label="Chess Ultimate board">
              {orderedSquares.map((index, displayIndex) => {
                const row = Math.floor(displayIndex / 8);
                const col = displayIndex % 8;
                const dark = (Math.floor(index / 8) + index % 8) % 2 === 1;
                return (
                  <button
                    key={index}
                    role="gridcell"
                    aria-label={squareName(index)}
                    className={`square ${dark ? "dark" : "light"} ${selected === index ? "selected" : ""}`}
                    onClick={() => handleSquare(index)}
                  >
                    {col === 0 && <span className="rank-label">{flipped ? row + 1 : 10 - row}</span>}
                    {row === 9 && <span className="file-label">{flipped ? files[7 - col] : files[col]}</span>}
                    {board[index] && <PieceToken piece={board[index]!} />}
                  </button>
                );
              })}
            </div>
          </div>

          <div className="player-card self"><span className="player-token light">W</span><div><small>IVORY</small><strong>You</strong></div><span className="clock active">10:00</span></div>
        </section>

        <aside className="right-rail">
          {view === "draft" ? (
            <section className="panel draft-panel">
              <div className="panel-heading"><div><p className="eyebrow">NATIVE 12-WINDOW DRAFT</p><h2>{draftWindow ? `${draftWindow.player === "white" ? "Ivory" : "Onyx"} ${draftWindow.action}` : "Draft complete"}</h2></div><span className="phase-pill">{Math.min(draftPhase + 1, 12)}/12</span></div>
              <div className="recovery-callout"><span>i</span><p>{draftWindow ? (draftWindow.action === "ban" ? "Choose exactly one character to ban in this window." : `Build the cumulative team between ${draftWindow.min} and ${draftWindow.max} points. Duplicate picks are legal.`) : "Both native draft sequences are complete. Use the drafted rosters while placing a starting position."}</p></div>
              <div className="draft-summary"><div><small>IVORY</small><strong>{pointsFor(draftTeams.white)}</strong></div><div><small>ONYX</small><strong>{pointsFor(draftTeams.black)}</strong></div><div><small>BANNED</small><strong>{banned.length}/6</strong></div></div>
              <div className="modal-actions">
                <button onClick={resetDraft}>Reset draft</button>
                <button className="primary-button" onClick={draftWindow ? commitDraftWindow : beginDeployment} disabled={Boolean(draftWindow && (draftWindow.action === "ban" ? draftPending.length !== 1 : draftPoints < draftWindow.min || draftPoints > draftWindow.max))}>{draftWindow ? `Commit ${draftWindow.action}` : "Deploy drafted armies"}</button>
              </div>
              <div className="draft-list">
                {roster.filter((piece) => !generatedDraftPieces.has(piece.id)).map((piece) => {
                  const pendingCount = draftPending.filter((id) => id === piece.id).length;
                  const unavailable = !draftWindow || banned.includes(piece.id) || (draftWindow.action === "pick" && draftPoints + draftCosts[piece.id] > draftWindow.max);
                  return (
                  <div className={`draft-row ${banned.includes(piece.id) ? "is-banned" : ""}`} key={piece.id}>
                    <span className="mini-mark">{piece.mark}</span><div><strong>{piece.name}</strong><small>{piece.family} · {draftCosts[piece.id]} pts</small></div>
                    <button className={pendingCount ? "picked" : ""} disabled={unavailable} onClick={() => chooseDraft(piece.id)}>{banned.includes(piece.id) ? "Banned" : draftWindow?.action === "ban" ? (pendingCount ? "Selected" : "Ban") : pendingCount ? `Add (${pendingCount})` : "Add"}</button>
                    <button disabled={!pendingCount} onClick={() => unchooseDraft(piece.id)}>Remove</button>
                  </div>
                  );
                })}
              </div>
            </section>
          ) : (
            <>
              <section className="panel evaluation-panel">
                <div className="panel-heading"><div><p className="eyebrow">ENGINE</p><h2>{view === "play" ? "Game controls" : "Analysis"}</h2></div><span className="eval-score">{analysis ? `${analysis.score >= 0 ? "+" : ""}${(analysis.score / 100).toFixed(2)}` : "—"}</span></div>
                <div className="eval-track"><span /></div>
                <div className="principal-variations">
                  <div><span>1</span><p><strong>{analysis?.pv.slice(0, 5).join(" ") || "No line yet"}</strong><small>{analysis ? `depth ${analysis.depth} · ${analysis.nodes.toLocaleString()} nodes` : "Start analysis to search this custom position"}</small></p><em>{analysis?.bestmove ?? "—"}</em></div>
                  <div><span>2</span><p><strong>{legalMoves.length} legal actions</strong><small>Special actions use ~ @ x ! and & notation</small></p><em>rules</em></div>
                  <div><span>3</span><p><strong>Lossless UPN positions</strong><small>Cooldowns, links, visibility, and continuations</small></p><em>ready</em></div>
                </div>
                <div className="engine-settings">
                  <label>Depth <output>{depth}</output><input type="range" min="1" max="16" value={depth} onChange={(event) => setDepth(Number(event.target.value))} /></label>
                </div>
                <button className="primary-button" onClick={() => void startAnalysis()} disabled={engineStatus === "thinking"}>{engineStatus === "thinking" ? "Ultimate Fish is thinking…" : view === "play" ? "Connect and start game" : "Start Ultimate analysis"}</button>
                <p className="disabled-note">{view === "play" ? "After connecting, click a character and one of its legal destinations." : engineMessage}</p>
              </section>
              <section className="panel inspector-panel">
                <div className="panel-heading"><div><p className="eyebrow">INSPECTOR</p><h2>{selected === null ? "Select a square" : squareName(selected)}</h2></div></div>
                {selectedPiece ? <div className="piece-detail"><PieceToken piece={selectedPiece} /><div><strong>{pieceById.get(selectedPiece.id)?.name}</strong><small>{pieceById.get(selectedPiece.id)?.summary}</small></div></div> : <p className="empty-state">Click a character to inspect its state. Edit mode places the selected palette character instead.</p>}
              </section>
            </>
          )}
        </aside>
      </section>

      {positionOpen && (
        <div className="modal-backdrop" role="presentation" onMouseDown={() => setPositionOpen(false)}>
          <section className="position-modal" role="dialog" aria-modal="true" aria-labelledby="position-title" onMouseDown={(event) => event.stopPropagation()}>
            <div className="panel-heading"><div><p className="eyebrow">CUSTOM ANALYSIS</p><h2 id="position-title">Ultimate Position</h2></div><button className="icon-button" onClick={() => setPositionOpen(false)}>×</button></div>
            <p>Paste lossless UPN to preserve cooldowns, visibility, links, and queued actions. Ultimate Position JSON v1 is also accepted for board editing.</p>
            <textarea value={positionText} onChange={(event) => setPositionText(event.target.value)} spellCheck={false} />
            {positionMessage && <div className="position-message">{positionMessage}</div>}
            <div className="modal-actions"><button onClick={() => navigator.clipboard?.writeText(upn)}>Copy current UPN</button><button className="primary-button" onClick={loadPosition}>Load position</button></div>
          </section>
        </div>
      )}
    </main>
  );
}
