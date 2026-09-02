#!/usr/bin/env bash
set -euo pipefail

readonly root=/mnt/ultimatefish/opposed-sniper-v12
readonly binary=/mnt/ultimatefish/opposed-sniper-directional-remap-v16/ultimate_ghost_sniper_information_tablebase-v16
readonly prefix=work/transitions/kghostksniper-current-v15
readonly scratch=work/solve-v12/kghostksniper
readonly input=tablebases/kghostksniper-current-v15.uftb
readonly output=work/results/kghostksniper-directional-v16.ufiw
readonly arbitrary=work/results/kghostksniper-directional-v16.ufgd
readonly log=${root}/work/logs/directional-certify-v16.log
readonly source_sha=4c1ddc17c5f60059018c3119e34ce63c4535236b10b629d3ca94bc9d4317631e

test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  f931ec88436dbcf7a1ff69b2d8c83a2e6ea7b445ef6aa16eaa319723baf222db
test "$(sha256sum "${root}/${prefix}.verified" | cut -d ' ' -f1)" = \
  a342986eb103720eb26ffe7536cece822ddb4c0015b289cb26cf43ef151dad44
test "$(sha256sum "${root}/${input}" | cut -d ' ' -f1)" = "${source_sha}"
test "$(sha256sum "${root}/tablebases/ksniperk.uftb" | cut -d ' ' -f1)" = \
  473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test -s "${root}/${scratch}.bdd-a.nodes"
test -s "${root}/${scratch}.bdd-a.unique"
test -s "${root}/${scratch}.owner-next"
test -s "${root}/${scratch}.observer-next"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"

cd "${root}"
install -d -m 0755 work/results work/logs
exec >>"${log}" 2>&1
echo 'sniper_ghost_opposed_directional_v16 completed_iteration 69 current_slot next bdd_slot a resume_converged 1 workers 32 vertical_color_remap 1'
common=(
  --orientation opposing
  --transition-prefix "${prefix}"
  --lower-dragon-table tablebases/ksniperk.uftb
  --lower-dragon-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
  --lower-dragon-source-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
  --lower-dragon-model-sha256 7eaa2c6b443d15a98f935cf7b9fec3e6991a841246a2af8290afadaf1b3feab1
  --source-sha256 "${source_sha}"
  --model-sha256 69cd2e01bf3286e4de59f8fe2f3b3c8b02aa48268d09e034ee02d9a4b09e0878
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)

"${binary}" --verify-transitions "${common[@]}"
"${binary}" --solve "${common[@]}" \
  --input "${input}" \
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
  --compact-every 1 \
  --resume-fixed-point \
  --resume-converged \
  --resume-iteration 69 \
  --resume-current-slot next \
  --resume-bdd-slot a

sha256sum "${output}" "${arbitrary}"
echo 'sniper_ghost_opposed_directional_v16 complete 1'
