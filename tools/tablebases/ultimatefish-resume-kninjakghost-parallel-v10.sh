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
readonly binary=${build_root}/ultimate_ghost_ordinary_information_tablebase-ninja-v8
readonly old_root=/mnt/ultimatefish/kninjakghost-migrated-v1/restore/kninjakghost-fresh-v1
readonly work=/mnt/ultimatefish/ghost-parallel-v7/kninjakghost-parallel-v14
readonly old_scratch=${old_root}/work/solve-source-order-v7/kninjakghost
readonly scratch=${work}/work/solve/kninjakghost
readonly old_transition=${old_root}/work/transitions/kninjakghost-source-order-v6
readonly transition=${work}/work/transitions/kninjakghost

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
    -DULTIMATE_GHOST_ORDINARY_PIECE=Ninja \
    -DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY \
    src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
    src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
    src/ultimate/tablebases/ghost_public_extra_model.cpp \
    src/ultimate/tablebases/external_robdd.cpp \
    src/ultimate/tablebases/ghost_information_probe.cpp \
    src/ultimate/tablebases/information.cpp \
    src/ultimate/position.cpp src/ultimate/nnue.cpp -pthread -o "${binary}"
fi

test "$(sha256sum "${old_root}/tablebases/kninjakghost.uftb" | cut -d ' ' -f 1)" = \
  4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027
test "$(sha256sum "${old_root}/tablebases/kninjak.uftb" | cut -d ' ' -f 1)" = \
  4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776
test "$(sha256sum "${old_root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

# Iteration 14 is complete in physical current with compact ROBDD a. Partial
# iteration-15 tuples are immutable and safe to retain, but no partial roots are.
if [[ ! -f "${scratch}.checkpoint-cloned" ]]; then
  cp --reflink=always --preserve=all "${old_scratch}.bdd-a.nodes" "${scratch}.bdd-a.nodes"
  cp --reflink=always --preserve=all "${old_scratch}.bdd-a.unique" "${scratch}.bdd-a.unique"
  cp --reflink=always --preserve=all "${old_scratch}.domains" "${scratch}.domains"
  for kind in owner observer visible-owner visible-observer; do
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-current" "${scratch}.${kind}-current"
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-current" "${scratch}.${kind}-next"
  done
  printf '%s\n' 'iteration=14' 'bdd_slot=a' 'current_slot=current' \
    "source_bundle_sha256=${source_sha}" >"${scratch}.checkpoint-cloned"
fi
if [[ ! -f "${transition}.isolated-clone" ]]; then
  for extension in header meta strata index blocks; do
    cp --reflink=always --preserve=all "${old_transition}.${extension}" \
      "${transition}.${extension}"
  done
  python3 "${rebind}" "${old_transition}" "${transition}" \
    --source-sha256 4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027 \
    --old-model-sha256 c68506845eceed07dee0f179f6bb97d4a2f0c085f809041fc2d5613ea1760cad \
    --new-model-sha256 9c61c12e871a68602b015fe2894d112ceb01c78d9c22a48cbd9a354dc020f4aa \
    --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --manifest "${transition}.rebind.json"
  printf '%s\n' "source=${old_transition}" "source_bundle_sha256=${source_sha}" \
    >"${transition}.isolated-clone"
fi

# The retained arena was sized for the prior 1.25B-node limit. Extending this
# isolated sparse clone supplies the declared 1.5B capacity without changing
# any authenticated tuple or checkpoint root.
truncate -s 13500000000 "${scratch}.bdd-a.nodes"

cd "${old_root}"
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
  --max-nodes 1500000000 --unique-slots 1073741824 --compact-every 1 \
  --resume-fixed-point --resume-iteration 14 --resume-current-slot current \
  --resume-bdd-slot a --workers 16
