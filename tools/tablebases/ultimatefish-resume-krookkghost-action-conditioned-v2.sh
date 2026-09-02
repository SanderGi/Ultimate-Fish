#!/usr/bin/env bash
# Continue the v1 corrected solve after rebinding an immutable copy of its
# authenticated transition marker to the corrected solver-model digest.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly root=/mnt/ultimatefish-penguin/info-remap-fix-v2/krookkghost-fresh-v1
readonly source_root=/mnt/ultimatefish/ghost-action-conditioned-v2-ce18d31a
readonly binary="${source_root}/ultimate_ghost_rook_action_conditioned_v1_linux"
readonly binary_sha=8a649973b504314b9226c26ba2a2b1118c2ecaa765873fcbef00c91a1d7d3a1f
readonly source_sha=ce18d31a3f25ef589e52cf36b9e40f97318241ad8f18b58f7c645005f4c1038a
readonly model_sha=8a3e0596b416054d485ef39e36eee37c3ee25daeeb0ea72fc468200f15e12194
readonly old_model_sha=ff692f8217e6327b6ab56ab41ee3b9a8f02eecf5b80fc59599a753f340dd80f5
readonly observation_sha=890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
readonly old_prefix="${root}/work/transitions/krookkghost-source-order-v6"
readonly new_prefix="${root}/work/transitions/krookkghost-action-conditioned-v2"
readonly relative_prefix=work/transitions/krookkghost-action-conditioned-v2
readonly scratch=work/solve-action-conditioned-v1/krookkghost
readonly output=work/results/krookkghost-action-conditioned-v2.ufiw
readonly arbitrary=work/results/krookkghost-action-conditioned-v2.ufgd
readonly log="${root}/work/logs/action-conditioned-v2.log"
readonly tool="${source_root}/rebind_ultimate_ghost_ordinary_transition_marker.py"
readonly tool_sha=15f5f6228a04a08af3639775b17dd74b30fb7fa856e721d0e0ae8f9d7b9155f0
readonly tool_version=2rsdltkem9gwQxUpcejDVLmwUoW.sVYx
readonly rebind_manifest="${root}/work/rook-action-conditioned-v2-transition-rebind.json"

grep -Fq 'Dragon transition marker model-binding residual' \
  "${root}/work/logs/action-conditioned-v1.log"
test "$(sha256sum "${binary}" | cut -d' ' -f1)" = "${binary_sha}"
test "$(sha256sum "${source_root}/source.tar" | cut -d' ' -f1)" = \
  "${source_sha}"
test -d "${root}/work/solve-action-conditioned-v1"
test ! -e "${log}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${rebind_manifest}"
for suffix in header meta strata index blocks verified; do
  test ! -e "${new_prefix}.${suffix}"
done

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "sources/tools/ghost-ordinary-transition-marker-rebind-v1/sha256/${tool_sha}/rebind_ultimate_ghost_ordinary_transition_marker.py" \
  --version-id "${tool_version}" "${tool}" \
  >"${source_root}/rebind-tool-get.json"
test "$(sha256sum "${tool}" | cut -d' ' -f1)" = "${tool_sha}"
chmod 0755 "${tool}"

# The edge graph is immutable. Reflink the five authenticated components and
# issue a new marker whose only byte changes are the 64-byte model field.
for suffix in header meta strata index blocks; do
  cp --reflink=always "${old_prefix}.${suffix}" "${new_prefix}.${suffix}"
done
python3 "${tool}" "${old_prefix}" "${new_prefix}" \
  --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
  --old-model-sha256 "${old_model_sha}" \
  --new-model-sha256 "${model_sha}" \
  --observation-sha256 "${observation_sha}" \
  --manifest "${rebind_manifest}" \
  >"${source_root}/rebind.log"

{
  echo "rook_ghost_action_conditioned_v2 source_sha256 ${source_sha} binary_sha256 ${binary_sha} model_sha256 ${model_sha} old_model_sha256 ${old_model_sha} transition_payload_sha256 c2e24e94af43e2be083c452c658aa30d355f3c5d7aa280fce3938858277ec20a old_iteration 22 old_checkpoint_preserved 1 transition_payload_changed 0"
  cd "${root}"
  taskset -c 4 "${binary}" --solve \
    --transition-prefix "${relative_prefix}" --orientation opposing \
    --lower-dragon-table tablebases/krookk.uftb \
    --lower-dragon-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
    --lower-dragon-source-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
    --lower-dragon-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1 \
    --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
    --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
    --input tablebases/krookkghost.uftb \
    --normalized-input work/self-test-source-order-v6/krookkghost.normalized.uftb \
    --normalized-sha256 790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785 \
    --lower-ghost-sidecar tablebases/kghostk.ufgm --scratch "${scratch}" \
    --output "${output}" --output-arbitrary "${arbitrary}" \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 "${observation_sha}" \
    --max-nodes 1250000000 --unique-slots 1073741824 \
    --compact-every 1 --resume-fixed-point --resume-iteration 22 \
    --resume-current-slot next --resume-bdd-slot b
  sha256sum "${output}" "${arbitrary}"
  echo 'rook_ghost_action_conditioned_v2 complete 1'
} >>"${log}" 2>&1

python3 "${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source_root}" --work "${root}" \
  --filename krookkghost.uftb --piece rook --orientation opposing \
  --source-table "${root}/tablebases/krookkghost.uftb" \
  --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
  --lower-table "${root}/tablebases/krookk.uftb" \
  --lower-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
  --lower-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1 \
  --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
  --lower-ghost-sidecar "${root}/tablebases/kghostk.ufgm" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 "${observation_sha}" \
  --finalize-existing
