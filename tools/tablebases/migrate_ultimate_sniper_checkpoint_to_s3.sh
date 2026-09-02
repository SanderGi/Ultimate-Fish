#!/usr/bin/env bash
set -euo pipefail

readonly bucket="${1:?bucket}"
readonly source_root="${2:-/mnt/ultimatefish}"
readonly marker=${source_root}/sniper-ghost-same-migration-v10.uploaded
readonly hash_file=${source_root}/sniper-ghost-same-migration-v10.sha256
readonly staging_key=staging/checkpoints/sniper-ghost/same/migration-v10/sniper-ghost-same-migration-v10.tar.zst

rm -f "${hash_file}"
tar --sparse -C "${source_root}" -cf - \
  info-substate-corrected-v1/kghostsniperk-v2 \
  compositional-transitions-v5-sniper-same-source \
  ultimatefish-solve-kghostsniperk-memory-v2.sh \
  | zstd -T2 -3 \
  | tee >(sha256sum | awk '{print $1}' >"${hash_file}") \
  | aws s3 cp - "s3://${bucket}/${staging_key}" \
      --expected-size 174700000000 \
      --only-show-errors

for unused in {1..60}; do
  test -s "${hash_file}" && break
  sleep 1
done
archive_sha256="$(cat "${hash_file}")"
readonly archive_sha256
test "${#archive_sha256}" = 64
readonly key="results/checkpoints/sniper-ghost/same/migration-v10/sha256/${archive_sha256}/sniper-ghost-same-migration-v10.tar.zst"

aws s3 cp "s3://${bucket}/${staging_key}" "s3://${bucket}/${key}" \
  --only-show-errors \
  --metadata-directive REPLACE \
  --metadata "sha256=${archive_sha256}"

printf '%s\n%s\n%s\n' "${archive_sha256}" "${key}" "${staging_key}" >"${marker}"
printf 'sniper_checkpoint_upload_complete sha256 %s key %s\n' \
  "${archive_sha256}" "${key}"
