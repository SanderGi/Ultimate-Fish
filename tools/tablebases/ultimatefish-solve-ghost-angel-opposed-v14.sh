#!/usr/bin/env bash
# Solve the authenticated opposing Angel/Ghost graph completed by v13.
set -euo pipefail

root=/mnt/ultimatefish/ghost-angel-v8-07721491
source_root=$root/source
work=$root/work-opposing
runner=$source_root/tools/tablebases/run_ultimate_ghost_ordinary_aws.py
binary=$work/ultimate_ghost_ordinary_information_tablebase
input=/mnt/ultimatefish/ghost-angel-v6-7425643c/inputs/kghostkangel.uftb
lower_ghost=/mnt/ultimatefish/info-substate-corrected-v1/kberserkerghostk-v2-resume-v5/tablebases/kghostk.ufgm

test "$(sha256sum "$runner" | cut -d' ' -f1)" = \
  431099a8715d42a56b421fefeb3a37fccb9d6071838ad148563c23a681048372
test "$(sha256sum "$work/work/transitions-ready.json" | cut -d' ' -f1)" = \
  2608aa8889cac859aeafcd3bfbb302cb30df864c5a4380a9e5ed1ba02f076a8b
test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  6338ba416f1e271f66908b9dd17b1ab94e7bc7d1f4dff68b99ecdc7e2cbcd54b
test "$(sha256sum "$input" | cut -d' ' -f1)" = \
  ad4be1cce6d7d5911fb91cf2bf5c3274d5eafcde23240a22c5b3dfdfa2507d9c
test "$(sha256sum "$lower_ghost" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test ! -e "$work/work/results/kghostkangel.ufiw"
test ! -e "$work/work/results/kghostkangel.ufgd"
test ! -e "$work/work/logs/solve.log"
test -z "$(find "$work/work/solve" -mindepth 1 -print -quit)"
test "$(df --output=avail -B1 /mnt/ultimatefish | tail -1)" -ge 85899345920

exec /usr/bin/python3 "$runner" \
  --source-root "$source_root" \
  --work "$work" \
  --filename kghostkangel.uftb \
  --piece angel \
  --orientation opposing \
  --source-table "$input" \
  --source-sha256 ad4be1cce6d7d5911fb91cf2bf5c3274d5eafcde23240a22c5b3dfdfa2507d9c \
  --lower-sha256 25a2afa32399de9c487352f23a7f7e84673ca301d75fa4f927d037b3bd8852ee \
  --lower-model-sha256 323829264a9c977ccb01a674c2a068bdc29c87d7232fbeb5feb839605264ecfe \
  --lower-ghost-sidecar "$lower_ghost" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --model-sha256 08091f1f58ce0abd3025fa9ca0a42d5ca0046006b86d234b89cc7749a09b8a3c \
  --observation-sha256 6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b \
  --solve-max-nodes 500000000 \
  --solve-existing
