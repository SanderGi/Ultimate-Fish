#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-substate-corrected-v1/kghostsniperk-v2
source_root=/mnt/ultimatefish/compositional-transitions-v5-sniper-same-source
binary=${source_root}/ultimate_ghost_sniper_compositional_v5_linux
old_prefix=work/transitions/kghostsniperk
new_prefix=work/transitions/kghostsniperk-compositional-v3
prior_journal=${root}/work/logs/solve-memory-v2-replay-journal.log
log=${root}/work/logs/compositional-merge-solve-v3.log
scratch=work/solve-compositional-v3/kghostsniperk
output=work/results/kghostsniperk-compositional-v3.ufiw
arbitrary=work/results/kghostsniperk-compositional-v3.ufgd

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  42e719d0723c0faaa45a705bc54561ca2e66406f23dff95a4999f54e3adc1775
test "$(sha256sum "${root}/tablebases/kghostsniperk.uftb" | cut -d ' ' -f 1)" = \
  b627e286865f3081805a8a81ab8ec4ec8938c8a8486f3df8a0ae6a3edc4a12f1
test "$(sha256sum "${root}/tablebases/ksniperk.uftb" | cut -d ' ' -f 1)" = \
  473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/work/self-test/kghostsniperk.normalized.uftb" | cut -d ' ' -f 1)" = \
  d4eecdb96138a9cec096718ee1781b1de6bc914b417b90ca2514be1291bf4814
test "$(sha256sum "${root}/work/transitions-ready.json" | cut -d ' ' -f 1)" = \
  deb4a7237d10cd06f5529270c6ab6567de8061f3100a3dc5f15797c174f6bd1e
grep -Fq '"orientation": "same"' "${root}/work/transitions-ready.json"
grep -Fq \
  '"source_sha256": "b627e286865f3081805a8a81ab8ec4ec8938c8a8486f3df8a0ae6a3edc4a12f1"' \
  "${root}/work/transitions-ready.json"
grep -Fq \
  '"model_sha256": "37ce97c6639e74eb2d8caf9843abed7bee8a3cb3937ad5383e582046c2183b0d"' \
  "${root}/work/transitions-ready.json"
test "$(sha256sum "${root}/${old_prefix}.verified" | cut -d ' ' -f 1)" = \
  3f0b1edd236d044d2e3589e9758edaad3ce24033b2bee1dee77d5850142f1656
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' -printf '%s\n' | sort -u)" = 248
test ! -e "${root}/${new_prefix}.header"
test ! -e "${log}"
test ! -e "${prior_journal}"
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 161061273600

journalctl -u ultimatefish-info-solve-current-kghostsniperk-memory-v2.service \
  --no-pager >"${prior_journal}.tmp"
grep -Fq 'ghost_extra_external_reload 750000/3943680' \
  "${prior_journal}.tmp"
mv "${prior_journal}.tmp" "${prior_journal}"

mkdir -p "${root}/work/self-test-compositional-v3" \
  "${root}/work/solve-compositional-v3" "${root}/work/results"
exec >>"${log}" 2>&1
echo "sniper_ghost_same_compositional_v3 source_bundle_sha256 0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856 source_bundle_version QdBG6EaSlckJI_v1rCztxgmbr0fOYW0L binary_sha256 42e719d0723c0faaa45a705bc54561ca2e66406f23dff95a4999f54e3adc1775 binary_version CN4tUKr0pKjklG7kfBEuWAvbrB0LeA58 exact_shards 64 old_graph_preserved ${old_prefix} redundant_replay_stopped_and_preserved ${prior_journal} max_nodes 1000000000 unique_slots 1073741824"
sha256sum "${prior_journal}"

cd "${root}"
"${binary}" \
  --self-test \
  --orientation same \
  --input tablebases/kghostsniperk.uftb \
  --source-sha256 b627e286865f3081805a8a81ab8ec4ec8938c8a8486f3df8a0ae6a3edc4a12f1 \
  --scratch work/self-test-compositional-v3/kghostsniperk
test "$(sha256sum work/self-test-compositional-v3/kghostsniperk.normalized.uftb | cut -d ' ' -f 1)" = \
  d4eecdb96138a9cec096718ee1781b1de6bc914b417b90ca2514be1291bf4814

common=(
  --orientation same
  --lower-dragon-table tablebases/ksniperk.uftb
  --lower-dragon-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
  --lower-dragon-source-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
  --lower-dragon-model-sha256 7eaa2c6b443d15a98f935cf7b9fec3e6991a841246a2af8290afadaf1b3feab1
  --source-sha256 b627e286865f3081805a8a81ab8ec4ec8938c8a8486f3df8a0ae6a3edc4a12f1
  --model-sha256 37ce97c6639e74eb2d8caf9843abed7bee8a3cb3937ad5383e582046c2183b0d
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
  --input tablebases/kghostsniperk.uftb \
  --normalized-input work/self-test/kghostsniperk.normalized.uftb \
  --normalized-sha256 d4eecdb96138a9cec096718ee1781b1de6bc914b417b90ca2514be1291bf4814 \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${output}" \
  --output-arbitrary "${arbitrary}" \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1000000000 \
  --unique-slots 1073741824 \
  --compact-every 1

sha256sum "${output}" "${arbitrary}"
echo 'sniper_ghost_same_compositional_v3 complete 1'
