#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-singleton-resume-v1/kghostpenguink-fixed-point-v1
source_root=/mnt/ultimatefish/compositional-transitions-v3-source
binary=${source_root}/ultimate_ghost_penguin_compositional_v3_linux
log=${root}/work/logs/resume-fixed-point-v3.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  75a1a89f7022478bc3fdcad8da83d1694e06463be559e0d1fbc804ba40b0cd83
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  999ce0cc34ea4583b1b15f64df69af3bb0b3743981352cf126bc906bc84faf55
test -f "${root}/work/logs/resume-fixed-point-v1.log"
test -f "${root}/work/logs/solve.log"
test "$(find "${root}/work/transitions" -maxdepth 1 -name 'shard-*.verified' | wc -l)" = 64
grep -Fq \
  'reciprocal_ghost_extra_iteration 61 bdd_nodes 82508552 changed_owner 0 changed_observer 0 changed_visible 0' \
  "${root}/work/logs/solve.log"

exec >>"${log}" 2>&1
echo "penguin_compositional_resume_v3 source_bundle_sha256 75a1a89f7022478bc3fdcad8da83d1694e06463be559e0d1fbc804ba40b0cd83 source_bundle_version 5Ig1aSjwOsVHwI4opCNihZRgvJg_2YXd binary_sha256 999ce0cc34ea4583b1b15f64df69af3bb0b3743981352cf126bc906bc84faf55 binary_version iV6sa521yjXCjpVSkOLaOxgCqvt9MHqM failed_v2_log_preserved ${root}/work/logs/resume-fixed-point-v2.log prior_log_preserved ${root}/work/logs/resume-fixed-point-v1.log exact_shards 64"

cd "${root}"
shards=()
for index in {00..63}; do
  shards+=(--shard "work/transitions/shard-${index}")
done

"${binary}" \
  --merge-transitions \
  --orientation same \
  --transition-prefix work/transitions/kghostpenguink \
  "${shards[@]}" \
  --expected-geometries 3943680 \
  --lower-dragon-table tablebases/kpenguink.uftb \
  --lower-dragon-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
  --lower-dragon-source-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
  --lower-dragon-model-sha256 3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84 \
  --source-sha256 e937f73d83338e2671e96be562fea208ce9740877b94dfd101c4dec058d8e9b4 \
  --model-sha256 3b9fdf33ca878d18e2a3bea7c3c6a591aaf9c6a453a9e649b0f87784085cbc8b \
  --observation-sha256 6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b

"${binary}" \
  --solve \
  --orientation same \
  --transition-prefix work/transitions/kghostpenguink \
  --input tablebases/kghostpenguink.uftb \
  --normalized-input work/solve/kghostpenguink.normalized.uftb \
  --normalized-sha256 d3e79eb74bff5722ba957a097646b5a58a06ce509084fbbd6b901ad412c30f95 \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch work/solve/kghostpenguink \
  --output work/results/kghostpenguink-resume-v3.ufiw \
  --output-arbitrary work/results/kghostpenguink-resume-v3.ufgd \
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
  --resume-iteration 61 \
  --resume-current-slot next \
  --resume-bdd-slot a

sha256sum work/results/kghostpenguink-resume-v3.ufiw \
  work/results/kghostpenguink-resume-v3.ufgd
echo 'penguin_compositional_resume_v3 complete 1'
