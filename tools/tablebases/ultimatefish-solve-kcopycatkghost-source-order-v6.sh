#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-remap-fix-v2/kcopycatkghost-fresh-v1
source_root=/mnt/ultimatefish/source-order-v6-copycat-a26d3665
binary=${source_root}/ultimate_ghost_copycat_source_order_v6_linux
runner=/mnt/ultimatefish/ghost-extra-remap-fix-v2-source/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
source_table=${root}/tablebases/kcopycatkghost.uftb
lower_table=${root}/tablebases/kcopycatk.uftb
lower_ghost_source=/mnt/ultimatefish/info-wave-aa82210e-work/kcopycatkghost/tablebases/kghostk.ufgm
lower_ghost=${root}/tablebases/kghostk.ufgm
ready=${root}/work/transitions-ready.json
old_prefix=work/transitions/kcopycatkghost
new_prefix=work/transitions/kcopycatkghost-source-order-v6
normalized=work/self-test-source-order-v6/kcopycatkghost.normalized.uftb
scratch=work/solve-source-order-v6/kcopycatkghost
output=work/results/kcopycatkghost-source-order-v6.ufiw
arbitrary=work/results/kcopycatkghost-source-order-v6.ufgd
log=${root}/work/logs/solve.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  e2889ac280f680f5bb3d33b190320fc13b55a7f9f90dc062e709181472827a3f
test "$(sha256sum "${source_table}" | cut -d ' ' -f 1)" = \
  24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50
test "$(sha256sum "${lower_table}" | cut -d ' ' -f 1)" = \
  98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb
test "$(sha256sum "${lower_ghost_source}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${ready}" | cut -d ' ' -f 1)" = \
  3694b19e2c04e02385c96ce4ff7d2f7c50d0d9bac048dc2cf512b2c04e9e3e0d
test "$(sha256sum "${root}/${old_prefix}.verified" | cut -d ' ' -f 1)" = \
  d688ffabfa966f32c13c68412d96aefd6c67b19a4a26da5094fc2e322614b6c3
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
  --lower-dragon-table tablebases/kcopycatk.uftb
  --lower-dragon-sha256 98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb
  --lower-dragon-source-sha256 98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb
  --lower-dragon-model-sha256 d94883c4fd100918a23947c3c71d5e461588778aefbbe037f260d48d5083095f
  --source-sha256 24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50
  --model-sha256 0f9a93e320f77de4d390b726ef4a34ba9c456e419c33cdb26e098d20280b3f8f
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)
shards=()
for index in {00..63}; do
  shards+=(--shard "work/transitions/shard-${index}")
done

{
  echo "copycat_ghost_opposed_source_order_v6 source_bundle_sha256 a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064 source_bundle_version WxnZuM9HYqwvK4MFGo9IjTs2A9_8eto3 binary_sha256 e2889ac280f680f5bb3d33b190320fc13b55a7f9f90dc062e709181472827a3f binary_version zf.DUIyfF72E6_Rmn1wklZrso1_BsD7X exact_shards 64 old_graph_preserved ${old_prefix} corrected_normalized_sha256 736075bc025eecff05311bdf2e14b5e0eb2ebfcd6af51e06bb8b94fdcddc8dfb max_nodes 500000000 unique_slots 1073741824"
  cd "${root}"
  "${binary}" \
    --self-test \
    --orientation opposing \
    --scratch work/self-test-source-order-v6/kcopycatkghost \
    --input tablebases/kcopycatkghost.uftb \
    --source-sha256 24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50
  test "$(sha256sum "${normalized}" | cut -d ' ' -f 1)" = \
    736075bc025eecff05311bdf2e14b5e0eb2ebfcd6af51e06bb8b94fdcddc8dfb

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
    --input tablebases/kcopycatkghost.uftb \
    --normalized-input "${normalized}" \
    --normalized-sha256 736075bc025eecff05311bdf2e14b5e0eb2ebfcd6af51e06bb8b94fdcddc8dfb \
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
  echo 'copycat_ghost_opposed_source_order_v6 complete 1'
} >>"${log}" 2>&1

/usr/bin/python3 "${runner}" \
  --source-root /mnt/ultimatefish/ghost-extra-remap-fix-v2-source \
  --work "${root}" \
  --filename kcopycatkghost.uftb \
  --piece copycat \
  --orientation opposing \
  --source-table "${source_table}" \
  --source-sha256 24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50 \
  --lower-table "${lower_table}" \
  --lower-sha256 98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb \
  --lower-model-sha256 d94883c4fd100918a23947c3c71d5e461588778aefbbe037f260d48d5083095f \
  --model-sha256 0f9a93e320f77de4d390b726ef4a34ba9c456e419c33cdb26e098d20280b3f8f \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sidecar "${lower_ghost}" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --finalize-existing >/dev/null
