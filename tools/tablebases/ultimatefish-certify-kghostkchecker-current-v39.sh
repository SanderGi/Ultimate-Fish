#!/usr/bin/env bash
# Certify the retained opposed Checker/Ghost zero-change fixed point against
# the current-rules oracle.  The independently normalized oracle is byte-for-
# byte identical to the dependency used by iteration 26, so reflink-cloning
# those roots is exact; the independent Bellman and singleton passes still run.
set -euo pipefail

readonly root=/mnt/ultimatefish/info-substate-corrected-v1/kghostkchecker
readonly stage=/mnt/ultimatefish/ghost-parallel-v21-bcfda23c
readonly binary=${stage}/ultimate_ghost_ordinary_information_tablebase-checker-v21
readonly input=${root}/tablebases/kghostkchecker-current-v2.uftb
readonly source_sha=231d2f45d8d1db2a6e47aa413485444d93599fc68ae0d069afabd5e60369ee10
readonly model_sha=bb4e8b3b44571b7ed5caad19dc17d26db9afe976393175f399f5e546ea490316
readonly observation_sha=890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
readonly normalized=${root}/work/self-test-current-v36/kghostkchecker.normalized.uftb
readonly normalized_sha=0400c77f6d33ba827b6eb53f5f707144eda3b0daeed430cd94ae5d491bc2fef2
readonly transition=${root}/work/transitions/kghostkchecker-current-v36
readonly old_scratch=${root}/work/solve-compositional-v4/kghostkchecker
readonly scratch=${root}/work/solve-current-v39/kghostkchecker
readonly old_log=${root}/work/logs/compositional-merge-solve-v4.log
readonly log=${root}/work/logs/checker-current-v39.log
readonly output=${root}/work/results/kghostkchecker-current-v39.ufiw
readonly arbitrary=${root}/work/results/kghostkchecker-current-v39.ufgd

test "$(sha256sum "${stage}/source.tar" | cut -d ' ' -f1)" = \
  bcfda23c2a00580c1e4ecd08faf70447a1ae8ffe144f95bc36a074ecefbe562e
test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  3cdc7b745887a3716f84633ff686f1bfb26612c9eb00694d0dd5512142391816
test "$(stat -c %s "${input}")" = 379579248
test "$(sha256sum "${input}" | cut -d ' ' -f1)" = "${source_sha}"
test "$(stat -c %s "${normalized}")" = 75915888
test "$(sha256sum "${normalized}" | cut -d ' ' -f1)" = "${normalized_sha}"
test "$(stat -c %s "${transition}.verified")" = 760
grep -Fq '"payload_changed": false' "${transition}.rebind.json"
grep -Fq '"residual": 0' "${transition}.rebind.json"
grep -Fq "\"new_source_sha256\": \"${source_sha}\"" \
  "${transition}.rebind.json"
test "$(grep -Fc 'reciprocal_ghost_extra_iteration 26 bdd_nodes 164178043 changed_owner 0 changed_observer 0 changed_visible 0' "${old_log}")" -ge 1
test "$(grep -Fc 'ghost_extra_external_symbolic_certificate bellman_residual 0 monotonicity_residual 0' "${old_log}")" -ge 1
test "$(stat -c %s "${old_scratch}.bdd-a.nodes")" = 13500000000
test "$(stat -c %s "${old_scratch}.owner-current")" = 1261977600
test ! -e "${output}"
test ! -e "${arbitrary}"
install -d -m 0755 "$(dirname "${scratch}")" "$(dirname "${output}")" \
  "$(dirname "${log}")"

if [[ ! -f "${scratch}.checkpoint-cloned" ]]; then
  test ! -e "${scratch}.bdd-a.nodes"
  for suffix in bdd-a.nodes bdd-a.unique domains owner-current owner-next \
      observer-current observer-next visible-owner-current visible-owner-next \
      visible-observer-current visible-observer-next; do
    cp --reflink=always --preserve=all \
      "${old_scratch}.${suffix}" "${scratch}.${suffix}"
  done
  printf '%s\n' 'iteration=26' 'bdd_slot=a' 'current_slot=current' \
    'normalized_sha256=0400c77f6d33ba827b6eb53f5f707144eda3b0daeed430cd94ae5d491bc2fef2' \
    'source_checkpoint=solve-compositional-v4' >"${scratch}.checkpoint-cloned"
fi

exec >>"${log}" 2>&1
echo "checker_opposed_current_v39 source_sha256=${source_sha} normalized_sha256=${normalized_sha} retained_zero_change_iteration=26 workers=32"

"${binary}" --solve --orientation opposing \
  --transition-prefix "${transition}" \
  --lower-dragon-table implicit-draw \
  --lower-dragon-sha256 0dcb4039ec2305298e9f35d36ce8d0e54fe76acc013c188d08e0e2a6da0ef22c \
  --lower-dragon-source-sha256 0dcb4039ec2305298e9f35d36ce8d0e54fe76acc013c188d08e0e2a6da0ef22c \
  --lower-dragon-model-sha256 42566cc6e4186de2f29a2ded6e9a0728e8dc6d7e748d936e574f15f5202c1763 \
  --source-sha256 "${source_sha}" --model-sha256 "${model_sha}" \
  --observation-sha256 "${observation_sha}" \
  --input "${input}" --normalized-input "${normalized}" \
  --normalized-sha256 "${normalized_sha}" \
  --lower-ghost-sidecar "${root}/tablebases/kghostk.ufgm" \
  --scratch "${scratch}" --output "${output}" --output-arbitrary "${arbitrary}" \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 "${observation_sha}" \
  --max-nodes 1500000000 --unique-slots 2147483648 --compact-every 1 \
  --resume-fixed-point --resume-iteration 26 \
  --resume-current-slot current --resume-bdd-slot a

sha256sum "${output}" "${arbitrary}"
echo 'checker_opposed_current_v39 complete 1'
