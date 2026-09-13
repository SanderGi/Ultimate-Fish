import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import path from "node:path";
import test from "node:test";
import {
  PASSWORD_EXEMPT_PATH,
  isPasswordExemptPath,
} from "../password-exemption.mjs";

const uiDirectory = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const repository = path.resolve(uiDirectory, "..");

test("production image serves the canonical tablebase plot", async () => {
  const [dockerfile, plot, proxy] = await Promise.all([
    readFile(path.join(repository, "Dockerfile"), "utf8"),
    readFile(path.join(repository, "tablebases", "ultimate-tablebase-grid.svg"), "utf8"),
    readFile(path.join(uiDirectory, "proxy.ts"), "utf8"),
  ]);

  assert.match(plot, /^<\?xml[^>]*>\s*<svg\b/s);
  assert.match(
    dockerfile,
    /^COPY tablebases\/ultimate-tablebase-grid\.svg public\/ultimate-tablebase-grid\.svg$/m,
  );
  assert.match(
    dockerfile,
    /^COPY --from=ui-builder \/build\/ui\/public public$/m,
  );
  assert.match(proxy, /matcher:\s*["']\/:path\*["']/);
  assert.match(proxy, /isPasswordExemptPath\(request\.nextUrl\.pathname\)/);
});

test("the canonical tablebase plot and explicit fly resources are password-exempt", () => {
  assert.equal(PASSWORD_EXEMPT_PATH, "/ultimate-tablebase-grid.svg");
  assert.equal(isPasswordExemptPath(PASSWORD_EXEMPT_PATH), true);

  for (const protectedPath of [
    "/",
    "/api/engine/state",
    "/api/fly/anything", "/fly/engine", "/fly/../api/engine/state",
    "/fly/data/private.json", "/fly/rules.mjs",
    "/ultimate-fish-logo.png",
    "/ultimate-tablebase-grid.svg/",
    "/plots/ultimate-tablebase-grid.svg",
    "/ultimate-tablebase-grid.svg.backup",
    "/Ultimate-tablebase-grid.svg",
  ]) {
    assert.equal(isPasswordExemptPath(protectedPath), false, protectedPath);
  }
});
