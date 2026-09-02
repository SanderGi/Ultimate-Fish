#!/usr/bin/env bash
set -euo pipefail

orientation="${1:?orientation is required}"
root="/mnt/ultimatefish-overflow/ghost-angel-v4"
source_root="$root/source"
inputs="$root/inputs"
runner_sha256="0343aadd56d82d67719f0cae7ccb51a6f4a7efac6d98dc9f48343f2140c2a861"
export ULTIMATE_GHOST_SELF_TEST_THREADS=11

case "$orientation" in
  same)
    filename="kghostangelk.uftb"
    source_sha256="d8422f3f62dc92e2c636cdbb0946ea7d8f9fe7c05102a039b9071190f3c55630"
    model_sha256="d79d37faa6acbd828732667b3467850ed280560c72230ec392f79d9fc709a0cf"
    ;;
  opposing)
    filename="kghostkangel.uftb"
    source_sha256="ad4be1cce6d7d5911fb91cf2bf5c3274d5eafcde23240a22c5b3dfdfa2507d9c"
    model_sha256="70b9950b4751f260da6a2765f37ad6b432da28a086e13bccfdc436b9c3aef2f2"
    ;;
  *)
    echo "invalid Angel/Ghost orientation: $orientation" >&2
    exit 2
    ;;
esac

test "$(sha256sum "$inputs/$filename" | cut -d' ' -f1)" = "$source_sha256"
test "$(sha256sum "$source_root/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" | cut -d' ' -f1)" = \
  "$runner_sha256"

exec /usr/bin/python3 \
  "$source_root/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "$source_root" \
  --work "$root/work-$orientation" \
  --filename "$filename" \
  --piece angel \
  --orientation "$orientation" \
  --source-table "$inputs/$filename" \
  --source-sha256 "$source_sha256" \
  --lower-sha256 25a2afa32399de9c487352f23a7f7e84673ca301d75fa4f927d037b3bd8852ee \
  --lower-model-sha256 323829264a9c977ccb01a674c2a068bdc29c87d7232fbeb5feb839605264ecfe \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --model-sha256 "$model_sha256" \
  --observation-sha256 6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b \
  --lower-ghost-source-sha256 11b7b57aa9819b1b7fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --parallelism 11 \
  --transitions-only
