import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import path from "node:path";
import test from "node:test";

const uiDirectory = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const repository = path.resolve(uiDirectory, "..");

test("production image serves the canonical tablebase plot behind authentication", async () => {
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
});
