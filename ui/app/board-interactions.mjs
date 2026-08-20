const files = "abcdefgh";

/**
 * Return the four engine squares occupied by a Giant whose canonical anchor is
 * its lower-left square.
 *
 * @param {number} anchor
 * @returns {number[]}
 */
export function giantFootprintSquaresAt(anchor) {
  const file = anchor % 8;
  const row = Math.floor(anchor / 8);
  return file < 7 && row > 0
    ? [anchor, anchor + 1, anchor - 8, anchor - 7]
    : [];
}

/**
 * Resolve a pointer inside the Giant's 2x2 artwork to the actual engine square
 * under that quadrant. The artwork rotates with a flipped board, so derive the
 * result through display coordinates instead of assuming the anchor is always
 * visually bottom-left.
 *
 * @param {number} anchor
 * @param {boolean} flipped
 * @param {number} horizontalRatio
 * @param {number} verticalRatio
 * @returns {number}
 */
export function giantPointerSquare(
  anchor,
  flipped,
  horizontalRatio,
  verticalRatio,
) {
  const cells = giantFootprintSquaresAt(anchor).map((square) => {
    const display = flipped ? 79 - square : square;
    return {
      square,
      row: Math.floor(display / 8),
      column: display % 8,
    };
  });
  if (cells.length !== 4) return anchor;

  const top = Math.min(...cells.map((cell) => cell.row));
  const left = Math.min(...cells.map((cell) => cell.column));
  const localColumn = horizontalRatio >= 0.5 ? 1 : 0;
  const localRow = verticalRatio >= 0.5 ? 1 : 0;
  return cells.find(
    (cell) => cell.row === top + localRow && cell.column === left + localColumn,
  )?.square ?? anchor;
}

/**
 * Map every highlighted destination cell to the exact engine action it must
 * submit. Most pieces target one exact square, including any particular cell
 * of a Giant. A moving Giant is the exception: each cell in its destination
 * footprint selects the same anchor-based Giant action.
 *
 * @param {readonly string[]} legalMoves
 * @param {number} selectedSquare
 * @param {boolean} selectedIsGiant
 * @returns {Map<number, string>}
 */
export function legalTargetMoveMap(
  legalMoves,
  selectedSquare,
  selectedIsGiant,
) {
  const targets = new Map();
  const selectedName = `${files[selectedSquare % 8]}${10 - Math.floor(selectedSquare / 8)}`;
  for (const move of legalMoves) {
    const match = move.match(
      /^([a-h](?:10|[1-9]))[-~@x!&]([a-h](?:10|[1-9]))$/,
    );
    if (match?.[1] !== selectedName) continue;
    const destination =
      (10 - Number(match[2].slice(1))) * 8 + files.indexOf(match[2][0]);
    const footprint = selectedIsGiant
      ? giantFootprintSquaresAt(destination)
      : [destination];
    for (const square of footprint) {
      // Giant destinations can overlap. Prefer the action whose canonical
      // anchor was clicked; otherwise retain the first highlighted footprint.
      if (!targets.has(square) || square === destination)
        targets.set(square, move);
    }
  }
  return targets;
}
