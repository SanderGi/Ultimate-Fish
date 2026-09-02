#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=d8f3180c406ef51631a7f72b042fadbfe1c307fbf4c3b9afeab4a64f226d996f
readonly source_version=XdBcy75hA5OW3iqg850H0niLoY4mQSsi
readonly source_key="sources/bundles/ghost-parallel-v19/sha256/${source_sha}/ultimatefish-ghost-parallel-v19-source.tar"
readonly binary_sha=c7ede89e6c27406f17402d121da09a5e58459efd81e99e823ac7c4340845e485
readonly binary_version=hdKQVYeTxlCkkLqIjCX.RUFu9lwSAsuG
readonly binary_key="sources/binaries/ghost-parallel-v23/prince/sha256/${binary_sha}/ultimate_ghost_ordinary_information_tablebase-prince-v23"
readonly stage=/mnt/ultimatefish/ghost-parallel-v23-${source_sha:0:8}
readonly source_archive=${stage}/source.tar
readonly binary=${stage}/ultimate_ghost_ordinary_information_tablebase-prince-v23
readonly work=/mnt/ultimatefish/ghost-parallel-v6/kghostkprince-canary-striped-bdd-b-8w
readonly scratch=${work}/solve/kghostkprince
readonly transition=${work}/transitions/kghostkprince
readonly inputs=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostkprince-solve-2b-v7

install -d -m 0755 "${stage}"
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
test ! -e "${work}/results/kghostkprince.ufiw"
test ! -e "${work}/results/kghostkprince.ufgd"
for path in "${transition}.verified" "${scratch}.bdd-b.nodes" \
  "${scratch}.bdd-b.unique" "${scratch}.domains" \
  "${scratch}.owner-next" "${scratch}.observer-next" \
  "${scratch}.visible-owner-next" "${scratch}.visible-observer-next"; do
  test -s "${path}"
done
test "$(sha256sum "${inputs}/tablebases/kghostkprince.uftb" | cut -d ' ' -f 1)" = \
  2858daf3770e0a47230f2d0bf410cd8ce4a9820b0c7cd0895ceab242ae42f65f
test "$(sha256sum "${inputs}/tablebases/kprincek.uftb" | cut -d ' ' -f 1)" = \
  7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0
test "$(sha256sum "${inputs}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

# Iteration 9 and its compaction are journal-authenticated.  Physical next and
# ROBDD b are the exact roots.  Partial iteration-10 nodes are valid reusable
# cache entries and cannot alter the retained roots.
printf '%s\n' 'iteration=9' 'current_slot=next' 'bdd_slot=b' \
  "source_bundle_sha256=${source_sha}" "source_binary_sha256=${binary_sha}" \
  >"${scratch}.action-conditioned-v11"

cd "${inputs}"
exec "${binary}" --solve --orientation opposing \
  --transition-prefix "${transition}" \
  --input tablebases/kghostkprince.uftb \
  --normalized-input "${scratch}.normalized.uftb" \
  --normalized-sha256 c81866fae696522d1a77af199f304f5199061ec582d225c2f5d4c4be2f5cfb6c \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${work}/results/kghostkprince.ufiw" \
  --output-arbitrary "${work}/results/kghostkprince.ufgd" \
  --lower-dragon-table tablebases/kprincek.uftb \
  --lower-dragon-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-dragon-source-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-dragon-model-sha256 5511ac5ac166fd2be042e66a4c849e034fef39da317988f40bf4e6b47e071927 \
  --source-sha256 2858daf3770e0a47230f2d0bf410cd8ce4a9820b0c7cd0895ceab242ae42f65f \
  --model-sha256 a9c480c453ddeb72085a43483dc60484ec24ca017fd365ed60a7c8f3973d8834 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 2000000000 --unique-slots 2147483648 --compact-every 1 \
  --resume-fixed-point --resume-iteration 9 \
  --resume-current-slot next --resume-bdd-slot b
