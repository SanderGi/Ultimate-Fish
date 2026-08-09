"use client";

import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import PieceIcon from "./PieceIcon";

type Color = "white" | "black";
type View = "play" | "analysis" | "draft";
type DraftAction = "ban" | "pick";
type GameResult = "ongoing" | "white" | "black" | "draw";

type PieceId =
  | "king" | "jester" | "knight" | "pawn" | "queen" | "rook" | "bishop"
  | "berserker" | "bomb" | "ninja" | "turtle" | "ghost" | "mage" | "goop"
  | "penguin" | "parasite" | "devil" | "minion" | "sludge" | "sniper"
  | "prince" | "checker" | "checkerKing" | "giant" | "copycat"
  | "copycatClone" | "angel" | "halo" | "fisherman" | "dragon";

type PositionPiece = {
  uid: string;
  id: PieceId;
  color: Color;
  square: number;
  action: number;
  cooldown: number;
  freeze: number;
  power: number;
  moved: boolean;
  visible: boolean;
  onBoard: boolean;
  link?: string;
  host?: string;
  attachmentOrder: number;
};

type PositionMeta = {
  halfmove: number;
  fullmove: number;
  ep: string;
  continuation: number;
  forced?: string;
  epVictim?: string;
};

type RosterPiece = {
  id: PieceId;
  name: string;
  family: "Classic" | "Melee" | "Ranged" | "Support" | "Linked";
  summary: string;
};

type EngineAnalysis = {
  bestmove: string | null;
  depth: number;
  scoreType: "cp" | "mate";
  score: number;
  nodes: number;
  time: number;
  pv: string[];
  moves: string[];
  upn: string | null;
  material: Record<Color, number>;
  result: GameResult;
  resultReason: string | null;
  engine?: EngineAnalysis | null;
  engineMoves?: string[];
};

type MoveRecord = { color: Color; notation: string; upn?: string };

