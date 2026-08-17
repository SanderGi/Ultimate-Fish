# syntax=docker/dockerfile:1

FROM node:22-bookworm-slim AS engine-builder

RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY src/ src/
RUN make -C src ultimatefish


FROM node:22-bookworm-slim AS ui-builder

WORKDIR /build/ui
COPY ui/package.json ui/package-lock.json ./
RUN --mount=type=cache,target=/root/.npm npm ci
COPY ui/ ./
RUN npm run build


FROM node:22-bookworm-slim AS runner

RUN apt-get update \
    && apt-get install -y --no-install-recommends python3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=engine-builder /build/src/ultimatefish src/ultimatefish
COPY tools/ tools/
COPY tablebases/*.uftb tablebases/*.ufgm tablebases/

WORKDIR /app/ui
COPY --from=ui-builder /build/ui/.next/standalone ./
COPY --from=ui-builder /build/ui/.next/static .next/static
COPY --from=ui-builder /build/ui/public public
COPY ui/engine-server.mjs ui/engine-settings.mjs ui/start-production.mjs ./

ENV NODE_ENV=production \
    HOSTNAME=0.0.0.0 \
    PORT=8080 \
    ULTIMATE_FISH_BINARY=/app/src/ultimatefish \
    ULTIMATE_FISH_PYTHON=python3 \
    ULTIMATE_FISH_PORT=3001 \
    ULTIMATE_TABLEBASE_PATH=/app/tablebases

EXPOSE 8080
USER node

CMD ["node", "start-production.mjs"]
