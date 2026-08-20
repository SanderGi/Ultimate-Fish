#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/kninjakghost-migrated-v1/restore/kninjakghost-fresh-v1
source_root=/mnt/ultimatefish/penguin-source-order-v6
binary=${source_root}/ultimate_ghost_ninja_source_order_v6_linux
runner=${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
lower_ghost_source=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostprincek/tablebases/kghostk.ufgm
lower_ghost=${root}/tablebases/kghostk.ufgm
ready=${root}/work/transitions-ready.json
inventory=${root}/work/migration-v1-inventory.sha256
old_prefix=work/transitions/kninjakghost
new_prefix=work/transitions/kninjakghost-source-order-v6
normalized=work/self-test-source-order-v6/kninjakghost.normalized.uftb
scratch=work/solve-source-order-v6/kninjakghost
output=work/results/kninjakghost-source-order-v6.ufiw
arbitrary=work/results/kninjakghost-source-order-v6.ufgd
log=${root}/work/logs/source-order-v6.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  536928d914f1fae04fc560f64ae077a7bdba1a6fb100f349b937b73493035380
test "$(sha256sum "${root}/tablebases/kninjakghost.uftb" | cut -d ' ' -f 1)" = \
  4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027
test "$(sha256sum "${root}/tablebases/kninjak.uftb" | cut -d ' ' -f 1)" = \
  4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776
test "$(sha256sum "${lower_ghost_source}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${ready}" | cut -d ' ' -f 1)" = \
  c72efdbaece2f4ebf3fb752e654188f02208067abcc7095f1d5ae342691a9e7e
test "$(sha256sum "${root}/${old_prefix}.verified" | cut -d ' ' -f 1)" = \
  d4568768b00a159bdd69b8cc26c3cf13777f8fb0efb98efef72ac7be4aabfb56
test "$(sha256sum "${inventory}" | cut -d ' ' -f 1)" = \
  f91fb5480d397c5e2ee6fea65713ab7b68ba9ce6297e4ef6c57708287ce7202d
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
test ! -e "${root}/${new_prefix}.header"
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${log}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 85899345920

cd "${root}"
sha256sum -c work/migration-v1-inventory.sha256 >/dev/null

if test ! -e "${lower_ghost}"; then
  cp "${lower_ghost_source}" "${lower_ghost}"
fi
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

mkdir -p work/self-test-source-order-v6 work/solve-source-order-v6 work/results

common=(
  --orientation opposing
  --lower-dragon-table tablebases/kninjak.uftb
  --lower-dragon-sha256 4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776
  --lower-dragon-source-sha256 4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776
  --lower-dragon-model-sha256 666ca18773faec47588c7d628b6bd5e4f61035fbf4d36a15a55a5558aacc0096
  --source-sha256 4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027
  --model-sha256 c68506845eceed07dee0f179f6bb97d4a2f0c085f809041fc2d5613ea1760cad
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)
shards=()
for index in {00..63}; do
  shards+=(--shard "work/transitions/shard-${index}")
done

{
  echo "ninja_ghost_opposed_source_order_v6 source_bundle_sha256 a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064 source_bundle_version WxnZuM9HYqwvK4MFGo9IjTs2A9_8eto3 migration_archive_sha256 1cbc9ac917c9e4c5f4d4399e0683859c7cd5d0e97394649c77554ffdee4753ed migration_archive_version KirtTpvJyHxkyALjTkPNK752xBj3Vc4H inventory_sha256 f91fb5480d397c5e2ee6fea65713ab7b68ba9ce6297e4ef6c57708287ce7202d binary_sha256 536928d914f1fae04fc560f64ae077a7bdba1a6fb100f349b937b73493035380 binary_version .s5ISFY93O5IZptYEy6Rv3YjJTnHbajb exact_shards 64 old_graph_preserved ${old_prefix} corrected_normalized_sha256 bd3d9de0a61fcdfb7910379a7c73283c2d299cd2adf17a38f474f4f7d650b58a max_nodes 500000000 unique_slots 1073741824"
  "${binary}" \
    --self-test \
    --orientation opposing \
    --scratch work/self-test-source-order-v6/kninjakghost \
    --input tablebases/kninjakghost.uftb \
    --source-sha256 4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027
  test "$(sha256sum "${normalized}" | cut -d ' ' -f 1)" = \
    bd3d9de0a61fcdfb7910379a7c73283c2d299cd2adf17a38f474f4f7d650b58a

  "${binary}" \
    --merge-transitions \
    --transition-prefix "${new_prefix}" \
    "${shards[@]}" \
    --expected-geometries 492960 \
    "${common[@]}"

  "${binary}" \
    --solve \
    --transition-prefix "${new_prefix}" \
    "${common[@]}" \
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
    --max-nodes 500000000 \
    --unique-slots 1073741824 \
    --compact-every 1

  sha256sum "${output}" "${arbitrary}"
  echo 'ninja_ghost_opposed_source_order_v6 complete 1'
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
  --lower-ghost-sidecar "${lower_ghost}" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --finalize-existing >/dev/null
