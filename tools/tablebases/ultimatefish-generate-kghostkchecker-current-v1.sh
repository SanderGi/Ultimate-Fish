#!/usr/bin/env bash
set -euo pipefail

readonly root=/mnt/ultimatefish/checker-concrete-current-v1
readonly stage=/mnt/ultimatefish-devil-fast/devil-spawned-solver-v30-de523aad
readonly source=${stage}/source.tar
readonly binary=${stage}/ultimate_tablebase-devil-spawned-v21
readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly lower_key=results/sha256/3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31/kghostk.uftb
readonly lower_version=YXV7VfOk5tIF_Ft2KCfAg3MYfMOcyXii

test "$(sha256sum "${source}" | cut -d ' ' -f1)" = \
  de523aad058723e70820d7cd645fc4ec3a23a0422e7348c836bac29b8f06f214
test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  6b8fe2bfab9f76fcbfdedfbf67119cae418a749c86b615b40004be3e5b36ea68

install -d -m 0755 "${root}/tablebases" "${root}/work" "${root}/logs"
if [[ ! -e "${root}/tablebases/kghostk.uftb" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key "${lower_key}" --version-id "${lower_version}" \
    "${root}/tablebases/kghostk.uftb" >/dev/null
fi
test "$(sha256sum "${root}/tablebases/kghostk.uftb" | cut -d ' ' -f1)" = \
  3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31
test ! -e "${root}/tablebases/kghostkchecker-current-v1.uftb"

exec >>"${root}/logs/generate-current-v1.log" 2>&1
echo 'checker_concrete_current_v1 workers 56 source_semantics current_checker_capture checkpoint_preserved 1'
cd "${root}"
"${binary}" \
  --piece checker --piece2 ghost --opposing \
  --disk-backed --workers 56 --checkpoint-every 2000000 \
  --output tablebases/kghostkchecker-current-v1.uftb \
  --checkpoint work/kghostkchecker-current-v1.checkpoint
sha256sum tablebases/kghostkchecker-current-v1.uftb
echo 'checker_concrete_current_v1 complete 1'
