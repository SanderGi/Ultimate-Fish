#!/usr/bin/env bash
set -euo pipefail

readonly restore=/mnt/ultimatefish/berserker-ghost-opposed-restore-v12
readonly root=${restore}/info-remap-fix-v1/kberserkerkghost-fresh-v5
readonly source=/mnt/ultimatefish/ghost-terminal-rebind-v11-1ae8341b/source.tar
readonly binary=${restore}/bin/ultimate_ghost_berserker_information_tablebase-v11-diagnostic
readonly prefix=work/transitions/kberserkerkghost
readonly log=${root}/work/logs/diagnostic-v19.log

export ULTIMATE_GHOST_TRANSITION_WORKERS=30

test "$(sha256sum "${source}" | cut -d ' ' -f 1)" = \
  1ae8341b0759f4e1b08fa6e681616734916fc85f4a08bb0e1caf027c0d04cb51
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  b0aca04d6e1de1a2e45d4398297009ef4d49446dc5154ad56db7ac055024605a
test "$(sha256sum "${root}/tablebases/kberserkerkghost.uftb" | cut -d ' ' -f 1)" = \
  d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55
test "$(sha256sum "${root}/tablebases/kberserkerk.uftb" | cut -d ' ' -f 1)" = \
  f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1
test ! -e "${root}/work/results/kberserkerkghost.ufiw"
test ! -e "${root}/work/results/kberserkerkghost.ufgd"

exec >>"${log}" 2>&1
echo 'berserker_ghost_opposed_diagnostic_v19 begin 1'
cd "${root}"
exec "${binary}" --verify-transitions \
  --orientation opposing \
  --transition-prefix "${prefix}" \
  --lower-dragon-table tablebases/kberserkerk.uftb \
  --lower-dragon-sha256 f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1 \
  --lower-dragon-source-sha256 f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1 \
  --lower-dragon-model-sha256 f2dfd61cb3c955e66ae8a48907466d30bdb0169cd1f64044b8f8f62645dccba4 \
  --source-sha256 d18c8c3bc2387b4e2a0ed5f9e38899cc6cb1d30ad10744695cad3a12b0ca9d55 \
  --model-sha256 4ba38541f75205f35986831577e168f66173a488e447348a87f33d3e2c531ffd \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
