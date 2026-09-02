#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly restore=/mnt/ultimatefish/berserker-ghost-opposed-restore-v12
readonly archive=${restore}/berserker-ghost-opposed-migration-v12.tar.zst
readonly archive_sha=10709917e47875e13c3e8f873df0f9fff9c507a1546f692ff4d552fc193930ea
readonly wrapper=${restore}/rebind_ultimate_opposed_berserker_v13.sh
readonly wrapper_sha=91e3e0fac1232f17f21ff3707e0619a8372a17ddddb7f547e8fa3ff2422e8a16
readonly wrapper_key=sources/tools/opposed-berserker-rebind-v14/sha256/${wrapper_sha}/rebind_ultimate_opposed_berserker_v13.sh
readonly wrapper_version=FFCxobpBZ_bmtvpjEY7w0pvAKWYbNH.1
readonly marker=${restore}/info-remap-fix-v1/kberserkerkghost-fresh-v5/work/transitions/kberserkerkghost.verified

test "$(systemctl show ultimatefish-rebind-berserker-opposed-v13.service -p ActiveState --value)" = inactive
test "$(sha256sum "${archive}" | cut -d ' ' -f 1)" = "${archive_sha}"
tar --zstd --sparse -xf "${archive}" -C "${restore}"
test "$(sha256sum "${marker}" | cut -d ' ' -f 1)" = \
  823ad1a14af45def9a1e41a3eec0040ec5d8797b51ba219c9505783c35ee90e0
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${wrapper_key}" --version-id "${wrapper_version}" \
  "${wrapper}.tmp" >/dev/null
test "$(sha256sum "${wrapper}.tmp" | cut -d ' ' -f 1)" = "${wrapper_sha}"
install -m 0755 "${wrapper}.tmp" "${wrapper}"
rm -f "${wrapper}.tmp"
systemd-run --unit ultimatefish-rebind-berserker-opposed-v14 \
  --property=AllowedCPUs=32-47 --property=MemoryMax=220G "${wrapper}"
echo 'berserker_ghost_opposed_restore_restart_v14 complete 1 residual 0'
