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
readonly binary=${build_root}/ultimate_ghost_ordinary_information_tablebase-angel-same-v20
readonly old_root=/mnt/ultimatefish/ghost-angel-v9-159eea06/work-same
readonly work=${build_root}/kghostangelk-parallel-v21
readonly old_scratch=${old_root}/work/solve/kghostangelk
readonly scratch=${work}/work/solve/kghostangelk
readonly old_transition=${old_root}/work/transitions/kghostangelk
readonly transition=${work}/work/transitions/kghostangelk
readonly input=/mnt/ultimatefish/ghost-angel-v6-7425643c/inputs/kghostangelk.uftb
readonly lower_ghost=/mnt/ultimatefish/info-substate-corrected-v1/kberserkerghostk-v2-resume-v5/tablebases/kghostk.ufgm

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
    -DULTIMATE_GHOST_ORDINARY_PIECE=Angel \
    -DULTIMATE_GHOST_EXTRA_SUBSTATES=3 \
    -DULTIMATE_GHOST_EXTRA_IS_ANGEL \
    -DULTIMATE_GHOST_ORDINARY_LOWER_DRAW_ONLY \
    src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
    src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
    src/ultimate/tablebases/ghost_public_extra_model.cpp \
    src/ultimate/tablebases/external_robdd.cpp \
    src/ultimate/tablebases/ghost_information_probe.cpp \
    src/ultimate/tablebases/information.cpp \
    src/ultimate/position.cpp src/ultimate/nnue.cpp -pthread -o "${binary}"
fi
test "$(sha256sum "${input}" | cut -d ' ' -f 1)" = \
  d8422f3f62dc92e2c636cdbb0946ea7d8f9fe7c05102a039b9071190f3c55630
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

if [[ ! -f "${scratch}.checkpoint-cloned" ]]; then
  cp --reflink=always --preserve=all "${old_scratch}.bdd-b.nodes" "${scratch}.bdd-b.nodes"
  cp --reflink=always --preserve=all "${old_scratch}.bdd-b.unique" "${scratch}.bdd-b.unique"
  cp --reflink=always --preserve=all "${old_scratch}.domains" "${scratch}.domains"
  for kind in owner observer visible-owner visible-observer; do
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-next" "${scratch}.${kind}-next"
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-next" "${scratch}.${kind}-current"
  done
  printf '%s\n' 'iteration=11' 'bdd_slot=b' 'current_slot=next' \
    "source_bundle_sha256=${source_sha}" >"${scratch}.checkpoint-cloned"
fi
if [[ ! -f "${transition}.isolated-clone" ]]; then
  for extension in header meta strata index blocks; do
    cp --reflink=always --preserve=all "${old_transition}.${extension}" "${transition}.${extension}"
  done
  python3 "${rebind}" "${old_transition}" "${transition}" \
    --source-sha256 d8422f3f62dc92e2c636cdbb0946ea7d8f9fe7c05102a039b9071190f3c55630 \
    --old-model-sha256 f6ab383679094d485cbcc780f3588f603d393927825fdb4b4b78415036f7de49 \
    --new-model-sha256 ba6ae35a77d2a3689e533de8bcf3c804ca3da723eea4c77ba221161a82d70fb7 \
    --observation-sha256 6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b \
    --manifest "${transition}.rebind.json"
  printf '%s\n' "source=${old_transition}" "source_bundle_sha256=${source_sha}" \
    >"${transition}.isolated-clone"
fi

truncate -s 6750000000 "${scratch}.bdd-b.nodes"

exec "${binary}" --solve --orientation same \
  --transition-prefix "${transition}" --input "${input}" \
  --lower-ghost-sidecar "${lower_ghost}" --scratch "${scratch}" \
  --output "${work}/work/results/kghostangelk.ufiw" \
  --output-arbitrary "${work}/work/results/kghostangelk.ufgd" \
  --lower-dragon-table implicit-draw \
  --lower-dragon-sha256 25a2afa32399de9c487352f23a7f7e84673ca301d75fa4f927d037b3bd8852ee \
  --lower-dragon-source-sha256 25a2afa32399de9c487352f23a7f7e84673ca301d75fa4f927d037b3bd8852ee \
  --lower-dragon-model-sha256 323829264a9c977ccb01a674c2a068bdc29c87d7232fbeb5feb839605264ecfe \
  --source-sha256 d8422f3f62dc92e2c636cdbb0946ea7d8f9fe7c05102a039b9071190f3c55630 \
  --model-sha256 ba6ae35a77d2a3689e533de8bcf3c804ca3da723eea4c77ba221161a82d70fb7 \
  --observation-sha256 6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 750000000 --unique-slots 1073741824 --compact-every 1 \
  --resume-fixed-point --resume-iteration 11 --resume-current-slot next \
  --resume-bdd-slot b --workers 8
