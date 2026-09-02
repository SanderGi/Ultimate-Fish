#!/usr/bin/env bash
set -euo pipefail

readonly bucket="${1:?bucket}"
readonly source_root="${2:-/mnt/ultimatefish}"
readonly archive="${source_root}/ghost-bomb-solve-v8-migration-v9.tar.zst"
readonly marker="${source_root}/ghost-bomb-solve-v8-migration-v9.uploaded"

tar --sparse -C "${source_root}" -cf - \
  ghost-bomb-solve-v8 \
  bomb-solve-v8-source \
  | zstd -T8 -3 -f -o "${archive}"

archive_sha256="$(sha256sum "${archive}" | awk '{print $1}')"
readonly archive_sha256
readonly key="results/checkpoints/bomb-ghost/opposed/migration-v9/sha256/${archive_sha256}/ghost-bomb-solve-v8-migration-v9.tar.zst"

aws s3 cp "${archive}" "s3://${bucket}/${key}" \
  --only-show-errors \
  --metadata "sha256=${archive_sha256}"

printf '%s  %s\n%s\n' "${archive_sha256}" "${archive}" "${key}" >"${marker}"
printf 'bomb_checkpoint_upload_complete sha256 %s key %s bytes %s\n' \
  "${archive_sha256}" "${key}" "$(stat -c %s "${archive}")"
