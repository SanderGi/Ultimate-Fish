#!/usr/bin/env bash
# Resume the failed opposed Copycat/Ghost fixed point with public
# observations conditioned on the observer's selected action.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=ce18d31a3f25ef589e52cf36b9e40f97318241ad8f18b58f7c645005f4c1038a
readonly source_version=.hehWH.kRWPBKMbZRINzvliH722Og98Q
readonly source_key="sources/bundles/ghost-action-conditioned-v2/sha256/${source_sha}/ultimatefish-ghost-action-conditioned-v2-source.tar"
readonly source_root=/mnt/ultimatefish/ghost-action-conditioned-copycat-v1-ce18d31a
readonly root=/mnt/ultimatefish/info-remap-fix-v2/kcopycatkghost-fresh-v1
readonly binary="${source_root}/ultimate_ghost_copycat_action_conditioned_v1_linux"
readonly restored="${binary}.restored"
readonly source_prefix="${root}/work/transitions/kcopycatkghost-source-order-v6"
readonly destination_prefix="${root}/work/transitions/kcopycatkghost-action-conditioned-v1"
readonly relative_prefix=work/transitions/kcopycatkghost-action-conditioned-v1
readonly old_directory="${root}/work/solve-source-order-v6"
readonly new_directory="${root}/work/solve-action-conditioned-v1"
readonly scratch=work/solve-action-conditioned-v1/kcopycatkghost
readonly output=work/results/kcopycatkghost-action-conditioned-v1.ufiw
readonly arbitrary=work/results/kcopycatkghost-action-conditioned-v1.ufgd
readonly log="${root}/work/logs/action-conditioned-v1.log"
readonly model_sha=c5a05259ddd9dab5bc8a1b74448e516d17240aaa7899287dd98fadc416fa485d
readonly old_model_sha=0f9a93e320f77de4d390b726ef4a34ba9c456e419c33cdb26e098d20280b3f8f
readonly observation_sha=890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
readonly tool_sha=15f5f6228a04a08af3639775b17dd74b30fb7fa856e721d0e0ae8f9d7b9155f0
readonly tool_version=2rsdltkem9gwQxUpcejDVLmwUoW.sVYx
readonly tool="${source_root}/rebind_ultimate_ghost_ordinary_transition_marker.py"
readonly rebind_manifest="${root}/work/copycat-action-conditioned-v1-transition-rebind.json"
readonly checkpoint_manifest="${root}/work/copycat-action-conditioned-v1-checkpoint.sha256"

test ! -e "${source_root}"
test ! -e "${new_directory}"
test ! -e "${log}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${rebind_manifest}"
test ! -e "${checkpoint_manifest}"
grep -Fq 'ghost_extra_singleton_residual 3462776' \
  "${root}/work/logs/solve.log"
grep -Fq \
  'reciprocal_ghost_extra_iteration 52 bdd_nodes 189423279 changed_owner 0 changed_observer 0 changed_visible 0' \
  "${root}/work/logs/solve.log"
test "$(sha256sum "${root}/tablebases/kcopycatkghost.uftb" | cut -d' ' -f1)" = \
  24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50
test "$(sha256sum "${root}/tablebases/kcopycatk.uftb" | cut -d' ' -f1)" = \
  98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb

install -d -m 0755 "${source_root}"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${source_key}" --version-id "${source_version}" \
  "${source_root}/source.tar" >"${source_root}/source-get.json"
test "$(sha256sum "${source_root}/source.tar" | cut -d' ' -f1)" = \
  "${source_sha}"
tar -xf "${source_root}/source.tar" -C "${source_root}"

cd "${source_root}"
taskset -c 30 clang++ \
  -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Wno-error=range-loop-construct -include sstream \
  -Isrc/ultimate -Isrc/ultimate/tablebases \
  -DULTIMATE_GHOST_ORDINARY_PIECE=Copycat \
  -DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY \
  -DULTIMATE_GHOST_EXTRA_IS_COPYCAT \
  src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
  src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
  src/ultimate/tablebases/ghost_public_extra_model.cpp \
  src/ultimate/tablebases/external_robdd.cpp \
  src/ultimate/tablebases/ghost_information_probe.cpp \
  src/ultimate/tablebases/information.cpp \
  src/ultimate/position.cpp src/ultimate/nnue.cpp \
  -o "${binary}"

install -d -m 0755 "${source_root}/self-test"
taskset -c 30 "${binary}" --self-test --orientation opposing \
  --scratch "${source_root}/self-test/kcopycatkghost" \
  --input "${root}/tablebases/kcopycatkghost.uftb" \
  --source-sha256 24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50 \
  >"${source_root}/self-test.log" 2>&1
