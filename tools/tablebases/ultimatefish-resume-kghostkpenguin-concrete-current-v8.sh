#!/usr/bin/env bash
# Prepare the solve scratch parent omitted by v7, retain its failed log, then
# run the exact authenticated v7 continuation without altering any graph bytes.
set -euo pipefail

readonly root=/mnt/ultimatefish-penguin/opposed-kghostkpenguin-fresh-v5
readonly prior=/usr/local/bin/ultimatefish-resume-kghostkpenguin-concrete-current-v7.sh
readonly failed_log="${root}/work/logs/concrete-current-v7.failed-scratch-parent.log"
readonly live_log="${root}/work/logs/concrete-current-v7.log"

test "$(systemctl show ultimatefish-info-kghostkpenguin-concrete-current-v7.service -p Result --value)" = exit-code
test "$(sha256sum "${prior}" | cut -d' ' -f1)" = 3c8a198dc8a33487b20535038e0725e1f2fc01ba87fa6d307e7186b246a28298
grep -Fq 'Dragon/Ghost error: failed writing normalized Dragon/Ghost source' "${live_log}"
test ! -e "${failed_log}"
mv "${live_log}" "${failed_log}"
install -d -m 0755 \
  "${root}/work/solve-concrete-current-v1" \
  "${root}/work/results"
echo 'penguin_ghost_concrete_current_v8 solve_scratch_parent_fixed 1 transition_payload_changed 0 fixed_point_reused 0' \
  >"${root}/work/logs/concrete-current-v8-bootstrap.log"
exec "${prior}"
