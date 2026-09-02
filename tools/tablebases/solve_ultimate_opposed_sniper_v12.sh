#!/usr/bin/env bash
set -euo pipefail

readonly root=/mnt/ultimatefish/opposed-sniper-v12
readonly binary=${root}/bin/ultimate_ghost_sniper_information_tablebase
readonly prefix=work/transitions/kghostksniper
readonly scratch=work/solve-v12/kghostksniper
readonly output=work/results/kghostksniper.ufiw
readonly arbitrary=work/results/kghostksniper.ufgd
readonly log=${root}/work/logs/solve-v12.log

test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  d067036152298e71e00fef4f620c7779dc65a8d1dcb00dc202962df994909be1
test "$(sha256sum "${root}/tablebases/kghostksniper.uftb" | cut -d ' ' -f 1)" = \
  c9541415922355346dddf284541edb67982b0250316549043bcfbe23efa79e78
test "$(sha256sum "${root}/tablebases/ksniperk.uftb" | cut -d ' ' -f 1)" = \
  473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/work/self-test/kghostksniper.normalized.uftb" | cut -d ' ' -f 1)" = \
  867864ea1af08aa9b3ba8e5e35f8dcdf6760be27dd1885e5bd3b228cd95209d3
test "$(sha256sum "${root}/${prefix}.verified" | cut -d ' ' -f 1)" = \
  d93fca5ede82c6e7a09f695ff80da2e9e2e446068bf1c5fbfdad97d6b27bbbc5
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 322122547200
mkdir -p "${root}/${scratch}" "${root}/work/logs" "${root}/work/results"

exec >>"${log}" 2>&1
echo "sniper_ghost_opposed_fresh_v12 transition_archive_sha256 70ccd4b4b757afc5b6ecf951fab137f4eae6558d40ecb2252afbac8053c64692 transition_archive_version BDet.ZYR8kAAi_wGG8QDrI32vAhGElus binary_sha256 d067036152298e71e00fef4f620c7779dc65a8d1dcb00dc202962df994909be1 fresh_fixed_point 1 workers 32 max_nodes 1500000000 unique_slots 2147483648"
cd "${root}"
common=(
  --orientation opposing
  --transition-prefix "${prefix}"
  --lower-dragon-table tablebases/ksniperk.uftb
  --lower-dragon-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
  --lower-dragon-source-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
  --lower-dragon-model-sha256 7eaa2c6b443d15a98f935cf7b9fec3e6991a841246a2af8290afadaf1b3feab1
  --source-sha256 c9541415922355346dddf284541edb67982b0250316549043bcfbe23efa79e78
  --model-sha256 69cd2e01bf3286e4de59f8fe2f3b3c8b02aa48268d09e034ee02d9a4b09e0878
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)

"${binary}" --verify-transitions "${common[@]}"
"${binary}" \
  --solve \
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
  --max-nodes 1500000000 \
  --unique-slots 2147483648 \
  --workers 32 \
  --compact-every 1

sha256sum "${output}" "${arbitrary}"
echo 'sniper_ghost_opposed_fresh_v12 complete 1'
