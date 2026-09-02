#!/usr/bin/env bash
set -euo pipefail

readonly restore=/mnt/ultimatefish/berserker-ghost-opposed-restore-v12
readonly root=${restore}/info-remap-fix-v1/kberserkerkghost-fresh-v5
readonly binary=${restore}/bin/ultimate_ghost_berserker_information_tablebase
readonly prefix=work/transitions/kberserkerkghost
readonly rebind_manifest=work/transitions/kberserkerkghost.rebind-v13.sha256
readonly scratch=work/solve-v17/kberserkerkghost
readonly output=work/results/kberserkerkghost.ufiw
readonly arbitrary=work/results/kberserkerkghost.ufgd
readonly rebind_log=${root}/work/logs/rebind-v13.log
readonly log=${root}/work/logs/solve-v17.log

test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  da683c8395d05f4d9df7d06a291a5be819cb559c76e3cb9e9c2188e871a2479a
test "$(sha256sum "${root}/tablebases/kberserkerkghost.uftb" | cut -d ' ' -f 1)" = \
  d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55
test "$(sha256sum "${root}/tablebases/kberserkerk.uftb" | cut -d ' ' -f 1)" = \
  f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1
test "$(sha256sum "${restore}/info-wave-aa82210e-stage/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
grep -Fqx 'berserker_ghost_opposed_rebind_v13 complete 1 residual 0' "${rebind_log}"
test -s "${root}/${rebind_manifest}"
(cd "${root}" && sha256sum --check --strict "${rebind_manifest}")
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${root}/${scratch}"

# The rejected pre-rebind attempt created only an empty scratch directory.
# Retain that evidence under a distinct name before creating the valid v17
# fixed-point state; never reuse state bound to the old transition marker.
readonly rejected=${root}/work/solve-v12/kberserkerkghost
readonly retained=${root}/work/solve-v12/kberserkerkghost.pre-rebind-empty
if [[ -d "${rejected}" ]]; then
  test -z "$(find "${rejected}" -mindepth 1 -print -quit)"
  test ! -e "${retained}"
  mv "${rejected}" "${retained}"
fi

test "$(df --output=avail -B1 "${restore}" | tail -1)" -ge 536870912000
mkdir -p "${root}/${scratch}" "${root}/work/logs" "${root}/work/results"

exec >>"${log}" 2>&1
echo 'berserker_ghost_opposed_after_rebind_v17 begin 1 workers 30 max_nodes 1500000000 unique_slots 2147483648'
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
  --workers 30 \
  --compact-every 1

sha256sum "${output}" "${arbitrary}"
echo 'berserker_ghost_opposed_after_rebind_v17 complete 1'
