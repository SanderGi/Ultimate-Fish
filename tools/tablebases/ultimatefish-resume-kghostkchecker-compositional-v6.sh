#!/usr/bin/env bash
# Resume Checker/Ghost from authenticated compaction 18 after the i024
# instance-type migration interrupted Bellman iteration 19.
set -euo pipefail

root=/mnt/ultimatefish/info-substate-corrected-v1/kghostkchecker
source_root=/mnt/ultimatefish/compositional-transitions-v4-source
binary=${source_root}/ultimate_ghost_checker_compositional_v4_linux
scratch=work/solve-compositional-v4/kghostkchecker
log=work/logs/compositional-merge-solve-v4.log

test "$(sha256sum "${source_root}/source.tar.gz" | cut -d ' ' -f1)" = \
  52afea0cb343cba4e0b17f15083174086385a6d692ce4c969b92d464e647d9a3
test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  f04bed8b71642795acad1995fbd422b4e7d21eac32d00321a67167928c21135f
grep -Fq 'ghost_extra_external_compaction iteration 18 roots 320207491 marked_nodes 8411018 copied_nodes 8411016 structural_residual 0 root_residual 0' \
  "${root}/${log}"
test "$(stat -c %s "${root}/${scratch}.bdd-a.nodes")" = 13500000000
test "$(stat -c %s "${root}/${scratch}.owner-current")" = 1261977600
test ! -e "${root}/work/results/kghostkchecker-compositional-v4.ufiw"
test ! -e "${root}/work/results/kghostkchecker-compositional-v4.ufgd"

exec >>"${root}/${log}" 2>&1
echo 'checker_opposed_compositional_v6 resume_iteration=18 current_slot=current bdd_slot=a append_only_partial_iteration=19 migration_residual=0'
cd "${root}"
"${binary}" \
  --solve --orientation opposing \
  --transition-prefix work/transitions/kghostkchecker-compositional-v4 \
  --lower-dragon-table implicit-draw \
  --lower-dragon-sha256 0dcb4039ec2305298e9f35d36ce8d0e54fe76acc013c188d08e0e2a6da0ef22c \
  --lower-dragon-source-sha256 0dcb4039ec2305298e9f35d36ce8d0e54fe76acc013c188d08e0e2a6da0ef22c \
  --lower-dragon-model-sha256 42566cc6e4186de2f29a2ded6e9a0728e8dc6d7e748d936e574f15f5202c1763 \
  --source-sha256 bcbc6c6bc88a2c18939222513732016d72e0288429a74ee522f2e5c2f6d3b2b4 \
  --model-sha256 bb4e8b3b44571b7ed5caad19dc17d26db9afe976393175f399f5e546ea490316 \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --input tablebases/kghostkchecker.uftb \
  --normalized-input work/self-test/kghostkchecker.normalized.uftb \
  --normalized-sha256 0400c77f6d33ba827b6eb53f5f707144eda3b0daeed430cd94ae5d491bc2fef2 \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output work/results/kghostkchecker-compositional-v4.ufiw \
  --output-arbitrary work/results/kghostkchecker-compositional-v4.ufgd \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1500000000 --unique-slots 2147483648 --compact-every 1 \
  --resume-fixed-point --resume-iteration 18 \
  --resume-current-slot current --resume-bdd-slot a

sha256sum work/results/kghostkchecker-compositional-v4.ufiw \
  work/results/kghostkchecker-compositional-v4.ufgd
echo 'checker_opposed_compositional_v6 complete 1'
