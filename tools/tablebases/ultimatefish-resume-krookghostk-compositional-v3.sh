#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-wave-aa82210e-work/krookghostk
source_root=/mnt/ultimatefish/compositional-transitions-v5-rook-source
binary=${source_root}/ultimate_ghost_rook_compositional_v5_linux
old_prefix=work/transitions/krookghostk
new_prefix=work/transitions/krookghostk-compositional-v3
prior_journal=${root}/work/logs/solve-memory-v2-journal.log
log=${root}/work/logs/compositional-merge-solve-v3.log
scratch=work/solve-compositional-v3/krookghostk
output=work/results/krookghostk-compositional-v3.ufiw
arbitrary=work/results/krookghostk-compositional-v3.ufgd

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  265f6e9aaa1e50e9ad04c2c1a347e9551ebf78ea7eea3add36b8dd4f1ca99482
test "$(sha256sum "${root}/tablebases/krookghostk.uftb" | cut -d ' ' -f 1)" = \
  0413b69806a348b158540d20c2aa13ec96cd51c49c7835f2651241f87d63a68f
test "$(sha256sum "${root}/tablebases/krookk.uftb" | cut -d ' ' -f 1)" = \
  abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/work/self-test/krookghostk.normalized.uftb" | cut -d ' ' -f 1)" = \
  f69a8d36eed9a1fe7c27a43cf328af4a2857578262030a5d972019b76c358246
test "$(sha256sum "${root}/work/transitions-ready.json" | cut -d ' ' -f 1)" = \
  d7f3e24ce0f2d7a4a6a83e78a8e52f421ef6cf1ca6533e8c90314d8964948ee1
test "$(sha256sum "${root}/${old_prefix}.verified" | cut -d ' ' -f 1)" = \
  dd1a7e8007ee77a52981a98fa9feb8a861225482ef51a2df16d72b784b139ed0
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
test ! -e "${root}/${new_prefix}.header"
test ! -e "${log}"
test ! -e "${prior_journal}"
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 107374182400

journalctl -u ultimatefish-info-solve-current-krookghostk-memory-v2.service \
  --no-pager >"${prior_journal}.tmp"
grep -Fq \
  'Dragon/Ghost error: exact external Ghost-extra solve exceeds the physical-memory gate' \
  "${prior_journal}.tmp"
mv "${prior_journal}.tmp" "${prior_journal}"

mkdir -p "${root}/work/self-test-compositional-v3" \
  "${root}/work/solve-compositional-v3" "${root}/work/results"
exec >>"${log}" 2>&1
echo "rook_ghost_same_compositional_v3 source_bundle_sha256 0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856 source_bundle_version QdBG6EaSlckJI_v1rCztxgmbr0fOYW0L binary_sha256 265f6e9aaa1e50e9ad04c2c1a347e9551ebf78ea7eea3add36b8dd4f1ca99482 exact_shards 64 old_graph_preserved ${old_prefix} prior_replay_and_memory_failure_preserved ${prior_journal} max_nodes 500000000 unique_slots 1073741824"
sha256sum "${prior_journal}"

cd "${root}"
"${binary}" \
  --self-test \
  --orientation same \
  --scratch work/self-test-compositional-v3/krookghostk \
  --input tablebases/krookghostk.uftb \
  --source-sha256 0413b69806a348b158540d20c2aa13ec96cd51c49c7835f2651241f87d63a68f
test "$(sha256sum work/self-test-compositional-v3/krookghostk.normalized.uftb | cut -d ' ' -f 1)" = \
  f69a8d36eed9a1fe7c27a43cf328af4a2857578262030a5d972019b76c358246

common=(
  --orientation same
  --lower-dragon-table tablebases/krookk.uftb
  --lower-dragon-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
  --lower-dragon-source-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
  --lower-dragon-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1
  --source-sha256 0413b69806a348b158540d20c2aa13ec96cd51c49c7835f2651241f87d63a68f
  --model-sha256 0b1c03c90ff851d3c30fedb30c99430c52717e6374ece92aac48ada83e4eceae
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
  --input tablebases/krookghostk.uftb \
  --normalized-input work/self-test/krookghostk.normalized.uftb \
  --normalized-sha256 f69a8d36eed9a1fe7c27a43cf328af4a2857578262030a5d972019b76c358246 \
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
echo 'rook_ghost_same_compositional_v3 complete 1'
