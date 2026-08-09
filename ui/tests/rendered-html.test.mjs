import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import path from "node:path";
import test, { after, before } from "node:test";

const base = process.env.ULTIMATE_FISH_UI_TEST_URL ?? "http://127.0.0.1:3010";
const uiDirectory = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
let server;

before(async () => {
  if (process.env.ULTIMATE_FISH_UI_TEST_URL) return;
  server = spawn(
    process.execPath,
    ["node_modules/next/dist/bin/next", "dev", "--webpack", "--hostname", "127.0.0.1", "--port", "3010"],
    {
      cwd: uiDirectory,
      env: { ...process.env, NEXT_TELEMETRY_DISABLED: "1" },
      stdio: ["ignore", "pipe", "pipe"],
    },
  );
  let output = "";
  server.stdout.on("data", (chunk) => { output += chunk; });
  server.stderr.on("data", (chunk) => { output += chunk; });
  for (let attempt = 0; attempt < 200; attempt += 1) {
    if (server.exitCode !== null)
      throw new Error(`Local UI exited before becoming ready:\n${output}`);
    try {
      const response = await fetch(base);
      if (response.ok) return;
    } catch {
      // Next may still be starting or compiling the initial route.
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  server.kill("SIGTERM");
  throw new Error(`Timed out waiting for the local UI:\n${output}`);
});

after(async () => {
  if (!server || server.exitCode !== null) return;
  const closed = new Promise((resolve) => server.once("close", resolve));
  server.kill("SIGTERM");
  await closed;
});

test("local server renders the Ultimate Fish workbench", async () => {
  const response = await fetch(base, { headers: { accept: "text/html" } });
  assert.equal(response.status, 200);
  assert.match(response.headers.get("content-type") ?? "", /^text\/html\b/i);
  const html = await response.text();
  assert.match(html, /<title>Ultimate Fish<\/title>/i);
  assert.match(html, /Ultimate Fish/);
  assert.match(html, /<img[^>]+src="\/ultimate-fish-logo\.png"/i);
  assert.match(html, /<svg[^>]+piece-icon piece-icon-king white/i);
  assert.match(html, /<svg[^>]+piece-icon piece-icon-king black/i);
  assert.match(html, /Board tools/);
  assert.ok(html.indexOf("Side to move") < html.indexOf("Place as"));
  assert.match(html, /Start Ultimate Analysis/);
  assert.match(html, /Max depth/);
  assert.match(html, /<input[^>]+type="range"[^>]+max="50"[^>]+value="16"/i);
});
