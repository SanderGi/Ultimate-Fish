#!/usr/bin/env bash
# Solve the authenticated horizontal-only same-side Pawn/Ghost graph with a
# current runner that passes the configured worker count into Bellman sweeps.
set -euo pipefail

readonly old=/mnt/ultimatefish/info-substate-corrected-v1/kpawnghostk-v4
readonly build=/mnt/ultimatefish/pawn-horizontal-domain-v12
readonly source_root=${build}/source/ghost-extra-remap-fix-v2-source
readonly runner=/mnt/ultimatefish/run_ultimate_ghost_ordinary_v11.py
readonly binary=${build}/ultimate_ghost_ordinary_information_tablebase-pawn-horizontal-v12
readonly work=/mnt/ultimatefish/info-substate-corrected-v1/kpawnghostk-fresh-v12

test "$(sha256sum "${runner}" | cut -d' ' -f1)" = \
  148160fc2eaa9819164b33364a0d8cfa8bb31935ad4532d6d93b3ba4f06b224d
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = \
  f277bb77bf9dbc6b7917e3e3bac61acafcd36b51d2168ead25a683df24e56706
test "$(sha256sum "${work}/work/transitions-ready.json" | cut -d' ' -f1)" = \
  ca1500745ceec762758fb184e1660a5eab97f808a8aa54b07ffa93957149b8f7
test ! -e "${work}/work/results/kpawnghostk.ufiw"
test ! -e "${work}/work/results/kpawnghostk.ufgd"

exec python3 "${runner}" \
  --source-root "${source_root}" \
  --work "${work}" \
  --filename kpawnghostk.uftb \
  --piece pawn \
  --orientation same \
  --source-table "${old}/tablebases/kpawnghostk.uftb" \
  --source-sha256 4f6a0f7300fd856518c7701fb49eea53298382fcd35c0fffca7a575113a669e7 \
  --lower-table "${old}/tablebases/kpawnk.uftb" \
  --lower-sha256 42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844 \
  --lower-model-sha256 6f84793f4343b5855fc0ddadd7a4452b9788c67070fa03b1f81bffb59ca4e011 \
  --lower-ghost-sidecar "${old}/tablebases/kghostk.ufgm" \
  --promoted-sidecar "${old}/tablebases/promoted-queen-ghost.ufgd" \
  --promoted-sidecar-sha256 09dbca339054c1c9e425d992b3a7de21c42a70824e2f04ae3ccef88f5718067b \
  --promoted-source-sha256 7b0ec34fc2f00524b0bf731f46deb7182c2c50fb17d1188dfa76f2238a697bb0 \
  --promoted-model-sha256 c7be59127e706959d44b8441ec308de3b1389b0405a595730c4daa3c584aef65 \
  --promoted-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --promoted-lower-ghost-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --promoted-lower-table "${old}/tablebases/kqueenk.uftb" \
  --promoted-lower-table-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --promoted-lower-model-sha256 87147fccee31cff98fd6875f02ca51d82d20c978bcaa6d7c4aaa81d114a23c6a \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --model-sha256 c550edb3f797695790872535c007a013374b6191394532ae3d6f6f3206935ded \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --solve-max-nodes 500000000 \
  --solve-workers 8 \
  --solve-existing
