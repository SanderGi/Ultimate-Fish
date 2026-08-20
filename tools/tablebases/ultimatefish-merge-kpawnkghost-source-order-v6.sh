#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-remap-fix-v2/kpawnkghost-fresh-v4
source_root=/mnt/ultimatefish/pawn-source-order-v6-a26d3665
binary=${source_root}/ultimate_ghost_pawn_source_order_v6_linux
source_table=${root}/tablebases/kpawnkghost.uftb
lower_table=${root}/tablebases/kpawnk.uftb
promoted_lower_table=${root}/tablebases/kqueenk.uftb
self_test_normalized=${source_root}/self-test-pawn/kpawnkghost.normalized.uftb
normalized=${root}/work/self-test-source-order-v6/kpawnkghost.normalized.uftb
old_prefix=work/transitions/kpawnkghost
new_prefix=work/transitions/kpawnkghost-source-order-v6
log=${root}/work/logs/source-order-v6-merge.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  ee0ce4d6e82689c17cfdb80dc7069f85dd70d8b4fa0dc5d4be9edbbb50ab1476
test "$(sha256sum "${source_table}" | cut -d ' ' -f 1)" = \
  6d2f23b873a861b0c51cb86a8378013619c5cce0adb29e0060bce07cc2a2ae46
test "$(sha256sum "${lower_table}" | cut -d ' ' -f 1)" = \
  42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844
test "$(sha256sum "${promoted_lower_table}" | cut -d ' ' -f 1)" = \
  1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
test "$(sha256sum "${self_test_normalized}" | cut -d ' ' -f 1)" = \
  87cdf78459ad1eaa90cb36c2dc8e8ded1b704d61fcf807c1d739eef687dc9105
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
test ! -e "${root}/${old_prefix}.header"
test ! -e "${root}/${new_prefix}.header"
test ! -e "${log}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 137438953472

mkdir -p "${root}/work/self-test-source-order-v6"
cp "${self_test_normalized}" "${normalized}"
test "$(sha256sum "${normalized}" | cut -d ' ' -f 1)" = \
  87cdf78459ad1eaa90cb36c2dc8e8ded1b704d61fcf807c1d739eef687dc9105

common=(
  --orientation opposing
  --lower-dragon-table tablebases/kpawnk.uftb
  --lower-dragon-sha256 42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844
  --lower-dragon-source-sha256 42a2cd002c3e1b9c15f78fa895f9a9e030e93ac2ddfd2f416c9301ba2a791844
  --lower-dragon-model-sha256 6f84793f4343b5855fc0ddadd7a4452b9788c67070fa03b1f81bffb59ca4e011
  --promoted-lower-dragon-table tablebases/kqueenk.uftb
  --promoted-lower-dragon-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
  --promoted-lower-dragon-source-sha256 1d5d15c2a5ed93d06aa423d458c9a32b8b7854bafba4f6d15bb4e8fb6cdf1fd3
  --promoted-lower-dragon-model-sha256 87147fccee31cff98fd6875f02ca51d82d20c978bcaa6d7c4aaa81d114a23c6a
  --source-sha256 6d2f23b873a861b0c51cb86a8378013619c5cce0adb29e0060bce07cc2a2ae46
  --model-sha256 3f66b94790014f072a1eb69659449e7f4b80d1e19330d00a92b270420d1ec4cd
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)
shards=()
for index in {00..63}; do
  shards+=(--shard "work/transitions/shard-${index}")
done

{
  echo "pawn_ghost_opposed_source_order_v6 source_bundle_sha256 a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064 source_bundle_version WxnZuM9HYqwvK4MFGo9IjTs2A9_8eto3 binary_sha256 ee0ce4d6e82689c17cfdb80dc7069f85dd70d8b4fa0dc5d4be9edbbb50ab1476 binary_version fJ0SYDJ09fIkyCptyrDhQJNC14VGPlR0 exact_shards 64 interrupted_replay_log work/logs/merge.log corrected_normalized_sha256 87cdf78459ad1eaa90cb36c2dc8e8ded1b704d61fcf807c1d739eef687dc9105 promoted_queen_dependency pending"
  cd "${root}"
  "${binary}" \
    --merge-transitions \
    --transition-prefix "${new_prefix}" \
    "${shards[@]}" \
    --expected-geometries 1971840 \
    "${common[@]}"
  sha256sum \
    "${new_prefix}.header" \
    "${new_prefix}.meta" \
    "${new_prefix}.strata" \
    "${new_prefix}.index" \
    "${new_prefix}.blocks"
  echo 'pawn_ghost_opposed_source_order_v6_merge complete 1 solve_waits_for_corrected_queen 1'
} >>"${log}" 2>&1
