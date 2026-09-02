#!/usr/bin/env bash
# Resume opposed Checker/Ghost from the complete iteration-19 compaction with
# the exact-v4-compatible reciprocal-parallel v16 solver. Partial iteration-20
# append remains retained; only checkpoint-19 roots are trusted.
set -euo pipefail

readonly root=/mnt/ultimatefish/info-substate-corrected-v1/kghostkchecker
readonly stage=/mnt/ultimatefish/ghost-parallel-v16-ed276d82
readonly source_archive=${stage}/source.tar
readonly binary=${stage}/ultimate_ghost_ordinary_information_tablebase-checker-v16
readonly scratch=work/solve-compositional-v4/kghostkchecker
readonly log=work/logs/compositional-merge-solve-v4.log

test "$(sha256sum "${source_archive}" | cut -d ' ' -f1)" = \
  ed276d8278458ecf2d01e53740674cf0e065c159e302debc4d8878606461189b
test "$(sha256sum "${binary}" | cut -d ' ' -f1)" = \
  596fd34474e356bd4dddec00c82ed50a6160242b515e11dd298fe356fb23e3d4
test "$(grep -Fc 'ghost_extra_external_compaction iteration 19 roots 320207491 marked_nodes 8494237 copied_nodes 8494235 structural_residual 0 root_residual 0' "${root}/${log}")" = 1
test "$(stat -c %s "${root}/${scratch}.bdd-b.nodes")" = 13500000000
test "$(stat -c %s "${root}/${scratch}.owner-current")" = 1261977600
test ! -e "${root}/work/results/kghostkchecker-compositional-v4.ufiw"
test ! -e "${root}/work/results/kghostkchecker-compositional-v4.ufgd"

exec >>"${root}/${log}" 2>&1
echo 'checker_opposed_parallel_v22 resume_iteration=19 current_slot=next bdd_slot=b workers=32 checkpoint_residual=0 exact_v4_model=1 reciprocal_parallel=1'
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
  --resume-fixed-point --resume-iteration 19 \
  --resume-current-slot next --resume-bdd-slot b

sha256sum work/results/kghostkchecker-compositional-v4.ufiw \
  work/results/kghostkchecker-compositional-v4.ufgd
echo 'checker_opposed_parallel_v22 complete 1'
