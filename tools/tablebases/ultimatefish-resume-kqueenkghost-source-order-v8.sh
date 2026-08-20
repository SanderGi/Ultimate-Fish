#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish-penguin/info-remap-fix-v2/kqueenkghost-fresh-v1
source_root=/mnt/ultimatefish/source-order-v7-queen-7cb5a18a
binary=${source_root}/ultimate_ghost_queen_resume_v7_ordinary_linux
runner=/mnt/ultimatefish/source-order-v6-queen-a26d3665/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
source_table=${root}/tablebases/kqueenkghost.uftb
lower_table=${root}/tablebases/kqueenk.uftb
lower_ghost=${root}/tablebases/kghostk.ufgm
transition_prefix=work/transitions/kqueenkghost-source-order-v6
scratch=work/solve-source-order-v7/kqueenkghost
output=work/results/kqueenkghost-source-order-v8.ufiw
arbitrary=work/results/kqueenkghost-source-order-v8.ufgd
log=${root}/work/logs/solve-source-order-v8.log
failed_log=${root}/work/logs/solve.log
parent_checkpoint_manifest=${root}/work/queen-v6-iteration2-current.sha256
checkpoint_manifest=${root}/work/queen-v7-resume-expanded.sha256

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  7cb5a18a5f93a320101365eaaf66125ee0b878c7a99958ddd45cc22e945563ae
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  4d33dec7ac7352e1447ae4afc1d7b92d6edc762c0ca3e178fadc4aac4c12a03b
test "$(sha256sum "${source_table}" | cut -d ' ' -f 1)" = \
  30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d
test "$(sha256sum "${lower_table}" | cut -d ' ' -f 1)" = \
  1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${parent_checkpoint_manifest}" | cut -d ' ' -f 1)" = \
  21c806f554d1b1c04671c2123da80a15c0d5001515c19fd518d79f8c42a0fa35
test "$(sha256sum "${checkpoint_manifest}" | cut -d ' ' -f 1)" = \
  76c3f1649c1b3e4767868ef50d6d44a0648d3f34a0e74fd631c46a595bb10832
grep -Fq \
  'reciprocal_ghost_extra_iteration 2 bdd_nodes 455341669' "${failed_log}"
grep -Fq \
  'ghost_extra_external_compaction iteration 2 roots 42648839' "${failed_log}"
grep -Fq \
  'reciprocal_ghost_extra_bellman iteration 3 geometry 475000/492960' \
  "${failed_log}"
grep -Fq 'external ROBDD exact node budget exhausted' "${failed_log}"
test -f "${root}/${transition_prefix}.verified"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${log}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 107374182400

(
  cd "${root}/work/solve-source-order-v7"
  sha256sum -c "${checkpoint_manifest}"
)

{
  echo "queen_ghost_opposed_source_order_v8 parent_failure_v6_preserved 1 parent_failure_v7_preserved 1 expanded_checkpoint_manifest_sha256 76c3f1649c1b3e4767868ef50d6d44a0648d3f34a0e74fd631c46a595bb10832 parent_checkpoint_manifest_sha256 21c806f554d1b1c04671c2123da80a15c0d5001515c19fd518d79f8c42a0fa35 source_bundle_sha256 7cb5a18a5f93a320101365eaaf66125ee0b878c7a99958ddd45cc22e945563ae source_bundle_version E1i2XhZzFcsT02PhLMKAFHG1tspK3LAn binary_sha256 4d33dec7ac7352e1447ae4afc1d7b92d6edc762c0ca3e178fadc4aac4c12a03b binary_version EaWa7EqyBgp.M8mGmzlXoUE.gwz2wL2n specialization queen-extra-primary exact_shards 64 exact_iteration 2 partial_iteration_3_nodes_reused 1 max_nodes 750000000 unique_slots 1073741824 read_only_implication 1"
  cd "${root}"
  "${binary}" \
    --solve \
    --transition-prefix "${transition_prefix}" \
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
    --resume-iteration 2 \
    --resume-current-slot current \
    --resume-bdd-slot a

  sha256sum "${output}" "${arbitrary}"
  echo 'queen_ghost_opposed_source_order_v8 complete 1'
} >>"${log}" 2>&1

/usr/bin/python3 "${runner}" \
  --source-root "${source_root}" \
  --work "${root}" \
  --filename kqueenkghost.uftb \
  --piece queen \
  --orientation opposing \
  --source-table "${source_table}" \
  --source-sha256 30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d \
  --lower-table "${lower_table}" \
  --lower-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --lower-model-sha256 87147fccee31cff98fd6875f02ca51d82d20c978bcaa6d7c4aaa81d114a23c6a \
  --model-sha256 d05ac1dbf726e8830a78d191bd1764572532bfcdee05b549de919f2bbda003f5 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sidecar "${lower_ghost}" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --finalize-existing >/dev/null
