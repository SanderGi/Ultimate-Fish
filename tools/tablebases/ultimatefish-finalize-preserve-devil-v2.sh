#!/usr/bin/env bash
set -euo pipefail

readonly campaign=/mnt/ultimatefish/devil-spawned-c1-v28
readonly roots=${campaign}/graphs/square-2/devil-2.roots
readonly receipt=${campaign}/merge-v1/devil-root-merge-receipt.json
readonly preserved=${campaign}/preservation-v2/preservation-retry/preservation-retry.json

while ! test -s "${roots}"; do
  sleep 20
done
if ! test -s "${receipt}"; then
  systemctl start ultimatefish-finalize-devil-spawned-v1.service
fi
test -s "${receipt}"
if ! test -s "${preserved}"; then
  systemctl start ultimatefish-preserve-devil-merged-v2.service
fi
test -s "${preserved}"

