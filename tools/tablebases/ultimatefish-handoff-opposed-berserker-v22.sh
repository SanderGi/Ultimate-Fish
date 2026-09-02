#!/usr/bin/env bash
set -euo pipefail

readonly restore=/mnt/ultimatefish/berserker-ghost-opposed-restore-v12
readonly root=${restore}/info-remap-fix-v1/kberserkerkghost-fresh-v5
readonly repair_log=${root}/work/logs/lower-restore-v21.log
readonly marker=${root}/work/transitions/kberserkerkghost.verified
readonly marker_backup=${marker}.before-lower-restore-v21
readonly wrapper=${restore}/ultimatefish-rebind-opposed-berserker-v22.sh
readonly launched=${root}/work/rebind-v22.launched
readonly unit=ultimatefish-rebind-berserker-opposed-v22.service

[[ ! -e "${launched}" ]] || exit 0
grep -qF 'berserker_ghost_opposed_lower_restore_v21 complete 1' \
  "${repair_log}" 2>/dev/null || exit 0
test "$(sha256sum "${marker}" | cut -d ' ' -f 1)" = \
  823ad1a14af45def9a1e41a3eec0040ec5d8797b51ba219c9505783c35ee90e0
cmp "${marker}" "${marker_backup}"
test "$(sha256sum "${wrapper}" | cut -d ' ' -f 1)" = \
  fece2ed4aca0ba097bc5af11eef9a2fd39eb112804b2a6bbc409192c5a3729fb

if [[ "$(systemctl show "${unit}" -p LoadState --value 2>/dev/null || true)" \
      != not-found ]]; then
  exit 0
fi
systemd-run --unit="${unit}" --collect \
  --property=AllowedCPUs=32-48,51-63 \
  --property=MemoryHigh=236223201280 \
  --property=MemoryMax=244813135872 \
  --property=IOWeight=10 --property=Nice=10 "${wrapper}"
printf '%s\n' "$(date -u +%FT%TZ) ${unit}" >"${launched}"

