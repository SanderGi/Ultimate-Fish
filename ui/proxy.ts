import { timingSafeEqual } from "node:crypto";
import { NextRequest, NextResponse } from "next/server";

function equal(left: string, right: string) {
  const leftBytes = Buffer.from(left);
  const rightBytes = Buffer.from(right);
  return leftBytes.length === rightBytes.length &&
    timingSafeEqual(leftBytes, rightBytes);
}

function unauthorized() {
  return new NextResponse("Authentication required.", {
    status: 401,
    headers: {
      "cache-control": "no-store",
      "www-authenticate": 'Basic realm="Ultimate Fish", charset="UTF-8"',
    },
  });
}

export function proxy(request: NextRequest) {
  const password = process.env.UI_PASSWORD;
  if (!password) {
    if (process.env.NODE_ENV !== "production") return NextResponse.next();
    return new NextResponse("UI_PASSWORD is not configured.", {
      status: 503,
      headers: { "cache-control": "no-store" },
    });
  }

  const authorization = request.headers.get("authorization");
  if (!authorization?.startsWith("Basic ")) return unauthorized();
  try {
    const credentials = Buffer.from(authorization.slice(6), "base64").toString("utf8");
    const separator = credentials.indexOf(":");
    if (separator < 0) return unauthorized();
    const username = credentials.slice(0, separator);
    const suppliedPassword = credentials.slice(separator + 1);
    const expectedUsername = process.env.UI_USERNAME ?? "ultimatefish";
    if (!equal(username, expectedUsername) || !equal(suppliedPassword, password))
      return unauthorized();
    return NextResponse.next();
  } catch {
    return unauthorized();
  }
}

export const config = { matcher: "/:path*" };
