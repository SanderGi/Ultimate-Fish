#!/bin/bash
set -euo pipefail

root=/mnt/ultimatefish/info-substate-corrected-v1/kberserkerghostk-v2
source_root=/mnt/ultimatefish/compositional-transitions-v5-source
binary=${source_root}/ultimate_ghost_berserker_compositional_v5_linux
lower_ghost=/mnt/ultimatefish/info-wave-aa82210e-stage/kghostk.ufgm
prior_log=${root}/work/logs/compositional-merge-solve-v3.log
log=${root}/work/logs/solve-allocation-v4.log
scratch=work/solve-allocation-v4/kberserkerghostk

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f 1)" = \
  0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  52a9da514544d6879b45b0ec1b9461cef313920c53a33f4c7266fb81086fef1d
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/work/self-test/kberserkerghostk.normalized.uftb" | cut -d ' ' -f 1)" = \
  864949136727ee37a3e03aa3e1c03a7cf1f5200ae1d958dc67c6b30dfc7bfc1d
test -f "${root}/work/transitions/kberserkerghostk.verified"
grep -Fq \
  'ghost_dragon_compositional_merge_certificate shards 64 payload_bound_shards 0 legacy_exhaustive_shards 64 full_replay_skipped 1 residual 0' \
  "${prior_log}"
grep -Fq \
  'Dragon/Ghost error: exact external Ghost-extra solve exceeds the 97 GiB gate' \
  "${prior_log}"
test ! -e "${log}"
test ! -e "${root}/${scratch}"
test ! -e "${root}/work/results/kberserkerghostk-allocation-v4.ufiw"
test ! -e "${root}/work/results/kberserkerghostk-allocation-v4.ufgd"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 103079215104

install -m 0644 "${lower_ghost}" "${root}/tablebases/kghostk.ufgm"
mkdir -p "${root}/$(dirname "${scratch}")" "${root}/work/results"

exec >>"${log}" 2>&1
echo "berserker_same_allocation_v4 source_bundle_sha256 0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856 source_bundle_version QdBG6EaSlckJI_v1rCztxgmbr0fOYW0L binary_sha256 52a9da514544d6879b45b0ec1b9461cef313920c53a33f4c7266fb81086fef1d binary_version MzEBuccF4St_JpmF86ya1VY773l3THOW authenticated_graph work/transitions/kberserkerghostk prior_gate_failure_preserved ${prior_log}"

cd "${root}"
"${binary}" \
  --solve \
  --orientation same \
  --transition-prefix work/transitions/kberserkerghostk \
  --input tablebases/kberserkerghostk.uftb \
  --normalized-input work/self-test/kberserkerghostk.normalized.uftb \
  --normalized-sha256 864949136727ee37a3e03aa3e1c03a7cf1f5200ae1d958dc67c6b30dfc7bfc1d \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output work/results/kberserkerghostk-allocation-v4.ufiw \
  --output-arbitrary work/results/kberserkerghostk-allocation-v4.ufgd \
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

sha256sum work/results/kberserkerghostk-allocation-v4.ufiw \
  work/results/kberserkerghostk-allocation-v4.ufgd
echo 'berserker_same_allocation_v4 complete 1'
