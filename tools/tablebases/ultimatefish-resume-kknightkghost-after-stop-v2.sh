#!/usr/bin/env bash
# Resume opposed Knight/Ghost from complete iteration 30 with the pinned
# compositional-v5 source model after the persisted pre-resume binary failed.
set -euo pipefail

root=/mnt/ultimatefish-penguin/info-remap-fix-v1/kknightkghost-fresh-v8
binary=/mnt/ultimatefish/knight-resume-build-v1/ultimate_ghost_knight_resume_v1_linux
prior_log=$root/work/logs/solve.log
failed_log=$root/work/logs/resume-after-stop-v1.log
log=$root/work/logs/resume-after-stop-v2.log
scratch=work/solve/kknightkghost
output=work/results/kknightkghost.ufiw
arbitrary=work/results/kknightkghost.ufgd

test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  b3cb47f867790ae392ca5fcf0bbabbc4b206bce4bdfdd66e915e3e7629cc273a
test "$(sha256sum /mnt/ultimatefish/knight-resume-build-v1/source.tar.gz | cut -d' ' -f1)" = \
  0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856
test "$(sha256sum "$root/tablebases/kknightkghost.uftb" | cut -d' ' -f1)" = \
  a17f58aa7908646af328963ae8023072f5b0f05e61cef8d4d4a6c502d2684250
test "$(sha256sum "$root/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "$root/work/transitions-ready.json" | cut -d' ' -f1)" = \
  32c81f2b49a53efdc804260e852713b467fe4f6f4b2e0407d33f811b673819b2
grep -Fq \
  'ghost_extra_external_compaction iteration 30 roots 42648839 marked_nodes 4495217 copied_nodes 4495215 structural_residual 0 root_residual 0' \
  "$prior_log"
grep -Fq 'reciprocal_ghost_extra_bellman iteration 31 geometry 160000/492960' \
  "$prior_log"
grep -Fq 'unknown option --resume-fixed-point' "$failed_log"
test -s "$root/work/transitions/kknightkghost.header"
test -s "$root/$scratch.owner-current"
test -s "$root/$scratch.bdd-a.nodes"
test ! -e "$root/$output"
test ! -e "$root/$arbitrary"
test ! -e "$log"

cd "$root"
exec "$binary" --solve \
  --transition-prefix work/transitions/kknightkghost \
  --orientation opposing \
  --lower-dragon-table implicit-draw \
  --lower-dragon-sha256 807693ac2c310cbe04547d4d8244794a7c5aa72e664f8cb16e74134f34d7118c \
  --lower-dragon-source-sha256 807693ac2c310cbe04547d4d8244794a7c5aa72e664f8cb16e74134f34d7118c \
  --lower-dragon-model-sha256 c10b2b1f474f1fdb6ac540c54c5fa359bf5f834a1714bf12e22accab84c117c1 \
  --source-sha256 a17f58aa7908646af328963ae8023072f5b0f05e61cef8d4d4a6c502d2684250 \
  --model-sha256 be268fc43e27fc69c39af5e4e98dc6a9cb40e2fbbb491af8b9e3bd7435659049 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --input tablebases/kknightkghost.uftb \
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
  --resume-iteration 30 \
  --resume-current-slot current \
  --resume-bdd-slot a >>"$log" 2>&1
