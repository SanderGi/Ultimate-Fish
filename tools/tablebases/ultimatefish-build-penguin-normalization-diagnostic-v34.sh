#!/usr/bin/env bash
# Build and restore-authenticate the Penguin source-normalization witness
# diagnostic. It binds one normalized singleton to its original packed byte.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=92a6438d9a16d40d2ebd0410493526aee2b6590c420fc38cead02a819659ad22
readonly source_version=oM7bofOmLx0Mw1U.EjoZZf6X8mug7QYT
readonly source_key="sources/bundles/penguin-normalization-diagnostic-v34/sha256/${source_sha}/ultimatefish-penguin-normalization-diagnostic-v34-source.tar"
readonly root=/mnt/ultimatefish/penguin-normalization-diagnostic-v34
readonly source=${root}/source.tar
readonly tree=${root}/source
readonly binary=${root}/ultimate_ghost_penguin_normalization_diagnostic_v34_linux

test ! -e "${root}"
install -d -m 0755 "${tree}"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${source_key}" --version-id "${source_version}" \
  "${source}" >"${root}/source-get.json"
test "$(sha256sum "${source}" | cut -d' ' -f1)" = "${source_sha}"
tar -xf "${source}" -C "${tree}"

cd "${tree}"
taskset -c 16-31 clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra \
  -Wpedantic -Werror -Wno-error=range-loop-construct -include sstream \
  -Isrc/ultimate -Isrc/ultimate/tablebases \
  -DULTIMATE_GHOST_ORDINARY_PIECE=Penguin \
  -DULTIMATE_GHOST_EXTRA_SUBSTATES=8 \
  -DULTIMATE_GHOST_ORDINARY_LOWER_SUBSTATES=4 \
  src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
  src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
  src/ultimate/tablebases/ghost_public_extra_model.cpp \
  src/ultimate/tablebases/external_robdd.cpp \
  src/ultimate/tablebases/ghost_information_probe.cpp \
  src/ultimate/tablebases/information.cpp src/ultimate/position.cpp \
  src/ultimate/nnue.cpp -pthread -o "${binary}"

readonly binary_sha="$(sha256sum "${binary}" | cut -d' ' -f1)"
readonly binary_size="$(stat -c %s "${binary}")"
readonly binary_key="sources/binaries/penguin-normalization-diagnostic-v34/sha256/${binary_sha}/ultimate_ghost_penguin_normalization_diagnostic_v34_linux"
readonly binary_version="$(aws s3api put-object --region us-west-2 \
  --bucket "${bucket}" --key "${binary_key}" --body "${binary}" \
  --metadata "sha256=${binary_sha},source-sha256=${source_sha},purpose=penguin-normalization-witness" \
  --query VersionId --output text)"
test -n "${binary_version}" && test "${binary_version}" != None
readonly restored="$(mktemp "${root}/restored.XXXXXX")"
trap 'rm -f "${restored}"' EXIT
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --version-id "${binary_version}" \
  "${restored}" >"${root}/binary-get.json"
test "$(sha256sum "${restored}" | cut -d' ' -f1)" = "${binary_sha}"
cmp "${binary}" "${restored}"
printf '%s\n' \
  "PENGUIN_NORMALIZATION_DIAGNOSTIC_V34_PRESERVED sha256=${binary_sha} size=${binary_size} version_id=${binary_version} key=${binary_key}"
