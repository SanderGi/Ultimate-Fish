#!/usr/bin/env bash
set -euo pipefail

readonly restore_root=/mnt/ultimatefish/sniper-ghost-same-restore-v11
readonly root=${restore_root}/info-substate-corrected-v1/kghostsniperk-v2
readonly binary=${restore_root}/bin/ultimate_ghost_sniper_information_tablebase
readonly prefix=work/transitions/kghostsniperk-compositional-v3
readonly scratch=work/solve-compositional-v3/kghostsniperk
readonly output=work/results/kghostsniperk-compositional-v3.ufiw
readonly arbitrary=work/results/kghostsniperk-compositional-v3.ufgd
readonly log=${root}/work/logs/compositional-merge-solve-v3.log

test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  d067036152298e71e00fef4f620c7779dc65a8d1dcb00dc202962df994909be1
test "$(sha256sum "${root}/tablebases/kghostsniperk.uftb" | cut -d ' ' -f 1)" = \
  b627e286865f3081805a8a81ab8ec4ec8938c8a8486f3df8a0ae6a3edc4a12f1
test "$(sha256sum "${root}/tablebases/ksniperk.uftb" | cut -d ' ' -f 1)" = \
  473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/work/self-test/kghostsniperk.normalized.uftb" | cut -d ' ' -f 1)" = \
  d4eecdb96138a9cec096718ee1781b1de6bc914b417b90ca2514be1291bf4814
test "$(sha256sum "${root}/${prefix}.verified" | cut -d ' ' -f 1)" = \
  5985a3d2b1421c5af60afd38e1d1e93a2decc9bd06cfa94c7bee5ebf7cbc2cc3

test -s "${root}/${scratch}.bdd-b.nodes"
test -s "${root}/${scratch}.bdd-b.unique"
test -s "${root}/${scratch}.owner-next"
test -s "${root}/${scratch}.observer-next"
test -s "${root}/${scratch}.visible-owner-next"
test -s "${root}/${scratch}.visible-observer-next"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 214748364800
mkdir -p "${root}/work/results"

exec >>"${log}" 2>&1
echo "sniper_ghost_same_resume_v12 completed_iteration 41 current_slot next bdd_slot b workers 30 max_nodes 1000000000 unique_slots 1073741824"
cd "${root}"
common=(
  --orientation same
  --transition-prefix "${prefix}"
  --lower-dragon-table tablebases/ksniperk.uftb
  --lower-dragon-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
  --lower-dragon-source-sha256 473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
  --lower-dragon-model-sha256 7eaa2c6b443d15a98f935cf7b9fec3e6991a841246a2af8290afadaf1b3feab1
  --source-sha256 b627e286865f3081805a8a81ab8ec4ec8938c8a8486f3df8a0ae6a3edc4a12f1
  --model-sha256 37ce97c6639e74eb2d8caf9843abed7bee8a3cb3937ad5383e582046c2183b0d
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
)

"${binary}" --verify-transitions "${common[@]}"
"${binary}" --solve "${common[@]}" \
  --input tablebases/kghostsniperk.uftb \
  --normalized-input work/self-test/kghostsniperk.normalized.uftb \
  --normalized-sha256 d4eecdb96138a9cec096718ee1781b1de6bc914b417b90ca2514be1291bf4814 \
  --lower-ghost-sidecar tablebases/kghostk.ufgm \
  --scratch "${scratch}" \
  --output "${output}" \
  --output-arbitrary "${arbitrary}" \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 1000000000 \
  --unique-slots 1073741824 \
  --workers 30 \
  --compact-every 1 \
  --resume-fixed-point \
  --resume-iteration 41 \
  --resume-current-slot next \
  --resume-bdd-slot b

sha256sum "${output}" "${arbitrary}"
echo 'sniper_ghost_same_resume_v12 complete 1'
