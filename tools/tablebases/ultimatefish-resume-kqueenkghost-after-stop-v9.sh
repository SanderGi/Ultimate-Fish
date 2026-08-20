#!/usr/bin/env bash
# Resume opposed Queen/Ghost from complete iteration 3 after the i03 stop.
set -euo pipefail

root=/mnt/ultimatefish-penguin/info-remap-fix-v2/kqueenkghost-fresh-v1
binary=/mnt/ultimatefish/recovery-binaries-after-stop-v1/queen
prior_log=$root/work/logs/solve-source-order-v8.log
log=$root/work/logs/resume-after-stop-v9.log
scratch=work/solve-source-order-v7/kqueenkghost
output=work/results/kqueenkghost-source-order-v8.ufiw
arbitrary=work/results/kqueenkghost-source-order-v8.ufgd

test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  4d33dec7ac7352e1447ae4afc1d7b92d6edc762c0ca3e178fadc4aac4c12a03b
test "$(sha256sum "$root/tablebases/kqueenkghost.uftb" | cut -d' ' -f1)" = \
  30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d
test "$(sha256sum "$root/tablebases/kqueenk.uftb" | cut -d' ' -f1)" = \
  1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
test "$(sha256sum "$root/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
grep -Fq \
  'ghost_extra_external_compaction iteration 3 roots 42648839 marked_nodes 7922514 copied_nodes 7922512 structural_residual 0 root_residual 0' \
  "$prior_log"
grep -Fq 'reciprocal_ghost_extra_bellman iteration 4 geometry 320000/492960' \
  "$prior_log"
test -s "$root/work/transitions/kqueenkghost-source-order-v6.header"
test -s "$root/$scratch.owner-next"
test -s "$root/$scratch.bdd-b.nodes"
test ! -e "$root/$output"
test ! -e "$root/$arbitrary"
test ! -e "$log"

cd "$root"
exec "$binary" --solve \
  --transition-prefix work/transitions/kqueenkghost-source-order-v6 \
  --orientation opposing \
  --lower-dragon-table tablebases/kqueenk.uftb \
  --lower-dragon-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --lower-dragon-source-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --lower-dragon-model-sha256 87147fccee31cff98fd6875f02ca51d82d20c978bcaa6d7c4aaa81d114a23c6a \
  --source-sha256 30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d \
  --model-sha256 d05ac1dbf726e8830a78d191bd1764572532bfcdee05b549de919f2bbda003f5 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --input tablebases/kqueenkghost.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "$scratch" \
  --output "$output" \
  --output-arbitrary "$arbitrary" \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 750000000 \
  --unique-slots 1073741824 \
  --compact-every 1 \
  --resume-fixed-point \
  --resume-iteration 3 \
  --resume-current-slot next \
  --resume-bdd-slot b >>"$log" 2>&1
