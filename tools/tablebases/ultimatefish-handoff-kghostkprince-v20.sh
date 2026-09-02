#!/usr/bin/env bash
# Fail closed unless the active 3B Prince/Ghost solve stopped specifically at
# its exact node-allocation gate.  Successful completion is left exclusively
# to the certification watcher.
set -euo pipefail

readonly current=ultimatefish-info-kghostkprince-action-conditioned-v19.service
readonly successor=ultimatefish-info-kghostkprince-action-conditioned-v20.service
readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly wrapper_sha=0d66bc8c69574e94cf507a889d19ca0d53aabeb2717bb992f27f14ae11173e7d
readonly wrapper_version=z1NJkG2xHrOY9jSO3of4bq2xAtBdk7cT
readonly wrapper_key="sources/wrappers/ghost-prince-capacity-v20/sha256/${wrapper_sha}/ultimatefish-resume-kghostkprince-action-conditioned-v20.sh"
readonly wrapper=/run/ultimatefish-resume-kghostkprince-action-conditioned-v20.sh
readonly work=/mnt/ultimatefish/ghost-parallel-v6/kghostkprince-canary-striped-bdd-b-8w

if systemctl is-active --quiet "${current}" || \
   systemctl is-active --quiet "${successor}"; then
  exit 0
fi
if [[ -s "${work}/results/kghostkprince.ufiw" ||
      -s "${work}/results/kghostkprince.ufgd" ]]; then
  exit 0
fi
test "$(systemctl show "${current}" -p Result --value)" = exit-code
journalctl -u "${current}" --no-pager | \
  grep -Fq 'external ROBDD exhausted its exact node allocation'
journalctl -u "${current}" --no-pager | \
  grep -Fq 'ghost_extra_external_compaction iteration 11'

aws s3api get-object --bucket "${bucket}" --key "${wrapper_key}" \
  --version-id "${wrapper_version}" "${wrapper}.download" >/dev/null
test "$(sha256sum "${wrapper}.download" | cut -d ' ' -f 1)" = \
  "${wrapper_sha}"
install -m 0755 "${wrapper}.download" "${wrapper}"
rm -f "${wrapper}.download"

systemd-run --unit "${successor%.service}" \
  --description='Opposed Prince/Ghost 4B exact-node checkpoint successor' \
  --property=AllowedCPUs=0-31 \
  --property=MemoryMax=206158430208 \
  --property=TasksMax=512 \
  --property=LimitNOFILE=1048576 \
  --property=TimeoutStartSec=infinity \
  --property=TimeoutStopSec=300 \
  "${wrapper}"
