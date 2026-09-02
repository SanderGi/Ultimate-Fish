#!/usr/bin/env bash
# Build a diagnostic Pawn/Ghost binary on top of the v8 adjacent-King fix.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
prior=/mnt/ultimatefish/pawn-resume-v8-12475566
build=/mnt/ultimatefish/pawn-resume-diagnostic-v9
source_dir=${build}/source/ghost-extra-remap-fix-v2-source
patch_file=${build}/dragon_transition_move_count_diagnostic_v2.patch
binary=${build}/ultimate_ghost_ordinary_information_tablebase-pawn-diagnostic-v9

test ! -e "${build}"
test "$(sha256sum "${prior}/ultimate_ghost_ordinary_information_tablebase-pawn-resume-v8" | cut -d' ' -f1)" = \
  f98c9fa41ae87647133e6a94d5deaf7f46bae038a0fa35dc60071856589daa46
install -d -m 0755 "${build}"
exec >"${build}/build.log" 2>&1
cp -a --reflink=always "${prior}/source" "${build}/source"
cp /mnt/ultimatefish/dragon_transition_move_count_diagnostic_v2.patch "${patch_file}"
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
