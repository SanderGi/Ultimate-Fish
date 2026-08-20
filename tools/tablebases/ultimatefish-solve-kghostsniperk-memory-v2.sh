#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-substate-corrected-v1/kghostsniperk-v2
binary=${root}/ultimate_ghost_ordinary_information_tablebase
scratch=work/solve/kghostsniperk-memory-v2
output=work/results/kghostsniperk-memory-v2.ufiw
arbitrary=work/results/kghostsniperk-memory-v2.ufgd

cd "${root}"
test ! -e "${scratch}"
test ! -e "${output}"
test ! -e "${arbitrary}"
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = 52d70a4e2e2d7b18e63ae01a158f3f56301158055398764462f8d5a527219db5
test "$(sha256sum work/transitions-ready.json | cut -d ' ' -f 1)" = deb4a7237d10cd06f5529270c6ab6567de8061f3100a3dc5f15797c174f6bd1e
test "$(sha256sum tablebases/kghostsniperk.uftb | cut -d ' ' -f 1)" = b627e286865f3081805a8a81ab8ec4ec8938c8a8486f3df8a0ae6a3edc4a12f1
test "$(sha256sum tablebases/ksniperk.uftb | cut -d ' ' -f 1)" = 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
test "$(sha256sum tablebases/kghostk.ufgm | cut -d ' ' -f 1)" = 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 322122547200

exec "${binary}" \
  --solve \
  --orientation same \
  --transition-prefix work/transitions/kghostsniperk \
  --input tablebases/kghostsniperk.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${output}" \
  --output-arbitrary "${arbitrary}" \
  --lower-dragon-table tablebases/ksniperk.uftb \
  --lower-dragon-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5 \
  --lower-dragon-source-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5 \
  --lower-dragon-model-sha256 7eaa2c6b443d15a98f935cf7b9fec3e6991a841246a2af8290afadaf1b3feab1 \
  --source-sha256 b627e286865f3081805a8a81ab8ec4ec8938c8a8486f3df8a0ae6a3edc4a12f1 \
  --model-sha256 37ce97c6639e74eb2d8caf9843abed7bee8a3cb3937ad5383e582046c2183b0d \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1000000000 \
  --unique-slots 1073741824 \
  --compact-every 1
