#!/usr/bin/env bash
# Verify and finalize opposed Rook/Ghost from the authenticated iteration-22
# fixed point retained after the 500M-node verification arena filled.
set -euo pipefail

root=/mnt/ultimatefish-penguin/info-remap-fix-v2/krookkghost-fresh-v1
source_root=/mnt/ultimatefish/source-order-v6-rook-a26d3665
binary=/mnt/ultimatefish/recovery-binaries-after-stop-v1/rook
runner=/mnt/ultimatefish/ghost-extra-remap-fix-v2-source/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
prior_wrapper=/tmp/ultimatefish-resume-krookkghost-after-stop-v7.sh
prior_log=$root/work/logs/resume-after-stop-v7.log
log=$root/work/logs/resume-converged-v8.log
scratch=work/solve-source-order-v6/krookkghost
output=work/results/krookkghost-source-order-v6.ufiw
arbitrary=work/results/krookkghost-source-order-v6.ufgd

test "$(sha256sum "$source_root/source.tar.gz" | cut -d' ' -f1)" = \
  a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064
test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  cd15f2ee785be750f86b68d10832ca3fbcc5f405b9f3e5a21fd6489a44a3fda1
test "$(sha256sum "$prior_wrapper" | cut -d' ' -f1)" = \
  f9bc88bf7d0741ab1d5a1cb98e4371e881788e68e375c0a3723acd01c4644c95
test "$(sha256sum "$root/tablebases/krookkghost.uftb" | cut -d' ' -f1)" = \
  bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41
test "$(sha256sum "$root/tablebases/krookk.uftb" | cut -d' ' -f1)" = \
  abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
test "$(sha256sum "$root/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "$root/work/self-test-source-order-v6/krookkghost.normalized.uftb" | cut -d' ' -f1)" = \
  790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785
test "$(sha256sum "$root/work/transitions/krookkghost-source-order-v6.verified" | cut -d' ' -f1)" = \
  e5d675b44ebc9d88c68840eddc31cc5044d9e4121f050b1352c1dcc67ef06351
test "$(sha256sum "$root/work/transitions-ready.json" | cut -d' ' -f1)" = \
  660ac0157db8dcce6bd839f2571f50220af95675d26bc6b0dfb7030c93a29e5f
grep -Fq \
  'reciprocal_ghost_extra_iteration 22 bdd_nodes 480490719 changed_owner 0 changed_observer 0 changed_visible 0' \
  "$prior_log"
grep -Fq 'external ROBDD exact node budget exhausted' "$prior_log"
test "$(stat -c %s "$root/$scratch.bdd-b.nodes")" = 4500000000
test "$(stat -c %s "$root/$scratch.bdd-b.unique")" = 4294967296
test -s "$root/$scratch.owner-next"
test -s "$root/$scratch.observer-next"
test ! -e "$root/$output"
test ! -e "$root/$arbitrary"
test ! -e "$log"
test "$(df --output=avail -B1 "$root" | tail -1)" -ge 107374182400

# The exact ROBDD is append-only. Grow only the sparse node capacity; the
# iteration-22 roots, unique table, and prior failed log remain untouched.
truncate -s 11250000000 "$root/$scratch.bdd-b.nodes"

{
  echo 'rook_ghost_opposed_converged_v8 resume_iteration 22 current_slot next bdd_slot b max_nodes 1250000000 unique_slots 1073741824 independent_bellman_verification 1'
  cd "$root"
  "$binary" --solve \
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
    --max-nodes 1250000000 \
    --unique-slots 1073741824 \
    --compact-every 1 \
    --resume-fixed-point \
    --resume-converged \
    --resume-iteration 22 \
    --resume-current-slot next \
    --resume-bdd-slot b
  sha256sum "$output" "$arbitrary"
  echo 'rook_ghost_opposed_converged_v8 complete 1'
} >>"$log" 2>&1

/usr/bin/python3 "$runner" \
  --source-root /mnt/ultimatefish/ghost-extra-remap-fix-v2-source \
  --work "$root" \
  --filename krookkghost.uftb \
  --piece rook \
  --orientation opposing \
  --source-table "$root/tablebases/krookkghost.uftb" \
  --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
  --lower-table "$root/tablebases/krookk.uftb" \
  --lower-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
  --lower-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1 \
  --model-sha256 ff692f8217e6327b6ab56ab41ee3b9a8f02eecf5b80fc59599a753f340dd80f5 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sidecar "$root/tablebases/kghostk.ufgm" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --finalize-existing >/dev/null
