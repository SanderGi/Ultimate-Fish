#!/usr/bin/env bash
# Fresh certifying opposed Checker/Ghost solve against the authenticated
# extra-primary concrete oracle.  Semantically wrong Ghost-primary roots are
# deliberately not reused.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly root=/mnt/ultimatefish/info-substate-corrected-v1/kghostkchecker
readonly stage=/mnt/ultimatefish/ghost-parallel-v22-bcfda23c
readonly binary=${stage}/ultimate_ghost_ordinary_information_tablebase-checker-v22
readonly binary_sha=b5da74fa4f53cc4d2a6b692061063bf500cf6e7aaef8041e85e91855e3f4f478
readonly source_sha=5a9cbeced03db4db2b4f6e66d87b0198da32dfd0c635cade3c84a06d320aed4d
readonly source_version=Dm4BZe6YQue3Nt46dUWf.LSgvGTuo7zI
readonly source_key=results/concrete/checker-current-v1/sha256/${source_sha}/kghostkchecker-current-v1.uftb
readonly input=${root}/tablebases/kcheckerkghost-current-v1.uftb
readonly rebind_sha=598e8a1f8bf21dbdc38e7be8825ad74ffaaa8573c52ab9ad91cb9f228e942101
readonly rebind=/mnt/ultimatefish/ghost-parallel-v21-bcfda23c/rebind_ultimate_ghost_ordinary_transition_marker.py
readonly old_source_sha=bcbc6c6bc88a2c18939222513732016d72e0288429a74ee522f2e5c2f6d3b2b4
readonly model_sha=bb4e8b3b44571b7ed5caad19dc17d26db9afe976393175f399f5e546ea490316
readonly observation_sha=890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
readonly old_transition=${root}/work/transitions/kghostkchecker-compositional-v4
readonly transition=${root}/work/transitions/kghostkchecker-extra-primary-v40
readonly normalize_scratch=${root}/work/self-test-extra-primary-v22/kghostkchecker
readonly normalized=${normalize_scratch}.normalized.uftb
readonly scratch=${root}/work/solve-extra-primary-v40/kghostkchecker
readonly output=${root}/work/results/kghostkchecker-extra-primary-v40.ufiw
readonly arbitrary=${root}/work/results/kghostkchecker-extra-primary-v40.ufgd
readonly log=${root}/work/logs/checker-extra-primary-v40.log

test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = "${binary_sha}"
install -d -m 0755 "$(dirname "${input}")" "$(dirname "${transition}")" \
  "$(dirname "${normalize_scratch}")" "$(dirname "${scratch}")" \
  "$(dirname "${output}")" "$(dirname "${log}")"
if [[ ! -f "${input}" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key "${source_key}" --version-id "${source_version}" "${input}" >/dev/null
fi
test "$(stat -c %s "${input}")" = 379579248
test "$(sha256sum "${input}" | cut -d ' ' -f1)" = "${source_sha}"
# Physical source contract: White Checker primary, opposing Ghost secondary.
test "$(od -An -t u4 -j 12 -N 4 "${input}" | tr -d ' ')" = 21
test "$(od -An -t u4 -j 40 -N 4 "${input}" | tr -d ' ')" = 11
test "$(od -An -t u4 -j 44 -N 4 "${input}" | tr -d ' ')" = 1
test "$(sha256sum "${rebind}" | cut -d ' ' -f1)" = "${rebind_sha}"

if [[ ! -f "${transition}.isolated-clone" ]]; then
  test ! -e "${transition}.verified"
  for extension in header meta strata index blocks; do
    cp --reflink=always --preserve=all \
      "${old_transition}.${extension}" "${transition}.${extension}"
  done
  python3 "${rebind}" "${old_transition}" "${transition}" \
    --source-sha256 "${old_source_sha}" \
    --new-source-sha256 "${source_sha}" \
    --old-model-sha256 "${model_sha}" \
    --new-model-sha256 "${model_sha}" \
    --observation-sha256 "${observation_sha}" \
    --manifest "${transition}.rebind.json"
  grep -Fq '"payload_changed": false' "${transition}.rebind.json"
  grep -Fq '"residual": 0' "${transition}.rebind.json"
  printf '%s\n' "source=${old_transition}" "source_sha256=${source_sha}" \
    'physical_order=checker-primary,ghost-secondary' \
    >"${transition}.isolated-clone"
fi

if [[ ! -f "${normalized}" ]]; then
  "${binary}" --self-test --orientation opposing --input "${input}" \
    --source-sha256 "${source_sha}" --scratch "${normalize_scratch}"
fi
test "$(stat -c %s "${normalized}")" = 75915888
readonly normalized_sha=$(sha256sum "${normalized}" | cut -d ' ' -f1)

test ! -e "${output}"
test ! -e "${arbitrary}"
test ! -e "${scratch}.checkpoint-cloned"
exec >>"${log}" 2>&1
echo "checker_opposed_extra_primary_v40 source_sha256=${source_sha} normalized_sha256=${normalized_sha} fresh_fixed_point=1 workers=${ULTIMATE_GHOST_SOLVE_WORKERS:-unset}"

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
  --max-nodes 1500000000 --unique-slots 2147483648 --compact-every 1

sha256sum "${output}" "${arbitrary}"
echo 'checker_opposed_extra_primary_v40 complete 1'
