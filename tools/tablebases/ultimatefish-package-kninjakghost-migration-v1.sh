#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish-penguin/info-remap-fix-v2/kninjakghost-fresh-v1
stage=/mnt/ultimatefish/kninjakghost-migration-v1
inventory=${root}/work/migration-v1-inventory.sha256
archive=${stage}/kninjakghost-fresh-v1.tar.zst

test "$(sha256sum "${root}/work/transitions-ready.json" | cut -d ' ' -f 1)" = \
  c72efdbaece2f4ebf3fb752e654188f02208067abcc7095f1d5ae342691a9e7e
test "$(sha256sum "${root}/work/transitions/kninjakghost.verified" | cut -d ' ' -f 1)" = \
  d4568768b00a159bdd69b8cc26c3cf13777f8fb0efb98efef72ac7be4aabfb56
test "$(sha256sum "${root}/ultimate_ghost_ordinary_information_tablebase" | cut -d ' ' -f 1)" = \
  501f73cbf224e7ada0fe0eda101986a2aacf7ad0f2d3645f09743dd310e509d0
test "$(sha256sum "${root}/tablebases/kninjakghost.uftb" | cut -d ' ' -f 1)" = \
  4c3161b6b46cfbb5ab8b824f56d262a58cfe80ec10bf795b39664522ce9c3027
test "$(sha256sum "${root}/tablebases/kninjak.uftb" | cut -d ' ' -f 1)" = \
  4552403a54317cfdb3fee192ea4e7e80d33769ef956ee5b0c8f82548efb87776
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
test ! -e "${inventory}"
test ! -e "${stage}"
test "$(df --output=avail -B1 /mnt/ultimatefish | tail -1)" -ge 32212254720

mkdir -p "${stage}"
cd "${root}"
find . -type f ! -path './work/migration-v1-inventory.sha256' -print0 \
  | sort -z | xargs -0 sha256sum >"${inventory}"
test "$(wc -l <"${inventory}")" -ge 400
sha256sum -c "${inventory}" >/dev/null

tar --zstd -cf "${archive}" \
  -C /mnt/ultimatefish-penguin/info-remap-fix-v2 \
  kninjakghost-fresh-v1

sha256sum "${inventory}" "${archive}"
stat -c '%s %n' "${inventory}" "${archive}"
echo 'ninja_ghost_migration_v1 complete 1'
