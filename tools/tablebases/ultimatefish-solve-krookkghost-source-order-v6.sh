#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish-penguin/info-remap-fix-v2/krookkghost-fresh-v1
source_root=/mnt/ultimatefish/source-order-v6-rook-a26d3665
binary=${source_root}/ultimate_ghost_rook_source_order_v6_linux
runner=/mnt/ultimatefish/ghost-extra-remap-fix-v2-source/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
source_table=${root}/tablebases/krookkghost.uftb
lower_table=${root}/tablebases/krookk.uftb
lower_ghost_source=/mnt/ultimatefish/info-wave-aa82210e-stage/kghostk.ufgm
lower_ghost=${root}/tablebases/kghostk.ufgm
ready=${root}/work/transitions-ready.json
old_prefix=work/transitions/krookkghost
new_prefix=work/transitions/krookkghost-source-order-v6
normalized=work/self-test-source-order-v6/krookkghost.normalized.uftb
scratch=work/solve-source-order-v6/krookkghost
output=work/results/krookkghost-source-order-v6.ufiw
arbitrary=work/results/krookkghost-source-order-v6.ufgd
log=${root}/work/logs/solve.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  cd15f2ee785be750f86b68d10832ca3fbcc5f405b9f3e5a21fd6489a44a3fda1
test "$(sha256sum "${source_table}" | cut -d ' ' -f 1)" = \
  bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41
test "$(sha256sum "${lower_table}" | cut -d ' ' -f 1)" = \
  abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
test "$(sha256sum "${lower_ghost_source}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${ready}" | cut -d ' ' -f 1)" = \
  660ac0157db8dcce6bd839f2571f50220af95675d26bc6b0dfb7030c93a29e5f
test "$(sha256sum "${root}/${old_prefix}.verified" | cut -d ' ' -f 1)" = \
  e16a0e70c1831d5710567e09aa6cb34dd351a9129a39d0789fd6b1e651b93c32
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
test ! -e "${root}/${new_prefix}.header"
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${log}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 85899345920

if test ! -e "${lower_ghost}"; then
  cp "${lower_ghost_source}" "${lower_ghost}"
fi
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

mkdir -p "${root}/work/self-test-source-order-v6" \
  "${root}/work/solve-source-order-v6" "${root}/work/results"

common=(
  --orientation opposing
  --lower-dragon-table tablebases/krookk.uftb
  --lower-dragon-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
  --lower-dragon-source-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
  --lower-dragon-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1
  --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41
  --model-sha256 ff692f8217e6327b6ab56ab41ee3b9a8f02eecf5b80fc59599a753f340dd80f5
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)
shards=()
for index in {00..63}; do
  shards+=(--shard "work/transitions/shard-${index}")
done

{
  echo "rook_ghost_opposed_source_order_v6 source_bundle_sha256 a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064 source_bundle_version WxnZuM9HYqwvK4MFGo9IjTs2A9_8eto3 binary_sha256 cd15f2ee785be750f86b68d10832ca3fbcc5f405b9f3e5a21fd6489a44a3fda1 binary_version YXdC2SHpooSP1SJ3X8Kq60nuo9KXhHAO exact_shards 64 old_graph_preserved ${old_prefix} corrected_normalized_sha256 790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785 max_nodes 500000000 unique_slots 1073741824"
  cd "${root}"
  "${binary}" \
    --self-test \
    --orientation opposing \
    --scratch work/self-test-source-order-v6/krookkghost \
    --input tablebases/krookkghost.uftb \
    --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41
  test "$(sha256sum "${normalized}" | cut -d ' ' -f 1)" = \
    790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785

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
    --input tablebases/krookkghost.uftb \
    --normalized-input "${normalized}" \
    --normalized-sha256 790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785 \
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
  echo 'rook_ghost_opposed_source_order_v6 complete 1'
} >>"${log}" 2>&1

/usr/bin/python3 "${runner}" \
  --source-root /mnt/ultimatefish/ghost-extra-remap-fix-v2-source \
  --work "${root}" \
  --filename krookkghost.uftb \
  --piece rook \
  --orientation opposing \
  --source-table "${source_table}" \
  --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
  --lower-table "${lower_table}" \
  --lower-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
  --lower-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1 \
  --model-sha256 ff692f8217e6327b6ab56ab41ee3b9a8f02eecf5b80fc59599a753f340dd80f5 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sidecar "${lower_ghost}" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --finalize-existing >/dev/null
