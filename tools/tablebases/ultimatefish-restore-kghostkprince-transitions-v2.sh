#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=22939ac3e35b76f578233ac682e6b4079b77074e0e65d731202f884d2fa31ed2
readonly source_version=MCnYcjjl6spYix98KabrKowVQWMxkHku
readonly source_key="sources/bundles/ghost-parallel-v4/sha256/${source_sha}/ultimatefish-ghost-parallel-v4-source.tar"
readonly root=/mnt/ultimatefish/ghost-parallel-v4
readonly archive=${root}/ultimatefish-ghost-parallel-v4-source.tar
readonly source=${root}/source
readonly binary=${root}/ultimate_ghost_ordinary_information_tablebase-prince-v4
readonly work=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostkprince-solve-2b-v7

install -d -m 0755 "${root}"
if [[ ! -f "${archive}" ]]; then
  aws s3api get-object \
    --bucket "${bucket}" --key "${source_key}" --version-id "${source_version}" \
    "${archive}"
fi
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = "${source_sha}"
if [[ ! -d "${source}" ]]; then
  install -d -m 0755 "${source}"
  tar -xf "${archive}" -C "${source}" --strip-components=1
fi
if [[ ! -x "${binary}" ]]; then
  cd "${source}"
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
  sha256sum "${binary}" | tee "${root}/prince-v2-binary.sha256"
fi

cd "${work}"
export ULTIMATE_GHOST_TRANSITION_WORKERS=8
exec "${binary}" \
  --restore-transitions \
  --orientation opposing \
  --transition-prefix work/transitions/kghostkprince \
  --lower-dragon-table tablebases/kprincek.uftb \
  --lower-dragon-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-dragon-source-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-dragon-model-sha256 5511ac5ac166fd2be042e66a4c849e034fef39da317988f40bf4e6b47e071927
