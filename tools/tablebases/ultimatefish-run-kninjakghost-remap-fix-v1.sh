#!/bin/bash
set -euo pipefail

source=/mnt/ultimatefish/ghost-extra-remap-fix-v2-source
archive=/mnt/ultimatefish/ghost-extra-remap-fix-v2-source-4be43c8b.tar.zst
work=/mnt/ultimatefish-penguin/info-remap-fix-v2/kninjakghost-fresh-v1
source_table=/mnt/ultimatefish/info-wave-aa82210e-work/kninjakghost/tablebases/kninjakghost.uftb
lower_table=/mnt/ultimatefish/info-wave-aa82210e-stage/kninjakghost/kninjak.uftb

test ! -e "${work}"
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = 4be43c8bf7c19f636cc051fc77a7d1496dd263ecb62f5d447a1e83a8ba646e35
test "$(sha256sum "${source}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" | cut -d ' ' -f 1)" = d987592645386f37ef6a1a99779ffcb19ed6aa833cf076bcecb5758991f18e19
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_dragon_information_solver.cpp" | cut -d ' ' -f 1)" = ca679a7817bf03160f9fe9c53335aea6e82b5b653a0501b850b5168d45705900
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_extra_information_tablebase.cpp" | cut -d ' ' -f 1)" = 94d3046781e5d2b04093d6bc13d2d6963f4e26723e34b8d8d35805905d438552
test "$(sha256sum "${source_table}" | cut -d ' ' -f 1)" = 4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027
test "$(sha256sum "${lower_table}" | cut -d ' ' -f 1)" = 4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776
test "$(df --output=avail -B1 /mnt/ultimatefish-penguin | tail -1)" -ge 268435456000

exec /usr/bin/python3 "${source}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source}" \
  --work "${work}" \
  --filename kninjakghost.uftb \
  --piece ninja \
  --orientation opposing \
  --source-table "${source_table}" \
  --source-sha256 4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027 \
  --lower-table "${lower_table}" \
  --lower-sha256 4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776 \
  --lower-model-sha256 666ca18773faec47588c7d628b6bd5e4f61035fbf4d36a15a55a5558aacc0096 \
  --model-sha256 c68506845eceed07dee0f179f6bb97d4a2f0c085f809041fc2d5613ea1760cad \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --parallelism 7 \
  --transitions-only
