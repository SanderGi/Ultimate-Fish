#!/bin/bash
set -euo pipefail

readonly root=/mnt/ultimatefish/same-ghost-pair-rebuild-after-stop-v1
readonly job="${root}/job"
readonly work=/mnt/ultimatefish/ghost-pair-parallel-v4-benchmark
readonly binary=/mnt/ultimatefish/ghost-pair-parallel-v4-5a775115/ultimate_ghost_pair_information_tablebase-parallel-v4
readonly transitions="${job}/work/transitions/kghostghostk"

test ! -e "${work}"
install -d -m 0755 "${work}"
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  7dd5ce8fc2d143fa92db54a09e731c0c7cef73facf43dac6da656859c1e2d97a
test "$(sha256sum "${transitions}.verified" | cut -d ' ' -f 1)" = \
  be09fcad0417b8a32fbb3b5a65fccc358231f6d17173dd9b53d2275e22811bca
test "$(sha256sum "${job}/tablebases/kghostghostk.uftb" | cut -d ' ' -f 1)" = \
  12e053d81f0594d363830db97eb71a87a27a73fc3de0dcfe8cb13f141bdda8e6
test "$(sha256sum "${job}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

cd "${job}"
/usr/bin/time -f 'wall_seconds %e user_seconds %U system_seconds %S max_rss_kib %M' \
  -o "${work}/time.txt" \
  "${binary}" --measure 1 --workers 30 \
  --transition-prefix work/transitions/kghostghostk \
  --input tablebases/kghostghostk.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${work}/kghostghostk" \
  --output "${work}/unused.ufiw" \
  --output-arbitrary "${work}/unused.ufgg" \
  --source-sha256 12e053d81f0594d363830db97eb71a87a27a73fc3de0dcfe8cb13f141bdda8e6 \
  --model-sha256 779e91a7df7dc806a472963b1073470d484e7a1b638d71dfc9c8dbe5c839e110 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-disk-bytes 300000000000 --max-resident-bytes 70000000000 \
  --min-free-disk-bytes 80000000000 --compact-every 1 \
  > "${work}/measure.log" 2>&1
grep -F 'information_symbolic_certificate iterations 1' \
  "${work}/measure.log"
cat "${work}/time.txt"

