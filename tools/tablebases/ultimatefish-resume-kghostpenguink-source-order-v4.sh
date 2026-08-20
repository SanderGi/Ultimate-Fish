#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-singleton-resume-v1/kghostpenguink-fixed-point-v1
source_root=/mnt/ultimatefish/penguin-source-order-v6
binary=${source_root}/ultimate_ghost_penguin_source_order_v6_linux
normalized=${source_root}/normalization/kghostpenguink.normalized.uftb
prior_log=${root}/work/logs/resume-fixed-point-v3.log
log=${root}/work/logs/resume-fixed-point-v4.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  c42240a49361a72d77c17e12bb7611dfb4ac74d677f341726abd6202083be98b
test "$(sha256sum "${root}/tablebases/kghostpenguink.uftb" | cut -d ' ' -f 1)" = \
  e937f73d83338e2671e96be562fea208ce9740877b94dfd101c4dec058d8e9b4
test "$(sha256sum "${normalized}" | cut -d ' ' -f 1)" = \
  729f67aaa74d79c44fa375d78b5ef51a0e120f44a48bb67b8b2d0d1e01d5998a
test "$(find "${root}/work/transitions" -maxdepth 1 \
  -name 'shard-*.verified' | wc -l)" = 64
grep -Fq \
  'reciprocal_ghost_extra_iteration 62 bdd_nodes 82528399 changed_owner 0 changed_observer 0 changed_visible 0' \
  "${prior_log}"
grep -Fq 'ghost_extra_singleton_residual 41914906' "${prior_log}"
test ! -e "${root}/work/results/kghostpenguink-resume-v4.ufiw"
test ! -e "${root}/work/results/kghostpenguink-resume-v4.ufgd"

exec >>"${log}" 2>&1
echo "penguin_source_order_resume_v4 source_bundle_sha256 a26d3665f7f07215f18d9b831f7178b18e0b46e5346dbd47116a10e88ec06064 source_bundle_version WxnZuM9HYqwvK4MFGo9IjTs2A9_8eto3 binary_sha256 c42240a49361a72d77c17e12bb7611dfb4ac74d677f341726abd6202083be98b binary_version ebPYbHSybdqnwqA7uC8nEDplLo_xnza9 raw_source_sha256 e937f73d83338e2671e96be562fea208ce9740877b94dfd101c4dec058d8e9b4 corrected_normalized_sha256 729f67aaa74d79c44fa375d78b5ef51a0e120f44a48bb67b8b2d0d1e01d5998a prior_transposed_normalized_sha256 d3e79eb74bff5722ba957a097646b5a58a06ce509084fbbd6b901ad412c30f95 retained_iteration 62 retained_bdd_slot a retained_current_slot current exact_shards 64 redundant_merge_skipped 1 redundant_mutating_sweep_skipped 1 prior_failure_preserved ${prior_log}"

cd "${root}"
"${binary}" \
  --solve \
  --orientation same \
  --transition-prefix work/transitions/kghostpenguink \
  --input tablebases/kghostpenguink.uftb \
  --normalized-input "${normalized}" \
  --normalized-sha256 729f67aaa74d79c44fa375d78b5ef51a0e120f44a48bb67b8b2d0d1e01d5998a \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch work/solve/kghostpenguink \
  --output work/results/kghostpenguink-resume-v4.ufiw \
  --output-arbitrary work/results/kghostpenguink-resume-v4.ufgd \
  --lower-dragon-table tablebases/kpenguink.uftb \
  --lower-dragon-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
  --lower-dragon-source-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
  --lower-dragon-model-sha256 3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84 \
  --source-sha256 e937f73d83338e2671e96be562fea208ce9740877b94dfd101c4dec058d8e9b4 \
  --model-sha256 3b9fdf33ca878d18e2a3bea7c3c6a591aaf9c6a453a9e649b0f87784085cbc8b \
  --observation-sha256 6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 500000000 \
  --unique-slots 1073741824 \
  --compact-every 1 \
  --resume-fixed-point \
  --resume-converged \
  --resume-iteration 62 \
  --resume-current-slot current \
  --resume-bdd-slot a

sha256sum work/results/kghostpenguink-resume-v4.ufiw \
  work/results/kghostpenguink-resume-v4.ufgd
echo 'penguin_source_order_resume_v4 complete 1'
