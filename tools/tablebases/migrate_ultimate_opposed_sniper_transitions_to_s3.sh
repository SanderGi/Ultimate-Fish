#!/usr/bin/env bash
set -euo pipefail

readonly bucket="${1:?bucket}"
readonly source=/mnt/ultimatefish/opposed-ghost-sniper-rebuild-after-stop-v1/job
readonly marker=/mnt/ultimatefish/opposed-sniper-transitions-v12.uploaded
readonly hash_file=/mnt/ultimatefish/opposed-sniper-transitions-v12.sha256
readonly staging_key=staging/checkpoints/sniper-ghost/opposed/transitions-v12/opposed-sniper-transitions-v12.tar.zst

rm -f "${hash_file}"
tar --sparse -C "${source}" -cf - \
  tablebases \
  work/transitions \
  work/transitions-ready.json \
  work/self-test \
  | zstd -T1 -3 \
  | tee >(sha256sum | awk '{print $1}' >"${hash_file}") \
  | aws s3 cp - "s3://${bucket}/${staging_key}" \
      --expected-size 95500000000 \
      --only-show-errors

for unused in {1..60}; do
  test -s "${hash_file}" && break
  sleep 1
done
archive_sha256="$(cat "${hash_file}")"
readonly archive_sha256
test "${#archive_sha256}" = 64
readonly key="results/checkpoints/sniper-ghost/opposed/transitions-v12/sha256/${archive_sha256}/opposed-sniper-transitions-v12.tar.zst"

aws s3 cp "s3://${bucket}/${staging_key}" "s3://${bucket}/${key}" \
  --only-show-errors \
  --metadata-directive REPLACE \
  --metadata "sha256=${archive_sha256}"

printf '%s\n%s\n%s\n' "${archive_sha256}" "${key}" "${staging_key}" >"${marker}"
printf 'opposed_sniper_transition_upload_complete sha256 %s key %s\n' \
  "${archive_sha256}" "${key}"
