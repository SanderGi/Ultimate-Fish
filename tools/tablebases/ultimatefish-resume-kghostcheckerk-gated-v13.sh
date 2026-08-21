#!/usr/bin/env bash
# Resume same-side Checker/Ghost from complete fixed-point iteration 16 after
# iteration 18 was externally OOM-killed.  The newer BDD-b/current files were
# mutated by iterations 17-18, so bind the older complete BDD-a/next parity.
set -euo pipefail

original=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostcheckerk-coherent-fresh-v9
target=/mnt/ultimatefish/info-singleton-domain-fix-v1/kghostcheckerk-coherent-fresh-v13
binary=/mnt/ultimatefish/checker-resume-build-v1/ultimate_ghost_checker_resume_v1_linux
marker=${original}/work/transitions-ready-gated-v10.json
scratch=work/solve/kghostcheckerk
log=work/logs/solve-resume-v13.log
output=work/results/kghostcheckerk-v13.ufiw
arbitrary=work/results/kghostcheckerk-v13.ufgd

test ! -e "${target}"
test "$(sha256sum "${marker}" | cut -d ' ' -f 1)" = \
  17c42b10197c10bca52bab3bf92991625b69c7cfcf2942b1b310af2f9a24fb3a
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  e12a96612df100d03a8272694158aebe11f464d6398655b97c44897a64d5cf29
test "$(sha256sum /mnt/ultimatefish/checker-resume-build-v1/source.tar.gz | cut -d ' ' -f 1)" = \
  0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856
test "$(sha256sum "${original}/tablebases/kghostcheckerk.uftb" | cut -d ' ' -f 1)" = \
  fdd9329ed29fb7823b27e4bd46263b62f6ac66e638b510e232b3ee386ca6be1e
test "$(sha256sum "${original}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
grep -Fq \
  'ghost_extra_external_compaction iteration 16 roots 320330183 marked_nodes 5561000 copied_nodes 5560998 structural_residual 0 root_residual 0' \
  "${original}/work/logs/solve.log"
grep -Fq \
  'reciprocal_ghost_extra_bellman iteration 18 geometry 1925000/3943680' \
  "${original}/work/logs/solve.log"
test "$(stat -c %s "${original}/${scratch}.bdd-a.nodes")" = 9000000000
test "$(stat -c %s "${original}/${scratch}.bdd-a.unique")" = 4294967296
test "$(stat -c %s "${original}/${scratch}.owner-next")" = 1261977600
test "$(stat -c %s "${original}/${scratch}.observer-next")" = 4233408
test "$(stat -c %s "${original}/${scratch}.visible-owner-next")" = 315494400
test "$(stat -c %s "${original}/${scratch}.visible-observer-next")" = 315494400
test "${original}/${scratch}.bdd-b.nodes" -nt \
  "${original}/${scratch}.bdd-a.nodes"
test "${original}/${scratch}.owner-current" -nt \
  "${original}/${scratch}.owner-next"
test ! -e "${original}/work/results/kghostcheckerk.ufiw"
test ! -e "${original}/work/results/kghostcheckerk.ufgd"
test "$(df --output=avail -B1 "${original}" | tail -1)" -ge 107374182400

cp -a --reflink=always "${original}" "${target}"

exec >>"${target}/${log}" 2>&1
echo "checker_same_gated_resume_v13 original=${original} clone=${target} source_bundle_sha256=0343a2ce487c4e4fd113451b691fd6b03a7698f372f855d008dee84c3cbd9856 binary_sha256=e12a96612df100d03a8272694158aebe11f464d6398655b97c44897a64d5cf29 resume_iteration=16 current_slot=next bdd_slot=a"

cd "${target}"
"${binary}" --solve \
  --orientation same \
  --transition-prefix work/transitions/kghostcheckerk-gated-v10 \
  --input tablebases/kghostcheckerk.uftb \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${output}" \
  --output-arbitrary "${arbitrary}" \
  --lower-dragon-table implicit-draw \
  --lower-dragon-sha256 0dcb4039ec2305298e9f35d36ce8d0e54fe76acc013c188d08e0e2a6da0ef22c \
  --lower-dragon-source-sha256 0dcb4039ec2305298e9f35d36ce8d0e54fe76acc013c188d08e0e2a6da0ef22c \
  --lower-dragon-model-sha256 42566cc6e4186de2f29a2ded6e9a0728e8dc6d7e748d936e574f15f5202c1763 \
  --source-sha256 fdd9329ed29fb7823b27e4bd46263b62f6ac66e638b510e232b3ee386ca6be1e \
  --model-sha256 273bfaf1940ddb3fd52893ae7d23b5b758cb7bab12877c0cebf41c95a83627fa \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1000000000 \
  --unique-slots 1073741824 \
  --compact-every 1 \
  --resume-fixed-point \
  --resume-iteration 16 \
  --resume-current-slot next \
  --resume-bdd-slot a

sha256sum "${output}" "${arbitrary}"
echo 'checker_same_gated_resume_v13 complete 1'
