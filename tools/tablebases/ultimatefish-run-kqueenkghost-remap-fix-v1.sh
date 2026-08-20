#!/bin/bash
set -euo pipefail

source=/mnt/ultimatefish/ghost-extra-remap-fix-v2-source
archive=/mnt/ultimatefish/ghost-extra-remap-fix-v2-source-4be43c8b.tar.zst
work=/mnt/ultimatefish-penguin/info-remap-fix-v2/kqueenkghost-fresh-v1
source_table=/mnt/ultimatefish-penguin/queen-ghost-migrated-work-v1/tablebases/kqueenkghost.uftb
lower_table=/mnt/ultimatefish-penguin/queen-ghost-migrated-work-v1/tablebases/kqueenk.uftb

test ! -e "${work}"
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = 4be43c8bf7c19f636cc051fc77a7d1496dd263ecb62f5d447a1e83a8ba646e35
test "$(sha256sum "${source}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" | cut -d ' ' -f 1)" = d987592645386f37ef6a1a99779ffcb19ed6aa833cf076bcecb5758991f18e19
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_dragon_information_solver.cpp" | cut -d ' ' -f 1)" = ca679a7817bf03160f9fe9c53335aea6e82b5b653a0501b850b5168d45705900
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_extra_information_tablebase.cpp" | cut -d ' ' -f 1)" = 94d3046781e5d2b04093d6bc13d2d6963f4e26723e34b8d8d35805905d438552
test "$(sha256sum "${source_table}" | cut -d ' ' -f 1)" = 30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d
test "$(sha256sum "${lower_table}" | cut -d ' ' -f 1)" = 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
test "$(df --output=avail -B1 /mnt/ultimatefish-penguin | tail -1)" -ge 268435456000

exec /usr/bin/python3 "${source}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source}" \
  --work "${work}" \
  --filename kqueenkghost.uftb \
  --piece queen \
  --orientation opposing \
  --source-table "${source_table}" \
  --source-sha256 30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d \
  --lower-table "${lower_table}" \
  --lower-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --lower-model-sha256 87147fccee31cff98fd6875f02ca51d82d20c978bcaa6d7c4aaa81d114a23c6a \
  --model-sha256 d05ac1dbf726e8830a78d191bd1764572532bfcdee05b549de919f2bbda003f5 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --parallelism 7 \
  --transitions-only
