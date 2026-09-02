#!/usr/bin/env bash
set -euo pipefail

readonly dragon_unit=ultimatefish-info-kghostkdragon-parallel-v27.service
readonly ninja_unit=ultimatefish-info-kninjakghost-action-conditioned-v20.service
readonly ninja_compaction_marker='reciprocal_ghost_extra_bellman iteration 17 geometries 492960/492960'
readonly ninja_next_sweep_marker='reciprocal_ghost_extra_bellman iteration 18 geometries 5000/492960'

expanded=0
restore_allocations() {
  if [[ "${expanded}" -eq 1 ]]; then
    # Shrink Dragon before expanding Ninja so the sets never overlap.
    systemctl set-property --runtime "${dragon_unit}" AllowedCPUs=0-3
    if systemctl is-active --quiet "${ninja_unit}"; then
      systemctl set-property --runtime "${ninja_unit}" AllowedCPUs=4-31
    fi
    echo "certification_sprint_i0b_phase_restore dragon 0-3 ninja 4-31"
    expanded=0
  fi
}
trap restore_allocations EXIT INT TERM

while systemctl is-active --quiet "${ninja_unit}"; do
  if journalctl -u "${ninja_unit}" -n 160 --no-pager -o cat |
      grep -Fq "${ninja_compaction_marker}"; then
    break
  fi
  sleep 15
done

systemctl is-active --quiet "${ninja_unit}" || exit 0
[[ "$(systemctl show "${dragon_unit}" -p AllowedCPUs --value)" == 0-3 ]]
[[ "$(systemctl show "${ninja_unit}" -p AllowedCPUs --value)" == 4-31 ]]

# Reduce Ninja first, then expand Dragon onto the released CPUs.
systemctl set-property --runtime "${ninja_unit}" AllowedCPUs=4-7
systemctl set-property --runtime "${dragon_unit}" AllowedCPUs=0-3,8-31
expanded=1
echo "certification_sprint_i0b_phase_expand dragon 0-3,8-31 ninja 4-7"

while systemctl is-active --quiet "${ninja_unit}"; do
  if journalctl -u "${ninja_unit}" -n 160 --no-pager -o cat |
      grep -Fq "${ninja_next_sweep_marker}"; then
    break
  fi
  sleep 5
done

restore_allocations
trap - EXIT INT TERM
