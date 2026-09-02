#!/usr/bin/env bash
set -euo pipefail

readonly restore=/mnt/ultimatefish/berserker-ghost-opposed-restore-v12
readonly root=${restore}/info-remap-fix-v1/kberserkerkghost-fresh-v5
readonly binary=${restore}/bin/ultimate_ghost_berserker_information_tablebase
readonly prefix=work/transitions/kberserkerkghost
readonly scratch=work/solve-v12/kberserkerkghost
readonly output=work/results/kberserkerkghost.ufiw
readonly arbitrary=work/results/kberserkerkghost.ufgd
readonly log=${root}/work/logs/solve-v12.log

test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  da683c8395d05f4d9df7d06a291a5be819cb559c76e3cb9e9c2188e871a2479a
test "$(sha256sum "${root}/tablebases/kberserkerkghost.uftb" | cut -d ' ' -f 1)" = \
  d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55
test "$(sha256sum "${root}/tablebases/kberserkerk.uftb" | cut -d ' ' -f 1)" = \
  f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1
test "$(sha256sum "${restore}/info-wave-aa82210e-stage/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/${prefix}.verified" | cut -d ' ' -f 1)" = \
  823ad1a14af45def9a1e41a3eec0040ec5d8797b51ba219c9505783c35ee90e0
test ! -e "${root}/${scratch}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(df --output=avail -B1 "${restore}" | tail -1)" -ge 536870912000
mkdir -p "${root}/${scratch}" "${root}/work/logs" "${root}/work/results"

exec >>"${log}" 2>&1
echo "berserker_ghost_opposed_fresh_v12 binary_sha256 da683c8395d05f4d9df7d06a291a5be819cb559c76e3cb9e9c2188e871a2479a fresh_fixed_point 1 workers 16 max_nodes 1500000000 unique_slots 2147483648"
cd "${root}"
common=(
  --orientation opposing
  --transition-prefix "${prefix}"
  --lower-dragon-table tablebases/kberserkerk.uftb
  --lower-dragon-sha256 f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1
  --lower-dragon-source-sha256 f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1
  --lower-dragon-model-sha256 f2dfd61cb3c955e66ae8a48907466d30bdb0169cd1f64044b8f8f62645dccba4
  --source-sha256 d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55
  --model-sha256 4ba38541f75205f35986831577e168f66173a488e447348a87f33d3e2c531ffd
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)

"${binary}" --verify-transitions "${common[@]}"
"${binary}" \
  --solve \
  "${common[@]}" \
  --input tablebases/kberserkerkghost.uftb \
  --lower-ghost-sidecar "${restore}/info-wave-aa82210e-stage/kghostk.ufgm" \
  --scratch "${scratch}" \
  --output "${output}" \
  --output-arbitrary "${arbitrary}" \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1500000000 \
  --unique-slots 2147483648 \
  --workers 16 \
  --compact-every 1

sha256sum "${output}" "${arbitrary}"
echo 'berserker_ghost_opposed_fresh_v12 complete 1'
