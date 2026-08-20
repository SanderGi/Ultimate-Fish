#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-substate-corrected-v1/kghostksniper
source_root=/mnt/ultimatefish/compositional-transitions-v5-source
binary=${source_root}/ultimate_ghost_sniper_compositional_v5_linux
lower_ghost=/mnt/ultimatefish/info-wave-aa82210e-stage/kghostk.ufgm
old_prefix=work/transitions/kghostksniper
new_prefix=work/transitions/kghostksniper-compositional-v5
prior_log=${root}/work/logs/solve-1b-v4.log
log=${root}/work/logs/compositional-merge-solve-v5.log
scratch=work/solve-compositional-v5/kghostksniper
output=work/results/kghostksniper-compositional-v5.ufiw
arbitrary=work/results/kghostksniper-compositional-v5.ufgd

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  42e719d0723c0faaa45a705bc54561ca2e66406f23dff95a4999f54e3adc1775
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/tablebases/kghostksniper.uftb" | cut -d ' ' -f 1)" = \
  c9541415922355346dddf284541edb67982b0250316549043bcfbe23efa79e78
test "$(sha256sum "${root}/tablebases/ksniperk.uftb" | cut -d ' ' -f 1)" = \
  473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
test "$(sha256sum "${root}/work/self-test/kghostksniper.normalized.uftb" | cut -d ' ' -f 1)" = \
  867864ea1af08aa9b3ba8e5e35f8dcdf6760be27dd1885e5bd3b228cd95209d3
test "$(sha256sum "${root}/work/transitions-ready.json" | cut -d ' ' -f 1)" = \
  181d1fee675c3f93231961f9ac31c36531d8b9329111e0a76020968c21df4600
grep -Fq '"orientation": "opposing"' "${root}/work/transitions-ready.json"
grep -Fq \
  '"source_sha256": "c9541415922355346dddf284541edb67982b0250316549043bcfbe23efa79e78"' \
  "${root}/work/transitions-ready.json"
grep -Fq \
  '"model_sha256": "69cd2e01bf3286e4de59f8fe2f3b3c8b02aa48268d09e034ee02d9a4b09e0878"' \
  "${root}/work/transitions-ready.json"
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
test "$(stat -c %s "${root}/${old_prefix}.verified")" = 248
grep -Fq \
  'ghost_extra_external_transition_certificate geometries 3943680 worlds 303663360 edges 2786616772 strata 1971816 metadata_residual 0 transition_residual 0 geometry_start 0 conservation_residual 0' \
  "${prior_log}"
grep -Fq \
  'Dragon/Ghost error: exact external Ghost-extra solve exceeds the physical-memory gate' \
  "${prior_log}"
test ! -e "${root}/${new_prefix}.header"
test ! -e "${log}"
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 150323855360

install -m 0644 "${lower_ghost}" "${root}/tablebases/kghostk.ufgm"
mkdir -p "${root}/work/self-test-compositional-v5" \
  "${root}/work/solve-compositional-v5" "${root}/work/results"

exec >>"${log}" 2>&1
echo "sniper_opposed_compositional_v5 source_bundle_sha256 0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856 source_bundle_version QdBG6EaSlckJI_v1rCztxgmbr0fOYW0L binary_sha256 42e719d0723c0faaa45a705bc54561ca2e66406f23dff95a4999f54e3adc1775 binary_version CN4tUKr0pKjklG7kfBEuWAvbrB0LeA58 exhaustive_replay_and_failure_log_preserved ${prior_log} exact_shards 64 old_merged_graph_preserved ${old_prefix} new_strong_graph ${new_prefix}"

cd "${root}"
"${binary}" \
  --self-test \
  --orientation opposing \
  --input tablebases/kghostksniper.uftb \
  --source-sha256 c9541415922355346dddf284541edb67982b0250316549043bcfbe23efa79e78 \
  --scratch work/self-test-compositional-v5/kghostksniper
test "$(sha256sum work/self-test-compositional-v5/kghostksniper.normalized.uftb | cut -d ' ' -f 1)" = \
  867864ea1af08aa9b3ba8e5e35f8dcdf6760be27dd1885e5bd3b228cd95209d3

common=(
  --orientation opposing
  --lower-dragon-table tablebases/ksniperk.uftb
  --lower-dragon-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
  --lower-dragon-source-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
  --lower-dragon-model-sha256 7eaa2c6b443d15a98f935cf7b9fec3e6991a841246a2af8290afadaf1b3feab1
  --source-sha256 c9541415922355346dddf284541edb67982b0250316549043bcfbe23efa79e78
  --model-sha256 69cd2e01bf3286e4de59f8fe2f3b3c8b02aa48268d09e034ee02d9a4b09e0878
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
  --input tablebases/kghostksniper.uftb \
  --normalized-input work/self-test/kghostksniper.normalized.uftb \
  --normalized-sha256 867864ea1af08aa9b3ba8e5e35f8dcdf6760be27dd1885e5bd3b228cd95209d3 \
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
echo 'sniper_opposed_compositional_v5 complete 1'
