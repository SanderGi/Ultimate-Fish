#!/usr/bin/env bash
# Resume opposed Turtle/Ghost from complete iteration 21 after the i03 stop.
set -euo pipefail

root=/mnt/ultimatefish-penguin/info-remap-fix-v2/kturtlekghost-fresh-v1
binary=/mnt/ultimatefish/recovery-binaries-after-stop-v1/turtle
prior_log=$root/work/logs/compositional-merge-solve-v4.log
log=$root/work/logs/resume-after-stop-v1.log
scratch=work/solve-compositional-v4/kturtlekghost
output=work/results/kturtlekghost-compositional-v4.ufiw
arbitrary=work/results/kturtlekghost-compositional-v4.ufgd

test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  c073ee36e7d6ef2811849a6b1f77a15618c1ab893f750045fcf32b34d08cf852
test "$(sha256sum "$root/tablebases/kturtlekghost.uftb" | cut -d' ' -f1)" = \
  40ddd27df10d73efac5e2203dc06c4c9ba1258927a2465ab812c3fb24af49e39
test "$(sha256sum "$root/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "$root/work/self-test/kturtlekghost.normalized.uftb" | cut -d' ' -f1)" = \
  85b8d05decef63632f5fff6127288ff2b2a306d07afe5f3b144bed236fb5f2a0
test "$(sha256sum "$root/work/transitions/kturtlekghost-compositional-v4.verified" | cut -d' ' -f1)" = \
  b73a2e73d8d17b9ab79c703216b962b1e0dd9c3a6890440b8609d5618798d6a7
grep -Fq \
  'ghost_extra_external_compaction iteration 21 roots 42648839 marked_nodes 6351685 copied_nodes 6351683 structural_residual 0 root_residual 0' \
  "$prior_log"
grep -Fq 'reciprocal_ghost_extra_bellman iteration 22 geometry 250000/492960' \
  "$prior_log"
test -s "$root/work/transitions/kturtlekghost-compositional-v4.header"
test -s "$root/$scratch.owner-next"
test -s "$root/$scratch.bdd-b.nodes"
test ! -e "$root/$output"
test ! -e "$root/$arbitrary"
test ! -e "$log"

cd "$root"
exec "$binary" --solve \
  --transition-prefix work/transitions/kturtlekghost-compositional-v4 \
  --orientation opposing \
  --lower-dragon-table implicit-draw \
  --lower-dragon-sha256 ec27a3cb8babe7c87b0c13229ff51d454da3bed542aa8c27862bfcc630ef26ce \
  --lower-dragon-source-sha256 ec27a3cb8babe7c87b0c13229ff51d454da3bed542aa8c27862bfcc630ef26ce \
  --lower-dragon-model-sha256 0bfcb5141c1732f3eef3830b1d7552540f9511aa047ec40169f369d24f5f37ee \
  --source-sha256 40ddd27df10d73efac5e2203dc06c4c9ba1258927a2465ab812c3fb24af49e39 \
  --model-sha256 b4f71efb40b7d7ed9c97f346b9074d9e934f3c542c5d19827dae9ccebdafbe7d \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --input tablebases/kturtlekghost.uftb \
  --normalized-input work/self-test/kturtlekghost.normalized.uftb \
  --normalized-sha256 85b8d05decef63632f5fff6127288ff2b2a306d07afe5f3b144bed236fb5f2a0 \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "$scratch" \
  --output "$output" \
  --output-arbitrary "$arbitrary" \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 500000000 \
  --unique-slots 1073741824 \
  --compact-every 1 \
  --resume-fixed-point \
  --resume-iteration 21 \
  --resume-current-slot next \
  --resume-bdd-slot b >>"$log" 2>&1
