#!/bin/bash
set -euo pipefail

source=/mnt/ultimatefish/ghost-extra-remap-fix-v3-source
archive=/mnt/ultimatefish/ghost-extra-remap-fix-v3-source-4c8b223d.tar.zst
work=/mnt/ultimatefish/info-remap-fix-v2/kpawnkghost-fresh-v4
source_table=/mnt/ultimatefish/info-substate-corrected-v1/kpawnkghost-v4/tablebases/kpawnkghost.uftb
lower_table=/mnt/ultimatefish/info-substate-corrected-v1/kpawnkghost-v4/tablebases/kpawnk.uftb
promoted_lower_table=/mnt/ultimatefish/info-substate-corrected-v1/kpawnkghost-v4/tablebases/kqueenk.uftb

test ! -e "${work}"
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = 4c8b223d157752497c5d006e9795e3c33e3b70faab672d2419c604ba47e0e263
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_dragon_information_solver.cpp" | cut -d ' ' -f 1)" = ca679a7817bf03160f9fe9c53335aea6e82b5b653a0501b850b5168d45705900
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_dragon_information_solver.h" | cut -d ' ' -f 1)" = 55f728120ce619782b4e4c7775f0a5e17321b211500e881c260a52e7b81a75e6
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_public_extra_information_solver.cpp" | cut -d ' ' -f 1)" = dc9df737a72961fa728fa7da61202aa857cfd8760d3dff7068e1338bfaa50bf7
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_extra_information_tablebase.cpp" | cut -d ' ' -f 1)" = 94d3046781e5d2b04093d6bc13d2d6963f4e26723e34b8d8d35805905d438552
test "$(sha256sum "${source}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" | cut -d ' ' -f 1)" = 422de069d68905ecaf47f8ec9a26f43699b75987a1ea159bc00ef792ea8fa47a
test "$(sha256sum "${source_table}" | cut -d ' ' -f 1)" = 6d2f23b873a861b0c51cb86a8378013619c5cce0adb29e0060bce07cc2a2ae46
test "$(sha256sum "${lower_table}" | cut -d ' ' -f 1)" = 42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844
test "$(sha256sum "${promoted_lower_table}" | cut -d ' ' -f 1)" = 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
test "$(df --output=avail -B1 /mnt/ultimatefish | tail -1)" -ge 858993459200

exec /usr/bin/python3 "${source}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source}" \
  --work "${work}" \
  --filename kpawnkghost.uftb \
  --piece pawn \
  --orientation opposing \
  --source-table "${source_table}" \
  --source-sha256 6d2f23b873a861b0c51cb86a8378013619c5cce0adb29e0060bce07cc2a2ae46 \
  --lower-table "${lower_table}" \
  --lower-sha256 42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844 \
  --lower-model-sha256 6f84793f4343b5855fc0ddadd7a4452b9788c67070fa03b1f81bffb59ca4e011 \
  --promoted-lower-table "${promoted_lower_table}" \
  --promoted-lower-table-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --promoted-lower-model-sha256 87147fccee31cff98fd6875f02ca51d82d20c978bcaa6d7c4aaa81d114a23c6a \
  --model-sha256 3f66b94790014f072a1eb69659449e7f4b80d1e19330d00a92b270420d1ec4cd \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --parallelism 12 \
  --transitions-only
