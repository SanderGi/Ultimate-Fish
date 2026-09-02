#!/usr/bin/env bash
set -euo pipefail

readonly old_unit=ultimatefish-info-kghostsniperk-resume-v11.service
readonly new_unit=ultimatefish-info-kghostsniperk-resume-v12.service
readonly log=/mnt/ultimatefish/sniper-ghost-same-restore-v11/info-substate-corrected-v1/kghostsniperk-v2/work/logs/compositional-merge-solve-v3.log
readonly output=/mnt/ultimatefish/sniper-ghost-same-restore-v11/info-substate-corrected-v1/kghostsniperk-v2/work/results/kghostsniperk-compositional-v3.ufiw
readonly arbitrary=/mnt/ultimatefish/sniper-ghost-same-restore-v11/info-substate-corrected-v1/kghostsniperk-v2/work/results/kghostsniperk-compositional-v3.ufgd
readonly marker='ghost_extra_external_compaction iteration 41 '

while systemctl is-active --quiet "${old_unit}"; do
  if test -s "${output}" -a -s "${arbitrary}"; then
    exit 0
  fi
  if grep -Fq "${marker}" "${log}"; then
    break
  fi
  sleep 15
done

if test -s "${output}" -a -s "${arbitrary}"; then
  exit 0
fi
grep -Fq "${marker}" "${log}"
systemctl stop "${old_unit}"
sync
systemctl start "${new_unit}"
sleep 5
systemctl is-active --quiet "${new_unit}"
