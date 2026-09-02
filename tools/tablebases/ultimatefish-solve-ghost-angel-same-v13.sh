#!/usr/bin/env bash
# Solve the authenticated same-team Angel/Ghost graph completed by v12.
set -euo pipefail

root=/mnt/ultimatefish/ghost-angel-v7-c8556f58
source_root=$root/source
work=$root/work-same
runner=$source_root/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
binary=$work/ultimate_ghost_ordinary_information_tablebase
input=/mnt/ultimatefish/ghost-angel-v6-7425643c/inputs/kghostangelk.uftb
lower_ghost=/mnt/ultimatefish/info-substate-corrected-v1/kberserkerghostk-v2-resume-v5/tablebases/kghostk.ufgm

test "$(sha256sum "$runner" | cut -d' ' -f1)" = \
  06de613a21394ae6fc85a42e58bf02438a0317f3d8525fc7732854d877440e72
test "$(sha256sum "$work/work/transitions-ready.json" | cut -d' ' -f1)" = \
  8a5c79da1531ff29dfbfb3d4c10f52976834b9377892a75c9e368ff75fb5284b
test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  c99c8e596e13c90c79801ddfc723712c2a3fc7112eed961bc42ff958cb54a1ef
test "$(sha256sum "$input" | cut -d' ' -f1)" = \
  d8422f3f62dc92e2c636cdbb0946ea7d8f9fe7c05102a039b9071190f3c55630
test "$(sha256sum "$lower_ghost" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test ! -e "$work/work/results/kghostangelk.ufiw"
test ! -e "$work/work/results/kghostangelk.ufgd"
test ! -e "$work/work/logs/solve.log"
test -z "$(find "$work/work/solve" -mindepth 1 -print -quit)"
test "$(df --output=avail -B1 /mnt/ultimatefish | tail -1)" -ge 85899345920

exec /usr/bin/python3 "$runner" \
  --source-root "$source_root" \
  --work "$work" \
  --filename kghostangelk.uftb \
  --piece angel \
  --orientation same \
  --source-table "$input" \
  --source-sha256 d8422f3f62dc92e2c636cdbb0946ea7d8f9fe7c05102a039b9071190f3c55630 \
  --lower-sha256 25a2afa32399de9c487352f23a7f7e84673ca301d75fa4f927d037b3bd8852ee \
  --lower-model-sha256 323829264a9c977ccb01a674c2a068bdc29c87d7232fbeb5feb839605264ecfe \
  --lower-ghost-sidecar "$lower_ghost" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --model-sha256 f8b14270ce3b6c58c4e3393550186d6a68684d512d40f5f4083e3b09a2fb98d0 \
  --observation-sha256 6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b \
  --solve-max-nodes 500000000 \
  --solve-existing
