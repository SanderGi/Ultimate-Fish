#!/usr/bin/env bash
set -euo pipefail

readonly bucket="${1:?bucket}"
readonly mount=/mnt/ultimatefish
readonly archive=${mount}/ghost-bomb-solve-v8-migration-v9.tar.zst
readonly archive_key=results/checkpoints/bomb-ghost/opposed/migration-v9/sha256/918b1122428a29b537450bb37c669ea3f9651bacdeb89733d2822b777689b1f1/ghost-bomb-solve-v8-migration-v9.tar.zst
readonly archive_version=.j1Dy1Ydu8FfX2ma7LJd_SDjldO24PB7
readonly source_dir=${mount}/bomb-solve-v9-source
readonly wrapper=${source_dir}/resume_ultimate_bomb_checkpoint_v9.sh
readonly unit=${source_dir}/ultimatefish-info-kbombkghost-resume-v9.service

test ! -e "${mount}/ghost-bomb-solve-v8"
test ! -e "${mount}/bomb-solve-v8-source"
test "$(df --output=avail -B1 "${mount}" | tail -1)" -ge 241591910400
mkdir -p "${source_dir}"

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${archive_key}" --version-id "${archive_version}" "${archive}" >/dev/null
test "$(stat -c %s "${archive}")" = 19157903097
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = \
  918b1122428a29b537450bb37c669ea3f9651bacdeb89733d2822b777689b1f1
tar --zstd -xf "${archive}" -C "${mount}"

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key sources/tools/bomb-resume-v9/sha256/fc0677c0991b3641c6fa7490bdafcc51c313730ea5b6cc715f585de6f01d8c3e/resume_ultimate_bomb_checkpoint_v9.sh \
  --version-id QbH0ottPrkoKknsImPlbFzqf8qx4a3t9 "${wrapper}" >/dev/null
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key sources/systemd/bomb-resume-v9/sha256/c8e46b17b47a56600e198adfd22537465c7210f9f68c885d8b411d15ac9cd313/ultimatefish-info-kbombkghost-resume-v9.service \
  --version-id dpxMGr3nb21jVGZNb7OY6T3lfbJFZdpP "${unit}" >/dev/null
test "$(sha256sum "${wrapper}" | cut -d ' ' -f 1)" = \
  fc0677c0991b3641c6fa7490bdafcc51c313730ea5b6cc715f585de6f01d8c3e
test "$(sha256sum "${unit}" | cut -d ' ' -f 1)" = \
  c8e46b17b47a56600e198adfd22537465c7210f9f68c885d8b411d15ac9cd313
chmod 0755 "${wrapper}"
install -m 0644 "${unit}" /etc/systemd/system/ultimatefish-info-kbombkghost-resume-v9.service
systemctl daemon-reload
systemctl start ultimatefish-info-kbombkghost-resume-v9.service
systemctl is-active ultimatefish-info-kbombkghost-resume-v9.service
printf 'bomb_checkpoint_restore_started archive_sha256 %s archive_version %s\n' \
  918b1122428a29b537450bb37c669ea3f9651bacdeb89733d2822b777689b1f1 \
  "${archive_version}"
