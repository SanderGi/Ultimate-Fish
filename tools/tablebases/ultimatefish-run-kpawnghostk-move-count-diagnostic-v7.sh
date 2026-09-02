#!/usr/bin/env bash
# Reproduce the Pawn/Ghost transition move-count residual on reflinked inputs.
# The authenticated transition graph and iteration-42 fixed point are read-only.
set -euo pipefail

root=/mnt/ultimatefish/info-substate-corrected-v1/kpawnghostk-v4
build=/mnt/ultimatefish/pawn-resume-diagnostic-v7-394f3536
binary=${build}/ultimate_ghost_ordinary_information_tablebase-pawn-diagnostic-v7
source_prefix=${root}/work/transitions/kpawnghostk
diagnostic_prefix=${root}/work/transitions/kpawnghostk-diagnostic-v7
scratch=work/diagnostic-v7/kpawnghostk
log=${root}/work/logs/move-count-diagnostic-v7.log

test "$(sha256sum "${binary}" | cut -d' ' -f1)" = \
  8b3b9fd0a3a20fcba496af4430b2c20cfe82db170f0e5dcfb35606e28489048b
test ! -e "${diagnostic_prefix}.header"
test ! -e "${root}/work/diagnostic-v7"
test ! -e "${log}"
for suffix in header meta strata index blocks verified; do
  cp --reflink=always "${source_prefix}.${suffix}" \
    "${diagnostic_prefix}.${suffix}"
done
install -d -m 0755 "${root}/work/diagnostic-v7"

cd "${root}"
set +e
"${binary}" --solve \
  --orientation same \
  --transition-prefix work/transitions/kpawnghostk-diagnostic-v7 \
  --input tablebases/kpawnghostk.uftb \
  --normalized-input work/solve/kpawnghostk.normalized.uftb \
  --normalized-sha256 74e4841be19e8eb72c847a49740e7398a6f28479efc09b3a36a3bb97e6407a2a \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output work/diagnostic-v7/kpawnghostk.ufiw \
  --output-arbitrary work/diagnostic-v7/kpawnghostk.ufgd \
  --lower-dragon-table tablebases/kpawnk.uftb \
  --lower-dragon-sha256 42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844 \
  --lower-dragon-source-sha256 42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844 \
  --lower-dragon-model-sha256 6f84793f4343b5855fc0ddadd7a4452b9788c67070fa03b1f81bffb59ca4e011 \
  --promoted-lower-dragon-table tablebases/kqueenk.uftb \
  --promoted-lower-dragon-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --promoted-lower-dragon-source-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --promoted-lower-dragon-model-sha256 87147fccee31cff98fd6875f02ca51d82d20c978bcaa6d7c4aaa81d114a23c6a \
  --source-sha256 4f6a0f7300fd856518c7701fb49eea53298382fcd35c0fffca7a575113a669e7 \
  --model-sha256 51da0899d0860fc6c805b4e13a0551e3cd4a2dc5329ed99a18b34c3b090ac594 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --promoted-sidecar tablebases/promoted-queen-ghost.ufgd \
  --promoted-sidecar-sha256 09dbca339054c1c9e425d992b3a7de21c42a70824e2f04ae3ccef88f5718067b \
  --promoted-source-sha256 7b0ec34fc2f00524b0bf731f46deb7182c2c50fb17d1188dfa76f2238a697bb0 \
  --promoted-model-sha256 c7be59127e706959d44b8441ec308de3b1389b0405a595730c4daa3c584aef65 \
  --promoted-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --promoted-lower-ghost-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --max-nodes 500000000 --unique-slots 1073741824 --compact-every 1 \
  --resume-fixed-point --resume-iteration 42 \
  --resume-current-slot current --resume-bdd-slot a >"${log}" 2>&1
status=$?
set -e
test "${status}" -eq 1
grep -F 'Dragon transition patch move-count residual geometry=' "${log}"
