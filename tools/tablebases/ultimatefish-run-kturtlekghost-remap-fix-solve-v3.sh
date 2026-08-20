#!/bin/bash
set -euo pipefail

source=/mnt/ultimatefish/ghost-extra-remap-fix-v2-source
runner=/mnt/ultimatefish/turtle-solve-v3-source/run_ultimate_ghost_ordinary_aws.py
work=/mnt/ultimatefish-penguin/info-remap-fix-v2/kturtlekghost-fresh-v1
executable=${work}/ultimate_ghost_ordinary_information_tablebase
ready=${work}/work/transitions-ready.json
source_table=/mnt/ultimatefish-penguin/kturtlekghost-migration-v2/restore/info-singleton-domain-fix-v1/kturtlekghost/tablebases/kturtlekghost.uftb
lower_ghost=/mnt/ultimatefish/info-wave-aa82210e-stage/kghostk.ufgm

test -d "${work}"
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_dragon_information_solver.cpp" | cut -d ' ' -f 1)" = \
  ca679a7817bf03160f9fe9c53335aea6e82b5b653a0501b850b5168d45705900
test "$(sha256sum "${runner}" | cut -d ' ' -f 1)" = \
  422de069d68905ecaf47f8ec9a26f43699b75987a1ea159bc00ef792ea8fa47a
test "$(sha256sum "${ready}" | cut -d ' ' -f 1)" = \
  bfbe326ee8c82ce376a9d3ea001b2f970c0eb9eabd34542987b83c4a970ac842
test "$(sha256sum "${executable}" | cut -d ' ' -f 1)" = \
  0c1ce381f1def47d1751cd489b15f928c6289526249eb6360ea4388037c9d393
test "$(sha256sum "${source_table}" | cut -d ' ' -f 1)" = \
  40ddd27df10d73efac5e2203dc06c4c9ba1258927a2465ab812c3fb24af49e39
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test ! -e "${work}/work/results/kturtlekghost.ufiw"
test ! -e "${work}/work/results/kturtlekghost.ufgd"
test ! -e "${work}/work/logs/solve.log"
test -z "$(find "${work}/work/solve" -mindepth 1 -print -quit)"
test "$(df --output=avail -B1 /mnt/ultimatefish-penguin | tail -1)" -ge \
  64424509440

exec /usr/bin/python3 "${runner}" \
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
  --lower-ghost-sidecar "${lower_ghost}" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --solve-max-nodes 500000000 \
  --solve-existing
