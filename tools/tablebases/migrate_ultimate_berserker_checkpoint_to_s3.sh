#!/usr/bin/env bash
set -euo pipefail

readonly bucket="${1:?bucket}"
readonly marker=/mnt/ultimatefish/berserker-ghost-opposed-migration-v12.uploaded
readonly hash_file=/mnt/ultimatefish/berserker-ghost-opposed-migration-v12.sha256
readonly staging_key=staging/checkpoints/berserker-ghost/opposed/migration-v12/berserker-ghost-opposed-migration-v12.tar.zst

rm -f "${hash_file}"
tar --sparse \
  -C /mnt/ultimatefish-overflow \
  -cf - info-remap-fix-v1/kberserkerkghost-fresh-v5 \
  -C /mnt/ultimatefish \
  info-wave-aa82210e-stage/kghostk.ufgm \
  | zstd -T2 -3 \
  | tee >(sha256sum | awk '{print $1}' >"${hash_file}") \
  | aws s3 cp - "s3://${bucket}/${staging_key}" \
      --expected-size 364000000000 \
      --only-show-errors

for unused in {1..60}; do
  test -s "${hash_file}" && break
  sleep 1
done
archive_sha256="$(cat "${hash_file}")"
readonly archive_sha256
test "${#archive_sha256}" = 64
readonly key="results/checkpoints/berserker-ghost/opposed/migration-v12/sha256/${archive_sha256}/berserker-ghost-opposed-migration-v12.tar.zst"

aws s3 cp "s3://${bucket}/${staging_key}" "s3://${bucket}/${key}" \
  --only-show-errors \
  --metadata-directive REPLACE \
  --metadata "sha256=${archive_sha256}"

printf '%s\n%s\n%s\n' "${archive_sha256}" "${key}" "${staging_key}" >"${marker}"
printf 'berserker_checkpoint_upload_complete sha256 %s key %s\n' \
  "${archive_sha256}" "${key}"
