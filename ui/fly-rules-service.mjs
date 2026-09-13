import {execFile} from 'node:child_process';
import path from 'node:path';

let active = 0;
export function validGame(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value)
    && Object.keys(value).every(key => ['preset', 'reflection', 'moves'].includes(key))
    && Number.isInteger(value.preset) && value.preset >= 0 && value.preset <= 2
    && Number.isInteger(value.reflection) && value.reflection >= 0 && value.reflection <= 1
    && Array.isArray(value.moves) && value.moves.length <= 400
    && value.moves.every(index => Number.isInteger(index) && index >= 0 && index < 4096);
}

// Like the analysis UI, requests run the native C++ rules in a server subprocess.
// A bounded public history replaces arbitrary UPN import and mutable server sessions.
export async function refereeGame(game, signal) {
  if (!validGame(game)) return {status: 400, body: {error: 'Invalid game history.'}};
  if (active >= 4) return {status: 429, body: {error: 'The referee is busy. Please retry.'}};
  ++active;
  try {
    const binary = process.env.ULTIMATE_FLY_RULES_BINARY || path.resolve(process.cwd(), '../src/ultimate_fly_rules');
    const stdout = await new Promise((resolve, reject) => {
      const child = execFile(binary, [], {timeout: 10000, maxBuffer: 1024 * 1024, signal}, (error, stdout) => {
        if (error) reject(error); else resolve(stdout);
      });
      child.stdin.on('error', () => {}); // The process callback handles early exits.
      child.stdin.end(`${game.preset} ${game.reflection} ${game.moves.length}\n${game.moves.join(' ')}\n`);
    });
    const body = JSON.parse(stdout);
    return {status: body.error ? 400 : 200, body};
  } catch {
    return {status: 503, body: {error: 'The native referee is unavailable. Please retry.'}};
  } finally {
    --active;
  }
}
