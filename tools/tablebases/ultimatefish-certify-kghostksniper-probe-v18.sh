#!/usr/bin/env bash
# Certify the opposed Sniper/Ghost information table against the corrected
# directional concrete oracle.  The expensive transition payload and converged
# information fixed point are retained; only their authenticated concrete-source
# binding is changed.
set -euo pipefail

readonly root=/mnt/ultimatefish/opposed-sniper-v12
readonly probe_root=/mnt/ultimatefish/sniper-concrete-probe-v18
readonly binary=/mnt/ultimatefish/opposed-sniper-admission-v17/ultimate_ghost_sniper_information_tablebase-v17
readonly rebind=${root}/stage/rebind_ultimate_ghost_extra_transition_marker.py
readonly old_prefix=work/transitions/kghostksniper-current-v15
readonly prefix=work/transitions/kghostksniper-probe-v18
readonly scratch=work/solve-v12/kghostksniper
readonly input=tablebases/kghostksniper-probe-v18.uftb
readonly output=work/results/kghostksniper-probe-v18.ufiw
readonly arbitrary=work/results/kghostksniper-probe-v18.ufgd
readonly log=${root}/work/logs/probe-certify-v18.log
readonly old_source_sha=4c1ddc17c5f60059018c3119e34ce63c4535236b10b629d3ca94bc9d4317631e
readonly source_sha=fb758b8997b8a081d350c8ddff72d4a6632c9d3a7378ce29aaee9d8a0cfa27cb
readonly rebind_sha=6a1ebee97cd4eb454639ddb4d9f6feb5c39e936fc37cd8e348369e97a2d1a1dd
readonly rebind_version=bGDIZJpQUZ5zR2SVnm4iP3A3M7luYYD3
readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652

test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  b50781da0901975a5a6b6b3b92a543847b8d246f3c65248464df4c8206143bce
test "$(sha256sum "${root}/${old_prefix}.verified" | cut -d ' ' -f1)" = \
  a342986eb103720eb26ffe7536cece822ddb4c0015b289cb26cf43ef151dad44
test "$(sha256sum "${probe_root}/tablebases/kghostksniper-probe-v18.uftb" | cut -d ' ' -f1)" = "${source_sha}"
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

install -d -m 0755 "${root}/stage" "${root}/tablebases" \
  "${root}/work/results" "${root}/work/logs"
if [[ ! -e "${root}/${input}" ]]; then
  cp --reflink=auto --preserve=mode,timestamps \
    "${probe_root}/tablebases/kghostksniper-probe-v18.uftb" "${root}/${input}"
fi
test "$(sha256sum "${root}/${input}" | cut -d ' ' -f1)" = "${source_sha}"

if [[ ! -e "${rebind}" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key "sources/tools/ghost-extra-transition-marker-rebind-v1/sha256/${rebind_sha}/rebind_ultimate_ghost_extra_transition_marker.py" \
    --version-id "${rebind_version}" "${rebind}" >/dev/null
fi
test "$(sha256sum "${rebind}" | cut -d ' ' -f1)" = "${rebind_sha}"

cd "${root}"
if [[ ! -e "${prefix}.verified" ]]; then
  python3 "${rebind}" "${old_prefix}" "${prefix}" \
    --old-source-sha256 "${old_source_sha}" \
    --new-source-sha256 "${source_sha}" \
    --model-sha256 69cd2e01bf3286e4de59f8fe2f3b3c8b02aa48268d09e034ee02d9a4b09e0878 \
    --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --manifest "${prefix}.rebind.json"
fi
grep -Fq '"payload_changed": false' "${prefix}.rebind.json"
grep -Fq '"residual": 0' "${prefix}.rebind.json"

exec >>"${log}" 2>&1
echo 'sniper_ghost_opposed_probe_v18 completed_iteration 69 current_slot next bdd_slot a resume_converged 1 workers 32 vertical_color_probe 1'
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
echo 'sniper_ghost_opposed_probe_v18 complete 1'