grep -Fq \
  'ghost_extra_observer_action_conditioning shared_source 1 collapsed_poisoned 1 winning_action_poisoned 0 live_image_cross_action 0 residual 0' \
  "${source_root}/self-test.log"
test "$(sha256sum "${source_root}/self-test/kcopycatkghost.normalized.uftb" | cut -d' ' -f1)" = \
  736075bc025eecff05311bdf2e14b5e0eb2ebfcd6af51e06bb8b94fdcddc8dfb

readonly binary_sha="$(sha256sum "${binary}" | cut -d' ' -f1)"
readonly binary_key="sources/binaries/ghost-action-conditioned-copycat-v1/sha256/${binary_sha}/ultimate_ghost_copycat_action_conditioned_v1_linux"
aws s3api put-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --body "${binary}" \
  --metadata "sha256=${binary_sha},source-sha256=${source_sha},model-sha256=${model_sha}" \
  >"${source_root}/binary-put.json"
readonly binary_version="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["VersionId"])' "${source_root}/binary-put.json")"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${binary_key}" --version-id "${binary_version}" \
  "${restored}" >"${source_root}/binary-get.json"
test "$(sha256sum "${restored}" | cut -d' ' -f1)" = "${binary_sha}"
cmp "${binary}" "${restored}"

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "sources/tools/ghost-ordinary-transition-marker-rebind-v1/sha256/${tool_sha}/rebind_ultimate_ghost_ordinary_transition_marker.py" \
  --version-id "${tool_version}" "${tool}" \
  >"${source_root}/rebind-tool-get.json"
test "$(sha256sum "${tool}" | cut -d' ' -f1)" = "${tool_sha}"
chmod 0755 "${tool}"

# Preserve the failed proof and give the corrected solve private mutable roots.
cp -a --reflink=always "${old_directory}" "${new_directory}"
(
  cd "${new_directory}"
  sha256sum kcopycatkghost.domains kcopycatkghost.owner-current \
    kcopycatkghost.observer-current kcopycatkghost.visible-owner-current \
    kcopycatkghost.visible-observer-current >"${checkpoint_manifest}"
  sha256sum -c "${checkpoint_manifest}"
)
for suffix in header meta strata index blocks; do
  cp --reflink=always "${source_prefix}.${suffix}" \
    "${destination_prefix}.${suffix}"
done
python3 "${tool}" "${source_prefix}" "${destination_prefix}" \
  --source-sha256 24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50 \
  --old-model-sha256 "${old_model_sha}" \
  --new-model-sha256 "${model_sha}" \
  --observation-sha256 "${observation_sha}" \
  --manifest "${rebind_manifest}" >"${source_root}/rebind.log"

{
  echo "copycat_ghost_action_conditioned_v1 source_sha256 ${source_sha} binary_sha256 ${binary_sha} binary_version ${binary_version} model_sha256 ${model_sha} old_model_sha256 ${old_model_sha} old_iteration 52 old_checkpoint_preserved 1 transition_payload_changed 0"
  cd "${root}"
  taskset -c 30 "${binary}" --solve \
    --transition-prefix "${relative_prefix}" --orientation opposing \
    --input tablebases/kcopycatkghost.uftb \
    --normalized-input work/self-test-source-order-v6/kcopycatkghost.normalized.uftb \
    --normalized-sha256 736075bc025eecff05311bdf2e14b5e0eb2ebfcd6af51e06bb8b94fdcddc8dfb \
    --lower-ghost-sidecar tablebases/kghostk.ufgm --scratch "${scratch}" \
    --output "${output}" --output-arbitrary "${arbitrary}" \
    --lower-dragon-table tablebases/kcopycatk.uftb \
    --lower-dragon-sha256 98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb \
    --lower-dragon-source-sha256 98dbd354da959fa923ed9b712c1bde7147f2039a9bcf50306cbdc2a9b3098aeb \
    --lower-dragon-model-sha256 d94883c4fd100918a23947c3c71d5e461588778aefbbe037f260d48d5083095f \
    --source-sha256 24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50 \
    --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 "${observation_sha}" \
    --max-nodes 500000000 --unique-slots 1073741824 --compact-every 1 \
    --resume-fixed-point --resume-iteration 52 \
    --resume-current-slot current --resume-bdd-slot b
  sha256sum "${output}" "${arbitrary}"
  echo 'copycat_ghost_action_conditioned_v1 complete 1'
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
  --lower-ghost-observation-sha256 "${observation_sha}" \
  --finalize-existing
