#!/usr/bin/env bash
# Promote Prince from 8 to 32 workers only after a newly observed, exact
# zero-residual iteration-8 compaction in the current v9 unit journal.
set -euo pipefail

readonly old_unit=ultimatefish-info-solve-kghostkprince-parallel-v9.service
readonly new_unit=ultimatefish-info-solve-kghostkprince-parallel-v10.service
readonly scratch=/mnt/ultimatefish/ghost-parallel-v6/kghostkprince-canary-striped-bdd-b-8w/solve/kghostkprince
readonly resume=/run/ultimatefish-resume-kghostkprince-parallel-v10.sh
readonly manifest=/opt/ultimatefish-certification-sprint/manifest
readonly pattern='^ghost_extra_external_compaction iteration 8 roots [0-9]+ .* structural_residual 0 root_residual 0$'

test "$(systemctl is-active "${old_unit}")" = active
test "$(journalctl -u "${old_unit}" --no-pager -o cat | grep -Ec "${pattern}" || true)" = 0
test -x "${resume}"
test ! -e "${scratch}.checkpoint8-authenticated"

while [[ "$(journalctl -u "${old_unit}" --no-pager -o cat | grep -Ec "${pattern}" || true)" = 0 ]]; do
  sleep 15
done
test "$(journalctl -u "${old_unit}" --no-pager -o cat | grep -Ec "${pattern}" || true)" = 1

systemctl stop "${old_unit}"
test "$(systemctl is-active "${old_unit}" || true)" != active
test "$(journalctl -u "${old_unit}" --no-pager -o cat | grep -Ec "${pattern}" || true)" = 1
printf '%s\n' \
  'iteration=8 bdd_slot=a current_slot=current structural_residual=0 root_residual=0' \
  >"${scratch}.checkpoint8-authenticated"

test "$(grep -Fc "${old_unit}|results/information/opposed/ghost-prince/sprint-v1|8|" "${manifest}")" = 1
sed -i \
  "s/${old_unit}|results\/information\/opposed\/ghost-prince\/sprint-v1|8|/${new_unit}|results\/information\/opposed\/ghost-prince\/sprint-v1|32|/" \
  "${manifest}"

systemd-run --unit "${new_unit}" --property AllowedCPUs=0-31 \
  --property MemoryMax=96G --property MemorySwapMax=0 "${resume}"