const roster: RosterPiece[] = [
  { id: "king", name: "King", family: "Classic", summary: "Royal; one square in any direction." },
  { id: "jester", name: "Jester", family: "Support", summary: "Appears to the opponent as a king." },
  { id: "queen", name: "Queen", family: "Classic", summary: "Slides in every direction." },
  { id: "rook", name: "Rook", family: "Classic", summary: "Slides orthogonally." },
  { id: "bishop", name: "Bishop", family: "Classic", summary: "Slides diagonally." },
  { id: "knight", name: "Knight", family: "Classic", summary: "Leaps in an L shape." },
  { id: "pawn", name: "Pawn", family: "Classic", summary: "Advances, captures diagonally, and promotes." },
  { id: "checker", name: "Checker", family: "Ranged", summary: "Compulsory jumping captures can chain." },
  { id: "checkerKing", name: "Checker King", family: "Ranged", summary: "A promoted checker that also moves backward." },
  { id: "berserker", name: "Berserker", family: "Melee", summary: "Leaps within a growing square radius after knockouts." },
  { id: "bomb", name: "Bomb", family: "Melee", summary: "A lethal hit explodes the surrounding radius." },
  { id: "ninja", name: "Ninja", family: "Melee", summary: "Moves up to three squares through characters." },
  { id: "turtle", name: "Turtle", family: "Melee", summary: "Moves one square orthogonally." },
  { id: "parasite", name: "Parasite", family: "Melee", summary: "Possesses pieces through combat." },
  { id: "giant", name: "Giant", family: "Ranged", summary: "Occupies a 2×2 footprint and shifts one full footprint." },
  { id: "dragon", name: "Dragon", family: "Melee", summary: "Diagonal slider plus knight leap." },
  { id: "ghost", name: "Ghost", family: "Support", summary: "Hidden until attacking or near an enemy royal." },
  { id: "mage", name: "Mage", family: "Support", summary: "Swaps locations with an ally." },
  { id: "penguin", name: "Penguin", family: "Support", summary: "Maintains a freeze around itself." },
  { id: "devil", name: "Devil", family: "Support", summary: "Spawns advancing minions, then sleeps." },
  { id: "minion", name: "Minion", family: "Support", summary: "Advances automatically at turn start." },
  { id: "sludge", name: "Sludge", family: "Support", summary: "Leaves retaliating goop behind." },
  { id: "goop", name: "Goop", family: "Support", summary: "Retaliates against melee attackers." },
  { id: "prince", name: "Prince", family: "Melee", summary: "Can move twice if its first move does not attack." },
  { id: "sniper", name: "Sniper", family: "Ranged", summary: "Shoots forward, then reloads for a turn." },
  { id: "fisherman", name: "Fisherman", family: "Support", summary: "Slides on empty rays or pulls a distant character adjacent." },
  { id: "copycat", name: "CopyCat", family: "Linked", summary: "Moves with an available mirrored partner; both halves share death." },
  { id: "copycatClone", name: "CopyCat Clone", family: "Linked", summary: "Mirrors linked moves unless unavailable, and shares death." },
  { id: "angel", name: "Angel", family: "Linked", summary: "Saves a linked ally and returns it to a halo." },
  { id: "halo", name: "Halo", family: "Linked", summary: "Immobile return point linked to an angel." },
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

const draftWindows: Array<{ player: Color; action: DraftAction; addMin: number; max: number }> = [
  { player: "white", action: "ban", addMin: 0, max: 0 },
  { player: "black", action: "ban", addMin: 0, max: 0 },
  { player: "white", action: "pick", addMin: 15, max: 40 },
  { player: "black", action: "pick", addMin: 15, max: 40 },
  { player: "white", action: "ban", addMin: 0, max: 0 },
  { player: "black", action: "ban", addMin: 0, max: 0 },
  { player: "white", action: "pick", addMin: 15, max: 80 },
  { player: "black", action: "pick", addMin: 40, max: 90 },
  { player: "white", action: "ban", addMin: 0, max: 0 },
  { player: "black", action: "ban", addMin: 0, max: 0 },
  { player: "white", action: "pick", addMin: 0, max: 100 },
  { player: "black", action: "pick", addMin: 0, max: 100 },
];

const firstDraftPickPhase: Record<Color, number> = {
  white: draftWindows.findIndex((window) => window.player === "white" && window.action === "pick"),
  black: draftWindows.findIndex((window) => window.player === "black" && window.action === "pick"),
};

const generatedDraftPieces = new Set<PieceId>([
  "king", "goop", "minion", "checkerKing", "copycatClone", "halo",
]);

let uidSequence = 0;
const newUid = (prefix = "piece") => `${prefix}-${++uidSequence}`;
const emptyMeta = (): PositionMeta => ({ halfmove: 0, fullmove: 1, ep: "-", continuation: 0 });

function squareName(index: number): string {
  return `${files[index % 8]}${10 - Math.floor(index / 8)}`;
}

function squareIndex(name: string): number {
  if (!/^[a-h](?:10|[1-9])$/.test(name)) return -1;
  return (10 - Number(name.slice(1))) * 8 + files.indexOf(name[0]);
}

function footprintSquares(piece: Pick<PositionPiece, "id" | "square">, anchor = piece.square): number[] {
  if (piece.id !== "giant") return [anchor];
  const file = anchor % 8;
  const row = Math.floor(anchor / 8);
  return file < 7 && row > 0 ? [anchor, anchor + 1, anchor - 8, anchor - 7] : [];
}

function basePiece(id: PieceId, color: Color, square: number, uid = newUid(id)): PositionPiece {
  return {
    uid, id, color, square, action: 0, cooldown: 0, freeze: 0, power: 0,
    moved: false, visible: id !== "ghost", onBoard: true, attachmentOrder: 0,
  };
}

function classicPosition(): { pieces: PositionPiece[]; turn: Color; meta: PositionMeta } {
  const pieces: PositionPiece[] = [];
  const back: PieceId[] = ["rook", "knight", "bishop", "queen", "king", "bishop", "knight", "rook"];
  back.forEach((id, file) => {
    pieces.push(basePiece(id, "black", file));
    pieces.push(basePiece("pawn", "black", 8 + file));
    pieces.push(basePiece("pawn", "white", 64 + file));
    pieces.push(basePiece(id, "white", 72 + file));
  });
  return { pieces, turn: "white", meta: emptyMeta() };
}

function parseUpn(upn: string): { pieces: PositionPiece[]; turn: Color; meta: PositionMeta } {
  const fields = upn.split(";");
  if (fields[0] !== "w" && fields[0] !== "b") throw new Error("UPN must begin with w or b.");
  const meta = emptyMeta();
  let forcedIndex = -1;
  let epVictimIndex = -1;
  const raw: Array<PositionPiece & { linkIndex: number; hostIndex: number }> = [];
  for (const field of fields.slice(1)) {
    if (field.startsWith("hm=")) { meta.halfmove = Number(field.slice(3)); continue; }
    if (field.startsWith("fm=")) { meta.fullmove = Number(field.slice(3)); continue; }
    if (field.startsWith("ep=")) { meta.ep = field.slice(3); continue; }
    if (field.startsWith("cont=")) { meta.continuation = Number(field.slice(5)); continue; }
    if (field.startsWith("forced=")) { forcedIndex = Number(field.slice(7)); continue; }
    if (field.startsWith("epv=")) { epVictimIndex = Number(field.slice(4)); continue; }
    if (!field.includes(",")) continue;
    const values = field.split(",");
    const id = values[0] as PieceId;
    const square = squareIndex(values[2]);
    if (!pieceById.has(id) || (values[1] !== "w" && values[1] !== "b") || square < 0)
      throw new Error(`Invalid UPN character entry: ${field}`);
    raw.push({
      ...basePiece(id, values[1] === "w" ? "white" : "black", square, newUid(`upn${raw.length}`)),
      action: Number(values[3] ?? 0), cooldown: Number(values[4] ?? 0),
      freeze: Number(values[5] ?? 0), power: Number(values[6] ?? 0),
      moved: values[7] === "1", visible: values[8] !== "0",
      linkIndex: Number(values[9] ?? -1), onBoard: values[10] !== "0",
      hostIndex: Number(values[11] ?? -1), attachmentOrder: Number(values[12] ?? 0),
    });
  }
  const pieces = raw.map(({ linkIndex, hostIndex, ...piece }) => ({
    ...piece,
    link: raw[linkIndex]?.uid,
    host: raw[hostIndex]?.uid,
  }));
  meta.forced = raw[forcedIndex]?.uid;
  meta.epVictim = raw[epVictimIndex]?.uid;
  return { pieces, turn: fields[0] === "w" ? "white" : "black", meta };
}

function positionUpn(pieces: PositionPiece[], turn: Color, meta: PositionMeta): string {
  const ids = new Map(pieces.map((piece, index) => [piece.uid, index]));
  const header = `${turn === "white" ? "w" : "b"};hm=${meta.halfmove};fm=${meta.fullmove};ep=${meta.ep};cont=${meta.continuation};forced=${ids.get(meta.forced ?? "") ?? -1};epv=${ids.get(meta.epVictim ?? "") ?? -1}`;
  const entries = pieces.map((piece) => [
    piece.id, piece.color === "white" ? "w" : "b", squareName(piece.square),
    piece.action, piece.cooldown, piece.freeze, piece.power, piece.moved ? 1 : 0,
    piece.visible ? 1 : 0, ids.get(piece.link ?? "") ?? -1, piece.onBoard ? 1 : 0,
    ids.get(piece.host ?? "") ?? -1, piece.attachmentOrder,
  ].join(","));
  return [header, ...entries].join(";");
}

function pointsFor(team: PieceId[]): number {
  return team.reduce((total, piece) => total + draftCosts[piece], 0);
}

function deploymentSlots(team: PieceId[]): number {
  return team.reduce((total, piece) => total + (piece === "giant" ? 4 : piece === "copycat" ? 2 : 1), 0);
}

function piecePoints(piece: PositionPiece): number {
  return piece.id === "berserker" ? draftCosts[piece.id] * (piece.power + 1) : draftCosts[piece.id];
}

function removeLast<T>(items: T[], value: T): T[] {
  const index = items.lastIndexOf(value);
  return index < 0 ? items : [...items.slice(0, index), ...items.slice(index + 1)];
}

function historyNotation(move: string, result: EngineAnalysis, viewer: Color): string {
  if (!result.upn) return move;
  const match = move.match(/^[a-h](?:10|[1-9])[-~@x!&]([a-h](?:10|[1-9]))$/);
  if (!match) return move;
  const destination = squareIndex(match[1]);
  const moved = parseUpn(result.upn).pieces.find((piece) => piece.onBoard &&
    piece.square === destination && piece.color !== viewer && piece.id === "ghost" && !piece.visible);
  return moved ? "•••" : move;
}

function displayScore(analysis: EngineAnalysis | null, turn: Color): { label: string; percent: number } {
  if (!analysis) return { label: "—", percent: 50 };
  const ivoryScore = turn === "white" ? analysis.score : -analysis.score;
  if (analysis.scoreType === "mate") {
    const value = Math.abs(ivoryScore);
    return { label: `${ivoryScore < 0 ? "−" : ""}M${value}`, percent: ivoryScore < 0 ? 2 : 98 };
  }
  return {
    label: `${ivoryScore >= 0 ? "+" : ""}${(ivoryScore / 100).toFixed(2)}`,
    percent: Math.max(3, Math.min(97, 50 + 47 * Math.tanh(ivoryScore / 550))),
  };
}

function PieceToken({ piece, view, playerSide, large = false }: {
  piece: PositionPiece; view: View; playerSide: Color; large?: boolean;
}) {
  const enemy = piece.color !== playerSide;
  if ((view === "play" || view === "draft") && enemy && piece.id === "ghost" && !piece.visible) return null;
  const disguised = (view === "play" || view === "draft") && enemy && piece.id === "jester";
  const shown = pieceById.get(disguised ? "king" : piece.id);
  const concealedAnalysis = view === "analysis" && enemy && piece.id === "ghost" && !piece.visible;
  return (
    <span
      className={`board-piece ${piece.color} ${large ? "large" : ""} ${concealedAnalysis ? "concealed-analysis" : ""}`}
      title={disguised ? `${piece.color} royal` : `${piece.color} ${shown?.name}`}
      aria-label={disguised ? `${piece.color} concealed royal` : `${piece.color} ${shown?.name}`}
    >
      <PieceIcon id={shown?.id ?? piece.id} color={piece.color} />
    </span>
  );
}

function autoDeploy(team: PieceId[], color: Color): PositionPiece[] {
  const placed: PositionPiece[] = [];
  const occupied = new Set<number>();
  const ranks = color === "white" ? [1, 2, 3] : [10, 9, 8];
  const filesByPriority = [3, 4, 2, 5, 1, 6, 0, 7];
  const candidates = ranks.flatMap((rank) => filesByPriority.map((file) => (10 - rank) * 8 + file));
  const ordered = [...team].sort((a, b) =>
    (a === "giant" ? -3 : a === "copycat" ? -2 : a === "king" ? -1 : 0) -
    (b === "giant" ? -3 : b === "copycat" ? -2 : b === "king" ? -1 : 0));

  for (const id of ordered) {
    if (id === "copycat") {
      const anchor = candidates.find((square) => !occupied.has(square) && !occupied.has(Math.floor(square / 8) * 8 + 7 - square % 8));
      if (anchor === undefined) continue;
      const mirror = Math.floor(anchor / 8) * 8 + 7 - anchor % 8;
      const host = basePiece("copycat", color, anchor);
      const clone = basePiece("copycatClone", color, mirror);
      host.link = clone.uid; clone.link = host.uid;
      placed.push(host, clone); occupied.add(anchor); occupied.add(mirror);
      continue;
    }
    const anchor = candidates.find((square) => {
      const probe = basePiece(id, color, square, "probe");
      const footprint = footprintSquares(probe);
      const rank = Number(squareName(square).slice(1));
      return footprint.length && footprint.every((cell) => !occupied.has(cell)) &&
        (id !== "giant" || (color === "white" ? rank <= 2 : rank <= 9));
    });
    if (anchor === undefined) continue;
    const piece = basePiece(id, color, anchor);
    placed.push(piece);
    footprintSquares(piece).forEach((square) => occupied.add(square));
  }
  return placed;
}

function deployLockedAdditions(
  existing: PositionPiece[], additions: PieceId[], color: Color,
): PositionPiece[] {
  const placed = [...existing];
  const occupied = new Set<number>();
  existing.filter((piece) => piece.onBoard).forEach((piece) => {
    footprintSquares(piece).forEach((square) => occupied.add(square));
  });
  const ranks = color === "white" ? [1, 2, 3] : [10, 9, 8];
  const filesByPriority = [3, 4, 2, 5, 1, 6, 0, 7];
  const candidates = ranks.flatMap((rank) => filesByPriority.map((file) => (10 - rank) * 8 + file));
  const ordered = [...additions].sort((a, b) =>
    (a === "giant" ? -2 : a === "copycat" ? -1 : 0) -
    (b === "giant" ? -2 : b === "copycat" ? -1 : 0));

  for (const id of ordered) {
    if (id === "copycat") {
      const anchor = candidates.find((square) => {
        const mirror = Math.floor(square / 8) * 8 + 7 - square % 8;
        return !occupied.has(square) && !occupied.has(mirror);
      });
      if (anchor === undefined) throw new Error("No locked deployment cells remain for CopyCat.");
      const mirror = Math.floor(anchor / 8) * 8 + 7 - anchor % 8;
      const host = basePiece("copycat", color, anchor);
      const clone = basePiece("copycatClone", color, mirror);
      host.link = clone.uid; clone.link = host.uid;
      placed.push(host, clone); occupied.add(anchor); occupied.add(mirror);
      continue;
    }
    const anchor = candidates.find((square) => {
      const probe = basePiece(id, color, square, "probe");
      const footprint = footprintSquares(probe);
      const rank = Number(squareName(square).slice(1));
      return footprint.length && footprint.every((cell) => !occupied.has(cell)) &&
        (id !== "giant" || (color === "white" ? rank <= 2 : rank <= 9));
    });
    if (anchor === undefined) throw new Error(`No locked deployment cells remain for ${id}.`);
    const piece = basePiece(id, color, anchor);
    placed.push(piece);
    footprintSquares(piece).forEach((square) => occupied.add(square));
  }
  return placed;
}

export function UltimateWorkbench() {
  const initial = useMemo(() => classicPosition(), []);
  const [view, setView] = useState<View>("analysis");
  const [pieces, setPieces] = useState<PositionPiece[]>(initial.pieces);
  const [turn, setTurn] = useState<Color>(initial.turn);
  const [meta, setMeta] = useState<PositionMeta>(initial.meta);
  const [playerSide, setPlayerSide] = useState<Color>("white");
  const [flipped, setFlipped] = useState(false);
  const [editing, setEditing] = useState(false);
  const [tool, setTool] = useState<PieceId | "erase">("king");
  const [toolColor, setToolColor] = useState<Color>("white");
  const [selected, setSelected] = useState<number | null>(null);
  const [draggedUid, setDraggedUid] = useState<string | null>(null);
  const [query, setQuery] = useState("");
  const [depth, setDepth] = useState(6);
  const [analysis, setAnalysis] = useState<EngineAnalysis | null>(null);
  const [analysisRunning, setAnalysisRunning] = useState(false);
  const [engineStatus, setEngineStatus] = useState<"offline" | "thinking" | "ready" | "error">("offline");
  const [engineMessage, setEngineMessage] = useState("Engine bridge not queried yet.");
  const [legalMoves, setLegalMoves] = useState<string[]>([]);
  const [gameActive, setGameActive] = useState(false);
  const [gameResult, setGameResult] = useState<{ result: GameResult; reason: string | null }>({ result: "ongoing", reason: null });
  const [moveHistory, setMoveHistory] = useState<MoveRecord[]>([]);
  const [draftPhase, setDraftPhase] = useState(0);
  const [draftTeams, setDraftTeams] = useState<Record<Color, PieceId[]>>({ white: ["king"], black: ["king"] });
  const [draftPending, setDraftPending] = useState<PieceId[]>([]);
  const [banned, setBanned] = useState<PieceId[]>([]);
  const [draftHistory, setDraftHistory] = useState<string[]>([]);
  const [draftAiBusy, setDraftAiBusy] = useState(false);
  const [draftPlacementPool, setDraftPlacementPool] = useState<PieceId[]>([]);
  const [draftPlacedUids, setDraftPlacedUids] = useState<string[]>([]);
  const [draftPlayerPieces, setDraftPlayerPieces] = useState<PositionPiece[]>(() => autoDeploy(["king"], "white"));
  const [draftOpponentPieces, setDraftOpponentPieces] = useState<PositionPiece[]>(() => autoDeploy(["king"], "black"));
  const [draftMessage, setDraftMessage] = useState("");
  const [positionOpen, setPositionOpen] = useState(false);
  const [positionText, setPositionText] = useState("");
  const [positionMessage, setPositionMessage] = useState("");
  const analysisAbort = useRef<AbortController | null>(null);
  const requestSequence = useRef(0);
  const draftAiRequest = useRef<string | null>(null);
  const liveGameUpn = useRef<string | null>(null);

  const upn = useMemo(() => positionUpn(pieces, turn, meta), [pieces, turn, meta]);
  const orderedSquares = useMemo(() => Array.from({ length: 80 }, (_, index) => flipped ? 79 - index : index), [flipped]);
  const boardMap = useMemo(() => {
    const board: Array<{ piece: PositionPiece; anchor: number } | null> = Array.from({ length: 80 }, () => null);
    pieces.filter((piece) => piece.onBoard).forEach((piece) => {
      footprintSquares(piece).forEach((square) => { if (square >= 0 && square < 80) board[square] = { piece, anchor: piece.square }; });
    });
    return board;
  }, [pieces]);
  const giants = useMemo(() => pieces.filter((piece) => piece.onBoard && piece.id === "giant"), [pieces]);
  const draftWindow = draftWindows[draftPhase] ?? null;
  const draftTeam = draftWindow ? draftTeams[draftWindow.player] : draftTeams.white;
  const draftPoints = pointsFor(draftTeam);
  const draftAddedPoints = pointsFor(draftPending);
  const draftPlacementActive = Boolean(view === "draft" && draftWindow?.player === playerSide && draftWindow.action === "pick");
  const rawSelectedPiece = selected === null ? null : boardMap[selected]?.piece ?? null;
  const selectedPiece = rawSelectedPiece && view === "play" && rawSelectedPiece.color !== playerSide &&
    rawSelectedPiece.id === "ghost" && !rawSelectedPiece.visible ? null : rawSelectedPiece;
  const filteredRoster = useMemo(() => {
    const normalized = query.trim().toLowerCase();
    return roster.filter((piece) => !normalized || `${piece.name} ${piece.family} ${piece.summary}`.toLowerCase().includes(normalized));
  }, [query]);
  const material = useMemo(() => pieces.reduce<Record<Color, number>>((totals, piece) => {
    totals[piece.color] += piecePoints(piece); return totals;
  }, { white: 0, black: 0 }), [pieces]);
  const inspectedId = selectedPiece && view === "play" && selectedPiece.color !== playerSide &&
    selectedPiece.id === "jester" ? "king" : selectedPiece?.id;
  const score = displayScore(analysis, turn);
  const showEvaluation = view === "analysis" || (view === "play" && !gameActive);
  const boardLocked = gameActive || (view === "draft" && !draftPlacementActive);
  const initialDraftKingUnlocked = draftPlacementActive && draftPhase === firstDraftPickPhase[playerSide];
  const topColor: Color = flipped ? "white" : "black";
  const bottomColor: Color = topColor === "white" ? "black" : "white";

  function draftPieceIsMovable(piece: PositionPiece): boolean {
    if (!draftPlacementActive || piece.color !== playerSide) return false;
    const hostUid = piece.id === "copycatClone" && piece.link ? piece.link : piece.uid;
    return draftPlacedUids.includes(hostUid) || (piece.id === "king" && initialDraftKingUnlocked);
  }

  const legalTargetMoves = useMemo(() => {
    const targets = new Map<number, string>();
    if (!selectedPiece) return targets;
    const from = squareName(selectedPiece.square);
    for (const move of legalMoves) {
      const match = move.match(/^([a-h](?:10|[1-9]))[-~@x!&]([a-h](?:10|[1-9]))$/);
      if (match?.[1] !== from) continue;
      const destination = squareIndex(match[2]);
      const footprint = selectedPiece.id === "giant" ? footprintSquares(selectedPiece, destination) : [destination];
      footprint.forEach((square) => {
        // A square can belong to overlapping Giant destinations. Prefer the move
        // whose anchor was clicked; otherwise retain the first matching footprint.
        if (!targets.has(square) || square === destination) targets.set(square, move);
      });
    }
    return targets;
  }, [legalMoves, selectedPiece]);
  const legalTargets = useMemo(() => new Set(legalTargetMoves.keys()), [legalTargetMoves]);

  const historyRows = useMemo(() => {
    const rows: Array<{ white: MoveRecord[]; black: MoveRecord[] }> = [];
    let previous: Color | null = null;
    for (const move of moveHistory) {
      let row = rows[rows.length - 1];
      if (!row || (move.color === "white" && previous === "black")) {
        row = { white: [], black: [] }; rows.push(row);
      }
      row[move.color].push(move);
      previous = move.color;
    }
    return rows;
  }, [moveHistory]);

  const engineRequest = useCallback(async (endpoint: string, payload: Record<string, unknown>, signal?: AbortSignal) => {
    const response = await fetch(`http://127.0.0.1:3001${endpoint}`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify(payload), signal,
    });
    const result = await response.json() as EngineAnalysis & { error?: string; choices?: PieceId[] };
    if (!response.ok) throw new Error(result.error ?? "Engine request failed");
    return result;
  }, []);

  const loadEnginePosition = useCallback((result: EngineAnalysis) => {
    if (!result.upn) return;
    const loaded = parseUpn(result.upn);
    setPieces(loaded.pieces); setTurn(loaded.turn); setMeta(loaded.meta);
    setLegalMoves(result.moves ?? []);
    setGameResult({ result: result.result ?? "ongoing", reason: result.resultReason ?? null });
  }, []);

  const loadHistoryPosition = useCallback((snapshot?: string) => {
    if (!snapshot) return;
    const loaded = parseUpn(snapshot);
    setPieces(loaded.pieces); setTurn(loaded.turn); setMeta(loaded.meta);
    setAnalysis(null); setLegalMoves([]); setSelected(null);
    setGameResult({ result: "ongoing", reason: null });
  }, []);

  const runAnalysis = useCallback(async () => {
    const sequence = ++requestSequence.current;
    analysisAbort.current?.abort();
    const controller = new AbortController();
    analysisAbort.current = controller;
    setEngineStatus("thinking"); setEngineMessage("Searching…");
    try {
      const result = await engineRequest("/analyze", { upn, depth }, controller.signal);
      if (sequence !== requestSequence.current) return;
      setAnalysis(result); setLegalMoves(result.moves); setEngineStatus("ready");
      setEngineMessage(`${result.moves.length} legal moves · ${result.nodes.toLocaleString()} nodes in ${result.time} ms`);
      setGameResult({ result: result.result, reason: result.resultReason });
    } catch (error) {
      if (controller.signal.aborted) return;
      setEngineStatus("error");
      setEngineMessage(error instanceof Error ? error.message : "Could not reach the engine bridge.");
    }
  }, [depth, engineRequest, upn]);

  useEffect(() => {
    if (!analysisRunning || view !== "analysis") return;
    const timer = window.setTimeout(() => void runAnalysis(), 180);
    return () => window.clearTimeout(timer);
  }, [analysisRunning, runAnalysis, view]);

  useEffect(() => {
    if (analysisRunning || view === "draft") return;
    const controller = new AbortController();
    const timer = window.setTimeout(async () => {
      try {
        const result = await engineRequest("/state", { upn }, controller.signal);
        setLegalMoves(result.moves);
        setGameResult({ result: result.result, reason: result.resultReason });
        if (engineStatus === "offline") setEngineStatus("ready");
        if (!analysis)
          setEngineMessage(`${result.moves.length} legal moves · 0 nodes in 0 ms`);
      } catch {
        if (!controller.signal.aborted) setLegalMoves([]);
      }
    }, 160);
    return () => { window.clearTimeout(timer); controller.abort(); };
  }, [analysis, analysisRunning, engineRequest, engineStatus, upn, view]);

  useEffect(() => {
    if (!draftWindow || draftWindow.player === playerSide || view !== "draft") return;
    const requestKey = `${draftPhase}:${playerSide}:${draftHistory.join("|")}`;
    if (draftAiRequest.current === requestKey) return;
    draftAiRequest.current = requestKey;
    let cancelled = false;
    const controller = new AbortController();
    void Promise.resolve().then(() => {
      if (cancelled) return null;
      setDraftAiBusy(true);
      return engineRequest("/draft-ai", { history: draftHistory }, controller.signal);
    }).then((result) => {
      if (!result) return;
      if (cancelled) return;
      const choices = (result.choices ?? []) as PieceId[];
      if (draftWindow.action === "ban") setBanned((items) => [...items, ...choices]);
      else {
        const nextTeam = [...draftTeams[draftWindow.player], ...choices];
        const nextOpponentPieces = deployLockedAdditions(
          draftOpponentPieces, choices, draftWindow.player,
        );
        setDraftTeams((teams) => ({ ...teams, [draftWindow.player]: nextTeam }));
        setDraftOpponentPieces(nextOpponentPieces);
        // Ranked reveals each completed opponent group immediately. Earlier
        // groups retain their exact cells; invisible Ghosts remain concealed
        // and later royal silhouettes are still rendered as public royals.
        setPieces((current) => [
          ...current.filter((piece) => piece.color === playerSide),
          ...nextOpponentPieces,
        ]);
      }
      setDraftHistory((history) => [...history, ...choices.map((id) => `draft choose ${id}`), "draft commit"]);
      setDraftPhase((phase) => phase + 1); setDraftPending([]); setDraftAiBusy(false);
    }).catch((error) => {
      draftAiRequest.current = null;
      if (!cancelled) { setDraftAiBusy(false); setEngineStatus("error"); setEngineMessage(error instanceof Error ? error.message : "Draft AI failed."); }
    });
    return () => { cancelled = true; controller.abort(); draftAiRequest.current = null; };
  }, [draftHistory, draftOpponentPieces, draftPhase, draftTeams, draftWindow, engineRequest, playerSide, view]);

  function resetTransient() {
    setMeta(emptyMeta()); setAnalysis(null); setLegalMoves([]); setSelected(null);
    setGameResult({ result: "ongoing", reason: null }); setMoveHistory([]);
  }

  function replacePosition(next: ReturnType<typeof classicPosition>) {
    setPieces(next.pieces); setTurn(next.turn); setMeta(next.meta); setAnalysis(null);
    setLegalMoves([]); setSelected(null); setMoveHistory([]); setGameResult({ result: "ongoing", reason: null });
  }

  function removeOccupants(current: PositionPiece[], squares: number[], exceptUid?: string) {
    const removed = new Set<string>();
    for (const piece of current)
      if (piece.onBoard && footprintSquares(piece).some((square) => squares.includes(square)))
        removed.add(piece.uid);
    if (exceptUid) removed.delete(exceptUid);
    let changed = true;
    while (changed) {
      changed = false;
      for (const piece of current) {
        const linkedPair = piece.id === "copycat" || piece.id === "copycatClone" ||
          piece.id === "angel" || piece.id === "halo";
        if (removed.has(piece.uid) && linkedPair && piece.link && !removed.has(piece.link)) {
          removed.add(piece.link); changed = true;
        }
        if (piece.host && removed.has(piece.host) && !removed.has(piece.uid)) {
          removed.add(piece.uid); changed = true;
          if (piece.link) removed.add(piece.link);
        }
      }
    }
    return current.filter((piece) => !removed.has(piece.uid));
  }

  function replaceDraftBoard(next: PositionPiece[]) {
    const local = next.filter((piece) => piece.color === playerSide);
    setPieces([...local, ...draftOpponentPieces]);
    setDraftPlayerPieces(local);
    setSelected(null);
  }

  function placeTool(index: number) {
    if (boardLocked) return;
    if (draftPlacementActive) {
      const rank = Number(squareName(index).slice(1));
      const inZone = playerSide === "white" ? rank <= 3 : rank >= 8;
      if (!inZone) { setDraftMessage("Place picks only inside your three-rank home zone."); return; }
      if (tool === "erase") {
        const occupant = boardMap[index]?.piece;
        if (!occupant || occupant.color !== playerSide) return;
        const hostUid = occupant.id === "copycatClone" ? occupant.link : occupant.uid;
        const host = pieces.find((piece) => piece.uid === hostUid);
        if (!host || !draftPlacedUids.includes(host.uid)) {
          setDraftMessage("Earlier pick groups are locked; only this window's picks can be removed."); return;
        }
        replaceDraftBoard(pieces.filter((piece) => piece.uid !== host.uid && piece.uid !== host.link));
        setDraftPlacementPool((pool) => [...pool, host.id]);
        setDraftPlacedUids((uids) => uids.filter((uid) => uid !== host.uid));
        return;
      }
      if (!draftPlacementPool.includes(tool)) { setDraftMessage("Choose a pending pick from the palette first."); return; }
      if (tool === "giant" && (index % 8 === 7 || (playerSide === "white" ? rank > 2 : rank > 9))) {
        setDraftMessage("The Giant's full 2×2 footprint must fit inside your home zone."); return;
      }
      const occupiedSquares = footprintSquares(basePiece(tool, playerSide, index, "probe"));
      if (!occupiedSquares.length || occupiedSquares.some((square) => boardMap[square])) { setDraftMessage("Choose an empty deployment footprint."); return; }
      let added: PositionPiece[];
      if (tool === "copycat") {
        const mirror = Math.floor(index / 8) * 8 + 7 - index % 8;
        if (boardMap[mirror]) { setDraftMessage("CopyCat needs its mirrored clone square empty."); return; }
        const host = basePiece("copycat", playerSide, index); const clone = basePiece("copycatClone", playerSide, mirror);
        host.link = clone.uid; clone.link = host.uid; added = [host, clone];
      } else added = [basePiece(tool, playerSide, index)];
      replaceDraftBoard([...pieces, ...added]);
      setDraftPlacementPool((pool) => removeLast(pool, tool));
      setDraftPlacedUids((uids) => [...uids, added[0].uid]);
      setDraftMessage(`${pieceById.get(tool)?.name} placed on ${squareName(index)}.`); resetTransient();
      return;
    }
    if (tool === "erase") {
      const occupant = boardMap[index]?.piece;
      if (occupant) setPieces((current) => removeOccupants(current, [index]));
      resetTransient(); return;
    }
    const probe = basePiece(tool, toolColor, index, "probe");
    const targetSquares = footprintSquares(probe);
    if (!targetSquares.length) { setEngineMessage("That Giant footprint leaves the board."); return; }
    setPieces((current) => {
      let next = removeOccupants(current, targetSquares);
      if (tool === "copycat") {
        const mirror = Math.floor(index / 8) * 8 + 7 - index % 8;
        next = removeOccupants(next, [mirror]);
        const host = basePiece("copycat", toolColor, index); const clone = basePiece("copycatClone", toolColor, mirror);
        host.link = clone.uid; clone.link = host.uid; return [...next, host, clone];
      }
      return [...next, basePiece(tool, toolColor, index)];
    });
    resetTransient();
  }

  function dragPiece(to: number) {
    if (!draggedUid || gameActive || (view !== "analysis" && !draftPlacementActive)) return;
    const moving = pieces.find((piece) => piece.uid === draggedUid);
    if (!moving) return;
    setDraggedUid(null);
    if (draftPlacementActive) {
      if (moving.color !== playerSide) return;
      const host = moving.id === "copycatClone" ? pieces.find((piece) => piece.uid === moving.link) : moving;
      if (!host) return;
      if (!draftPieceIsMovable(host)) {
        setDraftMessage("Earlier pick groups are locked and cannot be moved."); return;
      }
      const pair = new Set([host.uid, host.link].filter(Boolean));
      const rank = Number(squareName(to).slice(1));
      const inZone = playerSide === "white" ? rank <= 3 : rank >= 8;
      if (!inZone) { setDraftMessage("Keep every character inside your three-rank home zone."); return; }
      if (host.id === "copycat") {
        const hostTo = moving.id === "copycatClone" ? Math.floor(to / 8) * 8 + 7 - to % 8 : to;
        const cloneTo = Math.floor(hostTo / 8) * 8 + 7 - hostTo % 8;
        if ([hostTo, cloneTo].some((square) => boardMap[square] && !pair.has(boardMap[square]!.piece.uid))) {
          setDraftMessage("CopyCat and its clone need two empty mirrored squares."); return;
        }
        const next = pieces.map((piece) => piece.uid === host.uid ? { ...piece, square: hostTo, moved: true }
          : piece.uid === host.link ? { ...piece, square: cloneTo, moved: true } : piece);
        replaceDraftBoard(next); setSelected(hostTo); return;
      }
      const targets = footprintSquares(host, to);
      const targetRanks = targets.map((square) => Number(squareName(square).slice(1)));
      const fitsZone = targets.length && targetRanks.every((targetRank) => playerSide === "white" ? targetRank <= 3 : targetRank >= 8);
      if (!fitsZone || targets.some((square) => boardMap[square] && boardMap[square]!.piece.uid !== host.uid)) {
        setDraftMessage("Choose an empty footprint inside your home zone."); return;
      }
      const next = pieces.map((piece) => piece.uid === host.uid ? { ...piece, square: to, moved: true } : piece);
      replaceDraftBoard(next); setSelected(to); return;
    }
    const targets = footprintSquares(moving, to);
    if (!targets.length) { setEngineMessage("That footprint leaves the board."); return; }
    setPieces((current) => removeOccupants(current, targets, moving.uid).map((piece) =>
      piece.uid === moving.uid ? { ...piece, square: to, moved: true } : piece));
    setDraggedUid(null); resetTransient(); setSelected(to);
  }

  async function playMove(move: string) {
    if (!gameActive || turn !== playerSide) return;
    setEngineStatus("thinking"); setEngineMessage(`Playing ${move}…`); setSelected(null);
    try {
      const humanResult = await engineRequest("/move", { upn, move });
      loadEnginePosition(humanResult);
      if (humanResult.upn) liveGameUpn.current = humanResult.upn;
      setMoveHistory((history) => [...history, { color: playerSide, notation: move, upn: humanResult.upn ?? undefined }]);
      setAnalysis(null);
      if (humanResult.result !== "ongoing") {
        setGameActive(false); setEngineStatus("ready"); setEngineMessage("Game complete."); return;
      }
      const humanToMove = humanResult.upn?.[0] === (playerSide === "white" ? "w" : "b");
      if (humanToMove) {
        setEngineStatus("ready"); setEngineMessage(`${humanResult.moves.length} legal moves · continue your turn.`); return;
      }
      setEngineStatus("thinking"); setEngineMessage("Ultimate Fish is thinking…");
      const result = await engineRequest("/computer", {
        upn: humanResult.upn, player: playerSide, depth, movetime: 1500,
      });
      loadEnginePosition(result);
      if (result.upn) liveGameUpn.current = result.upn;
      const opponent: Color = playerSide === "white" ? "black" : "white";
      setMoveHistory((history) => [...history, ...(result.engineMoves ?? []).map((notation) => ({
        color: opponent, notation: historyNotation(notation, result, playerSide), upn: result.upn ?? undefined,
      }))]);
      setAnalysis(result.engine ?? null); setEngineStatus("ready");
      setEngineMessage(`${result.moves.length} legal moves · ${(result.engine?.nodes ?? 0).toLocaleString()} nodes in ${result.engine?.time ?? 0} ms`);
      if (result.result !== "ongoing") setGameActive(false);
    } catch (error) {
      setEngineStatus("error"); setEngineMessage(error instanceof Error ? error.message : "Move failed.");
    }
  }

  async function startGame(startingUpn = upn) {
    setAnalysisRunning(false); analysisAbort.current?.abort(); setEditing(false); setGameActive(true);
    setMoveHistory([]); setGameResult({ result: "ongoing", reason: null }); setFlipped(playerSide === "black");
    setEngineStatus("thinking"); setEngineMessage("Starting Ultimate game…");
    try {
      let result = await engineRequest("/state", { upn: startingUpn });
      loadEnginePosition(result);
      if (result.upn) liveGameUpn.current = result.upn;
      const playerCode = playerSide === "white" ? "w" : "b";
      if (result.upn?.[0] !== playerCode && result.result === "ongoing") {
        setEngineMessage("Ultimate Fish is thinking…");
        result = await engineRequest("/computer", { upn: result.upn, player: playerSide, depth, movetime: 1500 });
      }
      loadEnginePosition(result);
      if (result.upn) liveGameUpn.current = result.upn;
      const opponent: Color = playerSide === "white" ? "black" : "white";
      setMoveHistory((result.engineMoves ?? []).map((notation) => ({
        color: opponent, notation: historyNotation(notation, result, playerSide), upn: result.upn ?? undefined,
      })));
      setAnalysis(result.engine ?? null); setEngineStatus("ready");
      setEngineMessage(result.result === "ongoing" ? "Your move." : "Game complete.");
      if (result.result !== "ongoing") setGameActive(false);
    } catch (error) {
      setGameActive(false); setEngineStatus("error"); setEngineMessage(error instanceof Error ? error.message : "Could not start game.");
    }
  }

  function stopGame() {
    setGameActive(false); setEditing(false); setSelected(null); liveGameUpn.current = null;
    setEngineMessage("Game stopped. The position is editable again.");
  }

  function handleSquare(index: number) {
    if ((editing || draftPlacementActive) && !boardLocked) { placeTool(index); return; }
    const occupant = boardMap[index]?.piece;
    const clicked = occupant && view === "play" && occupant.color !== playerSide &&
      occupant.id === "ghost" && !occupant.visible ? undefined : occupant;
    if (view === "play" && gameActive && selectedPiece && selected !== index) {
      const move = legalTargetMoves.get(index);
      if (move) { void playMove(move); return; }
    }
    setSelected((current) => current === index ? null : (clicked?.square ?? index));
  }

  function changeView(next: View) {
    if (next !== "analysis") { setAnalysisRunning(false); analysisAbort.current?.abort(); }
    if (next === "draft" && gameActive) stopGame();
    if (view === "draft" && next !== "draft") {
      setDraftPlayerPieces(pieces.filter((piece) => piece.color === playerSide));
    }
    if (next === "draft") {
      setPieces([...draftPlayerPieces, ...draftOpponentPieces]); setTurn("white"); setMeta(emptyMeta()); setAnalysis(null); setLegalMoves([]);
      setFlipped(playerSide === "black"); setEditing(draftWindow?.player === playerSide && draftWindow.action === "pick");
    } else {
      if (next === "play" && gameActive && liveGameUpn.current) loadHistoryPosition(liveGameUpn.current);
      setEditing(false);
    }
    setSelected(null); setView(next);
  }

  function selectPlayerSide(side: Color) {
    if (gameActive || draftPhase > 0) return;
    const opponent: Color = side === "white" ? "black" : "white";
    const playerBase = autoDeploy(["king"], side);
    setPlayerSide(side); setToolColor(side); setFlipped(side === "black");
    setDraftTeams({ white: ["king"], black: ["king"] }); setDraftPending([]); setBanned([]); setDraftHistory([]);
    setDraftPlacementPool([]); setDraftPlacedUids([]); setDraftPlayerPieces(playerBase);
    setDraftOpponentPieces(autoDeploy(["king"], opponent));
    if (view === "draft") {
      setPieces([...playerBase, ...autoDeploy(["king"], opponent)]);
      setTurn("white"); setMeta(emptyMeta());
    }
  }

  function chooseDraft(id: PieceId) {
    if (!draftWindow || draftWindow.player !== playerSide || generatedDraftPieces.has(id) || banned.includes(id)) return;
    if (draftWindow.action === "ban") { setDraftPending([id]); return; }
    const footprint = id === "giant" ? 4 : id === "copycat" ? 2 : 1;
    if (draftPoints + draftCosts[id] > draftWindow.max || deploymentSlots(draftTeam) + footprint > 24) return;
    setDraftTeams((teams) => ({ ...teams, [draftWindow.player]: [...teams[draftWindow.player], id] }));
    setDraftPending((items) => [...items, id]);
    setDraftPlacementPool((pool) => [...pool, id]);
    setTool(id); setToolColor(playerSide); setEditing(true);
    setDraftMessage(`Place the new ${pieceById.get(id)?.name} in your home zone before committing.`);
  }

  function unchooseDraft(id: PieceId) {
    if (!draftWindow || draftWindow.player !== playerSide || !draftPending.includes(id)) return;
    if (draftWindow.action === "pick") {
      if (draftPlacementPool.includes(id)) setDraftPlacementPool((pool) => removeLast(pool, id));
      else {
        const placedUid = [...draftPlacedUids].reverse().find((uid) => pieces.find((piece) => piece.uid === uid)?.id === id);
        const placed = pieces.find((piece) => piece.uid === placedUid);
        if (!placed) return;
        replaceDraftBoard(pieces.filter((piece) => piece.uid !== placed.uid && piece.uid !== placed.link));
        setDraftPlacedUids((uids) => uids.filter((uid) => uid !== placed.uid));
      }
      setDraftTeams((teams) => ({
        ...teams, [draftWindow.player]: removeLast(teams[draftWindow.player], id),
      }));
    }
    setDraftPending((items) => removeLast(items, id));
  }

  function commitDraftWindow() {
    if (!draftWindow || draftWindow.player !== playerSide) return;
    if (draftWindow.action === "ban" && draftPending.length !== 1) return;
    if (draftWindow.action === "pick" && (draftAddedPoints < draftWindow.addMin || draftPoints > draftWindow.max)) return;
    if (draftWindow.action === "pick" && draftPlacementPool.length) {
      setDraftMessage(`Place all ${draftPlacementPool.length} pending character${draftPlacementPool.length === 1 ? "" : "s"} before committing.`); return;
    }
    if (draftWindow.action === "ban") setBanned((items) => [...items, draftPending[0]]);
    setDraftHistory((history) => [...history, ...draftPending.map((id) => `draft choose ${id}`), "draft commit"]);
    setDraftPending([]); setDraftPlacementPool([]); setDraftPlacedUids([]);
    setDraftPhase((phase) => phase + 1); setDraftMessage("");
  }

  function resetDraft() {
    const opponent: Color = playerSide === "white" ? "black" : "white";
    const playerBase = autoDeploy(["king"], playerSide);
    setDraftPhase(0); setDraftTeams({ white: ["king"], black: ["king"] }); setDraftPending([]);
    setBanned([]); setDraftHistory([]); setDraftPlacementPool([]); setDraftPlacedUids([]);
    setDraftPlayerPieces(playerBase); setDraftOpponentPieces(autoDeploy(["king"], opponent));
    setPieces([...playerBase, ...autoDeploy(["king"], opponent)]);
    setTurn("white"); setMeta(emptyMeta()); setDraftMessage(""); setEditing(false);
  }

  function beginDraftGame() {
    if (draftWindow || draftPlacementPool.length) return;
    const draftedPieces = [...draftPlayerPieces, ...draftOpponentPieces];
    const draftedMeta = emptyMeta();
    setPieces(draftedPieces);
    setTurn("white"); setMeta(draftedMeta); setAnalysis(null); setLegalMoves([]); setMoveHistory([]);
    setGameResult({ result: "ongoing", reason: null }); setEditing(false); setSelected(null); setView("play");
    setFlipped(playerSide === "black");
    void startGame(positionUpn(draftedPieces, "white", draftedMeta));
  }

  function openPositionEditor() {
    setPositionText(upn); setPositionMessage(""); setPositionOpen(true);
  }

  function loadPosition() {
    try {
      if (/^[wb];/.test(positionText.trim())) {
        const loaded = parseUpn(positionText.trim()); replacePosition(loaded);
        setPositionMessage("Lossless UPN loaded."); setEditing(false); return;
      }
      const parsed = JSON.parse(positionText) as { format?: string; version?: number; turn?: Color; pieces?: Array<{ id: PieceId; color: Color; square: string }> };
      if (parsed.format !== "ultimate-position" || parsed.version !== 1 || !Array.isArray(parsed.pieces))
        throw new Error("Expected Ultimate Position JSON version 1.");
      const loadedPieces = parsed.pieces.map((piece) => {
        const square = squareIndex(piece.square);
        if (square < 0 || !pieceById.has(piece.id) || !["white", "black"].includes(piece.color)) throw new Error(`Invalid piece at ${piece.square}.`);
        return basePiece(piece.id, piece.color, square);
      });
      replacePosition({ pieces: loadedPieces, turn: parsed.turn === "black" ? "black" : "white", meta: emptyMeta() });
      setPositionMessage("Position loaded for editing."); setEditing(true);
    } catch (error) { setPositionMessage(error instanceof Error ? error.message : "Could not load position."); }
  }

  const gameBanner = gameResult.result === "ongoing" ? null : gameResult.result === "draw"
    ? `Draw · ${(gameResult.reason ?? "game complete").replaceAll("-", " ")}`
    : gameResult.result === playerSide ? "You win · opposing King captured" : "Ultimate Fish wins · your King was captured";

  const historyCell = (records: MoveRecord[]) => {
    const label = records.map((record) => record.notation).join(" · ") || "—";
    const snapshot = records[records.length - 1]?.upn;
    return view === "analysis" && snapshot
      ? <button type="button" className="history-cell history-link" onClick={() => loadHistoryPosition(snapshot)} title="Load this half-turn position">{label}</button>
      : <span className="history-cell">{label}</span>;
  };

  return (
    <main className="workbench">
      <header className="topbar">
        <div className="brand-lockup">
          {/* The direct asset path also used by the working favicon avoids the dev server's image optimizer wrapper. */}
          {/* eslint-disable-next-line @next/next/no-img-element */}
          <img className="brand-mark" src="/ultimate-fish-logo.png" width={48} height={48} alt="Ultimate Fish" />
          <div><h1>Ultimate Fish</h1><span className="build-badge">5.731 rules</span></div>
        </div>
        <nav className="view-tabs" aria-label="Workbench view">
          {(["play", "analysis", "draft"] as View[]).map((item) => (
            <button key={item} className={view === item ? "active" : ""} onClick={() => changeView(item)}>
              {item === "analysis" ? "Analyze" : item[0].toUpperCase() + item.slice(1)}
            </button>
          ))}
        </nav>
        <div className="side-picker" aria-label="Player side">
          <span>You play</span>
          <div className="segmented compact-segment">
            <button disabled={gameActive || draftPhase > 0} className={playerSide === "white" ? "active" : ""} onClick={() => selectPlayerSide("white")}>Ivory</button>
            <button disabled={gameActive || draftPhase > 0} className={playerSide === "black" ? "active" : ""} onClick={() => selectPlayerSide("black")}>Onyx</button>
          </div>
        </div>
      </header>

      <section className="workspace-grid">
        <aside className={`left-rail panel ${boardLocked ? "tools-locked" : ""}`}>
          <div className="panel-heading">
            <div><p className="eyebrow">POSITION</p><h2>Board tools</h2></div>
            <button className={`icon-button ${editing ? "active" : ""}`} disabled={boardLocked || view === "draft"} onClick={() => setEditing(!editing)} title="Toggle position editor">✎</button>
          </div>
          <span className="tool-control-label">Side to move</span>
          <div className="segmented">
            <button disabled={boardLocked || view === "draft"} className={turn === "white" ? "active" : ""} onClick={() => { setTurn("white"); resetTransient(); }}>Ivory</button>
            <button disabled={boardLocked || view === "draft"} className={turn === "black" ? "active" : ""} onClick={() => { setTurn("black"); resetTransient(); }}>Onyx</button>
          </div>
          <span className="tool-control-label">Place as</span>
          <div className="segmented">
            <button disabled={boardLocked || view === "draft"} className={toolColor === "white" ? "active" : ""} onClick={() => setToolColor("white")}>Ivory</button>
            <button disabled={boardLocked || view === "draft"} className={toolColor === "black" ? "active" : ""} onClick={() => setToolColor("black")}>Onyx</button>
          </div>
          <label className="search-field"><span>⌕</span><input value={query} onChange={(event) => setQuery(event.target.value)} placeholder="Find a character" /></label>
          <div className="piece-palette" aria-label="Character palette">
            <button disabled={boardLocked || (view === "draft" && !draftPlacedUids.length)} className={`palette-item erase ${tool === "erase" ? "selected" : ""}`} onClick={() => { setTool("erase"); setEditing(true); }}><span>×</span><small>Erase</small></button>
            {filteredRoster.map((piece) => (
              <button key={piece.id} disabled={boardLocked || (view === "draft" && !draftPlacementPool.includes(piece.id))} className={`palette-item ${tool === piece.id ? "selected" : ""}`} onClick={() => { setTool(piece.id); setEditing(true); }} title={piece.summary}>
                <span className="palette-mark"><PieceIcon id={piece.id} color={toolColor} /></span><small>{piece.name}</small>
              </button>
            ))}
          </div>
          <p className="rail-note">{view === "draft" ? draftPlacementActive ? `${draftMessage || "Place this window's picks in your home zone."} ${draftPlacementPool.length} remaining.` : "The board is locked during bans and Ultimate Fish windows." : boardLocked ? "Board setup is locked while the game is active." : view === "analysis" ? "Use a palette tool, or drag any placed character to a new square." : editing ? "Choose a character, then click a square." : "Set the starting position, then start the game."}</p>
        </aside>

        <section className="board-stage">
          <div className="board-meta">
            <div className="player-card opponent"><span className={`player-token ${topColor === "white" ? "light" : ""}`}>{topColor === "white" ? "I" : "O"}</span><div><small>{topColor === "white" ? "IVORY" : "ONYX"}</small><strong>{playerSide === topColor ? "You" : "Ultimate Fish"}</strong></div><span className={`material-total ${turn === topColor ? "active" : ""}`}>{material[topColor]} pts</span></div>
            <div className="board-actions">
              <button disabled={boardLocked || view === "draft"} onClick={() => replacePosition(classicPosition())}>Reset</button>
              <button disabled={boardLocked || view === "draft"} onClick={() => replacePosition({ pieces: [], turn: "white", meta: emptyMeta() })}>Clear</button>
              <button onClick={() => setFlipped(!flipped)}>Flip ↻</button>
              <button disabled={boardLocked || view === "draft"} onClick={openPositionEditor}>Position code</button>
            </div>
          </div>

          {gameBanner && <div className={`game-banner ${gameResult.result}`}>{gameBanner}</div>}
          <div className="board-shell">
            <div className="chessboard" role="grid" aria-label="Chess Ultimate board">
              {orderedSquares.map((index, displayIndex) => {
                const row = Math.floor(displayIndex / 8); const col = displayIndex % 8;
                const dark = (Math.floor(index / 8) + index % 8) % 2 === 1;
                const mapped = boardMap[index]; const piece = mapped?.piece;
                const concealedTarget = piece && view === "play" && piece.color !== playerSide &&
                  piece.id === "ghost" && !piece.visible;
                const showPiece = piece && piece.id !== "giant" && piece.square === index;
                return (
                  <button key={index} role="gridcell" aria-label={squareName(index)}
                    className={`square ${dark ? "dark" : "light"} ${selected === index || (Boolean(piece) && selected !== null && boardMap[selected]?.piece.uid === piece?.uid) ? "selected" : ""} ${legalTargets.has(index) ? "legal-target" : ""}`}
                    onClick={() => handleSquare(index)}
                    onDragOver={(event) => { if (!gameActive && (view === "analysis" || draftPlacementActive)) event.preventDefault(); }}
                    onDrop={() => dragPiece(index)}>
                    {col === 0 && <span className="rank-label">{flipped ? row + 1 : 10 - row}</span>}
                    {row === 9 && <span className="file-label">{flipped ? files[7 - col] : files[col]}</span>}
                    {legalTargets.has(index) && <span className={mapped && !concealedTarget ? "capture-ring" : "move-dot"} />}
                    {showPiece && <span className="board-piece-wrap" draggable={!gameActive && (view === "analysis" || draftPieceIsMovable(piece))} onDragStart={() => setDraggedUid(piece.uid)}><PieceToken piece={piece} view={view} playerSide={playerSide} /></span>}
                  </button>
                );
              })}
              {giants.map((piece) => {
                const displayed = footprintSquares(piece).map((square) => orderedSquares.indexOf(square)).filter((index) => index >= 0);
                if (displayed.length !== 4) return null;
                const rows = displayed.map((index) => Math.floor(index / 8)); const cols = displayed.map((index) => index % 8);
                return (
                  <div key={piece.uid} className={`giant-piece ${piece.color}`}
                    style={{ top: `${Math.min(...rows) * 10}%`, left: `${Math.min(...cols) * 12.5}%`, width: "25%", height: "20%" }}>
                    <button type="button" className="giant-drag-handle" draggable={!gameActive && (view === "analysis" || (draftPlacementActive && piece.color === playerSide && draftPlacedUids.includes(piece.uid)))}
                      onDragStart={() => setDraggedUid(piece.uid)} onClick={() => handleSquare(piece.square)}>
                      <PieceToken piece={piece} view={view} playerSide={playerSide} large />
                    </button>
                  </div>
                );
              })}
            </div>
          </div>
          <div className="player-card self"><span className={`player-token ${bottomColor === "white" ? "light" : ""}`}>{bottomColor === "white" ? "I" : "O"}</span><div><small>{bottomColor === "white" ? "IVORY" : "ONYX"}</small><strong>{playerSide === bottomColor ? "You" : "Ultimate Fish"}</strong></div><span className={`material-total ${turn === bottomColor ? "active" : ""}`}>{material[bottomColor]} pts</span></div>
        </section>

        <aside className="right-rail">
          {view === "draft" ? (
            <section className="panel draft-panel">
              <div className="panel-heading"><div><p className="eyebrow">RANKED 12-WINDOW DRAFT</p><h2>{draftWindow ? `${draftWindow.player === "white" ? "Ivory" : "Onyx"} ${draftWindow.action}${draftWindow.action === "pick" ? " & place" : ""}` : "Draft complete"}</h2></div><span className="phase-pill">{Math.min(draftPhase + 1, 12)}/12</span></div>
              <div className="recovery-callout"><span>i</span><p>{draftAiBusy ? "Ultimate Fish is choosing and placing this group…" : draftWindow ? draftWindow.player !== playerSide ? "Ultimate Fish controls this window; its new group appears when committed." : draftWindow.action === "ban" ? "Choose exactly one character to ban." : `Add at least ${draftWindow.addMin} points without taking the cumulative team above ${draftWindow.max}. Earlier groups are locked.` : "Draft complete. Both locked armies are ready to enter Play."}</p></div>
              <div className="draft-summary"><div><small>IVORY</small><strong>{pointsFor(draftTeams.white)}</strong></div><div><small>ONYX</small><strong>{pointsFor(draftTeams.black)}</strong></div><div><small>BANNED</small><strong>{banned.length}/6</strong></div></div>
              <div className="modal-actions"><button onClick={resetDraft}>Reset draft</button><button className="primary-button" onClick={draftWindow ? commitDraftWindow : beginDraftGame} disabled={draftAiBusy || Boolean(draftWindow && (draftWindow.player !== playerSide || (draftWindow.action === "ban" ? draftPending.length !== 1 : draftAddedPoints < draftWindow.addMin || draftPoints > draftWindow.max || draftPlacementPool.length > 0)))}>{draftWindow ? `Commit ${draftWindow.action}` : "Start drafted game"}</button></div>
              <div className="draft-list">
                {roster.filter((piece) => !generatedDraftPieces.has(piece.id)).map((piece) => {
                  const pendingCount = draftPending.filter((id) => id === piece.id).length;
                  const footprint = piece.id === "giant" ? 4 : piece.id === "copycat" ? 2 : 1;
                  const unavailable = !draftWindow || draftWindow.player !== playerSide || banned.includes(piece.id) || (draftWindow.action === "pick" && (draftPoints + draftCosts[piece.id] > draftWindow.max || deploymentSlots(draftTeam) + footprint > 24));
                  return <div className={`draft-row ${banned.includes(piece.id) ? "is-banned" : ""}`} key={piece.id}>
                    <span className="mini-mark"><PieceIcon id={piece.id} /></span><div><strong>{piece.name}</strong><small>{piece.family} · {draftCosts[piece.id]} pts · {footprint} cell{footprint > 1 ? "s" : ""}</small></div>
                    <button className={pendingCount ? "picked" : ""} disabled={unavailable} onClick={() => chooseDraft(piece.id)}>{banned.includes(piece.id) ? "Banned" : draftWindow?.action === "ban" ? (pendingCount ? "Selected" : "Ban") : pendingCount ? `Add (${pendingCount})` : "Add"}</button>
                    <button disabled={!pendingCount || draftWindow?.player !== playerSide} onClick={() => unchooseDraft(piece.id)}>Remove</button>
                  </div>;
                })}
              </div>
            </section>
          ) : (
            <>
              <section className={`panel evaluation-panel ${!showEvaluation ? "game-only" : ""}`}>
                <div className="panel-heading"><div><p className="eyebrow">{view === "play" ? "GAME" : "ENGINE"}</p><h2>{view === "play" ? (gameActive ? "Ultimate game" : "Game setup") : "Analysis"}</h2></div>{showEvaluation && <span className="eval-score">{score.label}</span>}</div>
                {showEvaluation && <><div className="eval-track" aria-label={`Ivory evaluation ${score.label}`}><span style={{ width: `${score.percent}%` }} /></div><div className="engine-line"><strong>{analysis?.pv.slice(0, 8).join(" ") || "No engine line yet"}</strong></div></>}
                <div className="move-history"><div className="history-head"><span>#</span><span>Ivory</span><span>Onyx</span></div>{historyRows.length ? historyRows.map((row, index) => <div className="history-row" key={index}><span>{index + 1}.</span>{historyCell(row.white)}{historyCell(row.black)}</div>) : <p className="empty-state">Moves will appear here as the game is played.</p>}</div>
                <div className="engine-settings"><label>Depth <output>{depth}</output><input type="range" min="1" max="16" value={depth} onChange={(event) => setDepth(Number(event.target.value))} /></label></div>
                {view === "analysis" ? <button className={`primary-button ${analysisRunning ? "stop-button" : ""}`} onClick={() => { if (analysisRunning) { setAnalysisRunning(false); analysisAbort.current?.abort(); setEngineStatus("ready"); setEngineMessage(`${legalMoves.length} legal moves · ${(analysis?.nodes ?? 0).toLocaleString()} nodes in ${analysis?.time ?? 0} ms`); } else setAnalysisRunning(true); }}>{analysisRunning ? "Stop Ultimate Analysis" : "Start Ultimate Analysis"}</button> : <button className={`primary-button ${gameActive ? "stop-button" : ""}`} onClick={() => gameActive ? stopGame() : void startGame()}>{gameActive ? "Stop Ultimate Game" : "Start Ultimate Game"}</button>}
                <p className="disabled-note">{engineStatus === "thinking" ? "Ultimate Fish is thinking…" : engineMessage}</p>
              </section>
              <section className="panel inspector-panel">
                <div className="panel-heading"><div><p className="eyebrow">INSPECTOR</p><h2>{selected === null ? "Select a square" : squareName(selected)}</h2></div></div>
                {selectedPiece ? <div className="piece-detail"><PieceToken piece={selectedPiece} view={view} playerSide={playerSide} /><div><strong>{pieceById.get(inspectedId!)?.name}</strong><small>{inspectedId === "king" && selectedPiece.id === "jester" ? 0 : piecePoints(selectedPiece)} material pts · {pieceById.get(inspectedId!)?.summary}</small><div className="state-chips">{selectedPiece.power > 0 && <span>power {selectedPiece.power + 1}</span>}{selectedPiece.cooldown > 0 && <span>cooldown {selectedPiece.cooldown}</span>}{selectedPiece.freeze > 0 && <span>frozen ×{selectedPiece.freeze}</span>}{selectedPiece.id === "ghost" && <span>{selectedPiece.visible ? "revealed" : "hidden"}</span>}</div></div></div> : <p className="empty-state">Select a character to inspect its native state and preview every legal destination.</p>}
              </section>
            </>
          )}
        </aside>
      </section>

      {positionOpen && <div className="modal-backdrop" role="presentation" onMouseDown={() => setPositionOpen(false)}><section className="position-modal" role="dialog" aria-modal="true" aria-labelledby="position-title" onMouseDown={(event) => event.stopPropagation()}><div className="panel-heading"><div><p className="eyebrow">CUSTOM ANALYSIS</p><h2 id="position-title">Ultimate Position</h2></div><button className="icon-button" onClick={() => setPositionOpen(false)}>×</button></div><p>Paste lossless UPN to preserve cooldowns, Ghost visibility, links, queued actions, and off-board Angels. Ultimate Position JSON v1 is also accepted.</p><textarea value={positionText} onChange={(event) => setPositionText(event.target.value)} spellCheck={false} />{positionMessage && <div className="position-message">{positionMessage}</div>}<div className="modal-actions"><button onClick={() => navigator.clipboard?.writeText(upn)}>Copy current UPN</button><button className="primary-button" onClick={loadPosition}>Load position</button></div></section></div>}
    </main>
  );
}
