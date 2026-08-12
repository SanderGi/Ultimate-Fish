/** @typedef {"white" | "black"} Color */
/** @typedef {"play" | "analysis" | "draft"} View */

/**
 * Analysis is an omniscient board rendering: it never removes a Ghost. Play
 * and draft conceal only an invisible enemy Ghost from the selected viewer.
 *
 * @param {View} view
 * @param {Color} viewer
 * @param {{id: string, color: Color, visible: boolean}} piece
 */
export function concealGhost(view, viewer, piece) {
  return view !== "analysis" && piece.id === "ghost" && !piece.visible &&
    piece.color !== viewer;
}

/**
 * A hidden Ghost remains drawn in analysis, with the translucent treatment
 * keyed to the top-right viewer selector rather than the side to move.
 *
 * @param {View} view
 * @param {Color} viewer
 * @param {{id: string, color: Color, visible: boolean}} piece
 */
export function fadeGhost(view, viewer, piece) {
  return view === "analysis" && piece.id === "ghost" && !piece.visible &&
    piece.color !== viewer;
}

/**
 * Only Play renders an enemy Jester as a royal silhouette. Within Play, only
 * the exact surviving first-pick candidate set is disguised. An omitted set
 * means every enemy Jester may still be the King.
 *
 * @param {View} view
 * @param {Color} viewer
 * @param {{id: string, color: Color, square: number}} piece
 * @param {boolean} enemyKingKnown
 * @param {string[] | undefined} enemyKingCandidates
 * @param {(square: number) => string} squareName
 */
export function disguiseJester(
  view, viewer, piece, enemyKingKnown, enemyKingCandidates, squareName,
) {
  return view === "play" && piece.color !== viewer && piece.id === "jester" &&
    !enemyKingKnown && (enemyKingCandidates === undefined ||
      enemyKingCandidates.includes(squareName(piece.square)));
}

/**
 * Piece-knowledge diagnostics are private to that piece's owner during play
 * and draft. Analysis may inspect either side from the selected viewer's
 * reconstructed information set.
 *
 * @param {View} view
 * @param {Color} viewer
 * @param {Color} pieceColor
 */
export function showKnowledgeStatus(view, viewer, pieceColor) {
  return view === "analysis" || viewer === pieceColor;
}

/**
 * Build a replayable belief without tying the stored disclosure records to the
 * currently selected viewer. `contextPlayer` identifies which observer owns
 * `playerDisclosure`; changing `viewer` only changes presentation.
 *
 * @param {string} initialUpn
 * @param {string[]} moves
 * @param {"arbitrary" | "draft"} initialPositionKind
 * @param {Color} viewer
 * @param {Color} contextPlayer
 * @param {{enemyKingKnown: boolean, enemyKingCandidates?: string[]}} playerDisclosure
 * @param {{enemyKingKnown: boolean, enemyKingCandidates?: string[]}} engineDisclosure
 */
export function buildReplayBelief(
  initialUpn, moves, initialPositionKind, viewer, contextPlayer,
  playerDisclosure, engineDisclosure,
) {
  const clone = (disclosure) => ({
    enemyKingKnown: disclosure.enemyKingKnown,
    ...(disclosure.enemyKingCandidates?.length
      ? { enemyKingCandidates: [...disclosure.enemyKingCandidates] }
      : {}),
  });
  const other = contextPlayer === "white" ? "black" : "white";
  return {
    format: "ultimate-belief",
    version: 1,
    initialUpn,
    moves: [...moves],
    initialPositionKind,
    viewer,
    observers: {
      [contextPlayer]: clone(playerDisclosure),
      [other]: clone(engineDisclosure),
    },
  };
}
