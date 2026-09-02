#!/bin/bash
set -euo pipefail

unit=ultimatefish-devil-companion-v27-same-bishop-square-2.service
device=/dev/nvme4n1
device_serial=AWS851607FA46CEBE041
mountpoint=/mnt/ultimatefish-devil-fast
source_root=/mnt/ultimatefish-devil-v11/devil-companion-v26-retained/same-bishop/square-2
destination_root=${mountpoint}/devil-companion-v30/same-bishop/square-2
stage=${mountpoint}/.c1-gp3-to-nvme-v30.tmp

test -b "${device}"
test "$(lsblk -dn -o SERIAL "${device}" | xargs)" = "${device_serial}"
test "$(lsblk -bdn -o SIZE "${device}" | xargs)" -eq 1900000000000
test "$(lsblk -dn -o MODEL "${device}" | xargs)" = "Amazon EC2 NVMe Instance Storage"
test -z "$(lsblk -n -o FSTYPE "${device}")"
test -z "$(lsblk -n -o MOUNTPOINTS "${device}")"
test "$(findmnt -n -o SOURCE --target /mnt/ultimatefish-devil-v11)" != "${device}"
test "$(stat -c %s "${source_root}/devil-2.closure")" -eq 44
test "$(stat -c %s "${source_root}/devil-2.frontier")" -eq 84563509128
test "$(stat -c %s "${source_root}/devil-2.keys")" -eq 280000000000

systemctl stop "${unit}" 2>/dev/null || true
test "$(systemctl is-active "${unit}" || true)" = inactive

mkfs.xfs -f -L ufish-c1fast "${device}"
install -d -m 0700 "${mountpoint}"
mount -o noatime,nodiratime "${device}" "${mountpoint}"
test "$(findmnt -n -o SOURCE --target "${mountpoint}")" = "${device}"
test "$(df -B1 --output=avail "${mountpoint}" | tail -1)" -gt 1200000000000

install -d -m 0700 "${stage}"
for name in devil-2.closure devil-2.frontier devil-2.keys \
            devil-2.gp3-copy-v26.receipt; do
  cp --reflink=never --sparse=always "${source_root}/${name}" "${stage}/${name}"
done
sync -f "${stage}"

(cd "${source_root}" && sha256sum devil-2.closure devil-2.frontier \
  devil-2.keys devil-2.gp3-copy-v26.receipt) >"${stage}/source.manifest"
(cd "${stage}" && sha256sum devil-2.closure devil-2.frontier \
  devil-2.keys devil-2.gp3-copy-v26.receipt) >"${stage}/destination.manifest"
cmp "${stage}/source.manifest" "${stage}/destination.manifest"

manifest_sha=$(sha256sum "${stage}/destination.manifest" | cut -d ' ' -f 1)
source_receipt_sha=$(sha256sum \
  "${source_root}/devil-2.gp3-copy-v26.receipt" | cut -d ' ' -f 1)
{
  echo schema=ultimate-devil-c1-gp3-to-local-nvme-v1
  echo source=${source_root}
  echo destination=${destination_root}
  echo device=${device}
  echo filesystem=xfs
  echo source_preserved=1
  echo checkpoint_manifest_sha256=${manifest_sha}
  echo source_receipt_sha256=${source_receipt_sha}
  echo closure_bytes=44
  echo frontier_bytes=84563509128
  echo keys_bytes=280000000000
} >"${stage}/c1-gp3-to-nvme-v30.receipt"
sync -f "${stage}/c1-gp3-to-nvme-v30.receipt"

install -d -m 0700 "$(dirname "${destination_root}")"
mv "${stage}" "${destination_root}"
sync -f "$(dirname "${destination_root}")"
echo C1_NVME_MIGRATION_OK \
  manifest_sha256=${manifest_sha} \
  receipt_sha256=$(sha256sum \
    "${destination_root}/c1-gp3-to-nvme-v30.receipt" | cut -d ' ' -f 1)
