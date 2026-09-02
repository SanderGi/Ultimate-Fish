#!/usr/bin/env bash
# Continue the opposed Ghost/Penguin solve under the action-conditioned
# observer recurrence.  The v3/v4 verifier wrappers incorrectly declared the
# pre-action-conditioning iteration-82 roots converged and jumped directly to
# singleton certification.  Those roots are a sound monotone checkpoint, but
# they must pass through the corrected Bellman iteration before certification.
set -euo pipefail

readonly root=/mnt/ultimatefish-penguin/opposed-kghostkpenguin-fresh-v5
readonly source_root=/mnt/ultimatefish/ghost-penguin-color-swap-v4-cb96a414
readonly binary="${source_root}/ultimate_ghost_penguin_color_swap_v4_linux"
readonly binary_sha=e94ee29ea1fa1821393e253571357cd785166c20f780695d842c81de77ba1ee1
readonly model_sha=6c78476d6c51a9b5ac990759c6e9c9ee42d25d599e126363340654c00996735d
readonly observation_sha=6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b
readonly prefix=work/transitions/kghostkpenguin-color-swap-v4
readonly log="${root}/work/logs/fixed-point-v5.log"

test "$(systemctl show ultimatefish-info-kghostkpenguin-color-swap-v4.service -p ActiveState --value)" != active
grep -Fq 'ghost_extra_external_resume iteration 82 bdd_slot b current_slot current' \
  "${root}/work/logs/color-swap-v4.log"
grep -Fq 'dominance_residual 956684' \
  "${root}/work/logs/color-swap-v4.log"
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = "${binary_sha}"
test ! -e "${log}"
test ! -e "${root}/work/results/kghostkpenguin.ufiw"
test ! -e "${root}/work/results/kghostkpenguin.ufgd"

{
  echo "penguin_ghost_fixed_point_v5 binary_sha256 ${binary_sha} model_sha256 ${model_sha} resume_iteration 82 current_slot current bdd_slot b checkpoint_preserved 1 transition_payload_changed 0"
  cd "${root}"
  taskset -c 2 "${binary}" --solve \
    --orientation opposing \
    --transition-prefix "${prefix}" \
    --lower-dragon-table tablebases/kpenguink.uftb \
    --lower-dragon-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
    --lower-dragon-source-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
    --lower-dragon-model-sha256 3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84 \
    --source-sha256 d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a \
    --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
    --input tablebases/kghostkpenguin.uftb \
    --lower-ghost-sidecar tablebases/kghostk.ufgm \
    --scratch work/solve/kghostkpenguin \
    --output work/results/kghostkpenguin.ufiw \
    --output-arbitrary work/results/kghostkpenguin.ufgd \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --max-nodes 500000000 --unique-slots 1073741824 --compact-every 1 \
    --resume-fixed-point --resume-iteration 82 \
    --resume-current-slot current --resume-bdd-slot b
  sha256sum work/results/kghostkpenguin.ufiw work/results/kghostkpenguin.ufgd
  echo 'penguin_ghost_fixed_point_v5 complete 1'
} >>"${log}" 2>&1

python3 "${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source_root}" --work "${root}" \
  --filename kghostkpenguin.uftb --piece penguin --orientation opposing \
  --source-table "${root}/tablebases/kghostkpenguin.uftb" \
  --source-sha256 d75915f27619049fa261bb7cc581602d6132f06f395a8f966197145d9b8ed90a \
  --lower-table "${root}/tablebases/kpenguink.uftb" \
  --lower-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
  --lower-model-sha256 3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84 \
  --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
  --lower-ghost-sidecar "${root}/tablebases/kghostk.ufgm" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --finalize-existing
