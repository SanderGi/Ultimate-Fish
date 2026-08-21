import assert from "node:assert/strict";
import test from "node:test";

import { pieceStateLabels } from "../app/piece-state.mjs";

const piece = (id, state = {}) => ({
  id,
  action: 0,
  cooldown: 0,
  freeze: 0,
  power: 0,
  moved: false,
  visible: true,
  ...state,
});

test("inspector labels show ready and active special-piece state", () => {
  assert.deepEqual(pieceStateLabels(piece("ghost", { visible: false })), [
    "hidden",
  ]);
  assert.deepEqual(pieceStateLabels(piece("berserker", { power: 2 })), [
    "power 3",
  ]);
  assert.deepEqual(pieceStateLabels(piece("pawn")), ["double-step ready"]);
  assert.deepEqual(pieceStateLabels(piece("pawn", { moved: true })), [
    "moved",
  ]);
  assert.deepEqual(pieceStateLabels(piece("sniper")), ["shot ready"]);
  assert.deepEqual(pieceStateLabels(piece("sniper", { cooldown: 3 })), [
    "reload 3",
  ]);
  assert.deepEqual(pieceStateLabels(piece("devil")), ["spawn ready"]);
  assert.deepEqual(pieceStateLabels(piece("devil", { cooldown: 2 })), [
    "cooldown 2",
  ]);
  assert.deepEqual(pieceStateLabels(piece("penguin", { action: 0b10101 })), [
    "aura active",
  ]);
});

test("inspector labels retain generic and linked states", () => {
  assert.deepEqual(
    pieceStateLabels(piece("copycat", { link: "clone", freeze: 2 })),
    ["paired", "frozen ×2"],
  );
  assert.deepEqual(pieceStateLabels(piece("rook")), ["unmoved"]);
  assert.deepEqual(pieceStateLabels(piece("angel", { link: "halo" })), [
    "linked",
  ]);
});
