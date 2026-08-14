import assert from "node:assert/strict";
import test from "node:test";

import {
  analysisPerspective, applyEditorGhostVisibility, buildReplayBelief,
  concealGhost, disguiseJester,
  fadeGhost,
  showKnowledgeStatus,
} from "../app/public-information.mjs";

test("editor moves apply royal proximity to Ghost visibility", () => {
  const pieces = [
    { uid: "jester", id: "jester", color: "white", square: 76,
      visible: true }, // e1
    { uid: "ghost", id: "ghost", color: "black", square: 69,
      visible: false }, // f2
    { uid: "other", id: "ghost", color: "black", square: 8,
      visible: false },
  ];
  const ghostMoved = applyEditorGhostVisibility(pieces, "ghost");
  assert.equal(ghostMoved[1].visible, true);
  assert.equal(ghostMoved[2].visible, false);

  const ghostMovedAway = applyEditorGhostVisibility(
    ghostMoved.map((piece) => piece.uid === "ghost"
      ? { ...piece, square: 40 }
      : piece),
    "ghost",
  );
  assert.equal(ghostMovedAway[1].visible, false);

  const trackedMovedAway = applyEditorGhostVisibility(
    ghostMovedAway.map((piece) => piece.uid === "ghost"
      ? { ...piece, parasiteTracked: true }
      : piece),
    "ghost",
  );
  assert.equal(trackedMovedAway[1].visible, true);

  const royalMoved = applyEditorGhostVisibility(
    pieces.map((piece) => piece.uid === "jester"
      ? { ...piece, square: 9 }
      : piece),
    "jester",
  );
  assert.equal(royalMoved[2].visible, true);
});

const hiddenIvoryGhost = {
  id: "ghost", color: "white", visible: false, square: 12,
};

test("analysis always draws Ghosts and fades by selected viewer", () => {
  assert.equal(concealGhost("analysis", "black", hiddenIvoryGhost), false);
  assert.equal(fadeGhost("analysis", "black", hiddenIvoryGhost), true);
  assert.equal(fadeGhost("analysis", "white", hiddenIvoryGhost), false);

  // No side-to-move parameter exists: changing the game turn cannot alter an
  // analysis rendering decision.
  assert.equal(concealGhost("play", "black", hiddenIvoryGhost), true);
  assert.equal(concealGhost("play", "white", hiddenIvoryGhost), false);
});

test("only Play disguises first-pick royal-candidate Jesters", () => {
  const firstGroup = { id: "jester", color: "white", square: 1 };
  const laterGroup = { id: "jester", color: "white", square: 2 };
  const name = (square) => ["a1", "b1", "c1"][square];

  assert.equal(disguiseJester(
    "play", "black", firstGroup, false, ["a1", "b1"], name,
  ), true);
  assert.equal(disguiseJester(
    "play", "black", laterGroup, false, ["a1", "b1"], name,
  ), false);
  assert.equal(disguiseJester(
    "play", "black", firstGroup, true, ["a1", "b1"], name,
  ), false);
  assert.equal(disguiseJester(
    "analysis", "black", firstGroup, false, ["a1", "b1"], name,
  ), false);
  assert.equal(disguiseJester(
    "draft", "black", firstGroup, false, ["a1", "b1"], name,
  ), false);
});

test("knowledge pills respect ownership outside analysis", () => {
  assert.equal(showKnowledgeStatus("play", "white", "white"), true);
  assert.equal(showKnowledgeStatus("play", "white", "black"), false);
  assert.equal(showKnowledgeStatus("draft", "black", "black"), true);
  assert.equal(showKnowledgeStatus("analysis", "white", "black"), true);
});

test("analysis belief follows the selected viewer, not the side to move", () => {
  const ivory = {
    enemyKingKnown: true,
    enemyKingCandidates: ["d10"],
  };
  const onyx = {
    enemyKingKnown: false,
    enemyKingCandidates: ["c1", "e1"],
  };

  assert.deepEqual(
    analysisPerspective("black", "white", ivory, onyx),
    {
      observer: "black",
      enemyKingKnown: false,
      enemyKingCandidates: ["c1", "e1"],
    },
  );
  assert.deepEqual(
    analysisPerspective("white", "white", ivory, onyx),
    {
      observer: "white",
      enemyKingKnown: true,
      enemyKingCandidates: ["d10"],
    },
  );
});

test("replayable beliefs retain both observers when the viewer changes", () => {
  const ivory = {
    enemyKingKnown: false,
    enemyKingCandidates: ["d10", "e10"],
  };
  const onyx = {
    enemyKingKnown: false,
    enemyKingCandidates: ["a1", "b1"],
  };
  const belief = buildReplayBelief(
    "w;king,w,a1;jester,w,b1;king,b,d10;jester,b,e10",
    ["a1-a2"], "draft", "black", "white", ivory, onyx,
  );

  assert.equal(belief.viewer, "black");
  assert.equal(belief.version, 1);
  assert.deepEqual(belief.observers.white, ivory);
  assert.deepEqual(belief.observers.black, onyx);
  assert.notEqual(belief.observers.white, ivory);
  assert.notEqual(belief.observers.black.enemyKingCandidates,
    onyx.enemyKingCandidates);
});
