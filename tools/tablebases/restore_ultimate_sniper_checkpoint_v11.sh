#!/usr/bin/env bash
set -euo pipefail

readonly bucket="${1:?bucket}"
readonly destination="${2:-/mnt/ultimatefish/sniper-ghost-same-restore-v11}"
readonly archive_sha256=2261b5bed83f0ead28e78fb68bdb4cf65d0fbbfadd89b15d465f29b5874faffc
readonly archive_version=F9Yi0d6S4bEyr9pzHYANdZ7sDXoqNxcd
readonly archive_key="results/checkpoints/sniper-ghost/same/migration-v10/sha256/${archive_sha256}/sniper-ghost-same-migration-v10.tar.zst"
readonly archive="${destination}/sniper-ghost-same-migration-v10.tar.zst"

test ! -e "${destination}/restore-complete"
mkdir -p "${destination}"
aws s3api get-object \
  --bucket "${bucket}" \
  --key "${archive_key}" \
  --version-id "${archive_version}" \
  "${archive}" >/dev/null
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = "${archive_sha256}"
zstd -dc "${archive}" | tar --sparse -xf - -C "${destination}"

test -s "${destination}/info-substate-corrected-v1/kghostsniperk-v2/work/solve-compositional-v3/kghostsniperk.bdd-b.nodes"
test -s "${destination}/info-substate-corrected-v1/kghostsniperk-v2/work/solve-compositional-v3/kghostsniperk.owner-next"
printf '%s\n%s\n' "${archive_sha256}" "${archive_version}" >"${destination}/restore-complete"
printf 'sniper_checkpoint_restore_complete sha256 %s version %s\n' \
  "${archive_sha256}" "${archive_version}"
