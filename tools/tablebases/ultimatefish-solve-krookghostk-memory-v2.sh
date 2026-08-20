#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-wave-aa82210e-work/krookghostk
binary=${root}/ultimate_ghost_ordinary_information_tablebase
scratch=work/solve/krookghostk-memory-v2
output=work/results/krookghostk-memory-v2.ufiw
arbitrary=work/results/krookghostk-memory-v2.ufgd

cd "${root}"
test ! -e "${scratch}"
test ! -e "${output}"
test ! -e "${arbitrary}"
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = 22f530af6abc14868657a110a078318e5ae9f29059147583c583c576c7ab285c
test "$(sha256sum work/transitions-ready.json | cut -d ' ' -f 1)" = d7f3e24ce0f2d7a4a6a83e78a8e52f421ef6cf1ca6533e8c90314d8964948ee1
test "$(sha256sum tablebases/krookghostk.uftb | cut -d ' ' -f 1)" = 0413b69806a348b158540d20c2aa13ec96cd51c49c7835f2651241f87d63a68f
test "$(sha256sum tablebases/krookk.uftb | cut -d ' ' -f 1)" = abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
test "$(sha256sum tablebases/kghostk.ufgm | cut -d ' ' -f 1)" = 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 214748364800

exec "${binary}" \
  --solve \
  --orientation same \
  --transition-prefix work/transitions/krookghostk \
  --input tablebases/krookghostk.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${output}" \
  --output-arbitrary "${arbitrary}" \
  --lower-dragon-table tablebases/krookk.uftb \
  --lower-dragon-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
  --lower-dragon-source-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
  --lower-dragon-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1 \
  --source-sha256 0413b69806a348b158540d20c2aa13ec96cd51c49c7835f2651241f87d63a68f \
  --model-sha256 0b1c03c90ff851d3c30fedb30c99430c52717e6374ece92aac48ada83e4eceae \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1000000000 \
  --unique-slots 1073741824 \
  --compact-every 1
