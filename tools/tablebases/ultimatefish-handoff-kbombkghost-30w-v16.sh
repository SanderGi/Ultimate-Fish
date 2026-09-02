#!/usr/bin/env bash
set -euo pipefail

readonly old_unit=ultimatefish-info-kbombkghost-resume-v9.service
readonly new_unit=ultimatefish-info-kbombkghost-resume-v16.service
readonly log=/mnt/ultimatefish/ghost-bomb-solve-v8/restore/ghost-bomb-current-work-v1/kbombkghost/work/logs/solve-1.5b-v8.log
readonly output=/mnt/ultimatefish/ghost-bomb-solve-v8/restore/ghost-bomb-current-work-v1/kbombkghost/work/results/kbombkghost-1.5b-v8.ufiw
readonly arbitrary=/mnt/ultimatefish/ghost-bomb-solve-v8/restore/ghost-bomb-current-work-v1/kbombkghost/work/results/kbombkghost-1.5b-v8.ufgd
readonly marker='ghost_extra_external_compaction iteration 13 '

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
