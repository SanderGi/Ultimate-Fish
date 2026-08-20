#!/bin/bash
set -euo pipefail

# Run the exact domain-cache candidate beside the preserved six-day baseline.
# The candidate uses separate scratch and outputs so it cannot mutate the
# baseline attempt or the already-verified transition shards.

readonly root=/mnt/ultimatefish/info-wave-aa82210e-work/kghostghostk
readonly binary=/mnt/ultimatefish/ghost-pair-domain-cache-v1-build/ultimate_ghost_pair_information_tablebase
readonly transition_prefix=work/transitions/kghostghostk
readonly scratch=work/solve-domain-cache-v1/kghostghostk
readonly output=work/results/kghostghostk-domain-cache-v1.ufiw
readonly output_arbitrary=work/results/kghostghostk-domain-cache-v1.ufgg

test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  e5adb01295e596b6d56a028ebbd3e1b9e3d9fed0a5ad555f2d08180f3f37349a
test "$(sha256sum "${root}/tablebases/kghostghostk.uftb" | cut -d ' ' -f 1)" = \
  12e053d81f0594d363830db97eb71a87a27a73fc3de0dcfe8cb13f141bdda8e6
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test -e "${root}/${transition_prefix}.verified"
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${output_arbitrary}"

install -d -m 0755 \
  "${root}/work/solve-domain-cache-v1" \
  "${root}/work/results"

cd "${root}"
exec "${binary}" \
  --solve \
  --transition-prefix "${transition_prefix}" \
  --input tablebases/kghostghostk.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${output}" \
  --output-arbitrary "${output_arbitrary}" \
  --source-sha256 12e053d81f0594d363830db97eb71a87a27a73fc3de0dcfe8cb13f141bdda8e6 \
  --model-sha256 779e91a7df7dc806a472963b1073470d484e7a1b638d71dfc9c8dbe5c839e110 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-disk-bytes 1310000000000 \
  --max-resident-bytes 180000000000 \
  --min-free-disk-bytes 80000000000 \
  --compact-every 1
