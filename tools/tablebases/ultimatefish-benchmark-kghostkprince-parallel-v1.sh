#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=d22956a40eb30167adbad15ac5d02b30d7901b616014b5683d0c5c3ba5d555c5
readonly source_version=yIXrHZ2Qfq20xugQzDYLfJBFXu4p_cwz
readonly source_key="sources/bundles/ghost-parallel-v6/sha256/${source_sha}/ultimatefish-ghost-parallel-v6-source.tar"
readonly root=/mnt/ultimatefish/ghost-parallel-v6
readonly source_archive=${root}/ultimatefish-ghost-parallel-v6-source.tar
readonly source_root=${root}/source
readonly binary=${root}/ultimate_ghost_ordinary_information_tablebase-prince-v6
readonly old_work=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostkprince-solve-2b-v7
readonly work=${root}/kghostkprince-canary-striped-bdd-b-8w
readonly old_scratch=${old_work}/work/solve/kghostkprince
readonly scratch=${work}/solve/kghostkprince
readonly old_transition=${old_work}/work/transitions/kghostkprince
readonly transition=${work}/transitions/kghostkprince

install -d -m 0755 "${root}" "${work}/logs" "${work}/solve" \
  "${work}/results" "${work}/transitions"

if [[ ! -f "${source_archive}" ]]; then
  aws s3api get-object \
    --bucket "${bucket}" \
    --key "${source_key}" \
    --version-id "${source_version}" \
    "${source_archive}"
fi
test "$(sha256sum "${source_archive}" | cut -d ' ' -f 1)" = "${source_sha}"

if [[ ! -d "${source_root}" ]]; then
  install -d -m 0755 "${source_root}"
  tar -xf "${source_archive}" -C "${source_root}" --strip-components=1
fi

if [[ ! -x "${binary}" ]]; then
  cd "${source_root}"
  clang++ \
    -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
    -Wno-error=range-loop-construct -include sstream \
    -Isrc/ultimate -Isrc/ultimate/tablebases \
    -DULTIMATE_GHOST_ORDINARY_PIECE=Prince \
    -DULTIMATE_GHOST_EXTRA_SUBSTATES=2 \
    src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
    src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
    src/ultimate/tablebases/ghost_public_extra_model.cpp \
    src/ultimate/tablebases/external_robdd.cpp \
    src/ultimate/tablebases/ghost_information_probe.cpp \
    src/ultimate/tablebases/information.cpp \
    src/ultimate/position.cpp src/ultimate/nnue.cpp \
    -pthread -o "${binary}"
  sha256sum "${binary}" | tee "${root}/prince-v1-binary.sha256"
fi

test "$(sha256sum "${old_work}/tablebases/kghostkprince.uftb" | cut -d ' ' -f 1)" = \
  2858daf3770e0a47230f2d0bf410cd8ce4a9820b0c7cd0895ceab242ae42f65f
test "$(sha256sum "${old_work}/tablebases/kprincek.uftb" | cut -d ' ' -f 1)" = \
  7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0
test "$(sha256sum "${old_work}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

# Iteration 7 is the last complete fixed-point checkpoint. Odd compactions
# leave the active ROBDD in slot b. The stopped iteration-8 process appended
# only additional canonical tuples to that arena; iteration-7 roots reference
# its immutable 10,077,170-node prefix. Partial *-current outputs are rejected,
# while the complete iteration-7 *-next roots are cloned into both physical
# output slots so iteration 8 is recomputed in full.
if [[ ! -f "${scratch}.checkpoint-cloned" ]]; then
  cp --reflink=always --preserve=all "${old_scratch}.bdd-b.nodes" "${scratch}.bdd-b.nodes"
  cp --reflink=always --preserve=all "${old_scratch}.bdd-b.unique" "${scratch}.bdd-b.unique"
  cp --reflink=always --preserve=all "${old_scratch}.domains" "${scratch}.domains"
  for kind in owner observer visible-owner visible-observer; do
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-next" "${scratch}.${kind}-next"
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-next" "${scratch}.${kind}-current"
  done
  printf '%s\n' \
    'iteration=7' \
    'bdd_slot=b' \
    'current_slot=next' \
    'source_bundle_sha256=d22956a40eb30167adbad15ac5d02b30d7901b616014b5683d0c5c3ba5d555c5' \
    >"${scratch}.checkpoint-cloned"
fi

# The historical exhaustive transition certifier temporarily rewrites exact
# lower-material flags. Never point it at a store used by another solver.
if [[ ! -f "${transition}.isolated-clone" ]]; then
  for extension in header meta strata index blocks verified; do
    cp --reflink=always --preserve=all \
      "${old_transition}.${extension}" "${transition}.${extension}"
  done
  printf '%s\n' \
    'source=restored-kghostkprince-v7' \
    'source_bundle_sha256=d22956a40eb30167adbad15ac5d02b30d7901b616014b5683d0c5c3ba5d555c5' \
    >"${transition}.isolated-clone"
fi

cd "${old_work}"
exec "${binary}" \
  --solve \
  --orientation opposing \
  --transition-prefix "${transition}" \
  --input tablebases/kghostkprince.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${work}/results/kghostkprince.ufiw" \
  --output-arbitrary "${work}/results/kghostkprince.ufgd" \
  --lower-dragon-table tablebases/kprincek.uftb \
  --lower-dragon-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-dragon-source-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-dragon-model-sha256 5511ac5ac166fd2be042e66a4c849e034fef39da317988f40bf4e6b47e071927 \
  --source-sha256 2858daf3770e0a47230f2d0bf410cd8ce4a9820b0c7cd0895ceab242ae42f65f \
  --model-sha256 2d9d3b68ada57c6317ee058bf76605b19ed03ff9f6850fbb0f7cf679ae33cadd \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 2000000000 \
  --unique-slots 1073741824 \
  --compact-every 1 \
  --resume-fixed-point \
  --resume-iteration 7 \
  --resume-current-slot next \
  --resume-bdd-slot b \
  --workers 8
