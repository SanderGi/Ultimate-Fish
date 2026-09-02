#!/usr/bin/env bash
# Build and restore-authenticate the proof-only Penguin singleton child-WDL
# diagnostic. This source adds resume propagation and does not regenerate
# transitions or mutate the retained v29 checkpoint.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=f840cd750df8cf5aaad6484e0a3286ec9b2f3c5a1ba864d54b0758c5e821a8e5
readonly source_version=s7dC7m64ySjWWbHM2QoQ7GN0cnjsUJZP
readonly source_key="sources/bundles/penguin-singleton-child-diagnostic-v31/sha256/${source_sha}/ultimatefish-penguin-singleton-child-diagnostic-v31-source.tar"
readonly root=/mnt/ultimatefish/penguin-singleton-child-diagnostic-v31
readonly source=${root}/source.tar
readonly tree=${root}/source
readonly binary=${root}/ultimate_ghost_penguin_singleton_child_diagnostic_v31_linux

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
readonly binary_key="sources/binaries/penguin-singleton-child-diagnostic-v31/sha256/${binary_sha}/ultimate_ghost_penguin_singleton_child_diagnostic_v31_linux"
readonly binary_version="$(aws s3api put-object --region us-west-2 \
  --bucket "${bucket}" --key "${binary_key}" --body "${binary}" \
  --metadata "sha256=${binary_sha},source-sha256=${source_sha},purpose=singleton-child-wdl-diagnostic" \
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
  "PENGUIN_SINGLETON_CHILD_DIAGNOSTIC_V31_PRESERVED sha256=${binary_sha} size=${binary_size} version_id=${binary_version} key=${binary_key}"
