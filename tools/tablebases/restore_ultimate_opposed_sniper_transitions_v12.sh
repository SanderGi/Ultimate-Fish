#!/usr/bin/env bash
set -euo pipefail

readonly bucket="${1:?bucket}"
readonly destination="${2:-/mnt/ultimatefish/opposed-sniper-v12}"
readonly archive_sha256=70ccd4b4b757afc5b6ecf951fab137f4eae6558d40ecb2252afbac8053c64692
readonly archive_version=BDet.ZYR8kAAi_wGG8QDrI32vAhGElus
readonly archive_key="results/checkpoints/sniper-ghost/opposed/transitions-v12/sha256/${archive_sha256}/opposed-sniper-transitions-v12.tar.zst"
readonly archive="${destination}/opposed-sniper-transitions-v12.tar.zst"

test ! -e "${destination}/restore-complete"
mkdir -p "${destination}"
aws s3api get-object \
  --bucket "${bucket}" \
  --key "${archive_key}" \
  --version-id "${archive_version}" \
  "${archive}" >/dev/null
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = "${archive_sha256}"
zstd -dc "${archive}" | tar --sparse -xf - -C "${destination}"

test "$(sha256sum "${destination}/work/transitions/kghostksniper.verified" | cut -d ' ' -f 1)" = \
  d93fca5ede82c6e7a09f695ff80da2e9e2e446068bf1c5fbfdad97d6b27bbbc5
printf '%s\n%s\n' "${archive_sha256}" "${archive_version}" >"${destination}/restore-complete"
printf 'opposed_sniper_transition_restore_complete sha256 %s version %s\n' \
  "${archive_sha256}" "${archive_version}"
