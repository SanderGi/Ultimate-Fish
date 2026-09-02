#!/usr/bin/env bash
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly rebind_sha=598e8a1f8bf21dbdc38e7be8825ad74ffaaa8573c52ab9ad91cb9f228e942101
readonly rebind_version=haqGViqTNXjoED4UH0xtwLnr_G5rM8t0
readonly rebind_key="sources/tools/sha256/${rebind_sha}/rebind_ultimate_ghost_ordinary_transition_marker.py"
readonly verify_binary_sha=40d2013cb023f862ac23610a4524a3e9dcf2cfde587b62e0cc1173871b2df7b5
readonly verify_binary_version=XVlM16_jbf.gQvFOe4Zd6N_dGP5lr1M2
readonly verify_binary_key="sources/binaries/ghost-angel-opposed-parallel-verify-v1/sha256/${verify_binary_sha}/ultimate_ghost_ordinary_information_tablebase-angel-opposed-parallel-verify-v1"
readonly build_root=/mnt/ultimatefish/ghost-parallel-v7
readonly binary=${build_root}/ultimate_ghost_ordinary_information_tablebase-angel-opposed-parallel-verify-v1
readonly rebind=${build_root}/rebind_ultimate_ghost_ordinary_transition_marker.py
readonly old_work=${build_root}/kghostkangel-parallel-v21/work
readonly work=${build_root}/kghostkangel-current-source-v23/work
readonly old_scratch=${old_work}/solve/kghostkangel
readonly scratch=${work}/solve/kghostkangel
readonly old_transition=${old_work}/transitions/kghostkangel
readonly transition=${work}/transitions/kghostkangel
readonly input=/mnt/ultimatefish/concrete-ghost-angel-opposed-current-v1/work/outputs/kghostkangel.uftb
readonly lower_ghost=/mnt/ultimatefish/info-substate-corrected-v1/kberserkerghostk-v2-resume-v5/tablebases/kghostk.ufgm
readonly source_sha=55c131bb34977996b086111dbc23094c02a0d175f363de9796d59b37488062db
readonly old_source_sha=ad4be1cce6d7d5911fb91cf2bf5c3274d5eafcde23240a22c5b3dfdfa2507d9c
readonly model_sha=8a69f54a02be837ee83729fdd00a5fec6b149242cc58a126bd6d5f361621341c
readonly observation_sha=6263896741c27752f1510b9ed34d27c561392767b682c2917e7d6bdb5c8fd11b

install -d -m 0755 "${work}/solve" "${work}/results" "${work}/transitions" "${work}/logs"
test "$(sha256sum "${input}" | cut -d ' ' -f 1)" = "${source_sha}"
test "$(sha256sum "${lower_ghost}" | cut -d ' ' -f 1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
if [[ ! -x "${binary}" ]]; then
  aws s3api get-object --bucket "${bucket}" --key "${verify_binary_key}" \
    --version-id "${verify_binary_version}" "${binary}" >/dev/null
  chmod 0755 "${binary}"
fi
test "$(sha256sum "${binary}" | cut -d ' ' -f 1)" = "${verify_binary_sha}"
if [[ ! -f "${rebind}" ]]; then
  aws s3api get-object --bucket "${bucket}" --key "${rebind_key}" \
    --version-id "${rebind_version}" "${rebind}" >/dev/null
fi
test "$(sha256sum "${rebind}" | cut -d ' ' -f 1)" = "${rebind_sha}"

# The fixed point is independent of the concrete WDL payload.  Clone the
# converged iteration-43 checkpoint so the old failed run remains immutable.
if [[ ! -f "${scratch}.checkpoint-cloned" ]]; then
  cp --reflink=always --preserve=all "${old_scratch}.bdd-a.nodes" "${scratch}.bdd-a.nodes"
  cp --reflink=always --preserve=all "${old_scratch}.bdd-a.unique" "${scratch}.bdd-a.unique"
  cp --reflink=always --preserve=all "${old_scratch}.domains" "${scratch}.domains"
  for kind in owner observer visible-owner visible-observer; do
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-current" "${scratch}.${kind}-current"
    cp --reflink=always --preserve=all "${old_scratch}.${kind}-next" "${scratch}.${kind}-next"
  done
  printf '%s\n' 'iteration=43' 'bdd_slot=a' 'current_slot=current' \
    'source_checkpoint=kghostkangel-parallel-v21' >"${scratch}.checkpoint-cloned"
fi

# Transitions depend on geometry/rules, not terminal WDL.  Clone and
# cryptographically rebind their marker to the corrected concrete source.
if [[ ! -f "${transition}.isolated-clone" ]]; then
  for extension in header meta strata index blocks; do
    cp --reflink=always --preserve=all "${old_transition}.${extension}" "${transition}.${extension}"
  done
  python3 "${rebind}" "${old_transition}" "${transition}" \
    --source-sha256 "${old_source_sha}" \
    --new-source-sha256 "${source_sha}" \
    --old-model-sha256 "${model_sha}" \
    --new-model-sha256 "${model_sha}" \
    --observation-sha256 "${observation_sha}" \
    --manifest "${transition}.rebind.json"
  printf '%s\n' "source=${old_transition}" "source_sha256=${source_sha}" \
    >"${transition}.isolated-clone"
fi

exec "${binary}" --solve --orientation opposing \
  --transition-prefix "${transition}" --input "${input}" \
  --lower-ghost-sidecar "${lower_ghost}" --scratch "${scratch}" \
  --output "${work}/results/kghostkangel.ufiw" \
  --output-arbitrary "${work}/results/kghostkangel.ufgd" \
  --lower-dragon-table implicit-draw \
  --lower-dragon-sha256 25a2afa32399de9c487352f23a7f7e84673ca301d75fa4f927d037b3bd8852ee \
  --lower-dragon-source-sha256 25a2afa32399de9c487352f23a7f7e84673ca301d75fa4f927d037b3bd8852ee \
  --lower-dragon-model-sha256 323829264a9c977ccb01a674c2a068bdc29c87d7232fbeb5feb839605264ecfe \
  --source-sha256 "${source_sha}" --model-sha256 "${model_sha}" \
  --observation-sha256 "${observation_sha}" \
  --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --max-nodes 750000000 --unique-slots 1073741824 --compact-every 1 \
  --resume-fixed-point --resume-converged --resume-iteration 43 \
  --resume-current-slot current --resume-bdd-slot a --workers 28
