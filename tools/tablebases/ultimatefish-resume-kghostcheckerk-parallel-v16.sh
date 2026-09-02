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
readonly binary=${build_root}/ultimate_ghost_ordinary_information_tablebase-checker-same-v16
readonly old_root=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostcheckerk-coherent-fresh-v14
readonly work=/mnt/ultimatefish/ghost-parallel-v7/kghostcheckerk-parallel-v16
readonly old_scratch=${old_root}/work/solve/kghostcheckerk
readonly scratch=${work}/work/solve/kghostcheckerk
readonly old_transition=${old_root}/work/transitions/kghostcheckerk-gated-v10
readonly transition=${work}/work/transitions/kghostcheckerk

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
    -DULTIMATE_GHOST_ORDINARY_PIECE=Checker \
    -DULTIMATE_GHOST_EXTRA_SUBSTATES=4 \
    -DULTIMATE_GHOST_EXTRA_IS_CHECKER=1 \
    -DULTIMATE_GHOST_EXTRA_HORIZONTAL_ONLY=1 \
    -DULTIMATE_GHOST_ORDINARY_LOWER_DRAW_ONLY=1 \
    src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
    src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
    src/ultimate/tablebases/ghost_public_extra_model.cpp \
    src/ultimate/tablebases/external_robdd.cpp \
    src/ultimate/tablebases/ghost_information_probe.cpp \
    src/ultimate/tablebases/information.cpp \
    src/ultimate/position.cpp src/ultimate/nnue.cpp -pthread -o "${binary}"
fi

test "$(sha256sum "${old_root}/tablebases/kghostcheckerk.uftb" | cut -d ' ' -f 1)" = \
  fdd9329ed29fb7823b27e4bd46263b62f6ac66e638b510e232b3ee386ca6be1e
test "$(sha256sum "${old_root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

# Iteration 31 is the last complete checkpoint. Iteration 32 had only begun in
# the physical current roots when the serial unit stopped; keep the complete
# physical next roots and compact BDD b.
if [[ ! -f "${scratch}.checkpoint-cloned" ]]; then
  cp --reflink=always --preserve=all "${old_scratch}.bdd-b.nodes" "${scratch}.bdd-b.nodes"
  cp --reflink=always --preserve=all "${old_scratch}.bdd-b.unique" "${scratch}.bdd-b.unique"
  cp --reflink=always --preserve=all "${old_scratch}.domains" "${scratch}.domains"
  for kind in owner observer visible-owner visible-observer; do
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-next" "${scratch}.${kind}-next"
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-next" "${scratch}.${kind}-current"
  done
  printf '%s\n' 'iteration=31' 'bdd_slot=b' 'current_slot=next' \
    "source_bundle_sha256=${source_sha}" >"${scratch}.checkpoint-cloned"
fi
if [[ ! -f "${transition}.isolated-clone" ]]; then
  for extension in header meta strata index blocks; do
    cp --reflink=always --preserve=all "${old_transition}.${extension}" \
      "${transition}.${extension}"
  done
  python3 "${rebind}" "${old_transition}" "${transition}" \
    --source-sha256 fdd9329ed29fb7823b27e4bd46263b62f6ac66e638b510e232b3ee386ca6be1e \
    --old-model-sha256 273bfaf1940ddb3fd52893ae7d23b5b758cb7bab12877c0cebf41c95a83627fa \
    --new-model-sha256 f983e18aae182467f5a0279996c35087d4984b11c80fe6404f16d9214c26d01d \
    --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --manifest "${transition}.rebind.json"
  printf '%s\n' "source=${old_transition}" "source_bundle_sha256=${source_sha}" \
    >"${transition}.isolated-clone"
fi

truncate -s 9000000000 "${scratch}.bdd-b.nodes"

cd "${old_root}"
exec "${binary}" --solve --orientation same \
  --transition-prefix "${transition}" \
  --input tablebases/kghostcheckerk.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${work}/work/results/kghostcheckerk.ufiw" \
  --output-arbitrary "${work}/work/results/kghostcheckerk.ufgd" \
  --lower-dragon-table implicit-draw \
  --lower-dragon-sha256 0dcb4039ec2305298e9f35d36ce8d0e54fe76acc013c188d08e0e2a6da0ef22c \
  --lower-dragon-source-sha256 0dcb4039ec2305298e9f35d36ce8d0e54fe76acc013c188d08e0e2a6da0ef22c \
  --lower-dragon-model-sha256 42566cc6e4186de2f29a2ded6e9a0728e8dc6d7e748d936e574f15f5202c1763 \
  --source-sha256 fdd9329ed29fb7823b27e4bd46263b62f6ac66e638b510e232b3ee386ca6be1e \
  --model-sha256 f983e18aae182467f5a0279996c35087d4984b11c80fe6404f16d9214c26d01d \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1000000000 --unique-slots 1073741824 --compact-every 1 \
  --resume-fixed-point --resume-iteration 31 --resume-current-slot next \
  --resume-bdd-slot b --workers 8
