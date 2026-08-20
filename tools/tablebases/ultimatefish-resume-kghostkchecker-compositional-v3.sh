#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-substate-corrected-v1/kghostkchecker
source_root=/mnt/ultimatefish/compositional-transitions-v3-source
binary=${source_root}/ultimate_ghost_checker_compositional_v3_linux
lower_ghost=/mnt/ultimatefish/info-wave-aa82210e-stage/kghostk.ufgm
log=${root}/work/logs/compositional-merge-solve-v3.log
new_prefix=work/transitions/kghostkchecker-compositional-v3

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  75a1a89f7022478bc3fdcad8da83d1694e06463be559e0d1fbc804ba40b0cd83
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  a7cc54822d678a0e569ab963ce0857cc69d4b14c6c5cbf838257376254cd081f
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/tablebases/kghostkchecker.uftb" | cut -d ' ' -f 1)" = \
  bcbc6c6bc88a2c18939222513732016d72e0288429a74ee522f2e5c2f6d3b2b4
test "$(sha256sum "${root}/work/self-test/kghostkchecker.normalized.uftb" | cut -d ' ' -f 1)" = \
  0400c77f6d33ba827b6eb53f5f707144eda3b0daeed430cd94ae5d491bc2fef2
test "$(sha256sum "${root}/work/transitions-ready.json" | cut -d ' ' -f 1)" = \
  22ec7ab9352166d32d873d9cc59b0eed4a452d133ada9541698e596f00c21a6c
grep -Fq '"orientation": "opposing"' "${root}/work/transitions-ready.json"
grep -Fq '"source_sha256": "bcbc6c6bc88a2c18939222513732016d72e0288429a74ee522f2e5c2f6d3b2b4"' \
  "${root}/work/transitions-ready.json"
grep -Fq '"model_sha256": "bb4e8b3b44571b7ed5caad19dc17d26db9afe976393175f399f5e546ea490316"' \
  "${root}/work/transitions-ready.json"
test "$(find "${root}/work/transitions" -maxdepth 1 -name 'shard-*.verified' | wc -l)" = 64
test "$(stat -c %s "${root}/work/transitions/kghostkchecker.verified")" = 248
test ! -e "${root}/${new_prefix}.header"
test ! -e "${log}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 161061273600

install -m 0644 "${lower_ghost}" "${root}/tablebases/kghostk.ufgm"
mkdir -p "${root}/work/self-test-v3" "${root}/work/results" \
  "${root}/work/solve-compositional-v3"

exec >>"${log}" 2>&1
echo "checker_opposed_compositional_v3 source_bundle_sha256 75a1a89f7022478bc3fdcad8da83d1694e06463be559e0d1fbc804ba40b0cd83 source_bundle_version 5Ig1aSjwOsVHwI4opCNihZRgvJg_2YXd binary_sha256 a7cc54822d678a0e569ab963ce0857cc69d4b14c6c5cbf838257376254cd081f binary_version QTOUmUsw96Qf7Uz2jNq3j5yx_TZpMn2T exhaustive_replay_log_preserved ${root}/work/logs/merge.log exact_shards 64 old_merged_graph_preserved work/transitions/kghostkchecker"

cd "${root}"
"${binary}" \
  --self-test \
  --orientation opposing \
  --input tablebases/kghostkchecker.uftb \
  --source-sha256 bcbc6c6bc88a2c18939222513732016d72e0288429a74ee522f2e5c2f6d3b2b4 \
  --scratch work/self-test-v3/kghostkchecker
test "$(sha256sum work/self-test-v3/kghostkchecker.normalized.uftb | cut -d ' ' -f 1)" = \
  0400c77f6d33ba827b6eb53f5f707144eda3b0daeed430cd94ae5d491bc2fef2

common=(
  --orientation opposing
  --lower-dragon-table implicit-draw
  --lower-dragon-sha256 0dcb4039ec2305298e9f35d36ce8d0e54fe76acc013c188d08e0e2a6da0ef22c
  --lower-dragon-source-sha256 0dcb4039ec2305298e9f35d36ce8d0e54fe76acc013c188d08e0e2a6da0ef22c
  --lower-dragon-model-sha256 42566cc6e4186de2f29a2ded6e9a0728e8dc6d7e748d936e574f15f5202c1763
  --source-sha256 bcbc6c6bc88a2c18939222513732016d72e0288429a74ee522f2e5c2f6d3b2b4
  --model-sha256 bb4e8b3b44571b7ed5caad19dc17d26db9afe976393175f399f5e546ea490316
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
  --expected-geometries 3943680 \
  "${common[@]}"

"${binary}" \
  --solve \
  --transition-prefix "${new_prefix}" \
  "${common[@]}" \
  --input tablebases/kghostkchecker.uftb \
  --normalized-input work/self-test/kghostkchecker.normalized.uftb \
  --normalized-sha256 0400c77f6d33ba827b6eb53f5f707144eda3b0daeed430cd94ae5d491bc2fef2 \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch work/solve-compositional-v3/kghostkchecker \
  --output work/results/kghostkchecker-compositional-v3.ufiw \
  --output-arbitrary work/results/kghostkchecker-compositional-v3.ufgd \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1500000000 \
  --unique-slots 2147483648 \
  --compact-every 1

sha256sum work/results/kghostkchecker-compositional-v3.ufiw \
  work/results/kghostkchecker-compositional-v3.ufgd
echo 'checker_opposed_compositional_v3 complete 1'
