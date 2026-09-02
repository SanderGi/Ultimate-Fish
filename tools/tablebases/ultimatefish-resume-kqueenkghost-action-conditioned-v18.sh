#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=d8f3180c406ef51631a7f72b042fadbfe1c307fbf4c3b9afeab4a64f226d996f
readonly source_version=XdBcy75hA5OW3iqg850H0niLoY4mQSsi
readonly source_key="sources/bundles/ghost-parallel-v19/sha256/${source_sha}/ultimatefish-ghost-parallel-v19-source.tar"
readonly binary_sha=9223abc2fa867856b98a288242919f360602bd087730b38ebdb8577c11cf3779
readonly binary_version=flG8ggfHrMqvbgmVRs2DTbIpPf_RwX4Z
readonly binary_key="sources/binaries/ghost-parallel-v19/queen/sha256/${binary_sha}/ultimate_ghost_ordinary_information_tablebase-queen-v19"
readonly stage=/mnt/ultimatefish/ghost-parallel-v19-${source_sha:0:8}
readonly source_archive=${stage}/source.tar
readonly binary=${stage}/ultimate_ghost_ordinary_information_tablebase-queen-v19
readonly input_root=/mnt/ultimatefish-penguin/info-remap-fix-v2/kqueenkghost-fresh-v1
readonly work=/mnt/ultimatefish-penguin/ghost-parallel-v8/kqueenkghost-parallel-v16
readonly transition=${work}/work/transitions/kqueenkghost
readonly scratch=${work}/work/solve/kqueenkghost

install -d -m 0755 "${stage}" "${work}/work/results"
if [[ ! -f "${source_archive}" ]]; then
  aws s3api get-object --bucket "${bucket}" --key "${source_key}" \
    --version-id "${source_version}" "${source_archive}" >/dev/null
fi
test "$(sha256sum "${source_archive}" | cut -d ' ' -f 1)" = "${source_sha}"
if [[ ! -x "${binary}" ]]; then
  aws s3api get-object --bucket "${bucket}" --key "${binary_key}" \
    --version-id "${binary_version}" "${binary}" >/dev/null
  chmod 0755 "${binary}"
fi
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = "${binary_sha}"
test ! -e "${work}/work/results/kqueenkghost.ufiw"
test -s "${scratch}.checkpoint-cloned"
grep -qx iteration=4 "${scratch}.checkpoint-cloned"
for path in "${transition}.verified" "${scratch}.bdd-a.nodes" \
  "${scratch}.bdd-a.unique.slots-2147483648" "${scratch}.domains" \
  "${scratch}.owner-current" "${scratch}.observer-current" \
  "${scratch}.visible-owner-current" "${scratch}.visible-observer-current"; do
  test -s "${path}"
done
test "$(sha256sum "${input_root}/tablebases/kqueenkghost.uftb" | cut -d ' ' -f 1)" = \
  30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d
test "$(sha256sum "${input_root}/tablebases/kqueenk.uftb" | cut -d ' ' -f 1)" = \
  1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
test "$(sha256sum "${input_root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

# Iteration 5 never completed.  Physical current and ROBDD a remain the exact
# iteration-4 checkpoint.  Nodes appended by the interrupted iteration are
# valid, reusable cache entries; the larger arena prevents them from consuming
# the corrected recurrence's headroom.
truncate -s 13500000000 "${scratch}.bdd-a.nodes"
printf '%s\n' 'iteration=4' 'current_slot=current' 'bdd_slot=a' \
  "source_bundle_sha256=${source_sha}" "source_binary_sha256=${binary_sha}" \
  >"${scratch}.action-conditioned-v18"

cd "${input_root}"
exec "${binary}" --solve --orientation opposing \
  --transition-prefix "${transition}" \
  --input tablebases/kqueenkghost.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${work}/work/results/kqueenkghost.ufiw" \
  --output-arbitrary "${work}/work/results/kqueenkghost.ufgd" \
  --lower-dragon-table tablebases/kqueenk.uftb \
  --lower-dragon-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --lower-dragon-source-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3 \
  --lower-dragon-model-sha256 87147fccee31cff98fd6875f02ca51d82d20c978bcaa6d7c4aaa81d114a23c6a \
  --source-sha256 30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d \
  --model-sha256 b1b7a85e24483a27f5ee58acd9b9eae02f30b202dbac9d7d6aa46d0e9554eeff \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1500000000 --unique-slots 2147483648 --compact-every 1 \
  --resume-fixed-point --resume-iteration 4 --resume-current-slot current \
  --resume-bdd-slot a
