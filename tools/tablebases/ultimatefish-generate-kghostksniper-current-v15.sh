#!/usr/bin/env bash
# Regenerate the opposed Sniper/Ghost concrete oracle under current capture
# semantics.  The retained information fixed point is independent of this
# oracle; the oracle is used by the exhaustive singleton certification pass.
set -euo pipefail

readonly root=/mnt/ultimatefish/sniper-concrete-current-v15
readonly stage=${root}/stage
readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source=${stage}/ultimatefish-devil-spawned-solver-v27-source-authenticated.tar
readonly binary=${stage}/ultimate_tablebase-devil-spawned-v27
readonly source_sha=de523aad058723e70820d7cd645fc4ec3a23a0422e7348c836bac29b8f06f214
readonly binary_sha=6b8fe2bfab9f76fcbfdedfbf67119cae418a749c86b615b40004be3e5b36ea68
readonly ghost_sha=11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5
readonly sniper_sha=473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5

install -d -m 0755 "${stage}" "${root}/tablebases" "${root}/work" "${root}/logs"

fetch() {
  local key=$1 version=$2 output=$3 expected=$4
  if [[ ! -e "${output}" ]]; then
    aws s3api get-object --region us-west-2 --bucket "${bucket}" \
      --key "${key}" --version-id "${version}" "${output}" >/dev/null
  fi
  test "$(sha256sum "${output}" | cut -d ' ' -f1)" = "${expected}"
}

fetch \
  sources/bundles/devil-spawned-solver-v27/sha256/${source_sha}/ultimatefish-devil-spawned-solver-v27-source-authenticated.tar \
  MAFo7cMnzt3wo593lhCMHIe6fmttVnDg "${source}" "${source_sha}"
fetch \
  sources/binaries/devil-spawned-solver-v27/sha256/${binary_sha}/ultimate_tablebase-devil-spawned-v27 \
  Mh9h30fIAXagqVQUWn51_xmhQtNrJvMO "${binary}" "${binary_sha}"
fetch \
  sources/dependencies/concrete-penguin-current-v1/kghostk.uftb/sha256/${ghost_sha}/kghostk.uftb \
  zKtBJe_NMYQoYHNR8kA.oP7_VURDvYJD "${root}/tablebases/kghostk.uftb" "${ghost_sha}"
fetch \
  sources/dependencies/legacy-concrete/sha256/${sniper_sha}/ksniperk.uftb \
  zgd_7Of45KYhZB_In9djM_qTPeD4xv42 "${root}/tablebases/ksniperk.uftb" "${sniper_sha}"

chmod 0755 "${binary}"
test ! -e "${root}/tablebases/kghostksniper-current-v15.uftb"

exec >>"${root}/logs/generate-current-v15.log" 2>&1
echo 'sniper_concrete_current_v15 workers 30 source_semantics current_sniper_capture ghost_primary 1 checkpoint_preserved 1'
cd "${root}"
"${binary}" \
  --piece ghost --piece2 sniper --opposing \
  --disk-backed --workers 30 --checkpoint-every 2000000 \
  --output tablebases/kghostksniper-current-v15.uftb \
  --checkpoint work/kghostksniper-current-v15.checkpoint
sha256sum tablebases/kghostksniper-current-v15.uftb
echo 'sniper_concrete_current_v15 complete 1'
