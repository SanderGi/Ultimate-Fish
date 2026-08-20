#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/ghost-bomb-current-work-v1/kbombkghost
source_root=/mnt/ultimatefish/compositional-transitions-bomb-v6-source
binary=${source_root}/ultimate_ghost_bomb_compositional_v6_linux
old_prefix=work/transitions/kbombkghost
new_prefix=work/transitions/kbombkghost-compositional-v7
prior_log=${root}/work/logs/compositional-merge-v6.log
log=${root}/work/logs/compositional-merge-v7.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  3a142e9091f3543ebd941003ed1527308ba00b0e79756018477f078ca2a9cfa6
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  182faf8689d9ac4ab182f585d9c3264a4d4076cec8c91c93c18d7c45bbb2d5aa
test "$(sha256sum "${root}/tablebases/kbombkghost.uftb" | cut -d ' ' -f 1)" = \
  fc10b2a5ee22ca9ec8aecb1caa89a9de03a6431804a594cf95626f63a20eb1db
test "$(sha256sum "${root}/tablebases/kbombk.uftb" | cut -d ' ' -f 1)" = \
  18e057c83faf940db1ad7404a39623a5db208724de892d604735576d7583ce2f
test "$(sha256sum "${root}/work/self-test/kbombkghost.normalized.uftb" | cut -d ' ' -f 1)" = \
  141f0fd0996a15c9891c74d28270330001a6b5c85b4ecbc60df1abd8d2a9a8dd
test "$(sha256sum "${root}/${old_prefix}.verified" | cut -d ' ' -f 1)" = \
  f139320c909c3dc5b9d60fdf0b47ebb5cc01473a124f7372bcfcacbfe1c4b996
test "$(sha256sum "${prior_log}" | cut -d ' ' -f 1)" = \
  bbef9a3510d08d9fa9e727763f1ce3638c6f07fadb47dea2523b02303504b26f
grep -Fq 'Bomb/Ghost error: Bomb transition marker dependency residual' \
  "${prior_log}"
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
test "$(stat -c %s "${root}/work/transitions/shard-00.verified")" = 320
test "$(dd if="${root}/work/transitions/shard-00.verified" bs=1 \
  skip=184 count=64 status=none)" = \
  1248a304457b674250861c7f8c0ee14373c8b37a6b1d65e315aa045ae58cdfa3
test ! -e "${root}/${new_prefix}.header"
test ! -e "${log}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 21474836480

exec >>"${log}" 2>&1
echo 'bomb_ghost_opposed_compositional_v7 source_bundle_sha256 3a142e9091f3543ebd941003ed1527308ba00b0e79756018477f078ca2a9cfa6 source_bundle_version 8bdsZk66EeO20BXmzakYzhLmlOJrjSD0 binary_sha256 182faf8689d9ac4ab182f585d9c3264a4d4076cec8c91c93c18d7c45bbb2d5aa binary_version WTAz52TEb7rBKEEwRHLkBnjs.0K4oCGK legacy_lower_bomb_model_sha256 1248a304457b674250861c7f8c0ee14373c8b37a6b1d65e315aa045ae58cdfa3 exact_legacy_shards 64 prior_binding_failure_preserved work/logs/compositional-merge-v6.log old_full_replay_graph_preserved work/transitions/kbombkghost new_strong_graph work/transitions/kbombkghost-compositional-v7'

cd "${root}"
common=(
  --orientation opposing
  --lower-bomb-table tablebases/kbombk.uftb
  --lower-bomb-sha256 18e057c83faf940db1ad7404a39623a5db208724de892d604735576d7583ce2f
  --lower-bomb-source-sha256 18e057c83faf940db1ad7404a39623a5db208724de892d604735576d7583ce2f
  --lower-bomb-model-sha256 1248a304457b674250861c7f8c0ee14373c8b37a6b1d65e315aa045ae58cdfa3
  --source-sha256 fc10b2a5ee22ca9ec8aecb1caa89a9de03a6431804a594cf95626f63a20eb1db
  --model-sha256 d87ecb37913bbc5c67653a9742ee3e18cc823e97ecacd58aa0472008d7739f68
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)
shards=()
for index in {00..63}; do
  shards+=(--shard "work/transitions/shard-${index}")
done

"${binary}" \
  --merge-transitions \
  --transition-prefix "${new_prefix}" \
  "${shards[@]}" \
  --expected-geometries 492960 \
  "${common[@]}"

"${binary}" \
  --verify-transitions \
  --transition-prefix "${new_prefix}" \
  "${common[@]}"

sha256sum "${new_prefix}.header" "${new_prefix}.meta" \
  "${new_prefix}.strata" "${new_prefix}.index" \
  "${new_prefix}.blocks" "${new_prefix}.verified"
echo 'bomb_ghost_opposed_compositional_v7 complete 1'
