#!/usr/bin/env bash
# Regenerate the opposed Sniper/Ghost concrete oracle with rank-reversing
# canonical probes for Black directional lower material.
set -euo pipefail

readonly root=/mnt/ultimatefish/sniper-concrete-probe-v18
readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly binary=${root}/stage/ultimate_tablebase-sniper-probe-v18
readonly output=${root}/tablebases/kghostksniper-probe-v18.uftb
readonly log=${root}/logs/generate-probe-v18.log

test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  dac3e717e0edcfa94ce1f4a2f31b24b67d51a7a0da837fef6a981b08bdd071e3
test "$(sha256sum "${root}/stage/source.tar" | cut -d ' ' -f1)" = \
  b3a6e0c02d9add763a4ef8d8ddb0f15576b235b7beb96ee8277987a4773b624a
test "$(sha256sum "${root}/tablebases/kghostk.uftb" | cut -d ' ' -f1)" = \
  11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5
test "$(sha256sum "${root}/tablebases/ksniperk.uftb" | cut -d ' ' -f1)" = \
  473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
test ! -e "${output}"

cd "${root}"
exec >>"${log}" 2>&1
echo 'sniper_concrete_probe_v18 workers 32 vertical_color_probe 1 checkpoint_preserved 1'
"${binary}" --piece ghost --piece2 sniper --opposing \
  --disk-backed --workers 32 --checkpoint-every 2000000 \
  --output tablebases/kghostksniper-probe-v18.uftb \
  --checkpoint work/kghostksniper-probe-v18.checkpoint

readonly sha="$(sha256sum "${output}" | cut -d ' ' -f1)"
readonly size="$(stat -c %s "${output}")"
readonly key="results/concrete/sniper-probe-v18/sha256/${sha}/kghostksniper-probe-v18.uftb"
readonly version="$(aws s3api put-object --region us-west-2 --bucket "${bucket}" \
  --key "${key}" --body "${output}" \
  --metadata "sha256=${sha},binary-sha256=dac3e717e0edcfa94ce1f4a2f31b24b67d51a7a0da837fef6a981b08bdd071e3,source-bundle-sha256=b3a6e0c02d9add763a4ef8d8ddb0f15576b235b7beb96ee8277987a4773b624a" \
  --query VersionId --output text)"
aws s3api get-object --region us-west-2 --bucket "${bucket}" --key "${key}" \
  --version-id "${version}" "${output}.restore" >/dev/null
test "$(sha256sum "${output}.restore" | cut -d ' ' -f1)" = "${sha}"
rm -f "${output}.restore"
printf 'sniper_concrete_probe_v18 complete 1 sha256 %s size %s key %s version_id %s restore_residual 0\n' \
  "${sha}" "${size}" "${key}" "${version}"
