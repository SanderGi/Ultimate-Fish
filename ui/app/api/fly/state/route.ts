import { isSameOrigin } from '../../../../fly-service.mjs';
import { refereeGame } from '../../../../fly-rules-service.mjs';

export const runtime = 'nodejs';
export const dynamic = 'force-dynamic';

export async function POST(request: Request) {
  if (!isSameOrigin(request))
    return Response.json({ error: 'Same-origin requests only.' }, { status: 403 });
  const reader = request.body?.getReader();
  if (!reader) return Response.json({ error: 'Missing game history.' }, { status: 400 });
  let size = 0;
  const chunks: Uint8Array[] = [];
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    size += value.length;
    if (size > 8192) {
      await reader.cancel();
      return Response.json({ error: 'Request too large.' }, { status: 413 });
    }
    chunks.push(value);
  }
  let game;
  try { game = JSON.parse(Buffer.concat(chunks).toString('utf8')); }
  catch { return Response.json({ error: 'Invalid JSON.' }, { status: 400 }); }
  const result = await refereeGame(game, request.signal);
  return Response.json(result.body, { status: result.status, headers: {
    'cache-control': 'no-store', ...(result.status === 429 ? { 'retry-after': '1' } : {}),
  } });
}
