#!/bin/bash
set -euo pipefail

source_root=/mnt/ultimatefish/ghost-extra-remap-fix-v1-source
work=/mnt/ultimatefish-overflow/info-remap-fix-v1/kberserkerkghost-fresh-v5
runner=${work}/staged/run_ultimate_ghost_ordinary_aws.py
executable=${work}/ultimate_ghost_ordinary_information_tablebase
ready=${work}/work/transitions-ready.json
lower_sidecar=/mnt/ultimatefish/info-wave-aa82210e-stage/kghostk.ufgm

test -d "${work}/work/transitions"
test ! -e "${work}/work/results/kberserkerkghost.ufiw"
test ! -e "${work}/work/results/kberserkerkghost.ufgd"
test "$(sha256sum "${runner}" | cut -d ' ' -f 1)" = \
  4c66548b6c6e6c0efcc7cc4a0e36c66d44294a431cc5eba3bc0594fcc97446ba
test "$(sha256sum "${ready}" | cut -d ' ' -f 1)" = \
  58cf814a568ff1c92f16835c472e4d5dc3f1a255e460f52aac30797e25933e7d
test "$(sha256sum "${executable}" | cut -d ' ' -f 1)" = \
  7526e6eefd192a43b755ca687e0e924daf863a8e797cbde8c276fd8a2b957a1c
test "$(sha256sum "${lower_sidecar}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

export PYTHONPATH=${source_root}/tools/tablebases
exec /usr/bin/python3 "${runner}" \
  --source-root "${source_root}" \
  --work "${work}" \
  --filename kberserkerkghost.uftb \
  --piece berserker \
  --orientation opposing \
  --source-table "${work}/tablebases/kberserkerkghost.uftb" \
  --source-sha256 d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55 \
  --prebuilt-executable "${executable}" \
  --prebuilt-executable-sha256 7526e6eefd192a43b755ca687e0e924daf863a8e797cbde8c276fd8a2b957a1c \
  --prebuilt-model-sha256 4ba38541f75205f35986831577e168f66173a488e447348a87f33d3e2c531ffd \
  --lower-table "${work}/tablebases/kberserkerk.uftb" \
  --lower-sha256 f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1 \
  --lower-model-sha256 f2dfd61cb3c955e66ae8a48907466d30bdb0169cd1f64044b8f8f62645dccba4 \
  --lower-ghost-sidecar "${lower_sidecar}" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --model-sha256 4ba38541f75205f35986831577e168f66173a488e447348a87f33d3e2c531ffd \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --solve-max-nodes 500000000 \
  --solve-workers 1 \
  --solve-existing
