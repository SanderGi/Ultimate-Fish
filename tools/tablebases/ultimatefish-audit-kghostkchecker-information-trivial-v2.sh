#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly root=/mnt/ultimatefish/information-trivial-v3/checker-canonical-v42
readonly binary=${root}/ultimate_tablebase_linux
readonly source=/mnt/ultimatefish/info-substate-corrected-v1/kghostkchecker/tablebases/kghostkchecker-current-v2.uftb
readonly runner=${root}/run_ultimate_information_trivial_batch_aws.py
readonly archive=${root}/kghostkchecker.information-v1.tar.zst
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
test "$(sha256sum "${source}" | cut -d ' ' -f 1)" = \
  231d2f45d8d1db2a6e47aa413485444d93599fc68ae0d069afabd5e60369ee10

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key sources/tools/information-trivial-v5/sha256/f563fd54c5582d429f0da7bd0205cba36e4e400cd432a09fd5b41f6e2ad28690/run_ultimate_information_trivial_batch_aws.py \
  --version-id xPkITmGGvAuR40s26fqkSFOP8ulS_4Cd "${runner}" >/dev/null
test "$(sha256sum "${runner}" | cut -d ' ' -f 1)" = \
  f563fd54c5582d429f0da7bd0205cba36e4e400cd432a09fd5b41f6e2ad28690

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key results/information/opposed/ghost-checker/canonical-v42/sha256/5f9321d19ab86a17d302b4ab45983289ccf40ac33cd119ebbcaaee1bd55e7a03/kghostkchecker.information-v1.tar.zst \
  --version-id mFxNTgWIxia9aN8jamtlvRHvbCGkg59K "${archive}" >/dev/null
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = \
  5f9321d19ab86a17d302b4ab45983289ccf40ac33cd119ebbcaaee1bd55e7a03

ln -f "${source}" "${source_dir}/kghostkchecker.uftb"
tar --zstd -xOf "${archive}" payload/results/kghostkchecker.ufiw \
  >"${overlay_dir}/kghostkchecker.ufiw"
test "$(sha256sum "${overlay_dir}/kghostkchecker.ufiw" | cut -d ' ' -f 1)" = \
  a8de2c015ac21b1a128118ae7cc38bfaad53299378f7849495a12f0f4f4a5d23

python3 "${runner}" \
  --binary "${binary}" \
  --binary-sha256 36b5cb8f8d034de381892277848a21e3bc05608f79823bd5043e6a5be067a8f5 \
  --source-bundle-sha256 ed00a54be984d67bc23d5662dea6aa06c34ef2821f27c1cac6c4ec67b6fb6318 \
  --source-dir "${source_dir}" --overlay-dir "${overlay_dir}" \
  --output-dir "${output_dir}" --bucket "${bucket}" \
  --shard 0 --shards 1 --workers 32

test -s "${output_dir}/kghostkchecker.receipt.json"
echo 'checker_information_trivial_v2 complete 1'
