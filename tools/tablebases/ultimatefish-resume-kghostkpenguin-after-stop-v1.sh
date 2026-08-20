#!/usr/bin/env bash
# Resume the opposed Penguin/Ghost fixed point from its last complete EBS slot.
set -euo pipefail

root=/mnt/ultimatefish-penguin/opposed-kghostkpenguin-fresh-v5
binary=/mnt/ultimatefish/recovery-binaries-after-stop-v1/penguin
prior_log=$root/work/logs/solve.log
log=$root/work/logs/resume-after-stop-v1.log
scratch=work/solve/kghostkpenguin
output=work/results/kghostkpenguin.ufiw
arbitrary=work/results/kghostkpenguin.ufgd

test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  999ce0cc34ea4583b1b15f64df69af3bb0b3743981352cf126bc906bc84faf55
test "$(sha256sum "$root/tablebases/kghostkpenguin.uftb" | cut -d' ' -f1)" = \
  d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a
test "$(sha256sum "$root/tablebases/kpenguink.uftb" | cut -d' ' -f1)" = \
  5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd
test "$(sha256sum "$root/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
grep -Fq \
  'ghost_extra_external_compaction iteration 16 roots 319383203 marked_nodes 4849095 copied_nodes 4849093 structural_residual 0 root_residual 0' \
  "$prior_log"
grep -Fq 'reciprocal_ghost_extra_bellman iteration 17 geometry 2495000/3943680' \
  "$prior_log"
test -s "$root/work/transitions/kghostkpenguin.header"
test -s "$root/work/transitions/kghostkpenguin.meta"
test "$(sha256sum "$root/work/transitions-ready.json" | cut -d' ' -f1)" = \
  fa166c7347e4751bb234b21b24e5fcc81895f01926042223a1e5ba03b3116938
test -s "$root/$scratch.owner-current"
test ! -e "$root/$output"
test ! -e "$root/$arbitrary"
test ! -e "$log"

cd "$root"
exec "$binary" --solve \
  --orientation opposing \
  --transition-prefix work/transitions/kghostkpenguin \
  --lower-dragon-table tablebases/kpenguink.uftb \
  --lower-dragon-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
  --lower-dragon-source-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
  --lower-dragon-model-sha256 3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84 \
  --source-sha256 d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a \
  --model-sha256 268c2bdf5fb9e1b436406378e97ee213eb460c70b354bbeececd479ef8df33e8 \
  --observation-sha256 6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b \
  --input tablebases/kghostkpenguin.uftb \
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
  --resume-iteration 16 \
  --resume-current-slot current \
  --resume-bdd-slot a >>"$log" 2>&1
