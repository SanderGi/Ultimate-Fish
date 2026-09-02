#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=1107f9d8fe42a8e709694f30585f9564f52c0275021e1f634c01d9ce41404fd4
readonly source_version=XVx3oDV5a75swBXawsXAv.USmPXTftxj
readonly source_key="sources/bundles/ghost-parallel-v7/sha256/${source_sha}/ultimatefish-ghost-parallel-v7-source.tar"
readonly rebind_sha=598e8a1f8bf21dbdc38e7be8825ad74ffaaa8573c52ab9ad91cb9f228e942101
readonly rebind_version=haqGViqTNXjoED4UH0xtwLnr_G5rM8t0
readonly rebind_key="sources/tools/sha256/${rebind_sha}/rebind_ultimate_ghost_ordinary_transition_marker.py"
readonly build_root=/mnt/ultimatefish/ghost-parallel-v7
readonly source_archive=${build_root}/ultimatefish-ghost-parallel-v7-source.tar
readonly source_root=${build_root}/source
readonly rebind=${build_root}/rebind_ultimate_ghost_ordinary_transition_marker.py
readonly binary=${build_root}/ultimate_ghost_ordinary_information_tablebase-queen-v8
readonly old_root=/mnt/ultimatefish-penguin/info-remap-fix-v2/kqueenkghost-fresh-v1
readonly work=/mnt/ultimatefish-penguin/ghost-parallel-v7/kqueenkghost-parallel-v14
readonly old_scratch=${old_root}/work/solve-source-order-v7/kqueenkghost
readonly scratch=${work}/work/solve/kqueenkghost
readonly old_transition=${old_root}/work/transitions/kqueenkghost-source-order-v6
readonly transition=${work}/work/transitions/kqueenkghost

install -d -m 0755 "${build_root}" "${work}/work/solve" \
  "${work}/work/results" "${work}/work/transitions" "${work}/work/logs"
if [[ ! -f "${source_archive}" ]]; then
  aws s3api get-object --bucket "${bucket}" --key "${source_key}" \
    --version-id "${source_version}" "${source_archive}" >/dev/null
fi
test "$(sha256sum "${source_archive}" | cut -d ' ' -f 1)" = "${source_sha}"
if [[ ! -d "${source_root}" ]]; then
  install -d -m 0755 "${source_root}"
  tar -xf "${source_archive}" -C "${source_root}" --strip-components=1
fi
if [[ ! -f "${rebind}" ]]; then
  aws s3api get-object --bucket "${bucket}" --key "${rebind_key}" \
    --version-id "${rebind_version}" "${rebind}" >/dev/null
fi
test "$(sha256sum "${rebind}" | cut -d ' ' -f 1)" = "${rebind_sha}"
if [[ ! -x "${binary}" ]]; then
  cd "${source_root}"
  clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
    -Wno-error=range-loop-construct -include sstream \
    -Isrc/ultimate -Isrc/ultimate/tablebases \
    -DULTIMATE_GHOST_ORDINARY_PIECE=Queen \
    -DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY \
    src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
    src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
    src/ultimate/tablebases/ghost_public_extra_model.cpp \
    src/ultimate/tablebases/external_robdd.cpp \
    src/ultimate/tablebases/ghost_information_probe.cpp \
    src/ultimate/tablebases/information.cpp \
    src/ultimate/position.cpp src/ultimate/nnue.cpp -pthread -o "${binary}"
fi

test "$(sha256sum "${old_root}/tablebases/kqueenkghost.uftb" | cut -d ' ' -f 1)" = \
  30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d
test "$(sha256sum "${old_root}/tablebases/kqueenk.uftb" | cut -d ' ' -f 1)" = \
  1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
test "$(sha256sum "${old_root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

# Iteration 3 is the last complete checkpoint. Its roots are in the physical
# next slot and its compact ROBDD is b. Clone only after the serial unit stops.
if [[ ! -f "${scratch}.checkpoint-cloned" ]]; then
  cp --reflink=always --preserve=all "${old_scratch}.bdd-b.nodes" "${scratch}.bdd-b.nodes"
  cp --reflink=always --preserve=all "${old_scratch}.bdd-b.unique" "${scratch}.bdd-b.unique"
  cp --reflink=always --preserve=all "${old_scratch}.domains" "${scratch}.domains"
  for kind in owner observer visible-owner visible-observer; do
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-next" "${scratch}.${kind}-next"
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-next" "${scratch}.${kind}-current"
  done
  printf '%s\n' 'iteration=3' 'bdd_slot=b' 'current_slot=next' \
    "source_bundle_sha256=${source_sha}" >"${scratch}.checkpoint-cloned"
fi
if [[ ! -f "${transition}.isolated-clone" ]]; then
  for extension in header meta strata index blocks; do
    cp --reflink=always --preserve=all "${old_transition}.${extension}" \
      "${transition}.${extension}"
  done
  python3 "${rebind}" "${old_transition}" "${transition}" \
    --source-sha256 30fa9eb2a4d9519b44c2f5d2ce6406acd323ccc41898cf7b233120718f97126d \
    --old-model-sha256 d05ac1dbf726e8830a78d191bd1764572532bfcdee05b549de919f2bbda003f5 \
    --new-model-sha256 b1b7a85e24483a27f5ee58acd9b9eae02f30b202dbac9d7d6aa46d0e9554eeff \
    --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --manifest "${transition}.rebind.json"
  printf '%s\n' "source=${old_transition}" "source_bundle_sha256=${source_sha}" \
    >"${transition}.isolated-clone"
fi

# Supply the declared 1B-node capacity on the isolated sparse clone. Existing
# tuples and checkpoint roots remain byte-for-byte unchanged.
truncate -s 9000000000 "${scratch}.bdd-b.nodes"

cd "${old_root}"
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
  --resume-fixed-point --resume-iteration 3 --resume-current-slot next \
  --resume-bdd-slot b --workers 8
