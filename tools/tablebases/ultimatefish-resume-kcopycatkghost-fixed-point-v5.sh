#!/usr/bin/env bash
# Continue the opposed Copycat/Ghost fixed point after the v4 independent
# certificate proved that six retained roots were stale under the
# action-conditioned observer recurrence.  The checkpoint and authenticated
# transition payload are retained; only the false --resume-converged shortcut
# is removed.
set -euo pipefail

readonly root=/mnt/ultimatefish/info-remap-fix-v2/kcopycatkghost-fresh-v1
readonly source_root=/mnt/ultimatefish/ghost-copycat-certificate-diagnostic-v4-6e22a711
readonly binary="${source_root}/ultimate_ghost_copycat_certificate_diagnostic_v4_linux"
readonly binary_sha=2accbad1751fd676774fe77c0ef52c597e607cb9169c7ee3ac7da6831cfc652e
readonly model_sha=4ca03e2c208de7070d57b2324c4cf60978212bdaaa28a634b4657f9c9f530419
readonly observation_sha=890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
readonly log="${root}/work/logs/fixed-point-v5.log"
readonly output=work/results/kcopycatkghost-fixed-point-v5.ufiw
readonly arbitrary=work/results/kcopycatkghost-fixed-point-v5.ufgd

test "$(systemctl show ultimatefish-info-kcopycatkghost-certificate-diagnostic-v4.service -p ActiveState --value)" != active
grep -Fq 'ghost_extra_external_symbolic_certificate bellman_residual 6 monotonicity_residual 0' \
  "${root}/work/logs/certificate-diagnostic-v4.log"
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = "${binary_sha}"
test ! -e "${log}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"

{
  echo "copycat_ghost_fixed_point_v5 binary_sha256 ${binary_sha} model_sha256 ${model_sha} resume_iteration 53 current_slot next bdd_slot a checkpoint_preserved 1 transition_payload_changed 0"
  cd "${root}"
  taskset -c 30 "${binary}" --solve \
    --transition-prefix work/transitions/kcopycatkghost-singleton-dominance-v3 \
    --orientation opposing --input tablebases/kcopycatkghost.uftb \
    --normalized-input work/self-test-source-order-v6/kcopycatkghost.normalized.uftb \
    --normalized-sha256 736075bc025eecff05311bdf2e14b5e0eb2ebfcd6af51e06bb8b94fdcddc8dfb \
    --lower-dragon-table tablebases/kcopycatk.uftb \
    --lower-dragon-sha256 98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb \
    --lower-dragon-source-sha256 98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb \
    --lower-dragon-model-sha256 d94883c4fd100918a23947c3c71d5e461588778aefbbe037f260d48d5083095f \
    --source-sha256 24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50 \
    --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
    --lower-ghost-sidecar tablebases/kghostk.ufgm \
    --scratch work/solve-action-conditioned-v2/kcopycatkghost \
    --output "${output}" --output-arbitrary "${arbitrary}" \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 "${observation_sha}" \
    --max-nodes 500000000 --unique-slots 1073741824 --compact-every 1 \
    --resume-fixed-point --resume-iteration 53 \
    --resume-current-slot next --resume-bdd-slot a
  sha256sum "${output}" "${arbitrary}"
  echo 'copycat_ghost_fixed_point_v5 complete 1'
} >>"${log}" 2>&1

python3 "${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source_root}" --work "${root}" \
  --filename kcopycatkghost.uftb --piece copycat --orientation opposing \
  --source-table "${root}/tablebases/kcopycatkghost.uftb" \
  --source-sha256 24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50 \
  --lower-table "${root}/tablebases/kcopycatk.uftb" \
  --lower-sha256 98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb \
  --lower-model-sha256 d94883c4fd100918a23947c3c71d5e461588778aefbbe037f260d48d5083095f \
  --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
  --lower-ghost-sidecar "${root}/tablebases/kghostk.ufgm" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 "${observation_sha}" --finalize-existing
