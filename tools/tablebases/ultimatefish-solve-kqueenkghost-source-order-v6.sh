#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish-penguin/info-remap-fix-v2/kqueenkghost-fresh-v1
source_root=/mnt/ultimatefish/source-order-v6-queen-a26d3665
binary=${source_root}/ultimate_ghost_queen_source_order_v6_linux
runner=${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
source_table=${root}/tablebases/kqueenkghost.uftb
lower_table=${root}/tablebases/kqueenk.uftb
lower_ghost_source=/mnt/ultimatefish-penguin/info-remap-fix-v2/krookkghost-fresh-v1/tablebases/kghostk.ufgm
lower_ghost=${root}/tablebases/kghostk.ufgm
old_prefix=work/transitions/kqueenkghost
new_prefix=work/transitions/kqueenkghost-source-order-v6
normalized=work/self-test-source-order-v6/kqueenkghost.normalized.uftb
scratch=work/solve-source-order-v6/kqueenkghost
output=work/results/kqueenkghost-source-order-v6.ufiw
arbitrary=work/results/kqueenkghost-source-order-v6.ufgd
log=${root}/work/logs/solve.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  d305be9bbb8ad8401d3be31c8ba6daf3c56efc306b5def0111f21a8ae492699c
test "$(sha256sum "${source_table}" | cut -d ' ' -f 1)" = \
  30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d
test "$(sha256sum "${lower_table}" | cut -d ' ' -f 1)" = \
  1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
test "$(sha256sum "${lower_ghost_source}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
test ! -e "${root}/${old_prefix}.header"
test ! -e "${root}/${new_prefix}.header"
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${log}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 68719476736

if test ! -e "${lower_ghost}"; then
  cp "${lower_ghost_source}" "${lower_ghost}"
fi
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

mkdir -p "${root}/work/self-test-source-order-v6" \
  "${root}/work/solve-source-order-v6" "${root}/work/results"

common=(
  --orientation opposing
  --lower-dragon-table tablebases/kqueenk.uftb
  --lower-dragon-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
  --lower-dragon-source-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
  --lower-dragon-model-sha256 87147fccee31cff98fd6875f02ca51d82d20c978bcaa6d7c4aaa81d114a23c6a
  --source-sha256 30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d
  --model-sha256 d05ac1dbf726e8830a78d191bd1764572532bfcdee05b549de919f2bbda003f5
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)
shards=()
for index in {00..63}; do
  shards+=(--shard "work/transitions/shard-${index}")
done

{
  echo "queen_ghost_opposed_source_order_v6 source_bundle_sha256 a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064 source_bundle_version WxnZuM9HYqwvK4MFGo9IjTs2A9_8eto3 binary_sha256 d305be9bbb8ad8401d3be31c8ba6daf3c56efc306b5def0111f21a8ae492699c binary_version 4Wanayq8gd18cwrEfNgbAj6YGeM1Asme exact_shards 64 interrupted_replay_log work/logs/merge.log corrected_normalized_sha256 8bd2029be1f63502b80db7a0e173fa00d441723ce31170b4b998c85aa2b3511a max_nodes 500000000 unique_slots 1073741824"
  cd "${root}"
  "${binary}" \
    --self-test \
    --orientation opposing \
    --scratch work/self-test-source-order-v6/kqueenkghost \
    --input tablebases/kqueenkghost.uftb \
    --source-sha256 30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d
  test "$(sha256sum "${normalized}" | cut -d ' ' -f 1)" = \
    8bd2029be1f63502b80db7a0e173fa00d441723ce31170b4b998c85aa2b3511a

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
    --input tablebases/kqueenkghost.uftb \
    --normalized-input "${normalized}" \
    --normalized-sha256 8bd2029be1f63502b80db7a0e173fa00d441723ce31170b4b998c85aa2b3511a \
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
  echo 'queen_ghost_opposed_source_order_v6 complete 1'
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
