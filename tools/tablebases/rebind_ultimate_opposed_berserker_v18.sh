#!/usr/bin/env bash
set -euo pipefail

readonly restore=/mnt/ultimatefish/berserker-ghost-opposed-restore-v12
readonly root=${restore}/info-remap-fix-v1/kberserkerkghost-fresh-v5
readonly binary=${restore}/bin/ultimate_ghost_berserker_information_tablebase-v10
readonly prefix=work/transitions/kberserkerkghost
readonly log=${root}/work/logs/rebind-v18.log
export ULTIMATE_GHOST_TRANSITION_WORKERS=30

test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  108fdb074dfa1e746508583c03546cb9e1ed3e0eec57f4e5220b41e520f09650
test "$(sha256sum "${root}/tablebases/kberserkerkghost.uftb" | cut -d ' ' -f 1)" = \
  d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55
test "$(sha256sum "${root}/tablebases/kberserkerk.uftb" | cut -d ' ' -f 1)" = \
  f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1
test "$(sha256sum "${root}/${prefix}.verified" | cut -d ' ' -f 1)" = \
  823ad1a14af45def9a1e41a3eec0040ec5d8797b51ba219c9505783c35ee90e0
test ! -e "${root}/work/results/kberserkerkghost.ufiw"
test ! -e "${root}/work/results/kberserkerkghost.ufgd"

cp -p "${root}/${prefix}.verified" \
  "${root}/${prefix}.verified.before-rebind-v18"
cp -p "${root}/${prefix}.meta" \
  "${root}/${prefix}.meta.before-rebind-v18"
exec >>"${log}" 2>&1
echo 'berserker_ghost_opposed_rebind_v18 begin 1'
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
"${binary}" --rebind-transitions "${common[@]}"
"${binary}" --verify-transitions "${common[@]}"
sha256sum "${prefix}.header" "${prefix}.meta" "${prefix}.strata" \
  "${prefix}.index" "${prefix}.blocks" "${prefix}.verified" \
  >work/transitions/kberserkerkghost.rebind-v13.sha256
echo 'berserker_ghost_opposed_rebind_v13 complete 1 residual 0'
echo 'berserker_ghost_opposed_rebind_v18 complete 1 terminal_metadata_residual 0'
