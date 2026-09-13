# syntax=docker/dockerfile:1

FROM python:3.12-slim AS fly-data-builder
WORKDIR /build
RUN pip install --no-cache-dir numpy==2.2.6 pyarrow==25.0.1
COPY tools/fly/download-data.py tools/fly/prepare-connectome.py tools/fly/
RUN --mount=type=cache,id=ultimate-fly-malecns-v1,target=/tmp/malecns,sharing=locked \
    python tools/fly/download-data.py /tmp/malecns \
    && python tools/fly/prepare-connectome.py /tmp/malecns /build/fly-data

FROM node:22-bookworm-slim AS native-builder

RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

FROM native-builder AS engine-builder
COPY src/ src/
RUN make -C src ultimatefish

FROM native-builder AS fly-brain-builder
COPY tools/fly/build-native.sh tools/fly/brain.cpp tools/fly/
RUN tools/fly/build-native.sh brain

FROM native-builder AS fly-rules-builder
COPY src/ultimate/position.cpp src/ultimate/position.h src/ultimate/nnue.h src/ultimate/
COPY tools/fly/build-native.sh tools/fly/rules.cpp tools/fly/rules-server.cpp tools/fly/
RUN tools/fly/build-native.sh rules


FROM node:22-bookworm-slim AS ui-builder

WORKDIR /build/ui
COPY ui/package.json ui/package-lock.json ./
RUN --mount=type=cache,target=/root/.npm npm ci
COPY ui/ ./
COPY docs/fly/ /build/docs/fly/
COPY src/ultimate/ /build/src/ultimate/
COPY tools/fly/ /build/tools/fly/
COPY Copying.txt /build/Copying.txt
COPY tablebases/ultimate-tablebase-grid.svg public/ultimate-tablebase-grid.svg
RUN npm run build


FROM node:22-bookworm-slim AS runner

RUN apt-get update \
    && apt-get install -y --no-install-recommends python3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
RUN mkdir -p tablebases && chown node:node tablebases
COPY --from=engine-builder /build/src/ultimatefish src/ultimatefish
COPY --from=fly-brain-builder /build/src/ultimate_fly_brain src/ultimate_fly_brain
COPY --from=fly-rules-builder /build/src/ultimate_fly_rules src/ultimate_fly_rules
COPY --from=fly-data-builder /build/fly-data/connectome.bin networks/fly/connectome.bin
# Public draft search and its transitive local Python imports only.
COPY tools/search_ultimate_public_draft.py tools/evolve_ultimate_army.py \
    tools/evolve_ultimate_draft.py tools/ultimate_phone.py tools/

WORKDIR /app/ui
COPY --from=ui-builder /build/ui/.next/standalone ./
COPY --from=ui-builder /build/ui/.next/static .next/static
COPY --from=ui-builder /build/ui/public public
COPY ui/engine-server.mjs ui/engine-settings.mjs ui/tablebase-manager.mjs ui/start-production.mjs ./

ENV NODE_ENV=production \
    HOSTNAME=0.0.0.0 \
    PORT=8080 \
    ULTIMATE_FISH_BINARY=/app/src/ultimatefish \
    ULTIMATE_FLY_BINARY=/app/src/ultimate_fly_brain \
    ULTIMATE_FLY_RULES_BINARY=/app/src/ultimate_fly_rules \
    ULTIMATE_FLY_DATA=/app/networks/fly/connectome.bin \
    ULTIMATE_FISH_PYTHON=python3 \
    ULTIMATE_FISH_PORT=3001 \
    ULTIMATE_TABLEBASE_PATH=/app/tablebases

EXPOSE 8080
USER node

CMD ["node", "start-production.mjs"]
