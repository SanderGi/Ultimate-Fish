#!/usr/bin/env bash
# Re-verify the retained opposed Rook/Ghost fixed point under the sound
# information-dominance singleton contract.  Future private Ghost moves can
# expand a singleton belief, so equality with perfect-information WDL is not a
# valid certificate; the Bellman fixed point and all retained edges are reused.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=8320df74822d873ded883d806b6e10a126646bbd1335f6eb2364d6488b63b0f4
readonly source_version=7e.5ydbHxYLgdIfLshhjQIlZppxvjqqC
readonly source_key="sources/bundles/ghost-singleton-dominance-v3/sha256/${source_sha}/ultimatefish-ghost-singleton-dominance-v3-source.tar"
readonly model_sha=5deb367a58b100f8da7e318910d956b03dd5ef383782884718c0a30341073618
readonly old_model_sha=8a3e0596b416054d485ef39e36eee37c3ee25daeeb0ea72fc468200f15e12194
readonly observation_sha=890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
readonly source_root=/mnt/ultimatefish/ghost-singleton-dominance-v3-8320df74-rook
readonly root=/mnt/ultimatefish-penguin/info-remap-fix-v2/krookkghost-fresh-v1
readonly binary="${source_root}/ultimate_ghost_rook_singleton_dominance_v3_linux"
readonly restored="${binary}.restored"
readonly old_prefix="${root}/work/transitions/krookkghost-action-conditioned-v2"
readonly new_prefix="${root}/work/transitions/krookkghost-singleton-dominance-v3"
readonly relative_prefix=work/transitions/krookkghost-singleton-dominance-v3
readonly scratch=work/solve-action-conditioned-v1/krookkghost
readonly output=work/results/krookkghost-singleton-dominance-v3.ufiw
readonly arbitrary=work/results/krookkghost-singleton-dominance-v3.ufgd
readonly log="${root}/work/logs/singleton-dominance-v3.log"
readonly tool_sha=15f5f6228a04a08af3639775b17dd74b30fb7fa856e721d0e0ae8f9d7b9155f0
readonly tool_version=2rsdltkem9gwQxUpcejDVLmwUoW.sVYx
readonly tool="${source_root}/rebind_ultimate_ghost_ordinary_transition_marker.py"

test "$(systemctl show ultimatefish-info-krookkghost-action-conditioned-v2.service -p ActiveState --value)" != active
grep -Fq 'reciprocal_ghost_extra_iteration 23 bdd_nodes 910532134 changed_owner 0 changed_observer 0 changed_visible 0' \
  "${root}/work/logs/action-conditioned-v2.log"
test "$(sha256sum "${root}/tablebases/krookkghost.uftb" | cut -d' ' -f1)" = \
  bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41
test "$(sha256sum "${root}/tablebases/krookk.uftb" | cut -d' ' -f1)" = \
  abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test ! -e "${source_root}"
test ! -e "${log}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
for suffix in header meta strata index blocks verified; do
  test ! -e "${new_prefix}.${suffix}"
done

install -d -m 0755 "${source_root}"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${source_key}" --version-id "${source_version}" \
  "${source_root}/source.tar" >"${source_root}/source-get.json"
test "$(sha256sum "${source_root}/source.tar" | cut -d' ' -f1)" = "${source_sha}"
tar -xf "${source_root}/source.tar" -C "${source_root}"

cd "${source_root}"
taskset -c 4 clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Wno-error=range-loop-construct -include sstream \
  -Isrc/ultimate -Isrc/ultimate/tablebases \
  -DULTIMATE_GHOST_ORDINARY_PIECE=Rook \
  -DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY \
  src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
  src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
  src/ultimate/tablebases/ghost_public_extra_model.cpp \
  src/ultimate/tablebases/external_robdd.cpp \
  src/ultimate/tablebases/ghost_information_probe.cpp \
  src/ultimate/tablebases/information.cpp \
  src/ultimate/position.cpp src/ultimate/nnue.cpp -o "${binary}"
