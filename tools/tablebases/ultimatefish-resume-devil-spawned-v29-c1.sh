#!/usr/bin/env bash
# Resume authentic lone-Devil C1 from the v28 retained graph and completed
# reverse buckets, replacing the write-amplifying scatter merge with v29's
# per-shard sort and buffered sequential k-way merge.
set -euo pipefail

readonly root=/mnt/ultimatefish/devil-spawned-c1-v28
readonly prefix=${root}/graphs/square-2/devil-2
readonly stage=/mnt/ultimatefish/devil-spawned-solver-v29-368e7230
readonly source=${stage}/source.tar
readonly binary=${stage}/ultimate_tablebase-devil-spawned-v29
readonly log=${root}/logs/square-2-v29.log

test "$(sha256sum "${source}" | cut -d ' ' -f1)" = \
  368e7230e5e2c82604b8be1d019311eaa4c344e96c04bc08db249fe9fe24915a
test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  acc758ed81e56fba827c1e6ecfccfb464bfcdf8823ce35540cbb7cd3144c7df6
test "$(find "${prefix}.reverse-spool" -maxdepth 1 -name 'shard-*.complete' | wc -l)" = 512
test "$(stat -c %s "${prefix}.nodes")" = 30889705392
test "$(stat -c %s "${prefix}.degrees")" = 15444852696
test "$(stat -c %s "${prefix}.offsets")" = 30889705400
test "$(stat -c %s "${prefix}.predecessors")" = 196864270064
test "$(stat -c %s "${prefix}.predecessor-sides")" = 24608033758

exec >>"${log}" 2>&1
echo "devil_v29_resume source_sha256 368e7230e5e2c82604b8be1d019311eaa4c344e96c04bc08db249fe9fe24915a binary_sha256 acc758ed81e56fba827c1e6ecfccfb464bfcdf8823ce35540cbb7cd3144c7df6 adopted_buckets 3,5,6,7,8,9,10,11,12,13,14,15,16"
exec "${binary}" --piece devil --workers 24 \
  --solve-devil-spawned-square 2 --devil-spawned-limit 4200000000 \
  --devil-spawned-work "${root}/graphs/square-2"
