#!/usr/bin/env bash
set -euo pipefail

readonly root=/mnt/ultimatefish/opposed-sniper-v12
readonly binary=/mnt/ultimatefish/opposed-sniper-remap-auth-v14/ultimate_ghost_sniper_information_tablebase-v14
readonly prefix=work/transitions/kghostksniper
readonly scratch=work/solve-v12/kghostksniper
readonly output=work/results/kghostksniper.ufiw
readonly arbitrary=work/results/kghostksniper.ufgd
readonly normalized=work/self-test/kghostksniper-corrected-v14.normalized.uftb
readonly log=${root}/work/logs/remap-auth-resume-v14.log

test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  62644c757bb0c8e9d6931322a188c67e51d65b5db94e74bb4121e4f42d872721
test "$(sha256sum "${root}/tablebases/kghostksniper.uftb" | cut -d ' ' -f 1)" = \
  c9541415922355346dddf284541edb67982b0250316549043bcfbe23efa79e78
test "$(sha256sum "${root}/tablebases/ksniperk.uftb" | cut -d ' ' -f 1)" = \
  473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/${normalized}" | cut -d ' ' -f 1)" = \
  5ecd1e7d5854eee3a5cd48de4c4dc6932c880e2645fe82b53376a0b7ab79c4d3
test "$(sha256sum "${root}/${prefix}.verified" | cut -d ' ' -f 1)" = \
  d93fca5ede82c6e7a09f695ff80da2e9e2e446068bf1c5fbfdad97d6b27bbbc5
test -s "${root}/${scratch}.bdd-a.nodes"
test -s "${root}/${scratch}.bdd-a.unique"
test -s "${root}/${scratch}.owner-next"
test -s "${root}/${scratch}.observer-next"
test -s "${root}/${scratch}.visible-owner-next"
test -s "${root}/${scratch}.visible-observer-next"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 214748364800
mkdir -p "${root}/work/results"

exec >>"${log}" 2>&1
echo "sniper_ghost_opposed_remap_auth_v14 completed_iteration 69 current_slot next bdd_slot a resume_converged 1 workers 32"
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
"${binary}" --solve "${common[@]}" \
  --input tablebases/kghostksniper.uftb \
  --normalized-input "${normalized}" \
  --normalized-sha256 5ecd1e7d5854eee3a5cd48de4c4dc6932c880e2645fe82b53376a0b7ab79c4d3 \
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
echo 'sniper_ghost_opposed_remap_auth_v14 complete 1'
