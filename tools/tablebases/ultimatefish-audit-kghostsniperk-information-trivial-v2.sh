#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly root=/mnt/ultimatefish/information-trivial-v3/sniper-same-v15
readonly retained=/mnt/ultimatefish/sniper-ghost-same-restore-v11/info-substate-corrected-v1/kghostsniperk-v2
readonly binary=${root}/ultimate_tablebase_linux
readonly runner=${root}/run_ultimate_information_trivial_batch_aws.py
readonly source=${retained}/tablebases/kghostsniperk.uftb
readonly overlay=${retained}/work/results/kghostsniperk-compositional-v3.ufiw
readonly source_dir=${root}/source
readonly overlay_dir=${root}/overlay
readonly output_dir=${root}/output

install -d -m 0755 "${root}" "${source_dir}" "${overlay_dir}" "${output_dir}"
if [[ ! -e "${binary}" ]]; then
  aws s3api get-object --region us-west-2 --bucket "${bucket}" \
    --key sources/binaries/information-trivial-v5/sha256/36b5cb8f8d034de381892277848a21e3bc05608f79823bd5043e6a5be067a8f5/ultimate_tablebase_linux \
    --version-id frAtKN9Xejmj3i4nWo0EYvNw._Ndw1az "${binary}" >/dev/null
  chmod 0755 "${binary}"
fi
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  36b5cb8f8d034de381892277848a21e3bc05608f79823bd5043e6a5be067a8f5

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key sources/tools/information-trivial-v5/sha256/f563fd54c5582d429f0da7bd0205cba36e4e400cd432a09fd5b41f6e2ad28690/run_ultimate_information_trivial_batch_aws.py \
  --version-id xPkITmGGvAuR40s26fqkSFOP8ulS_4Cd "${runner}" >/dev/null
test "$(sha256sum "${runner}" | cut -d ' ' -f 1)" = \
  f563fd54c5582d429f0da7bd0205cba36e4e400cd432a09fd5b41f6e2ad28690
test "$(sha256sum "${source}" | cut -d ' ' -f 1)" = \
  b627e286865f3081805a8a81ab8ec4ec8938c8a8486f3df8a0ae6a3edc4a12f1
test "$(sha256sum "${overlay}" | cut -d ' ' -f 1)" = \
  1b05388f639d1789377951dfabe19162807466be0a512bea5f2fe72edd743526

cp --reflink=auto --preserve=mode,timestamps \
  "${source}" "${source_dir}/kghostsniperk.uftb"
cp --reflink=auto --preserve=mode,timestamps \
  "${overlay}" "${overlay_dir}/kghostsniperk.ufiw"
test "$(sha256sum "${source_dir}/kghostsniperk.uftb" | cut -d ' ' -f 1)" = \
  b627e286865f3081805a8a81ab8ec4ec8938c8a8486f3df8a0ae6a3edc4a12f1
test "$(sha256sum "${overlay_dir}/kghostsniperk.ufiw" | cut -d ' ' -f 1)" = \
  1b05388f639d1789377951dfabe19162807466be0a512bea5f2fe72edd743526
python3 "${runner}" \
  --binary "${binary}" \
  --binary-sha256 36b5cb8f8d034de381892277848a21e3bc05608f79823bd5043e6a5be067a8f5 \
  --source-bundle-sha256 ed00a54be984d67bc23d5662dea6aa06c34ef2821f27c1cac6c4ec67b6fb6318 \
  --source-dir "${source_dir}" --overlay-dir "${overlay_dir}" \
  --output-dir "${output_dir}" --bucket "${bucket}" \
  --shard 0 --shards 1 --workers 30
test -s "${output_dir}/kghostsniperk.receipt.json"
echo 'sniper_same_information_trivial_v2 complete 1'
