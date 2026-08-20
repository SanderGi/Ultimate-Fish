#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/kninjakghost-migrated-v1/restore/kninjakghost-fresh-v1
source_root=/mnt/ultimatefish/source-order-v7-ninja-7cb5a18a
binary=${source_root}/ultimate_ghost_ninja_resume_v7_ordinary_linux
runner=/mnt/ultimatefish/penguin-source-order-v6/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
source_table=${root}/tablebases/kninjakghost.uftb
lower_table=${root}/tablebases/kninjak.uftb
lower_ghost=${root}/tablebases/kghostk.ufgm
transition_prefix=work/transitions/kninjakghost-source-order-v6
scratch=work/solve-source-order-v7/kninjakghost
normalized=work/self-test-source-order-v6/kninjakghost.normalized.uftb
output=work/results/kninjakghost-source-order-v7.ufiw
arbitrary=work/results/kninjakghost-source-order-v7.ufgd
log=${root}/work/logs/solve.log
failed_log=${root}/work/logs/source-order-v6.log
checkpoint_manifest=${root}/work/ninja-v7-resume-expanded.sha256
build_certificate=${source_root}/ninja-v7-build-certificate.json

test "$(sha256sum "${source_root}/source.tar.gz" | awk '{print $1}')" = \
  7cb5a18a5f93a320101365eaaf66125ee0b878c7a99958ddd45cc22e945563ae
test "$(sha256sum "${binary}" | awk '{print $1}')" = \
  80707812c70d30ba000842eb0963eeb7c007d7ea778dd17caf216da7073b8b8e
test "$(sha256sum "${build_certificate}" | awk '{print $1}')" = \
  cad23cf18fe6806d251896df61514a427957c321c0ccd930bead6ea5565bb85c
test "$(sha256sum "${checkpoint_manifest}" | awk '{print $1}')" = \
  f8d1e600f46597030bc8279e427f31a2026250f28332b437d60f30d66f065b2b
test "$(sha256sum "${source_table}" | awk '{print $1}')" = \
  4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027
test "$(sha256sum "${lower_table}" | awk '{print $1}')" = \
  4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776
test "$(sha256sum "${lower_ghost}" | awk '{print $1}')" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/${normalized}" | awk '{print $1}')" = \
  bd3d9de0a61fcdfb7910379a7c73283c2d299cd2adf17a38f474f4f7d650b58a
grep -Fq 'reciprocal_ghost_extra_iteration 3 bdd_nodes 387478882' \
  "${failed_log}"
grep -Fq \
  'ghost_extra_external_compaction iteration 3 roots 42648839 marked_nodes 6281543 copied_nodes 6281541 structural_residual 0 root_residual 0' \
  "${failed_log}"
grep -Fq \
  'reciprocal_ghost_extra_bellman iteration 4 geometry 490000/492960' \
  "${failed_log}"
grep -Fq 'external ROBDD exact node budget exhausted' "${failed_log}"
test -f "${root}/${transition_prefix}.verified"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${log}"
test ! -e "${root}/work/artifact-manifest.json"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 107374182400

(
  cd "${root}/work/solve-source-order-v7"
  sha256sum -c "${checkpoint_manifest}"
)

{
  echo "ninja_ghost_opposed_source_order_v7 parent_failure_v6_preserved 1 expanded_checkpoint_manifest_sha256 f8d1e600f46597030bc8279e427f31a2026250f28332b437d60f30d66f065b2b build_certificate_sha256 cad23cf18fe6806d251896df61514a427957c321c0ccd930bead6ea5565bb85c source_bundle_sha256 7cb5a18a5f93a320101365eaaf66125ee0b878c7a99958ddd45cc22e945563ae source_bundle_version E1i2XhZzFcsT02PhLMKAFHG1tspK3LAn binary_sha256 80707812c70d30ba000842eb0963eeb7c007d7ea778dd17caf216da7073b8b8e binary_version TPBin4MV9NVy5gX4TRK576rl0pm2u0TM specialization ninja-extra-primary exact_shards 64 exact_iteration 3 partial_iteration_4_nodes_reused 1 max_nodes 750000000 unique_slots 1073741824 read_only_implication 1"
  cd "${root}"
  "${binary}" \
    --solve \
    --transition-prefix "${transition_prefix}" \
    --orientation opposing \
    --lower-dragon-table tablebases/kninjak.uftb \
    --lower-dragon-sha256 4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776 \
    --lower-dragon-source-sha256 4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776 \
    --lower-dragon-model-sha256 666ca18773faec47588c7d628b6bd5e4f61035fbf4d36a15a55a5558aacc0096 \
    --source-sha256 4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027 \
    --model-sha256 c68506845eceed07dee0f179f6bb97d4a2f0c085f809041fc2d5613ea1760cad \
    --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --input tablebases/kninjakghost.uftb \
    --normalized-input "${normalized}" \
    --normalized-sha256 bd3d9de0a61fcdfb7910379a7c73283c2d299cd2adf17a38f474f4f7d650b58a \
    --lower-ghost-sidecar tablebases/kghostk.ufgm \
    --scratch "${scratch}" \
    --output "${output}" \
    --output-arbitrary "${arbitrary}" \
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
    --resume-bdd-slot b

  sha256sum "${output}" "${arbitrary}"
  echo 'ninja_ghost_opposed_source_order_v7 complete 1'
} >>"${log}" 2>&1

/usr/bin/python3 "${runner}" \
  --source-root "${source_root}" \
  --work "${root}" \
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
  --lower-ghost-sidecar "${lower_ghost}" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --finalize-existing >/dev/null
