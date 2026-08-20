#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostprincek
binary=${root}/ultimate_ghost_ordinary_information_tablebase
scratch=work/solve/kghostprincek-memory-v2
output=work/results/kghostprincek-memory-v2.ufiw
arbitrary=work/results/kghostprincek-memory-v2.ufgd

cd "${root}"
test ! -e "${scratch}"
test ! -e "${output}"
test ! -e "${arbitrary}"
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = 62147b92ddc626ee867436325bfc76b204b8ab8e0d4821d864672b9ffe420f55
test "$(sha256sum work/transitions-ready.json | cut -d ' ' -f 1)" = b59a4155f64c5f950f014d41bdaf43f5390bf0485f75917fb35e26d0519369ac
test "$(sha256sum tablebases/kghostprincek.uftb | cut -d ' ' -f 1)" = bd77080a64813a7ba4ef9bfeedde322409295944012417e2a7376689cf1cf9b8
test "$(sha256sum tablebases/kprincek.uftb | cut -d ' ' -f 1)" = 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0
test "$(sha256sum tablebases/kghostk.ufgm | cut -d ' ' -f 1)" = 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 322122547200

exec "${binary}" \
  --solve \
  --orientation same \
  --transition-prefix work/transitions/kghostprincek \
  --input tablebases/kghostprincek.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${output}" \
  --output-arbitrary "${arbitrary}" \
  --lower-dragon-table tablebases/kprincek.uftb \
  --lower-dragon-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-dragon-source-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-dragon-model-sha256 5511ac5ac166fd2be042e66a4c849e034fef39da317988f40bf4e6b47e071927 \
  --source-sha256 bd77080a64813a7ba4ef9bfeedde322409295944012417e2a7376689cf1cf9b8 \
  --model-sha256 3fc5612e277bfe1469999746d5c97a573252dd0785237a3cf3b383093c496086 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1000000000 \
  --unique-slots 1073741824 \
  --compact-every 1
