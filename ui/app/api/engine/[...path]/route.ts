import { NextRequest } from "next/server";

export const dynamic = "force-dynamic";

type RouteContext = { params: Promise<{ path: string[] }> };

async function forward(request: NextRequest, context: RouteContext) {
  const { path } = await context.params;
  const engineOrigin = process.env.ULTIMATE_FISH_ENGINE_URL ?? "http://127.0.0.1:3001";
  const target = new URL(`/${path.map(encodeURIComponent).join("/")}`, engineOrigin);
  const headers = new Headers();
  const contentType = request.headers.get("content-type");
  if (contentType) headers.set("content-type", contentType);

  try {
    const upstream = await fetch(target, {
      method: request.method,
      headers,
      body: request.method === "GET" || request.method === "HEAD"
        ? undefined
        : await request.arrayBuffer(),
      cache: "no-store",
      signal: request.signal,
    });
    const responseHeaders = new Headers();
    for (const name of ["content-type", "cache-control"]) {
      const value = upstream.headers.get(name);
      if (value) responseHeaders.set(name, value);
    }
    return new Response(upstream.body, {
      status: upstream.status,
      headers: responseHeaders,
    });
  } catch (error) {
    return Response.json(
      {
        error: error instanceof Error
          ? `Engine bridge unavailable: ${error.message}`
          : "Engine bridge unavailable.",
      },
      { status: 502 },
    );
  }
}

export const GET = forward;
export const POST = forward;
