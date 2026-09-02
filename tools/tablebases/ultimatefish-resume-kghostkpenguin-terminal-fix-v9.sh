#!/usr/bin/env bash
# Resume the authenticated v9 rebind after its launch wrapper transcribed the
# lower-observation SHA without one character and failed before the solve.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly root=/mnt/ultimatefish-penguin/opposed-kghostkpenguin-fresh-v5
readonly source_root=/mnt/ultimatefish/ghost-penguin-color-swap-v4-cb96a414
readonly binary="${source_root}/ultimate_ghost_penguin_color_swap_v4_linux"
readonly binary_sha=e94ee29ea1fa1821393e253571357cd785166c20f780695d842c81de77ba1ee1
readonly model_sha=6c78476d6c51a9b5ac990759c6e9c9ee42d25d599e126363340654c00996735d
readonly observation_sha=6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b
readonly old_source_sha=4e968ac9fa26e187a53071d9df99df160c0923de19238341ca803e69662b7719
readonly source_sha=06dc64a6ae06df68df65c9819de7a8abec06c124b59ea5cd5010a60769b84b64
readonly source_version=LPA.Ol.SVqn5RvxaS.qA5DmoLQmyzxxn
readonly source_key="results/concrete/penguin-terminal-fix-v1/sha256/${source_sha}/kghostkpenguin.uftb"
readonly stage="${root}/work/terminal-fix-v1"
readonly restored="${stage}/kghostkpenguin.uftb"
readonly old_prefix="${root}/work/transitions/kghostkpenguin-concrete-current-v1"
readonly new_prefix="${root}/work/transitions/kghostkpenguin-terminal-fix-v1"
readonly relative_prefix=work/transitions/kghostkpenguin-terminal-fix-v1
readonly scratch=work/solve-concrete-current-v1/kghostkpenguin
readonly output=work/results/kghostkpenguin.ufiw
readonly arbitrary=work/results/kghostkpenguin.ufgd
readonly log="${root}/work/logs/terminal-fix-v10.log"
readonly rebind_manifest="${root}/work/penguin-terminal-fix-v1-transition-rebind.json"
readonly tool="${source_root}/rebind_ultimate_ghost_ordinary_transition_marker_v2.py"

test "$(systemctl show ultimatefish-info-kghostkpenguin-terminal-fix-v9.service -p Result --value)" = exit-code
grep -Fq 'lower observation SHA is invalid' "${root}/work/logs/terminal-fix-v9.log"
grep -Fq 'dominance_residual 706536' "${root}/work/logs/concrete-current-v7.log"
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = "${binary_sha}"
test "$(sha256sum "${tool}" | cut -d' ' -f1)" = 598e8a1f8bf21dbdc38e7be8825ad74ffaaa8573c52ab9ad91cb9f228e942101
test ! -e "${log}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test "$(stat -c %s "${restored}")" = 759158448
test "$(sha256sum "${restored}" | cut -d' ' -f1)" = "${source_sha}"
grep -Fq 'remap_residual 0' "${stage}/self-test.log"
test -s "${rebind_manifest}"
test -s "${new_prefix}.verified"

{
  echo "penguin_ghost_terminal_fix_v10 binary_sha256 ${binary_sha} model_sha256 ${model_sha} old_source_sha256 ${old_source_sha} source_sha256 ${source_sha} source_version ${source_version} transition_payload_changed 0 retained_fixed_point_reused 1 v9_invalid_lower_observation_sha_fixed 1"
  cd "${root}"
  taskset -c 2 "${binary}" --solve \
    --orientation opposing --transition-prefix "${relative_prefix}" \
    --lower-dragon-table tablebases/kpenguink.uftb \
    --lower-dragon-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
    --lower-dragon-source-sha256 5abd70c847e799b85ed08c49f01c4e5deb3d253505583b8bc145290a84417edd \
    --lower-dragon-model-sha256 3fe2a86bf9df534b23cc119d075a3230dd4045e22c5fa84a49520a80665e0a84 \
    --source-sha256 "${source_sha}" --model-sha256 "${model_sha}" \
    --observation-sha256 "${observation_sha}" --input "${restored}" \
    --lower-ghost-sidecar tablebases/kghostk.ufgm --scratch "${scratch}" \
    --output "${output}" --output-arbitrary "${arbitrary}" \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --max-nodes 500000000 --unique-slots 1073741824 --compact-every 1
  sha256sum "${output}" "${arbitrary}"
  echo 'penguin_ghost_terminal_fix_v10 solve_complete 1'
} >>"${log}" 2>&1

python3 "${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source_root}" --work "${root}" \
  --filename kghostkpenguin.uftb --piece penguin --orientation opposing \
  --source-table "${restored}" --source-sha256 "${source_sha}" \
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
