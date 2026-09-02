#!/usr/bin/env bash
set -euo pipefail

readonly restore=/mnt/ultimatefish/berserker-ghost-opposed-restore-v12
readonly root=${restore}/info-remap-fix-v1/kberserkerkghost-fresh-v5
readonly source=/mnt/ultimatefish/ghost-terminal-rebind-v12-9dd55ab6/source.tar
readonly binary=${restore}/bin/ultimate_ghost_berserker_information_tablebase-v12-diagnostic
readonly prefix=work/transitions/kberserkerkghost
readonly marker=${root}/${prefix}.verified
readonly marker_backup=${root}/${prefix}.verified.before-lower-restore-v21
readonly log=${root}/work/logs/lower-restore-v21.log

export ULTIMATE_GHOST_TRANSITION_WORKERS=30

test "$(sha256sum "${source}" | cut -d ' ' -f 1)" = \
  9dd55ab62de368e48529320022b99e0f61ea48e3ec1c0627800d341140285161
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  ef4345b9098d7b17dccfdb5cb09cc375f7de6ab8179ae89b73d7bd1be60bf55e
test "$(sha256sum "${root}/tablebases/kberserkerkghost.uftb" | cut -d ' ' -f 1)" = \
  d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55
test "$(sha256sum "${root}/tablebases/kberserkerk.uftb" | cut -d ' ' -f 1)" = \
  f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1
test ! -e "${root}/work/results/kberserkerkghost.ufiw"
test ! -e "${root}/work/results/kberserkerkghost.ufgd"

cp -p "${marker}" "${marker_backup}"
restore_marker() {
  cp -p "${marker_backup}" "${marker}"
}
trap restore_marker EXIT

exec >>"${log}" 2>&1
echo 'berserker_ghost_opposed_lower_restore_v21 begin 1'
cd "${root}"
"${binary}" --restore-transitions \
  --orientation opposing \
  --transition-prefix "${prefix}" \
  --lower-dragon-table tablebases/kberserkerk.uftb \
  --lower-dragon-sha256 f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1 \
  --lower-dragon-source-sha256 f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1 \
  --lower-dragon-model-sha256 f2dfd61cb3c955e66ae8a48907466d30bdb0169cd1f64044b8f8f62645dccba4 \
  --source-sha256 d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55 \
  --model-sha256 4ba38541f75205f35986831577e168f66173a488e447348a87f33d3e2c531ffd \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
echo 'berserker_ghost_opposed_lower_restore_v21 complete 1 marker_restored 1'
