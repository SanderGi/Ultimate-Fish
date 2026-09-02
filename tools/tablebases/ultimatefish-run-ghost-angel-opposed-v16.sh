#!/usr/bin/env bash
# Rebuild opposed Angel/Ghost with Angel-protected adjacent-King children admitted.
set -euo pipefail

root=/mnt/ultimatefish/ghost-angel-v9-159eea06
source_root=$root/source
binary=$root/ultimate_ghost_angel_opposed_v9
input=/mnt/ultimatefish/ghost-angel-v6-7425643c/inputs/kghostkangel.uftb

test "$(sha256sum "$root/source.tar" | cut -d' ' -f1)" = \
  159eea065bd196755b0142966d1872eaf6265888900be4e28579b03788e7df7b
test "$(sha256sum "$binary" | cut -d' ' -f1)" = \
  e12de6a1a2ad7b1faa2d7380c5be640e32d868ffc5ebd8250449fb9d1222e1a9
test "$(sha256sum "$input" | cut -d' ' -f1)" = \
  ad4be1cce6d7d5911fb91cf2bf5c3274d5eafcde23240a22c5b3dfdfa2507d9c
test ! -e "$root/work-opposing"

exec /usr/bin/python3 \
  "$source_root/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "$source_root" \
  --work "$root/work-opposing" \
  --filename kghostkangel.uftb \
  --piece angel \
  --orientation opposing \
  --source-table "$input" \
  --source-sha256 ad4be1cce6d7d5911fb91cf2bf5c3274d5eafcde23240a22c5b3dfdfa2507d9c \
  --lower-sha256 25a2afa32399de9c487352f23a7f7e84673ca301d75fa4f927d037b3bd8852ee \
  --lower-model-sha256 323829264a9c977ccb01a674c2a068bdc29c87d7232fbeb5feb839605264ecfe \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --model-sha256 1580faba5340a934b6efb8e5ddb1c5f947aab723630ce1e2eae19f8d8feedcb4 \
  --observation-sha256 6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b \
  --parallelism 17 \
  --prebuilt-executable "$binary" \
  --prebuilt-executable-sha256 e12de6a1a2ad7b1faa2d7380c5be640e32d868ffc5ebd8250449fb9d1222e1a9 \
  --prebuilt-model-sha256 1580faba5340a934b6efb8e5ddb1c5f947aab723630ce1e2eae19f8d8feedcb4 \
  --transitions-only
