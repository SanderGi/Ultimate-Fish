import assert from "node:assert/strict";
import test from "node:test";
import {
  MAX_ENGINE_SEARCH_DEPTH,
  normalizedSearchDepth,
} from "../engine-settings.mjs";

test("all engine analysis modes accept the UI depth range", () => {
  assert.equal(MAX_ENGINE_SEARCH_DEPTH, 50);
  assert.equal(normalizedSearchDepth(17), 17);
  assert.equal(normalizedSearchDepth(50), 50);
  assert.equal(normalizedSearchDepth(51), 50);
});

test("specialized callers can retain a smaller explicit ceiling", () => {
  assert.equal(normalizedSearchDepth(20, 4, 16), 16);
  assert.equal(normalizedSearchDepth(undefined, 6), 6);
  assert.equal(normalizedSearchDepth(0), 4);
});
