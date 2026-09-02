#!/usr/bin/env bash
# Resume the retained v5 Berserker/Ghost arena after an EC2 instance-type
# migration interrupted Bellman iteration 3.  Iteration 2 is the last complete
# checkpoint; the partial next-slot/Bdd-a files are deliberately ignored.  The
# resized-host binary is rebuilt from the same authenticated source bundle and
# independently self-tested because the transient old binary lived only on
# instance store.
set -euo pipefail

root=/mnt/ultimatefish/info-substate-corrected-v1/kberserkerghostk-v2-resume-v5
source_root=/mnt/ultimatefish/compositional-transitions-v5-source
binary=${source_root}/ultimate_ghost_berserker_compositional_v5_linux
scratch=work/solve-allocation-v4/kberserkerghostk
log=work/logs/solve-allocation-resume-v5.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f1)" = \
  0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856
test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  1eee89793daea2ac1162ec7f92cf83813cd3bff1ac486fb0d7fa4638140b693c
grep -Fq 'ghost_extra_external_resume iteration 2 bdd_slot b current_slot current' \
  "${root}/${log}"
test "$(stat -c %s "${root}/${scratch}.bdd-b.nodes")" = 4500000000
test "$(stat -c %s "${root}/${scratch}.owner-current")" = 1577472000
test ! -e "${root}/work/results/kberserkerghostk-allocation-v5.ufiw"
test ! -e "${root}/work/results/kberserkerghostk-allocation-v5.ufgd"

exec >>"${root}/${log}" 2>&1
echo 'berserker_same_allocation_resume_v6 resume_iteration=2 current_slot=current bdd_slot=b migration_residual=0'
cd "${root}"
"${binary}" \
  --solve --orientation same \
  --transition-prefix work/transitions/kberserkerghostk \
  --input tablebases/kberserkerghostk.uftb \
  --normalized-input work/self-test/kberserkerghostk.normalized.uftb \
  --normalized-sha256 864949136727ee37a3e03aa3e1c03a7cf1f5200ae1d958dc67c6b30dfc7bfc1d \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output work/results/kberserkerghostk-allocation-v5.ufiw \
  --output-arbitrary work/results/kberserkerghostk-allocation-v5.ufgd \
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
  --max-nodes 500000000 --unique-slots 1073741824 --compact-every 1 \
  --resume-fixed-point --resume-iteration 2 \
  --resume-current-slot current --resume-bdd-slot b

sha256sum work/results/kberserkerghostk-allocation-v5.ufiw \
  work/results/kberserkerghostk-allocation-v5.ufgd
echo 'berserker_same_allocation_resume_v6 complete 1'
