#!/usr/bin/env bash
# Solve the authenticated same-side Ghost/Ghost graph rebuilt after the stopped
# uncached baseline.  This stage must never rebuild or overlap transition data.
set -euo pipefail

readonly root=/mnt/ultimatefish/same-ghost-pair-rebuild-after-stop-v1
readonly work=${root}/job
readonly runner=${root}/run_ultimate_ghost_pair_current_aws-solve-existing-v2.py
readonly binary=${work}/ultimate_ghost_pair_information_tablebase
readonly marker=${work}/work/transitions-ready.json

test "$(sha256sum "${root}/source.tar.gz" | cut -d' ' -f1)" = \
  5bbb139577ee0d4ee577f85813c98f6f2c8e499ebc8dc39b72dbd1daffd950d0
test "$(sha256sum "${runner}" | cut -d' ' -f1)" = \
  422f23cf58972b80875f41e6105f8814e421c6116256381ff7bf11ba48ddb97a
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = \
  e5adb01295e596b6d56a028ebbd3e1b9e3d9fed0a5ad555f2d08180f3f37349a
test "$(sha256sum "${marker}" | cut -d' ' -f1)" = \
  f99c1fbdf150d3e41a05238d75fb92700d7d37dc08bcf61c8a09426da2343032
test "$(sha256sum "${work}/tablebases/kghostghostk.uftb" | cut -d' ' -f1)" = \
  12e053d81f0594d363830db97eb71a87a27a73fc3de0dcfe8cb13f141bdda8e6
test "$(sha256sum "${work}/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test ! -e "${work}/work/logs/solve.log"
test ! -e "${work}/work/results/kghostghostk.ufiw"
test ! -e "${work}/work/results/kghostghostk.ufgg"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 858993459200

exec /usr/bin/python3 "${runner}" \
  --source-root "${root}/source" \
  --work "${work}" \
  --source-table "${work}/tablebases/kghostghostk.uftb" \
  --source-sha256 12e053d81f0594d363830db97eb71a87a27a73fc3de0dcfe8cb13f141bdda8e6 \
  --lower-ghost-sidecar "${work}/tablebases/kghostk.ufgm" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --model-sha256 779e91a7df7dc806a472963b1073470d484e7a1b638d71dfc9c8dbe5c839e110 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --parallelism 30 \
  --solve-existing \
  --prebuilt-executable "${binary}" \
  --prebuilt-executable-sha256 e5adb01295e596b6d56a028ebbd3e1b9e3d9fed0a5ad555f2d08180f3f37349a \
  --prebuilt-model-sha256 779e91a7df7dc806a472963b1073470d484e7a1b638d71dfc9c8dbe5c839e110 \
  --max-disk-bytes 1200000000000 \
  --max-resident-bytes 220000000000 \
  --min-free-disk-bytes 80000000000
