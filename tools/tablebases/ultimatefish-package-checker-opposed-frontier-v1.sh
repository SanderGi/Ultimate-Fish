#!/bin/bash
set -euo pipefail

source=/mnt/ultimatefish/current-wave0-009-checker-opposed-48g-v4
target=/mnt/ultimatefish-resume/checker-opposed-frontier-1d3850ef-v1.tar.zst

test ! -e "${target}"
test ! -e "${target}.sha256"
test "$(sha256sum "${source}/resume-local-manifest.json" | cut -d ' ' -f 1)" = 1d3850efa3d267c459cb93bc36a2bc9bdcc89e406d80e02cdf5d9d95d4e7c45b
test "$(sha256sum "${source}/scratch/kberserkerkchecker.nodes" | cut -d ' ' -f 1)" = 64186c6511d1d8be7b37f9dd74104ef067bc61b0e46cac1ffdc932f41beff226
test "$(sha256sum "${source}/scratch/kberserkerkchecker.degrees" | cut -d ' ' -f 1)" = 4e06ca949814f208e1dac5aafa1d3672331f79bfce6773e6dc06abea6621f84b
test "$(df --output=avail -B1 /mnt/ultimatefish-resume | tail -1)" -ge 214748364800

tar --sparse --sort=name --mtime='UTC 1970-01-01' \
  --owner=0 --group=0 --numeric-owner \
  -C /mnt/ultimatefish -cf - current-wave0-009-checker-opposed-48g-v4 \
  | zstd -5 -T11 -o "${target}"
sha256sum "${target}" > "${target}.sha256"
zstd -t "${target}"
stat --printf='archive_bytes %s\n' "${target}"
