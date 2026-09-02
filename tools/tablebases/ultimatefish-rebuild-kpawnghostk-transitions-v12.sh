#!/usr/bin/env bash
# Rebuild the full 1,971,840-geometry same-side Pawn/Ghost graph from an empty
# tree with the authenticated horizontal-only Pawn domain.
set -euo pipefail

old=/mnt/ultimatefish/info-substate-corrected-v1/kpawnghostk-v4
build=/mnt/ultimatefish/pawn-horizontal-domain-v12
source_root=${build}/source/ghost-extra-remap-fix-v2-source
runner=/mnt/ultimatefish/run_ultimate_ghost_ordinary_v10.py
binary=${build}/ultimate_ghost_ordinary_information_tablebase-pawn-horizontal-v12
work=/mnt/ultimatefish/info-substate-corrected-v1/kpawnghostk-fresh-v12

test ! -e "${work}"
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = \
  f277bb77bf9dbc6b7917e3e3bac61acafcd36b51d2168ead25a683df24e56706
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
  --promoted-lower-table "${old}/tablebases/kqueenk.uftb" \
  --promoted-lower-table-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --promoted-lower-model-sha256 87147fccee31cff98fd6875f02ca51d82d20c978bcaa6d7c4aaa81d114a23c6a \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --model-sha256 c550edb3f797695790872535c007a013374b6191394532ae3d6f6f3206935ded \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --prebuilt-executable "${binary}" \
  --prebuilt-executable-sha256 f277bb77bf9dbc6b7917e3e3bac61acafcd36b51d2168ead25a683df24e56706 \
  --prebuilt-model-sha256 c550edb3f797695790872535c007a013374b6191394532ae3d6f6f3206935ded \
  --parallelism 2 \
  --transitions-only
