#!/bin/bash
set -euo pipefail

source_root=/mnt/ultimatefish/singleton-domain-fix-v1-source
runner="${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py"
retained=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostkprince-fresh-v6
work=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostkprince-solve-2b-v7

test -d "${retained}/work/transitions"
test -f "${retained}/work/transitions-ready.json"
test "$(sha256sum "${retained}/work/transitions-ready.json" | cut -d ' ' -f 1)" = 08de4e98e854832a9449f694a14617ec01bd71e803440fffc5ec77f2ef7a055f
test "$(sha256sum "${retained}/ultimate_ghost_ordinary_information_tablebase" | cut -d ' ' -f 1)" = 71fcb13099ae8862823b8a440fdfa59f19f35d37fc6c26afcc8b517e062c778e
test ! -e "${work}"

install -d "${work}/work/transitions" "${work}/tablebases"
cp --reflink=always -a "${retained}/work/transitions/kghostkprince."* \
  "${work}/work/transitions/"
cp --reflink=always -a "${retained}/tablebases/." "${work}/tablebases/"
cp --reflink=always -a "${retained}/work/transitions-ready.json" "${work}/work/"
cp --reflink=always -a "${retained}/ultimate_ghost_ordinary_information_tablebase" \
  "${work}/"
test "$(stat -c %i "${retained}/work/transitions/kghostkprince.header")" != \
  "$(stat -c %i "${work}/work/transitions/kghostkprince.header")"
test "$(sha256sum "${work}/work/transitions-ready.json" | cut -d ' ' -f 1)" = 08de4e98e854832a9449f694a14617ec01bd71e803440fffc5ec77f2ef7a055f
test "$(sha256sum "${work}/ultimate_ghost_ordinary_information_tablebase" | cut -d ' ' -f 1)" = 71fcb13099ae8862823b8a440fdfa59f19f35d37fc6c26afcc8b517e062c778e

exec /usr/bin/python3 "${runner}" \
  --source-root "${source_root}" \
  --work "${work}" \
  --filename kghostkprince.uftb \
  --piece prince \
  --orientation opposing \
  --source-table /mnt/ultimatefish/info-wave-aa82210e-work/kghostkprince/tablebases/kghostkprince.uftb \
  --source-sha256 2858daf3770e0a47230f2d0bf410cd8ce4a9820b0c7cd0895ceab242ae42f65f \
  --lower-table /mnt/ultimatefish/info-wave-aa82210e-work/kghostkprince/tablebases/kprincek.uftb \
  --lower-sha256 7bce11ab717f49e1e2b6e1e0844825139382fe324b2ad88a59b5fc027ba0e6c0 \
  --lower-model-sha256 5511ac5ac166fd2be042e66a4c849e034fef39da317988f40bf4e6b47e071927 \
  --model-sha256 a9c480c453ddeb72085a43483dc60484ec24ca017fd365ed60a7c8f3973d8834 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sidecar /mnt/ultimatefish/info-wave-aa82210e-stage/kghostk.ufgm \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --solve-existing \
  --solve-max-nodes 2000000000
