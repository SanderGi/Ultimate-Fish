#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=2d1ff830eb622de7880af5961cf1c3e4b6f14817a657da4103b9227a88400be7
readonly source_version=1.mg.armfxwA91tewK8YZvLeUoXonqvj
readonly source_key="sources/bundles/ghost-parallel-v8/sha256/${source_sha}/ultimatefish-ghost-parallel-v8-source.tar"
readonly binary_sha=afa30d3378385bc43e8fc281f1946be58dcf947ec47b304daeb0f09617d99282
readonly binary_version=ToHD34xtsotuDnTJtv7Kgvq_cHdDrw17
readonly binary_key="sources/binaries/ghost-parallel-v8/queen/sha256/${binary_sha}/ultimate_ghost_ordinary_information_tablebase-queen-v8"
readonly stage=/mnt/ultimatefish/ghost-parallel-v8-${source_sha:0:8}
readonly source_archive=${stage}/source.tar
readonly binary=${stage}/ultimate_ghost_ordinary_information_tablebase-queen-v8
readonly input_root=/mnt/ultimatefish-penguin/info-remap-fix-v2/kqueenkghost-fresh-v1
readonly prior=/mnt/ultimatefish-penguin/ghost-parallel-v7/kqueenkghost-parallel-v14
readonly work=/mnt/ultimatefish-penguin/ghost-parallel-v8/kqueenkghost-parallel-v16
readonly prior_scratch=${prior}/work/solve/kqueenkghost
readonly scratch=${work}/work/solve/kqueenkghost
readonly prior_transition=${prior}/work/transitions/kqueenkghost
readonly transition=${work}/work/transitions/kqueenkghost

install -d -m 0755 "${stage}" "${work}/work/solve" \
  "${work}/work/results" "${work}/work/transitions"
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

test "$(sha256sum "${input_root}/tablebases/kqueenkghost.uftb" | cut -d ' ' -f 1)" = \
  30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d
test "$(sha256sum "${input_root}/tablebases/kqueenk.uftb" | cut -d ' ' -f 1)" = \
  1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
test "$(sha256sum "${input_root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

# Iteration 4 and its compaction are authenticated in the v15 journal. Clone
# only that complete root set and its append-only ROBDD; the partial iteration-5
# next roots are deliberately ignored. The old v15 checkpoint remains intact.
if [[ ! -f "${scratch}.checkpoint-cloned" ]]; then
  test ! -e "${work}/work/results/kqueenkghost.ufiw"
  for extension in nodes unique; do
    cp --reflink=always --preserve=all \
      "${prior_scratch}.bdd-a.${extension}" "${scratch}.bdd-a.${extension}"
  done
  cp --reflink=always --preserve=all \
    "${prior_scratch}.domains" "${scratch}.domains"
  for kind in owner observer visible-owner visible-observer; do
    cp --reflink=always --preserve=all \
      "${prior_scratch}.${kind}-current" "${scratch}.${kind}-current"
    cp --reflink=always --preserve=all \
      "${prior_scratch}.${kind}-current" "${scratch}.${kind}-next"
  done
  printf '%s\n' 'iteration=4' 'bdd_slot=a' 'current_slot=current' \
    "source_bundle_sha256=${source_sha}" \
    "source_binary_sha256=${binary_sha}" >"${scratch}.checkpoint-cloned"
fi
if [[ ! -f "${transition}.isolated-clone" ]]; then
  for extension in header meta strata index blocks verified; do
    cp --reflink=always --preserve=all \
      "${prior_transition}.${extension}" "${transition}.${extension}"
  done
  printf '%s\n' "source=${prior_transition}" \
    "source_bundle_sha256=${source_sha}" >"${transition}.isolated-clone"
fi
# A failed preflight may leave the clone marker from before the authenticated
# dependency marker was installed. Recopying this immutable sidecar is safe and
# makes retries fail closed on the actual transition payload binding.
cp --reflink=always --preserve=all \
  "${prior_transition}.verified" "${transition}.verified"
truncate -s 9000000000 "${scratch}.bdd-a.nodes"

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
  --max-nodes 1000000000 --unique-slots 1073741824 --compact-every 1 \
  --resume-fixed-point --resume-iteration 4 --resume-current-slot current \
  --resume-bdd-slot a --workers 28
