#!/usr/bin/env bash
set -euo pipefail

readonly dragon_unit=ultimatefish-info-kghostkdragon-parallel-v27.service
readonly ninja_unit=ultimatefish-info-kninjakghost-action-conditioned-v20.service
readonly dragon_result=/mnt/ultimatefish-jg/opposed-ghost-dragon-memory-v4/restore/ghost-dragon-current-work-v1/kghostkdragon/work/results/kghostkdragon-memory-v5
readonly ninja_result=/mnt/ultimatefish/ghost-parallel-v7/kninjakghost-parallel-v14/work/results/kninjakghost

complete_result() {
  local stem=$1
  [[ -s "${stem}.ufiw" && -s "${stem}.ufgd" ]]
}

# A solver launch, inactive unit, or partial output is never sufficient.  Only
# hand off CPUs after both terminal result files exist and the producer has
# stopped.  The certification watcher independently authenticates the files;
# this helper changes scheduling only and cannot promote a ledger row.
if ! systemctl is-active --quiet "${ninja_unit}" &&
   complete_result "${ninja_result}" &&
   systemctl is-active --quiet "${dragon_unit}"; then
  systemctl set-property --runtime "${dragon_unit}" AllowedCPUs=0-31
  echo "certification_sprint_cpu_handoff completed=ninja survivor=dragon cpus=0-31"
fi

if ! systemctl is-active --quiet "${dragon_unit}" &&
   complete_result "${dragon_result}" &&
   systemctl is-active --quiet "${ninja_unit}"; then
  systemctl set-property --runtime "${ninja_unit}" AllowedCPUs=0-31
  echo "certification_sprint_cpu_handoff completed=dragon survivor=ninja cpus=0-31"
fi
