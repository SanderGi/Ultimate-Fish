#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=d8f3180c406ef51631a7f72b042fadbfe1c307fbf4c3b9afeab4a64f226d996f
readonly source_version=XdBcy75hA5OW3iqg850H0niLoY4mQSsi
readonly source_key="sources/bundles/ghost-parallel-v19/sha256/${source_sha}/ultimatefish-ghost-parallel-v19-source.tar"
readonly binary_sha=946f6db6b8fe00fa236241a22a7b0203258cd2ab32a8392a0ff2da60405d7768
readonly binary_version=EGSlFkw0I7wNxQM_Dt4ZKzuPukYuLlCX
readonly binary_key="sources/binaries/ghost-parallel-v19/ninja/sha256/${binary_sha}/ultimate_ghost_ordinary_information_tablebase-ninja-v19"
readonly stage=/mnt/ultimatefish/ghost-parallel-v19-${source_sha:0:8}
readonly source_archive=${stage}/source.tar
readonly binary=${stage}/ultimate_ghost_ordinary_information_tablebase-ninja-v19
readonly input_root=/mnt/ultimatefish/kninjakghost-migrated-v1/restore/kninjakghost-fresh-v1
readonly work=/mnt/ultimatefish/ghost-parallel-v7/kninjakghost-parallel-v14
readonly transition=${work}/work/transitions/kninjakghost
readonly scratch=${work}/work/solve/kninjakghost

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
test ! -e "${work}/work/results/kninjakghost.ufiw"
for path in "${transition}.verified" "${scratch}.bdd-b.nodes" \
  "${scratch}.bdd-b.unique" "${scratch}.domains" \
  "${scratch}.owner-next" "${scratch}.observer-next" \
  "${scratch}.visible-owner-next" "${scratch}.visible-observer-next"; do
  test -s "${path}"
done
test "$(sha256sum "${input_root}/tablebases/kninjakghost.uftb" | cut -d ' ' -f 1)" = \
  4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027
test "$(sha256sum "${input_root}/tablebases/kninjak.uftb" | cut -d ' ' -f 1)" = \
  4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776
test "$(sha256sum "${input_root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

# The v17 journal authenticated iteration 15 and its compaction.  Physical
# next and ROBDD b are therefore the exact checkpoint.  Partial iteration-16
# nodes are valid cache entries and are retained; increasing only the sparse
# arena capacity gives the corrected recurrence adequate append headroom.
truncate -s 18000000000 "${scratch}.bdd-b.nodes"
printf '%s\n' 'iteration=15' 'current_slot=next' 'bdd_slot=b' \
  "source_bundle_sha256=${source_sha}" "source_binary_sha256=${binary_sha}" \
  >"${scratch}.action-conditioned-v18"

cd "${input_root}"
exec "${binary}" --solve --orientation opposing \
  --transition-prefix "${transition}" \
  --input tablebases/kninjakghost.uftb \
  --normalized-input work/self-test-source-order-v6/kninjakghost.normalized.uftb \
  --normalized-sha256 bd3d9de0a61fcdfb7910379a7c73283c2d299cd2adf17a38f474f4f7d650b58a \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${work}/work/results/kninjakghost.ufiw" \
  --output-arbitrary "${work}/work/results/kninjakghost.ufgd" \
  --lower-dragon-table tablebases/kninjak.uftb \
  --lower-dragon-sha256 4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776 \
  --lower-dragon-source-sha256 4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776 \
  --lower-dragon-model-sha256 666ca18773faec47588c7d628b6bd5e4f61035fbf4d36a15a55a5558aacc0096 \
  --source-sha256 4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027 \
  --model-sha256 9c61c12e871a68602b015fe2894d112ceb01c78d9c22a48cbd9a354dc020f4aa \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 2000000000 --unique-slots 2147483648 --compact-every 1 \
  --resume-fixed-point --resume-iteration 15 --resume-current-slot next \
  --resume-bdd-slot b
