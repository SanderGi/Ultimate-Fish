#!/usr/bin/env bash
set -euo pipefail

readonly restore_root=/mnt/ultimatefish/sniper-ghost-same-restore-v11
readonly root=${restore_root}/info-substate-corrected-v1/kghostsniperk-v2
readonly binary=/mnt/ultimatefish/sniper-ghost-remap-auth-v14/ultimate_ghost_sniper_information_tablebase-v14
readonly prefix=work/transitions/kghostsniperk-compositional-v3
readonly scratch=work/solve-compositional-v3/kghostsniperk
readonly output=work/results/kghostsniperk-compositional-v3.ufiw
readonly arbitrary=work/results/kghostsniperk-compositional-v3.ufgd
readonly normalized=work/self-test/kghostsniperk-corrected-v13.normalized.uftb
readonly log=${root}/work/logs/remap-auth-resume-v14.log

test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = \
  62644c757bb0c8e9d6931322a188c67e51d65b5db94e74bb4121e4f42d872721
test "$(sha256sum "${root}/tablebases/kghostsniperk.uftb" | cut -d ' ' -f 1)" = \
  b627e286865f3081805a8a81ab8ec4ec8938c8a8486f3df8a0ae6a3edc4a12f1
test "$(sha256sum "${root}/tablebases/ksniperk.uftb" | cut -d ' ' -f 1)" = \
  473022c95908a45eec7ac8aab7752482d30033cb24f66b65bdf329a7c49c49c5
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/${normalized}" | cut -d ' ' -f 1)" = \
  6250560a341019f4a0f86a2f73b6c52acde24a593241aca0b8e73dba34df0a73
test "$(sha256sum "${root}/${prefix}.verified" | cut -d ' ' -f 1)" = \
  5985a3d2b1421c5af60afd38e1d1e93a2decc9bd06cfa94c7bee5ebf7cbc2cc3
test -s "${root}/${scratch}.bdd-a.nodes"
test -s "${root}/${scratch}.bdd-a.unique"
test -s "${root}/${scratch}.owner-next"
test -s "${root}/${scratch}.observer-next"
test -s "${root}/${scratch}.visible-owner-next"
test -s "${root}/${scratch}.visible-observer-next"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 214748364800
mkdir -p "${root}/work/results"

exec >>"${log}" 2>&1
echo "sniper_ghost_same_remap_auth_v14 completed_iteration 49 current_slot next bdd_slot a resume_converged 1 workers 30"
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
  --normalized-input "${normalized}" \
  --normalized-sha256 6250560a341019f4a0f86a2f73b6c52acde24a593241aca0b8e73dba34df0a73 \
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
  --resume-converged \
  --resume-iteration 49 \
  --resume-current-slot next \
  --resume-bdd-slot a

sha256sum "${output}" "${arbitrary}"
echo 'sniper_ghost_same_remap_auth_v14 complete 1'
