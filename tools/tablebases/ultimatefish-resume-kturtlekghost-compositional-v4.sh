#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish-penguin/info-remap-fix-v2/kturtlekghost-fresh-v1
source_root=/mnt/ultimatefish/compositional-transitions-v5-source
binary=${source_root}/ultimate_ghost_turtle_compositional_v5_linux
old_prefix=work/transitions/kturtlekghost
new_prefix=work/transitions/kturtlekghost-compositional-v4
prior_log=${root}/work/logs/solve.log
log=${root}/work/logs/compositional-merge-solve-v4.log
scratch=work/solve-compositional-v4/kturtlekghost
output=work/results/kturtlekghost-compositional-v4.ufiw
arbitrary=work/results/kturtlekghost-compositional-v4.ufgd

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  c073ee36e7d6ef2811849a6b1f77a15618c1ab893f750045fcf32b34d08cf852
test "$(sha256sum "${root}/tablebases/kturtlekghost.uftb" | cut -d ' ' -f 1)" = \
  40ddd27df10d73efac5e2203dc06c4c9ba1258927a2465ab812c3fb24af49e39
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/work/self-test/kturtlekghost.normalized.uftb" | cut -d ' ' -f 1)" = \
  85b8d05decef63632f5fff6127288ff2b2a306d07afe5f3b144bed236fb5f2a0
test "$(sha256sum "${root}/work/transitions-ready.json" | cut -d ' ' -f 1)" = \
  bfbe326ee8c82ce376a9d3ea001b2f970c0eb9eabd34542987b83c4a970ac842
test "$(sha256sum "${root}/${old_prefix}.verified" | cut -d ' ' -f 1)" = \
  23397b92b7f7673c3913ead5b7208eb1fe2cc13654b301c9824df564e1194cf8
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
grep -Fq 'ghost_extra_external_reload 30000/492960' "${prior_log}"
test ! -e "${root}/${new_prefix}.header"
test ! -e "${log}"
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 64424509440

mkdir -p "${root}/work/self-test-compositional-v4" \
  "${root}/work/solve-compositional-v4" "${root}/work/results"
exec >>"${log}" 2>&1
echo "turtle_ghost_opposed_compositional_v4 source_bundle_sha256 0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856 source_bundle_version QdBG6EaSlckJI_v1rCztxgmbr0fOYW0L binary_sha256 c073ee36e7d6ef2811849a6b1f77a15618c1ab893f750045fcf32b34d08cf852 binary_version FOz2Y0M5zXbuIihq2uWrhfAvtc6uMpO1 exact_shards 64 old_graph_preserved ${old_prefix} redundant_replay_stopped_and_preserved ${prior_log} max_nodes 500000000 unique_slots 1073741824"

cd "${root}"
"${binary}" \
  --self-test \
  --orientation opposing \
  --scratch work/self-test-compositional-v4/kturtlekghost \
  --input tablebases/kturtlekghost.uftb \
  --source-sha256 40ddd27df10d73efac5e2203dc06c4c9ba1258927a2465ab812c3fb24af49e39
test "$(sha256sum work/self-test-compositional-v4/kturtlekghost.normalized.uftb | cut -d ' ' -f 1)" = \
  85b8d05decef63632f5fff6127288ff2b2a306d07afe5f3b144bed236fb5f2a0

common=(
  --orientation opposing
  --lower-dragon-table implicit-draw
  --lower-dragon-sha256 ec27a3cb8babe7c87b0c13229ff51d454da3bed542aa8c27862bfcc630ef26ce
  --lower-dragon-source-sha256 ec27a3cb8babe7c87b0c13229ff51d454da3bed542aa8c27862bfcc630ef26ce
  --lower-dragon-model-sha256 0bfcb5141c1732f3eef3830b1d7552540f9511aa047ec40169f369d24f5f37ee
  --source-sha256 40ddd27df10d73efac5e2203dc06c4c9ba1258927a2465ab812c3fb24af49e39
  --model-sha256 b4f71efb40b7d7ed9c97f346b9074d9e934f3c542c5d19827dae9ccebdafbe7d
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)
shards=()
for index in {00..63}; do
  shards+=(--shard "work/transitions/shard-${index}")
done

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
  --input tablebases/kturtlekghost.uftb \
  --normalized-input work/self-test/kturtlekghost.normalized.uftb \
  --normalized-sha256 85b8d05decef63632f5fff6127288ff2b2a306d07afe5f3b144bed236fb5f2a0 \
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
echo 'turtle_ghost_opposed_compositional_v4 complete 1'
