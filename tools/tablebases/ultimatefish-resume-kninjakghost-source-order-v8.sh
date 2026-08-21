#!/usr/bin/env bash
# Resume opposed Ninja/Ghost from the retained iteration-11 fixed point after
# the 750M-node arena filled partway through iteration 12.
set -euo pipefail

root=/mnt/ultimatefish/kninjakghost-migrated-v1/restore/kninjakghost-fresh-v1
source_root=/mnt/ultimatefish/source-order-v7-ninja-7cb5a18a
binary=${source_root}/ultimate_ghost_ninja_resume_v7_ordinary_linux
runner=/mnt/ultimatefish/penguin-source-order-v6/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
scratch=work/solve-source-order-v7/kninjakghost
prior_log=${root}/work/logs/solve.log
log=${root}/work/logs/solve-v8.log
output=work/results/kninjakghost-source-order-v8.ufiw
arbitrary=work/results/kninjakghost-source-order-v8.ufgd

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d' ' -f1)" = \
  7cb5a18a5f93a320101365eaaf66125ee0b878c7a99958ddd45cc22e945563ae
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = \
  80707812c70d30ba000842eb0963eeb7c007d7ea778dd17caf216da7073b8b8e
test "$(sha256sum "${root}/tablebases/kninjakghost.uftb" | cut -d' ' -f1)" = \
  4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027
test "$(sha256sum "${root}/tablebases/kninjak.uftb" | cut -d' ' -f1)" = \
  4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
grep -Fq \
  'reciprocal_ghost_extra_iteration 11 bdd_nodes 741692807 changed_owner 201 changed_observer 61564 changed_visible 247824' \
  "${prior_log}"
grep -Fq \
  'ghost_extra_external_compaction iteration 11 roots 42648839 marked_nodes 11651083 copied_nodes 11651081 structural_residual 0 root_residual 0' \
  "${prior_log}"
grep -Fq 'reciprocal_ghost_extra_bellman iteration 12 geometry 465000/492960' \
  "${prior_log}"
grep -Fq 'external ROBDD exact node budget exhausted' "${prior_log}"
test "$(stat -c %s "${root}/${scratch}.bdd-b.nodes")" = 6750000000
test "$(stat -c %s "${root}/${scratch}.bdd-b.unique")" = 4294967296
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${log}"

# The arena is append-only and the aborted iteration left a structurally valid
# 750M-node prefix. Extend only its sparse capacity; the unique table and the
# compact iteration-11 roots remain untouched. Reprocessing iteration 12 can
# reuse the retained partial nodes collision-safely.
truncate -s 11250000000 "${root}/${scratch}.bdd-b.nodes"

{
  echo 'ninja_ghost_opposed_source_order_v8 resume_iteration 11 current_slot next bdd_slot b retained_partial_iteration_12 1 max_nodes 1250000000 unique_slots 1073741824'
  cd "${root}"
  "${binary}" \
    --solve \
    --transition-prefix work/transitions/kninjakghost-source-order-v6 \
    --orientation opposing \
    --lower-dragon-table tablebases/kninjak.uftb \
    --lower-dragon-sha256 4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776 \
    --lower-dragon-source-sha256 4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776 \
    --lower-dragon-model-sha256 666ca18773faec47588c7d628b6bd5e4f61035fbf4d36a15a55a5558aacc0096 \
    --source-sha256 4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027 \
    --model-sha256 c68506845eceed07dee0f179f6bb97d4a2f0c085f809041fc2d5613ea1760cad \
    --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --input tablebases/kninjakghost.uftb \
    --normalized-input work/self-test-source-order-v6/kninjakghost.normalized.uftb \
    --normalized-sha256 bd3d9de0a61fcdfb7910379a7c73283c2d299cd2adf17a38f474f4f7d650b58a \
    --lower-ghost-sidecar tablebases/kghostk.ufgm \
    --scratch "${scratch}" \
    --output "${output}" \
    --output-arbitrary "${arbitrary}" \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --max-nodes 1250000000 \
    --unique-slots 1073741824 \
    --compact-every 1 \
    --resume-fixed-point \
    --resume-iteration 11 \
    --resume-current-slot next \
    --resume-bdd-slot b
  sha256sum "${root}/${output}" "${root}/${arbitrary}"
  echo 'ninja_ghost_opposed_source_order_v8 complete 1'
} >>"${log}" 2>&1

/usr/bin/python3 "${runner}" \
  --source-root "${source_root}" \
  --work "${root}" \
  --filename kninjakghost.uftb \
  --piece ninja \
  --orientation opposing \
  --source-table "${root}/tablebases/kninjakghost.uftb" \
  --source-sha256 4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027 \
  --lower-table "${root}/tablebases/kninjak.uftb" \
  --lower-sha256 4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776 \
  --lower-model-sha256 666ca18773faec47588c7d628b6bd5e4f61035fbf4d36a15a55a5558aacc0096 \
  --model-sha256 c68506845eceed07dee0f179f6bb97d4a2f0c085f809041fc2d5613ea1760cad \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sidecar "${root}/tablebases/kghostk.ufgm" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --finalize-existing >/dev/null
