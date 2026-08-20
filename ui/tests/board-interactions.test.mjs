import assert from "node:assert/strict";
import test from "node:test";

import {
  giantFootprintSquaresAt,
  giantPointerSquare,
  legalTargetMoveMap,
} from "../app/board-interactions.mjs";

const f6 = 37;
const f7 = 29;
const g7 = 30;
const g6 = 38;

test("every Giant artwork quadrant resolves to its own board square", () => {
  assert.deepEqual(giantFootprintSquaresAt(f6), [f6, g6, f7, g7]);

  assert.equal(giantPointerSquare(f6, false, 0.25, 0.25), f7);
  assert.equal(giantPointerSquare(f6, false, 0.75, 0.25), g7);
  assert.equal(giantPointerSquare(f6, false, 0.25, 0.75), f6);
  assert.equal(giantPointerSquare(f6, false, 0.75, 0.75), g6);
});

test("Giant quadrant targeting follows a flipped board", () => {
  assert.equal(giantPointerSquare(f6, true, 0.25, 0.25), g6);
  assert.equal(giantPointerSquare(f6, true, 0.75, 0.25), f6);
  assert.equal(giantPointerSquare(f6, true, 0.25, 0.75), g7);
  assert.equal(giantPointerSquare(f6, true, 0.75, 0.75), f7);
});

test("all action syntaxes retain the exact clicked Giant footprint cell", () => {
  const interactions = [
    ["c2~f7", f7], // Mage swap
    ["c3!g7", g7], // Fisherman pull
    ["f3xf7", f7], // Sniper shot or ordinary capture
    ["c4&g6", g6], // Angel link
    ["c5@g7", g7], // Devil spawn target
    ["c6-f6", f6], // Ordinary, Ninja, Parasite, or Checker action
  ];

  for (const [move, target] of interactions) {
    // Each fixture's actor file/rank differs, so select its parsed source.
    const source = move.match(/^([a-h](?:10|[1-9]))/)?.[1];
    assert.ok(source);
    const sourceSquare =
      (10 - Number(source.slice(1))) * 8 + "abcdefgh".indexOf(source[0]);
    const exactMap = legalTargetMoveMap([move], sourceSquare, false);
    assert.equal(exactMap.get(target), move);
    assert.equal(exactMap.size, 1);
  }
});

test("clicking any destination cell selects an anchor-based Giant move", () => {
  const d4 = 51;
  const move = "d4-f6";
  const map = legalTargetMoveMap([move], d4, true);
  for (const target of [f6, g6, f7, g7])
    assert.equal(map.get(target), move);
});
