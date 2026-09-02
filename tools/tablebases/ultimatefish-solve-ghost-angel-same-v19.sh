#!/usr/bin/env bash
# Solve the authenticated same-team Angel/Ghost graph completed by v15.
set -euo pipefail

readonly root=/mnt/ultimatefish/ghost-angel-v9-159eea06
readonly source_root=${root}/source
readonly work=${root}/work-same
readonly runner=${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
readonly binary=${work}/ultimate_ghost_ordinary_information_tablebase
readonly input=/mnt/ultimatefish/ghost-angel-v6-7425643c/inputs/kghostangelk.uftb
readonly lower_ghost=/mnt/ultimatefish/info-substate-corrected-v1/kberserkerghostk-v2-resume-v5/tablebases/kghostk.ufgm

test "$(sha256sum "${root}/source.tar" | cut -d' ' -f1)" = \
  159eea065bd196755b0142966d1872eaf6265888900be4e28579b03788e7df7b
test "$(sha256sum "${runner}" | cut -d' ' -f1)" = \
  431099a8715d42a56b421fefeb3a37fccb9d6071838ad148563c23a681048372
test "$(sha256sum "${work}/work/transitions-ready.json" | cut -d' ' -f1)" = \
  2aac70e770a0321f78183404f0e9baad3e140f4fcd74662b78023c378b8d6b64
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = \
  d5cc7e98115c5f96da4f6508b145790b4796c3c1217ac7cdab4df95f40d91e78
test "$(sha256sum "${input}" | cut -d' ' -f1)" = \
  d8422f3f62dc92e2c636cdbb0946ea7d8f9fe7c05102a039b9071190f3c55630
test "$(sha256sum "${lower_ghost}" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test ! -e "${work}/work/results/kghostangelk.ufiw"
test ! -e "${work}/work/results/kghostangelk.ufgd"
test ! -e "${work}/work/logs/solve.log"
test -z "$(find "${work}/work/solve" -mindepth 1 -print -quit)"
test "$(df --output=avail -B1 /mnt/ultimatefish | tail -1)" -ge 85899345920

exec /usr/bin/python3 "${runner}" \
  --source-root "${source_root}" \
  --work "${work}" \
  --filename kghostangelk.uftb \
  --piece angel \
  --orientation same \
  --source-table "${input}" \
  --source-sha256 d8422f3f62dc92e2c636cdbb0946ea7d8f9fe7c05102a039b9071190f3c55630 \
  --lower-sha256 25a2afa32399de9c487352f23a7f7e84673ca301d75fa4f927d037b3bd8852ee \
  --lower-model-sha256 323829264a9c977ccb01a674c2a068bdc29c87d7232fbeb5feb839605264ecfe \
  --lower-ghost-sidecar "${lower_ghost}" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --model-sha256 f6ab383679094d485cbcc780f3588f603d393927825fdb4b4b78415036f7de49 \
  --observation-sha256 6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b \
  --prebuilt-executable "${binary}" \
  --prebuilt-executable-sha256 d5cc7e98115c5f96da4f6508b145790b4796c3c1217ac7cdab4df95f40d91e78 \
  --prebuilt-model-sha256 f6ab383679094d485cbcc780f3588f603d393927825fdb4b4b78415036f7de49 \
  --solve-max-nodes 500000000 \
  --solve-existing
