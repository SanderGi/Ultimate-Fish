#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-substate-corrected-v1/kberserkerghostk-v2
source_root=/mnt/ultimatefish/compositional-transitions-v3-source
binary=${source_root}/ultimate_ghost_berserker_compositional_v3_linux
lower_ghost=/mnt/ultimatefish/info-wave-aa82210e-stage/kghostk.ufgm
log=${root}/work/logs/compositional-merge-solve-v3.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  75a1a89f7022478bc3fdcad8da83d1694e06463be559e0d1fbc804ba40b0cd83
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  16e7241c9e5b59994cce165d2e395288e2e4f80a8460ae1f4659fb2a3936286b
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/work/self-test/kberserkerghostk.normalized.uftb" | cut -d ' ' -f 1)" = \
  864949136727ee37a3e03aa3e1c03a7cf1f5200ae1d958dc67c6b30dfc7bfc1d
test -f "${root}/work/logs/merge.log"
test "$(find "${root}/work/transitions" -maxdepth 1 -name 'shard-*.verified' | wc -l)" = 64

install -m 0644 "${lower_ghost}" "${root}/tablebases/kghostk.ufgm"
exec >>"${log}" 2>&1
echo "berserker_compositional_resume_v3 source_bundle_sha256 75a1a89f7022478bc3fdcad8da83d1694e06463be559e0d1fbc804ba40b0cd83 source_bundle_version 5Ig1aSjwOsVHwI4opCNihZRgvJg_2YXd binary_sha256 16e7241c9e5b59994cce165d2e395288e2e4f80a8460ae1f4659fb2a3936286b binary_version Omlm3sfG3Px5wofM3y2N3PP0JJoTJtiQ failed_v2_log_preserved ${root}/work/logs/compositional-merge-solve-v2.log exhaustive_replay_log_preserved ${root}/work/logs/merge.log exact_shards 64"

cd "${root}"
shards=()
for index in {00..63}; do
  shards+=(--shard "work/transitions/shard-${index}")
done

"${binary}" \
  --merge-transitions \
  --orientation same \
  --transition-prefix work/transitions/kberserkerghostk \
  "${shards[@]}" \
  --expected-geometries 4929600 \
  --lower-dragon-table tablebases/kberserkerk.uftb \
  --lower-dragon-sha256 f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1 \
  --lower-dragon-source-sha256 f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1 \
  --lower-dragon-model-sha256 f2dfd61cb3c955e66ae8a48907466d30bdb0169cd1f64044b8f8f62645dccba4 \
  --source-sha256 c1965fd898985c5b509506b7af0c4970f15c0644fb86e0dcadeea4f4180e3cbf \
  --model-sha256 5c98a4c3aed735bb60ebc017d21e2194fa19e8e83b33375a8888a24bd90046d5 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23

"${binary}" \
  --solve \
  --orientation same \
  --transition-prefix work/transitions/kberserkerghostk \
  --input tablebases/kberserkerghostk.uftb \
  --normalized-input work/self-test/kberserkerghostk.normalized.uftb \
  --normalized-sha256 864949136727ee37a3e03aa3e1c03a7cf1f5200ae1d958dc67c6b30dfc7bfc1d \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch work/solve/kberserkerghostk \
  --output work/results/kberserkerghostk-compositional-v3.ufiw \
  --output-arbitrary work/results/kberserkerghostk-compositional-v3.ufgd \
  --lower-dragon-table tablebases/kberserkerk.uftb \
  --lower-dragon-sha256 f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1 \
  --lower-dragon-source-sha256 f41245a06eb280136c458e6337ef4a588a0d5142b606413b92f84c248487e2b1 \
  --lower-dragon-model-sha256 f2dfd61cb3c955e66ae8a48907466d30bdb0169cd1f64044b8f8f62645dccba4 \
  --source-sha256 c1965fd898985c5b509506b7af0c4970f15c0644fb86e0dcadeea4f4180e3cbf \
  --model-sha256 5c98a4c3aed735bb60ebc017d21e2194fa19e8e83b33375a8888a24bd90046d5 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 500000000 \
  --unique-slots 1073741824 \
  --compact-every 1

sha256sum work/results/kberserkerghostk-compositional-v3.ufiw \
  work/results/kberserkerghostk-compositional-v3.ufgd
echo 'berserker_compositional_resume_v3 complete 1'
