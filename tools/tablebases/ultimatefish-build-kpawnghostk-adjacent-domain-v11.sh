#!/usr/bin/env bash
# Build the Pawn/Ghost solver with adjacent-real-King padding excluded at the
# shared transition compiler/verifier world-domain gate.
set -euo pipefail

prior=/mnt/ultimatefish/pawn-resume-v8-12475566
build=/mnt/ultimatefish/pawn-adjacent-domain-v11
source_dir=${build}/source/ghost-extra-remap-fix-v2-source
patch_file=${build}/ghost_public_extra_exclude_adjacent_kings_v1.patch
binary=${build}/ultimate_ghost_ordinary_information_tablebase-pawn-adjacent-v11

test ! -e "${build}"
test "$(sha256sum "${prior}/source/ghost-extra-remap-fix-v2-source/src/ultimate/tablebases/ghost_extra_information_tablebase.cpp" | cut -d' ' -f1)" = \
  fd57b301ddae49c7f75bacad5bf18f7e85887ed460d49438e06e80627631c526
test "$(sha256sum "${prior}/source/ghost-extra-remap-fix-v2-source/src/ultimate/tablebases/ghost_dragon_information_solver.cpp" | cut -d' ' -f1)" = \
  7922f8ac29b124f04ec7c8fcb116553b42c21b6a0929d2ff57a4f99c5acf39f0
install -d -m 0755 "${build}"
exec >"${build}/build.log" 2>&1
cp -a --reflink=always "${prior}/source" "${build}/source"
cp /mnt/ultimatefish/ghost_public_extra_exclude_adjacent_kings_v1.patch "${patch_file}"
patch -p1 -d "${source_dir}" <"${patch_file}"

cd "${source_dir}"
taskset -c 0 clang++ \
  -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Wno-error=range-loop-construct -include sstream \
  -Isrc/ultimate -Isrc/ultimate/tablebases \
  -DULTIMATE_GHOST_ORDINARY_PIECE=Pawn \
  -DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY \
  -DULTIMATE_GHOST_EXTRA_SUBSTATES=2 \
  -DULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN \
  src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
  src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
  src/ultimate/tablebases/ghost_public_extra_model.cpp \
  src/ultimate/tablebases/external_robdd.cpp \
  src/ultimate/tablebases/ghost_information_probe.cpp \
  src/ultimate/tablebases/information.cpp \
  src/ultimate/position.cpp src/ultimate/nnue.cpp \
  -o "${binary}"
sha256sum "${binary}" >"${build}/binary.sha256"
"${binary}" --self-test \
  --orientation same \
  --input /mnt/ultimatefish/info-substate-corrected-v1/kpawnghostk-v4/tablebases/kpawnghostk.uftb \
  --source-sha256 4f6a0f7300fd856518c7701fb49eea53298382fcd35c0fffca7a575113a669e7 \
  --scratch "${build}/self-test"
