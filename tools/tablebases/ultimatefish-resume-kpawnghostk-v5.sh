#!/bin/bash
# Resume the migrated same-side Pawn/Ghost fixed point from the last completed
# and compacted iteration 42.  Iteration 43 only touched the alternate arrays.
set -euo pipefail

root=/mnt/ultimatefish/info-substate-corrected-v1/kpawnghostk-v4
binary=/mnt/ultimatefish/pawn-resume-v5-25899b61/ultimate_ghost_ordinary_information_tablebase-pawn-resume-v5
scratch=work/solve/kpawnghostk
log=${root}/work/logs/solve.log

test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  aee958ad1f2ae34cf1fe770a23de3d2dc4ac10a760b0b6a9ba06ab1f8b03afaa
test "$(sha256sum "${root}/work/transitions-ready.json" | cut -d ' ' -f 1)" = \
  e4a05684150aabb68819c13d86a624dd108b8c9ff98cd15755660be0cfcf1107
grep -Fqx \
  'reciprocal_ghost_extra_iteration 42 bdd_nodes 122225097 changed_owner 255988 changed_observer 0 changed_visible 255988 peak_rss_bytes 10196910080' \
  "${log}"
grep -Fqx \
  'ghost_extra_external_compaction iteration 42 roots 172727679 marked_nodes 14741076 copied_nodes 14741074 structural_residual 0 root_residual 0' \
  "${log}"
! grep -q '^reciprocal_ghost_extra_iteration 43 ' "${log}"
test ! -e "${root}/work/results/kpawnghostk.ufiw"
test ! -e "${root}/work/results/kpawnghostk.ufgd"

exec >>"${log}" 2>&1
echo 'pawn_ghost_same_resume_v5 resume_iteration 42 current_slot current bdd_slot a checkpoint_migration 1 base_source_sha256 4be43c8bf7c19f636cc051fc77a7d1496dd263ecb62f5d447a1e83a8ba646e35 correction_sha256 da563e009e78d8b5d6b2df670471debbf04751a7ba8548cc1bec27b2fb53b654 resume_patch_sha256 25899b617a89f7aa63a965d4c30a78563bb298a087c1f9bc4e0735f3ea3295c8 binary_sha256 aee958ad1f2ae34cf1fe770a23de3d2dc4ac10a760b0b6a9ba06ab1f8b03afaa'
cd "${root}"
"${binary}" --solve \
  --orientation same \
  --transition-prefix work/transitions/kpawnghostk \
  --input tablebases/kpawnghostk.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output work/results/kpawnghostk.ufiw \
  --output-arbitrary work/results/kpawnghostk.ufgd \
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
  --resume-current-slot current --resume-bdd-slot a
