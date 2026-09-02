#!/usr/bin/env bash
# Build an authenticated Pawn/Ghost fixed-point resume binary with exact
# existing-ROBDD reopening. This builds code only and never opens a tablebase
# checkpoint or result path.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
previous=/mnt/ultimatefish/pawn-resume-v5-25899b61
build=/mnt/ultimatefish/pawn-resume-v6-7136049d
source_dir=${build}/source/ghost-extra-remap-fix-v2-source
binary=${build}/ultimate_ghost_ordinary_information_tablebase-pawn-resume-v6
base=${previous}/downloads/base.tar.zst
correction=${previous}/downloads/correction.tar.gz
resume_patch=${previous}/downloads/resume.patch
open_patch=${build}/external_robdd_resume_open_v1.patch

test ! -e "${build}"
test "$(sha256sum "${base}" | cut -d ' ' -f1)" = \
  4be43c8bf7c19f636cc051fc77a7d1496dd263ecb62f5d447a1e83a8ba646e35
test "$(sha256sum "${correction}" | cut -d ' ' -f1)" = \
  da563e009e78d8b5d6b2df670471debbf04751a7ba8548cc1bec27b2fb53b654
test "$(sha256sum "${resume_patch}" | cut -d ' ' -f1)" = \
  25899b617a89f7aa63a965d4c30a78563bb298a087c1f9bc4e0735f3ea3295c8

install -d -m 0755 "${build}/source"
exec >"${build}/build.log" 2>&1
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key sources/patches/ghost-resume-open-v1/sha256/7136049d11357523571c3d1c36660a4adcd89405700bba94b998272d702ff4af/external_robdd_resume_open_v1.patch \
  --version-id UlFqU6y2u_uyLtw4N23fZ8nF1ScsMjqc "${open_patch}" \
  >"${build}/patch-get.json"
test "$(sha256sum "${open_patch}" | cut -d ' ' -f1)" = \
  7136049d11357523571c3d1c36660a4adcd89405700bba94b998272d702ff4af

tar --zstd -xf "${base}" -C "${build}/source"
tar -xzf "${correction}" -C "${source_dir}"
patch -p1 -d "${source_dir}" <"${resume_patch}"
patch -p1 -d "${source_dir}" <"${open_patch}"

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

binary_sha=$(sha256sum "${binary}" | cut -d ' ' -f1)
binary_key="sources/binaries/pawn-ghost-resume-v6/sha256/${binary_sha}/ultimate_ghost_ordinary_information_tablebase-pawn-resume-v6"
aws s3api put-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --body "${binary}" \
  --metadata "sha256=${binary_sha},base-source-sha256=4be43c8bf7c19f636cc051fc77a7d1496dd263ecb62f5d447a1e83a8ba646e35,correction-sha256=da563e009e78d8b5d6b2df670471debbf04751a7ba8548cc1bec27b2fb53b654,resume-patch-sha256=25899b617a89f7aa63a965d4c30a78563bb298a087c1f9bc4e0735f3ea3295c8,open-patch-sha256=7136049d11357523571c3d1c36660a4adcd89405700bba94b998272d702ff4af" \
  >"${build}/binary-put.json"
binary_version=$(python3 -c \
  'import json,sys;print(json.load(open(sys.argv[1]))["VersionId"])' \
  "${build}/binary-put.json")
aws s3api head-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --version-id "${binary_version}" \
  >"${build}/binary-head.json"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --version-id "${binary_version}" \
  "${binary}.restored" >"${build}/binary-get.json"
test "$(sha256sum "${binary}.restored" | cut -d ' ' -f1)" = "${binary_sha}"
cmp "${binary}" "${binary}.restored"

python3 - "${build}/build-certificate.json" "${binary_sha}" \
  "${binary_key}" "${binary_version}" <<'PY'
import json
import pathlib
import sys
path, binary_sha, key, version = sys.argv[1:]
record = {
    "schema": "ultimate-pawn-ghost-resume-build-v6",
    "binary_sha256": binary_sha,
    "binary_key": key,
    "binary_version_id": version,
    "base_source_sha256": "4be43c8bf7c19f636cc051fc77a7d1496dd263ecb62f5d447a1e83a8ba646e35",
    "correction_sha256": "da563e009e78d8b5d6b2df670471debbf04751a7ba8548cc1bec27b2fb53b654",
    "resume_patch_sha256": "25899b617a89f7aa63a965d4c30a78563bb298a087c1f9bc4e0735f3ea3295c8",
    "open_patch_sha256": "7136049d11357523571c3d1c36660a4adcd89405700bba94b998272d702ff4af",
    "s3_restore_residual": 0,
}
pathlib.Path(path).write_text(json.dumps(record, sort_keys=True) + "\n")
PY
cat "${build}/build-certificate.json"
