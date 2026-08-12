export type InformationColor = "white" | "black";
export type InformationView = "play" | "analysis" | "draft";
export type InformationPiece = {
  id: string;
  color: InformationColor;
  square: number;
  visible: boolean;
};
export type HistoryDisclosure = {
  enemyKingKnown: boolean;
  enemyKingCandidates?: string[];
};
export type ReplayBeliefDocument = {
  format: "ultimate-belief";
  version: 1;
  initialUpn: string;
  moves: string[];
  initialPositionKind: "arbitrary" | "draft";
  viewer: InformationColor;
  observers: Record<InformationColor, HistoryDisclosure>;
};

export function concealGhost(
  view: InformationView, viewer: InformationColor,
  piece: InformationPiece,
): boolean;
export function fadeGhost(
  view: InformationView, viewer: InformationColor,
  piece: InformationPiece,
): boolean;
export function disguiseJester(
  view: InformationView, viewer: InformationColor,
  piece: InformationPiece, enemyKingKnown: boolean,
  enemyKingCandidates: string[] | undefined,
  squareName: (square: number) => string,
): boolean;
export function showKnowledgeStatus(
  view: InformationView, viewer: InformationColor,
  pieceColor: InformationColor,
): boolean;
export function buildReplayBelief(
  initialUpn: string, moves: string[],
  initialPositionKind: "arbitrary" | "draft",
  viewer: InformationColor, contextPlayer: InformationColor,
  playerDisclosure: HistoryDisclosure,
  engineDisclosure: HistoryDisclosure,
): ReplayBeliefDocument;
