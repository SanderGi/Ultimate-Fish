#!/usr/bin/env bash
# Build a source-authenticated diagnostic binary that reports the exact
# transition edge behind a fail-closed Parasite lower-table residual. It does
# not read or mutate any transition or fixed-point checkpoint.
set -euo pipefail

bucket=ultimatefish-info-20260808-a4e679c6-831688117652
root=/mnt/ultimatefish/parasite-tracked-current-opposing-v1
resume_build=/mnt/ultimatefish/parasite-resume-v1-0316dac2-build
build=/mnt/ultimatefish/parasite-transition-diagnostic-v1-65a0dde0
resume_patch=${resume_build}/parasite-resume-v1.patch
diagnostic_patch=${build}/parasite_transition_residual_diagnostic_v1.patch
binary=${build}/ultimate_ghost_parasite_transition_diagnostic_v1

test ! -e "${build}"
test "$(sha256sum "${root}/bundle-manifest.json" | cut -d ' ' -f1)" = \
  f83bad4dade55e3c95e3168052b830e20edf9ffe420fe057d2c81f223553e4a7
test "$(sha256sum "${resume_patch}" | cut -d ' ' -f1)" = \
  0316dac2ad23df78bb6e41101ed6fde0535bbd17faaf2aef1453d16ba7eb72a2

install -d -m 0755 "${build}"
exec >"${build}/build.log" 2>&1
cp -a "${root}/src" "${build}/src"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key sources/patches/ghost-parasite-transition-diagnostic-v1/sha256/65a0dde02ed48e45cb29ba57efff65c01f75866c505f7daff8831e3abab677e6/parasite_transition_residual_diagnostic_v1.patch \
  --version-id SyLvHke4BumGgWn_8S5.mQdvfrglLnln "${diagnostic_patch}" \
  >"${build}/patch-get.json"
test "$(sha256sum "${diagnostic_patch}" | cut -d ' ' -f1)" = \
  65a0dde02ed48e45cb29ba57efff65c01f75866c505f7daff8831e3abab677e6
patch -p1 -d "${build}" <"${resume_patch}"
patch -p1 -d "${build}" <"${diagnostic_patch}"

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
binary_key="sources/binaries/ghost-parasite-transition-diagnostic-v1/sha256/${binary_sha}/ultimate_ghost_parasite_transition_diagnostic_v1"
aws s3api put-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --body "${binary}" \
  --metadata "sha256=${binary_sha},source-manifest-sha256=f83bad4dade55e3c95e3168052b830e20edf9ffe420fe057d2c81f223553e4a7,resume-patch-sha256=0316dac2ad23df78bb6e41101ed6fde0535bbd17faaf2aef1453d16ba7eb72a2,diagnostic-patch-sha256=65a0dde02ed48e45cb29ba57efff65c01f75866c505f7daff8831e3abab677e6" \
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
