#!/usr/bin/env bash
# Resume opposed Prince/Ghost from the authenticated iteration-8 compaction.
set -euo pipefail

readonly root=/mnt/ultimatefish/ghost-parallel-v12-c1013b4d
readonly binary=${root}/ultimate_ghost_ordinary_information_tablebase-prince-v12
readonly work=/mnt/ultimatefish/ghost-parallel-v6/kghostkprince-canary-striped-bdd-b-8w
readonly scratch=${work}/solve/kghostkprince
readonly transition=${work}/transitions/kghostkprince
readonly inputs=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostkprince-solve-2b-v7

test "$(sha256sum "${root}/source.tar" | cut -d ' ' -f1)" = \
  c1013b4dda5fb0f8b89c3936c2f880efa4184d726acda78381f043480e873e3f
test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  0c480b0e340abef9f19bcf7eab2cae46fca420ae1d3ae0ab1cff030006b1452c
test "$(cat "${scratch}.checkpoint8-authenticated")" = \
  'iteration=8 bdd_slot=a current_slot=current structural_residual=0 root_residual=0'
test -s "${scratch}.bdd-a.nodes"
test -s "${scratch}.owner-current"
test ! -e "${work}/results/kghostkprince.ufiw"
test ! -e "${work}/results/kghostkprince.ufgd"

cd "${inputs}"
exec "${binary}" \
  --solve --orientation opposing \
  --transition-prefix "${transition}" \
  --input tablebases/kghostkprince.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${work}/results/kghostkprince.ufiw" \
  --output-arbitrary "${work}/results/kghostkprince.ufgd" \
  --lower-dragon-table tablebases/kprincek.uftb \
  --lower-dragon-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-dragon-source-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-dragon-model-sha256 5511ac5ac166fd2be042e66a4c849e034fef39da317988f40bf4e6b47e071927 \
  --source-sha256 2858daf3770e0a47230f2d0bf410cd8ce4a9820b0c7cd0895ceab242ae42f65f \
  --model-sha256 a9c480c453ddeb72085a43483dc60484ec24ca017fd365ed60a7c8f3973d8834 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 2000000000 --unique-slots 2147483648 --compact-every 1 \
  --resume-fixed-point --resume-iteration 8 \
  --resume-current-slot current --resume-bdd-slot a --workers 32
