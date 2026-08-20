#!/bin/bash
set -euo pipefail

source=/mnt/ultimatefish/ghost-extra-remap-fix-v2-source
archive=/mnt/ultimatefish/ghost-extra-remap-fix-v2-source-4be43c8b.tar.zst
work=/mnt/ultimatefish-penguin/info-remap-fix-v2/kturtlekghost-fresh-v1
source_table=/mnt/ultimatefish-penguin/kturtlekghost-migration-v2/restore/info-singleton-domain-fix-v1/kturtlekghost/tablebases/kturtlekghost.uftb

test ! -e "${work}"
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = 4be43c8bf7c19f636cc051fc77a7d1496dd263ecb62f5d447a1e83a8ba646e35
test "$(sha256sum "${source}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" | cut -d ' ' -f 1)" = d987592645386f37ef6a1a99779ffcb19ed6aa833cf076bcecb5758991f18e19
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_dragon_information_solver.cpp" | cut -d ' ' -f 1)" = ca679a7817bf03160f9fe9c53335aea6e82b5b653a0501b850b5168d45705900
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_extra_information_tablebase.cpp" | cut -d ' ' -f 1)" = 94d3046781e5d2b04093d6bc13d2d6963f4e26723e34b8d8d35805905d438552
test "$(sha256sum "${source_table}" | cut -d ' ' -f 1)" = 40ddd27df10d73efac5e2203dc06c4c9ba1258927a2465ab812c3fb24af49e39
test "$(df --output=avail -B1 /mnt/ultimatefish-penguin | tail -1)" -ge 268435456000

exec /usr/bin/python3 "${source}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source}" \
  --work "${work}" \
  --filename kturtlekghost.uftb \
  --piece turtle \
  --orientation opposing \
  --source-table "${source_table}" \
  --source-sha256 40ddd27df10d73efac5e2203dc06c4c9ba1258927a2465ab812c3fb24af49e39 \
  --lower-sha256 ec27a3cb8babe7c87b0c13229ff51d454da3bed542aa8c27862bfcc630ef26ce \
  --lower-model-sha256 0bfcb5141c1732f3eef3830b1d7552540f9511aa047ec40169f369d24f5f37ee \
  --model-sha256 b4f71efb40b7d7ed9c97f346b9074d9e934f3c542c5d19827dae9ccebdafbe7d \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --parallelism 7 \
  --transitions-only
