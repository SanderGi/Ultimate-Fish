#!/usr/bin/env bash
# Rebuild the opposed Parasite/Ghost resume binary with authenticated
# fixed-point resume support and exact existing-ROBDD reopening.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
root=/mnt/ultimatefish/parasite-tracked-current-opposing-v1
previous=/mnt/ultimatefish/parasite-resume-v1-0316dac2-build
build=/mnt/ultimatefish/parasite-resume-v2-7136049d-build
resume_patch=${previous}/parasite-resume-v1.patch
open_patch=${build}/external_robdd_resume_open_v1.patch
binary=${build}/ultimate_ghost_parasite_information_tablebase

test ! -e "${build}"
test "$(sha256sum "${root}/bundle-manifest.json" | cut -d ' ' -f1)" = \
  f83bad4dade55e3c95e3168052b830e20edf9ffe420fe057d2c81f223553e4a7
test "$(sha256sum "${resume_patch}" | cut -d ' ' -f1)" = \
  0316dac2ad23df78bb6e41101ed6fde0535bbd17faaf2aef1453d16ba7eb72a2

install -d -m 0755 "${build}"
exec >"${build}/build.log" 2>&1
cp -a "${root}/src" "${build}/src"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key sources/patches/ghost-resume-open-v1/sha256/7136049d11357523571c3d1c36660a4adcd89405700bba94b998272d702ff4af/external_robdd_resume_open_v1.patch \
  --version-id UlFqU6y2u_uyLtw4N23fZ8nF1ScsMjqc "${open_patch}" \
  >"${build}/patch-get.json"
test "$(sha256sum "${open_patch}" | cut -d ' ' -f1)" = \
  7136049d11357523571c3d1c36660a4adcd89405700bba94b998272d702ff4af
patch -p1 -d "${build}" <"${resume_patch}"
patch -p1 -d "${build}" <"${open_patch}"

cd "${build}"
python3 - "${root}/bundle-manifest.json" "${binary}" <<'PY'
import json
import subprocess
import sys
manifest, binary = sys.argv[1:]
command = json.load(open(manifest, encoding="utf-8"))["commands"]["build"]
if command[0] != "clang++" or "-o" not in command:
    raise SystemExit("Parasite build command residual")
command[command.index("-o") + 1] = binary
subprocess.run(["taskset", "-c", "2", *command], check=True)
PY

binary_sha=$(sha256sum "${binary}" | cut -d ' ' -f1)
binary_key="sources/binaries/ghost-parasite-resume-v2/sha256/${binary_sha}/ultimate_ghost_parasite_information_tablebase"
aws s3api put-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --body "${binary}" \
  --metadata "sha256=${binary_sha},source-manifest-sha256=f83bad4dade55e3c95e3168052b830e20edf9ffe420fe057d2c81f223553e4a7,resume-patch-sha256=0316dac2ad23df78bb6e41101ed6fde0535bbd17faaf2aef1453d16ba7eb72a2,open-patch-sha256=7136049d11357523571c3d1c36660a4adcd89405700bba94b998272d702ff4af" \
  >"${build}/binary-put.json"
binary_version=$(python3 -c \
  'import json,sys;print(json.load(open(sys.argv[1]))["VersionId"])' \
  "${build}/binary-put.json")
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --version-id "${binary_version}" \
  "${binary}.restored" >"${build}/binary-get.json"
test "$(sha256sum "${binary}.restored" | cut -d ' ' -f1)" = "${binary_sha}"
cmp "${binary}" "${binary}.restored"
printf '%s %s %s\n' "${binary_sha}" "${binary_key}" "${binary_version}" \
  >"${build}/build-certificate.txt"
cat "${build}/build-certificate.txt"
