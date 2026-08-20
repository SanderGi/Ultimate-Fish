#!/usr/bin/env bash
# Resume opposed Rook/Ghost from complete iteration 9 after the i03 stop.
set -euo pipefail

root=/mnt/ultimatefish-penguin/info-remap-fix-v2/krookkghost-fresh-v1
binary=/mnt/ultimatefish/recovery-binaries-after-stop-v1/rook
prior_log=$root/work/logs/solve.log
log=$root/work/logs/resume-after-stop-v7.log
scratch=work/solve-source-order-v6/krookkghost
output=work/results/krookkghost-source-order-v6.ufiw
arbitrary=work/results/krookkghost-source-order-v6.ufgd

test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  cd15f2ee785be750f86b68d10832ca3fbcc5f405b9f3e5a21fd6489a44a3fda1
test "$(sha256sum "$root/tablebases/krookkghost.uftb" | cut -d' ' -f1)" = \
  bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41
test "$(sha256sum "$root/tablebases/krookk.uftb" | cut -d' ' -f1)" = \
  abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
test "$(sha256sum "$root/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "$root/work/self-test-source-order-v6/krookkghost.normalized.uftb" | cut -d' ' -f1)" = \
  790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785
grep -Fq \
  'ghost_extra_external_compaction iteration 9 roots 42648839 marked_nodes 7402850 copied_nodes 7402848 structural_residual 0 root_residual 0' \
  "$prior_log"
grep -Fq 'reciprocal_ghost_extra_bellman iteration 10 geometry 375000/492960' \
  "$prior_log"
test -s "$root/work/transitions/krookkghost-source-order-v6.header"
test -s "$root/$scratch.owner-next"
test -s "$root/$scratch.bdd-b.nodes"
test ! -e "$root/$output"
test ! -e "$root/$arbitrary"
test ! -e "$log"

cd "$root"
exec "$binary" --solve \
  --transition-prefix work/transitions/krookkghost-source-order-v6 \
  --orientation opposing \
  --lower-dragon-table tablebases/krookk.uftb \
  --lower-dragon-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
  --lower-dragon-source-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
  --lower-dragon-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1 \
  --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
  --model-sha256 ff692f8217e6327b6ab56ab41ee3b9a8f02eecf5b80fc59599a753f340dd80f5 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --input tablebases/krookkghost.uftb \
  --normalized-input work/self-test-source-order-v6/krookkghost.normalized.uftb \
  --normalized-sha256 790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785 \
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
  --resume-iteration 9 \
  --resume-current-slot next \
  --resume-bdd-slot b >>"$log" 2>&1
