#!/usr/bin/env bash
# Build exact Pawn/Ghost v6 resume source with unreachable adjacent-King
# padding excluded from transition regeneration, matching singleton proof scope.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
prior=/mnt/ultimatefish/pawn-resume-v6-7136049d
build=/mnt/ultimatefish/pawn-resume-v8-12475566
source_dir=${build}/source/ghost-extra-remap-fix-v2-source
patch_file=${build}/dragon_transition_adjacent_padding_v1.patch
binary=${build}/ultimate_ghost_ordinary_information_tablebase-pawn-resume-v8

test ! -e "${build}"
test "$(sha256sum "${prior}/ultimate_ghost_ordinary_information_tablebase-pawn-resume-v6" | cut -d' ' -f1)" = \
  2a25f6a0ff36b8882f53e9917a6dab8a45b3c73377a112cf6687bb2292039498
install -d -m 0755 "${build}"
exec >"${build}/build.log" 2>&1
cp -a --reflink=always "${prior}/source" "${build}/source"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key sources/patches/ghost-adjacent-padding-v1/sha256/124755668de5fbb1e67efe648a7014100592aa3ee75dc1cb357baa5766f1be4a/dragon_transition_adjacent_padding_v1.patch \
  --version-id VBNuF2VTLR7R5lI.2qvXVBCcEjG4XrkR "${patch_file}" \
  >"${build}/patch-get.json"
test "$(sha256sum "${patch_file}" | cut -d' ' -f1)" = \
  124755668de5fbb1e67efe648a7014100592aa3ee75dc1cb357baa5766f1be4a
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

sha=$(sha256sum "${binary}" | cut -d' ' -f1)
key="sources/binaries/pawn-ghost-resume-v8/sha256/${sha}/ultimate_ghost_ordinary_information_tablebase-pawn-resume-v8"
aws s3api put-object --region us-west-2 --bucket "${bucket}" --key "${key}" \
  --body "${binary}" \
  --metadata "sha256=${sha},resume-v6-binary-sha256=2a25f6a0ff36b8882f53e9917a6dab8a45b3c73377a112cf6687bb2292039498,adjacent-padding-patch-sha256=124755668de5fbb1e67efe648a7014100592aa3ee75dc1cb357baa5766f1be4a" \
  >"${build}/binary-put.json"
version=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["VersionId"])' "${build}/binary-put.json")
aws s3api get-object --region us-west-2 --bucket "${bucket}" --key "${key}" \
  --version-id "${version}" "${binary}.restored" >"${build}/binary-get.json"
test "$(sha256sum "${binary}.restored" | cut -d' ' -f1)" = "${sha}"
cmp "${binary}" "${binary}.restored"
printf '{"adjacent_padding_patch_sha256":"%s","binary_key":"%s","binary_sha256":"%s","binary_version_id":"%s","resume_v6_binary_sha256":"%s","s3_restore_residual":0}\n' \
  124755668de5fbb1e67efe648a7014100592aa3ee75dc1cb357baa5766f1be4a \
  "${key}" "${sha}" "${version}" \
  2a25f6a0ff36b8882f53e9917a6dab8a45b3c73377a112cf6687bb2292039498 \
  >"${build}/build-certificate.json"
cat "${build}/build-certificate.json"
