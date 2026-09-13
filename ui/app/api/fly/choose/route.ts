import { inferFly, isSameOrigin } from '../../../../fly-service.mjs';
export const runtime = 'nodejs';
export const dynamic = 'force-dynamic';
export async function POST(request: Request) {
  if (!isSameOrigin(request))
    return Response.json({ error: 'Same-origin requests only.' }, { status: 403 });
  const reader = request.body?.getReader();
  if (!reader) return Response.json({ error: 'Missing moves.' }, { status: 400 });
  let size = 0;
  const chunks: Uint8Array[] = [];
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    size += value.length;
    if (size > 32768) {
      await reader.cancel();
      return Response.json({ error: 'Request too large.' }, { status: 413 });
    }
    chunks.push(value);
  }
  let candidates;
  try { candidates = JSON.parse(Buffer.concat(chunks).toString('utf8')).candidates; }
  catch { return Response.json({ error: 'Invalid JSON.' }, { status: 400 }); }
  const result = await inferFly(candidates);
  return Response.json(result.body, { status: result.status, headers: {
    'cache-control': 'no-store', ...(result.status === 429 ? { 'retry-after': '5' } : {}),
  } });
}
