import { spawn } from "node:child_process";

const children = new Set();
let stopping = false;
let exitCode = 0;

function launch(label, args, env = process.env) {
  const child = spawn(process.execPath, args, {
    cwd: new URL(".", import.meta.url),
    env,
    stdio: "inherit",
  });
  children.add(child);
  child.once("close", (code, signal) => {
    children.delete(child);
    if (!stopping) {
      exitCode = code || 1;
      console.error(`${label} exited unexpectedly (${signal ?? exitCode}).`);
      stop("SIGTERM");
    }
    if (!children.size) process.exit(exitCode);
  });
  return child;
}

function stop(signal) {
  if (stopping) return;
  stopping = true;
  for (const child of children) child.kill(signal);
  setTimeout(() => {
    for (const child of children) child.kill("SIGKILL");
  }, 5000).unref();
}

for (const signal of ["SIGINT", "SIGTERM"])
  process.once(signal, () => stop(signal));

launch("engine bridge", ["engine-server.mjs"]);
launch("Next server", ["server.js"], {
  ...process.env,
  HOSTNAME: process.env.HOSTNAME ?? "0.0.0.0",
  PORT: process.env.PORT ?? "8080",
});
