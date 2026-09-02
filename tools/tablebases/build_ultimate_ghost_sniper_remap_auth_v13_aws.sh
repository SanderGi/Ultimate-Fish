#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly root=/mnt/ultimatefish/sniper-ghost-same-restore-v11
readonly work=/mnt/ultimatefish/sniper-ghost-remap-auth-v14
readonly source_bundle=${work}/ultimatefish-source.tar
readonly source=${work}/source
readonly binary=${work}/ultimate_ghost_sniper_information_tablebase-v14
readonly source_bundle_sha=40aaf70586f0b3eff895f6db5f02f0be15cbc45401dafaaa98918407556e376d
readonly patch_sha=019d5a36126544b07775c33fb0f153ca53e01c8493a4edf3623e103a1f799a7c

test ! -e "${work}"
install -d -m 0755 "${work}" "${source}"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "sources/bundles/sniper-remap-auth-v14/sha256/${source_bundle_sha}/ultimatefish-sniper-remap-auth-v14-source.tar" \
  --version-id BqHyNICjz8OPao0XR6bUlwA57w95r9bY "${source_bundle}"
test "$(sha256sum "${source_bundle}" | cut -d ' ' -f 1)" = "${source_bundle_sha}"
tar -xf "${source_bundle}" -C "${source}"
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_dragon_information_solver.cpp" | cut -d ' ' -f 1)" = "${patch_sha}"

cd "${source}"
readonly sources=(
  src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp
  src/ultimate/tablebases/ghost_ordinary_information_solver.cpp
  src/ultimate/tablebases/ghost_public_extra_model.cpp
  src/ultimate/tablebases/external_robdd.cpp
  src/ultimate/tablebases/ghost_information_probe.cpp
  src/ultimate/tablebases/information.cpp
  src/ultimate/position.cpp
  src/ultimate/nnue.cpp
)
clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Wno-error=range-loop-construct -include sstream \
  -Isrc/ultimate -Isrc/ultimate/tablebases \
  -DULTIMATE_GHOST_ORDINARY_PIECE=Sniper \
  -DULTIMATE_GHOST_EXTRA_SUBSTATES=4 \
  -DULTIMATE_GHOST_EXTRA_HORIZONTAL_ONLY \
  "${sources[@]}" -o "${binary}" >"${work}/build.log" 2>&1
ULTIMATE_GHOST_SELF_TEST_THREADS=30 "${binary}" --self-test \
  --orientation same --scratch "${work}/self-test" \
  >"${work}/self-test.log" 2>&1
grep -F 'ghost_dragon_exact_self_test' "${work}/self-test.log"

readonly binary_sha="$(sha256sum "${binary}" | cut -d ' ' -f 1)"
readonly binary_key="sources/binaries/sniper-remap-auth-v14/sha256/${binary_sha}/ultimate_ghost_sniper_information_tablebase-v14"
readonly binary_version="$(aws s3api put-object --region us-west-2 \
  --bucket "${bucket}" --key "${binary_key}" --body "${binary}" \
  --metadata "sha256=${binary_sha},source-bundle-sha256=${source_bundle_sha},patch-sha256=${patch_sha}" \
  --query VersionId --output text)"
test -n "${binary_version}"
install -d -m 0755 "${work}/restore"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --version-id "${binary_version}" \
  "${work}/restore/ultimate_ghost_sniper_information_tablebase-v14"
cmp -s "${binary}" \
  "${work}/restore/ultimate_ghost_sniper_information_tablebase-v14"
printf 'binary_sha256 %s\nbinary_key %s\nbinary_version %s\nsource_bundle_sha256 %s\npatch_sha256 %s\nrestore_residual 0\n' \
  "${binary_sha}" "${binary_key}" "${binary_version}" \
  "${source_bundle_sha}" "${patch_sha}" | tee "${work}/build-receipt.txt"
