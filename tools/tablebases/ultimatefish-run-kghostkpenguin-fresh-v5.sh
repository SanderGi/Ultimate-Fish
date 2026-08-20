#!/bin/bash
set -euo pipefail

source_root=/mnt/ultimatefish/compositional-transitions-v4-source
binary=${source_root}/ultimate_ghost_penguin_compositional_v4_linux
work=/mnt/ultimatefish-penguin/opposed-kghostkpenguin-fresh-v5
source_table=/mnt/ultimatefish-penguin/staging-v1/downloads/kghostkpenguin.uftb
preflight=/mnt/ultimatefish-penguin/opposed-kghostkpenguin-v4-preflight/kghostkpenguin.normalized.uftb
failed_v4=/mnt/ultimatefish-penguin/opposed-kghostkpenguin-fresh-v4/work/logs/self-test.log
lower_table=/mnt/ultimatefish-penguin/staging-v1/downloads/kpenguink.uftb
lower_ghost=/mnt/ultimatefish/info-wave-aa82210e-stage/kghostk.ufgm

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  52afea0cb343cba4e0b17f15083174086385a6d692ce4c969b92d464e647d9a3
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  999ce0cc34ea4583b1b15f64df69af3bb0b3743981352cf126bc906bc84faf55
test "$(sha256sum "${source_table}" | cut -d ' ' -f 1)" = \
  d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a
test "$(sha256sum "${preflight}" | cut -d ' ' -f 1)" = \
  88ee486b26e5cac0eede64535c391a81c4b6abcd19bfe0e65476c3bf3400ff06
grep -Fq \
  'Dragon/Ghost error: Dragon/Ghost source header does not match its orientation' \
  "${failed_v4}"
test "$(sha256sum "${lower_table}" | cut -d ' ' -f 1)" = \
  5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test ! -e "${work}"
test "$(df --output=avail -B1 /mnt/ultimatefish-penguin | tail -1)" \
  -ge 150323855360

echo "penguin_opposed_fresh_v5 source_bundle_sha256 52afea0cb343cba4e0b17f15083174086385a6d692ce4c969b92d464e647d9a3 source_bundle_version gt4MiZbuAgHzEe69Pq3uknvdzvfrNUNE binary_sha256 999ce0cc34ea4583b1b15f64df69af3bb0b3743981352cf126bc906bc84faf55 binary_version iV6sa521yjXCjpVSkOLaOxgCqvt9MHqM raw_source_sha256 d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a independently_preflighted_normalized_source_sha256 88ee486b26e5cac0eede64535c391a81c4b6abcd19bfe0e65476c3bf3400ff06 model_sha256 268c2bdf5fb9e1b436406378e97ee213eb460c70b354bbeececd479ef8df33e8 parallelism 19 failed_v4_preserved ${failed_v4} fresh_work ${work}"

exec python3 "${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source_root}" \
  --work "${work}" \
  --filename kghostkpenguin.uftb \
  --piece penguin \
  --orientation opposing \
  --source-table "${source_table}" \
  --source-sha256 d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a \
  --lower-table "${lower_table}" \
  --lower-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
  --lower-model-sha256 3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84 \
  --lower-ghost-sidecar "${lower_ghost}" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --model-sha256 268c2bdf5fb9e1b436406378e97ee213eb460c70b354bbeececd479ef8df33e8 \
  --observation-sha256 6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --parallelism 19 \
  --prebuilt-executable "${binary}" \
  --prebuilt-executable-sha256 999ce0cc34ea4583b1b15f64df69af3bb0b3743981352cf126bc906bc84faf55 \
  --solve-max-nodes 500000000
