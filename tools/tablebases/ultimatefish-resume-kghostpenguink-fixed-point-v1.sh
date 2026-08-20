#!/bin/bash
set -euo pipefail

original=/mnt/ultimatefish/info-substate-corrected-v1/kghostpenguink-privacy-v3
parent=/mnt/ultimatefish/info-singleton-resume-v1
target=${parent}/kghostpenguink-fixed-point-v1
source_root=/mnt/ultimatefish/penguin-fixed-point-resume-v1-source
resume_executable=${source_root}/ultimate_ghost_ordinary_penguin_resume_v1

test ! -e "${target}"
test "$(sha256sum "${resume_executable}" | cut -d ' ' -f 1)" = \
  1ce95f3fd127038a2d7bad27b757c23750aeae3007e7c3fa30a6e92de50030f0
test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  15c5bb24f2a04c7b5450045c88b2671229669cb1a902c66a189b209b0724d4fa
test "$(sha256sum "${original}/ultimate_ghost_ordinary_information_tablebase" |
  cut -d ' ' -f 1)" = \
  523a7c64a2d0bd2e9f702633dc031ab490bcda597d593b871ae19f58dacd3b99
test "$(sha256sum "${original}/tablebases/kghostpenguink.uftb" |
  cut -d ' ' -f 1)" = \
  e937f73d83338e2671e96be562fea208ce9740877b94dfd101c4dec058d8e9b4
test "$(sha256sum "${original}/tablebases/kpenguink.uftb" |
  cut -d ' ' -f 1)" = \
  5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd
test "$(sha256sum "${original}/tablebases/kghostk.ufgm" |
  cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${original}/work/solve/kghostpenguink.normalized.uftb" |
  cut -d ' ' -f 1)" = \
  d3e79eb74bff5722ba957a097646b5a58a06ce509084fbbd6b901ad412c30f95
grep -Fq \
  'reciprocal_ghost_extra_iteration 61 bdd_nodes 82508552 changed_owner 0 changed_observer 0 changed_visible 0' \
  "${original}/work/logs/solve.log"
grep -Fq \
  'Dragon/Ghost error: external Ghost-extra singleton result differs from concrete WDL' \
  "${original}/work/logs/solve.log"
test "$(stat -c %s "${original}/work/solve/kghostpenguink.bdd-a.nodes")" = \
  4500000000
test "$(stat -c %s "${original}/work/solve/kghostpenguink.bdd-a.unique")" = \
  4294967296
test "$(stat -c %s "${original}/work/solve/kghostpenguink.owner-next")" = \
  1261977600
test "$(stat -c %s "${original}/work/solve/kghostpenguink.observer-next")" = \
  2228992
test "${original}/work/solve/kghostpenguink.bdd-a.nodes" -nt \
  "${original}/work/solve/kghostpenguink.bdd-b.nodes"
test "${original}/work/solve/kghostpenguink.owner-current" -nt \
  "${original}/work/solve/kghostpenguink.owner-next"

mkdir -p "${parent}"
cp -a --reflink=always "${original}" "${target}"
install -m 0755 "${resume_executable}" \
  "${target}/ultimate_ghost_ordinary_penguin_resume_v1"

exec >>"${target}/work/logs/resume-fixed-point-v1.log" 2>&1
echo "penguin_fixed_point_resume source_bundle_sha256 15c5bb24f2a04c7b5450045c88b2671229669cb1a902c66a189b209b0724d4fa source_bundle_version DfoFoIBiLC6dIXtEjC3GbthGR5zjVyaV executable_sha256 1ce95f3fd127038a2d7bad27b757c23750aeae3007e7c3fa30a6e92de50030f0 original_preserved ${original} clone ${target} bdd_slot a current_slot next iteration 61"

cd "${target}"
./ultimate_ghost_ordinary_penguin_resume_v1 \
  --solve \
  --orientation same \
  --transition-prefix work/transitions/kghostpenguink \
  --input tablebases/kghostpenguink.uftb \
  --normalized-input work/solve/kghostpenguink.normalized.uftb \
  --normalized-sha256 d3e79eb74bff5722ba957a097646b5a58a06ce509084fbbd6b901ad412c30f95 \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch work/solve/kghostpenguink \
  --output work/results/kghostpenguink-resume-v1.ufiw \
  --output-arbitrary work/results/kghostpenguink-resume-v1.ufgd \
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

sha256sum work/results/kghostpenguink-resume-v1.ufiw \
  work/results/kghostpenguink-resume-v1.ufgd
echo 'penguin_fixed_point_resume complete 1'