install -d -m 0755 "${source_root}/self-test"
taskset -c 4 "${binary}" --self-test --orientation opposing \
  --scratch "${source_root}/self-test/krookkghost" \
  --input "${root}/tablebases/krookkghost.uftb" \
  --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
  >"${source_root}/self-test.log" 2>&1
test "$(sha256sum "${source_root}/self-test/krookkghost.normalized.uftb" | cut -d' ' -f1)" = \
  790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785

readonly binary_sha="$(sha256sum "${binary}" | cut -d' ' -f1)"
readonly binary_key="sources/binaries/ghost-singleton-dominance-v3/sha256/${binary_sha}/ultimate_ghost_rook_singleton_dominance_v3_linux"
aws s3api put-object --region us-west-2 --bucket "${bucket}" --key "${binary_key}" \
  --body "${binary}" \
  --metadata "sha256=${binary_sha},source-sha256=${source_sha},model-sha256=${model_sha}" \
  >"${source_root}/binary-put.json"
readonly binary_version="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["VersionId"])' "${source_root}/binary-put.json")"
aws s3api get-object --region us-west-2 --bucket "${bucket}" --key "${binary_key}" \
  --version-id "${binary_version}" "${restored}" >"${source_root}/binary-get.json"
test "$(sha256sum "${restored}" | cut -d' ' -f1)" = "${binary_sha}"
cmp "${binary}" "${restored}"

aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "sources/tools/ghost-ordinary-transition-marker-rebind-v1/sha256/${tool_sha}/rebind_ultimate_ghost_ordinary_transition_marker.py" \
  --version-id "${tool_version}" "${tool}" >"${source_root}/rebind-tool-get.json"
test "$(sha256sum "${tool}" | cut -d' ' -f1)" = "${tool_sha}"
chmod 0755 "${tool}"
for suffix in header meta strata index blocks; do
  cp --reflink=always "${old_prefix}.${suffix}" "${new_prefix}.${suffix}"
done
python3 "${tool}" "${old_prefix}" "${new_prefix}" \
  --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
  --old-model-sha256 "${old_model_sha}" --new-model-sha256 "${model_sha}" \
  --observation-sha256 "${observation_sha}" \
  --manifest "${root}/work/rook-singleton-dominance-v3-transition-rebind.json" \
  >"${source_root}/rebind.log"

{
  echo "rook_ghost_singleton_dominance_v3 source_sha256 ${source_sha} source_version ${source_version} binary_sha256 ${binary_sha} binary_version ${binary_version} model_sha256 ${model_sha} iteration 23 checkpoint_preserved 1 transition_payload_changed 0"
  cd "${root}"
  taskset -c 4 "${binary}" --solve --transition-prefix "${relative_prefix}" \
    --orientation opposing --input tablebases/krookkghost.uftb \
    --normalized-input work/self-test-source-order-v6/krookkghost.normalized.uftb \
    --normalized-sha256 790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785 \
    --lower-dragon-table tablebases/krookk.uftb \
    --lower-dragon-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
    --lower-dragon-source-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
    --lower-dragon-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1 \
    --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
    --model-sha256 "${model_sha}" --observation-sha256 "${observation_sha}" \
    --lower-ghost-sidecar tablebases/kghostk.ufgm --scratch "${scratch}" \
    --output "${output}" --output-arbitrary "${arbitrary}" \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 "${observation_sha}" \
    --max-nodes 1250000000 --unique-slots 1073741824 --compact-every 1 \
    --resume-fixed-point --resume-converged --resume-iteration 23 \
    --resume-current-slot current --resume-bdd-slot a
  sha256sum "${output}" "${arbitrary}"
  echo 'rook_ghost_singleton_dominance_v3 complete 1'
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
  --lower-ghost-observation-sha256 "${observation_sha}" --finalize-existing
