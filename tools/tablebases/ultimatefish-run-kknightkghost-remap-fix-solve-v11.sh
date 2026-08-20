#!/bin/bash
set -euo pipefail

source=/mnt/ultimatefish/remap-fix-v1-source
work=/mnt/ultimatefish-penguin/info-remap-fix-v1/kknightkghost-fresh-v8
runner="${source}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py"
executable="${work}/ultimate_ghost_ordinary_information_tablebase"
ready="${work}/work/transitions-ready.json"

test -d "${work}"
test ! -e "${work}/work/results/kknightkghost.ufiw"
test ! -e "${work}/work/results/kknightkghost.ufgd"
test "$(sha256sum "${source}/src/ultimate/tablebases/ghost_dragon_information_solver.cpp" | cut -d ' ' -f 1)" = ca679a7817bf03160f9fe9c53335aea6e82b5b653a0501b850b5168d45705900
test "$(sha256sum "${runner}" | cut -d ' ' -f 1)" = ee805e9dc7f9c24c36b9dbd8cab172eed5dc9af574e75fc3df69b8acb1e29d44
test "$(sha256sum "${ready}" | cut -d ' ' -f 1)" = 32c81f2b49a53efdc804260e852713b467fe4f6f4b2e0407d33f811b673819b2
test "$(sha256sum "${executable}" | cut -d ' ' -f 1)" = 99f700f88cbfc1b28e887516a905f0f15f18597f041befc317e02bfe2015a224

exec /usr/bin/python3 "${runner}" \
  --source-root "${source}" \
  --work "${work}" \
  --filename kknightkghost.uftb \
  --piece knight \
  --orientation opposing \
  --source-table /mnt/ultimatefish/info-wave-aa82210e-stage/kknightkghost/kknightkghost.uftb \
  --source-sha256 a17f58aa7908646af328963ae8023072f5b0f05e61cef8d4d4a6c502d2684250 \
  --lower-sha256 807693ac2c310cbe04547d4d8244794a7c5aa72e664f8cb16e74134f34d7118c \
  --lower-model-sha256 c10b2b1f474f1fdb6ac540c54c5fa359bf5f834a1714bf12e22accab84c117c1 \
  --lower-ghost-sidecar /mnt/ultimatefish/info-wave-aa82210e-stage/kghostk.ufgm \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --model-sha256 be268fc43e27fc69c39af5e4e98dc6a9cb40e2fbbb491af8b9e3bd7435659049 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --solve-max-nodes 500000000 \
  --solve-existing
