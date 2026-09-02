#!/usr/bin/env bash
# Resume the retained opposed Rook/Ghost fixed point with observer-selected
# action/observation relations conditioned on the selected public action.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=ce18d31a3f25ef589e52cf36b9e40f97318241ad8f18b58f7c645005f4c1038a
readonly source_version=.hehWH.kRWPBKMbZRINzvliH722Og98Q
readonly source_key="sources/bundles/ghost-action-conditioned-v2/sha256/${source_sha}/ultimatefish-ghost-action-conditioned-v2-source.tar"
readonly model_sha=8a3e0596b416054d485ef39e36eee37c3ee25daeeb0ea72fc468200f15e12194
readonly source_root=/mnt/ultimatefish/ghost-action-conditioned-v2-ce18d31a
readonly root=/mnt/ultimatefish-penguin/info-remap-fix-v2/krookkghost-fresh-v1
readonly old_directory="${root}/work/solve-source-order-v6"
readonly new_directory="${root}/work/solve-action-conditioned-v1"
readonly scratch=work/solve-action-conditioned-v1/krookkghost
readonly binary="${source_root}/ultimate_ghost_rook_action_conditioned_v1_linux"
readonly restored="${binary}.restored"
readonly log="${root}/work/logs/action-conditioned-v1.log"
readonly output=work/results/krookkghost-action-conditioned-v1.ufiw
readonly arbitrary=work/results/krookkghost-action-conditioned-v1.ufgd
readonly checkpoint_manifest="${root}/work/rook-action-conditioned-v1-checkpoint.sha256"

test ! -e "${source_root}"
test ! -e "${new_directory}"
test ! -e "${log}"
test ! -e "${root}/${output}"
test ! -e "${root}/${arbitrary}"
test ! -e "${checkpoint_manifest}"
test "$(sha256sum "${root}/tablebases/krookkghost.uftb" | cut -d' ' -f1)" = \
  bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41
test "$(sha256sum "${root}/tablebases/krookk.uftb" | cut -d' ' -f1)" = \
  abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44
test "$(sha256sum "${root}/tablebases/kghostk.ufgm" | cut -d' ' -f1)" = \
  472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb
test "$(sha256sum "${root}/work/transitions/krookkghost-source-order-v6.verified" | cut -d' ' -f1)" = \
  e5d675b44ebc9d88c68840eddc31cc5044d9e4121f050b1352c1dcc67ef06351
grep -Fq 'ghost_extra_singleton_residual 4341148' \
  "${root}/work/logs/resume-converged-v8.log"
grep -Fq \
  'reciprocal_ghost_extra_iteration 22 bdd_nodes 480490719 changed_owner 0 changed_observer 0 changed_visible 0' \
  "${root}/work/logs/resume-after-stop-v7.log"
test "$(df --output=avail -B1 "${root}" | tail -1)" -ge 128849018880

install -d -m 0755 "${source_root}"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${source_key}" --version-id "${source_version}" \
  "${source_root}/source.tar" >"${source_root}/source-get.json"
test "$(sha256sum "${source_root}/source.tar" | cut -d' ' -f1)" = \
  "${source_sha}"
tar -xf "${source_root}/source.tar" -C "${source_root}"

cd "${source_root}"
taskset -c 4 clang++ \
  -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
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
  src/ultimate/position.cpp src/ultimate/nnue.cpp \
  -o "${binary}"

install -d -m 0755 "${source_root}/self-test"
taskset -c 4 "${binary}" --self-test --orientation opposing \
  --scratch "${source_root}/self-test/krookkghost" \
  --input "${root}/tablebases/krookkghost.uftb" \
  --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
  >"${source_root}/self-test.log" 2>&1
test "$(sha256sum "${source_root}/self-test/krookkghost.normalized.uftb" | cut -d' ' -f1)" = \
  790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785

readonly binary_sha="$(sha256sum "${binary}" | cut -d' ' -f1)"
readonly binary_key="sources/binaries/ghost-action-conditioned-v1/sha256/${binary_sha}/ultimate_ghost_rook_action_conditioned_v1_linux"
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

# Keep every byte of the failed proof tree.  XFS reflinks give the corrected
# recurrence an independent mutable root/ROBDD set without duplicating the
# retained checkpoint until a block actually changes.
cp -a --reflink=always "${old_directory}" "${new_directory}"
(
  cd "${new_directory}"
  sha256sum krookkghost.domains krookkghost.owner-next \
    krookkghost.observer-next krookkghost.visible-owner-next \
    krookkghost.visible-observer-next >"${checkpoint_manifest}"
  sha256sum -c "${checkpoint_manifest}"
)

{
  echo "rook_ghost_action_conditioned_v1 source_sha256 ${source_sha} source_version ${source_version} binary_sha256 ${binary_sha} binary_version ${binary_version} model_sha256 ${model_sha} transition_payload_sha256 c2e24e94af43e2be083c452c658aa30d355f3c5d7aa280fce3938858277ec20a old_iteration 22 old_checkpoint_preserved 1"
  cd "${root}"
  taskset -c 4 "${binary}" --solve \
    --transition-prefix work/transitions/krookkghost-source-order-v6 \
    --orientation opposing \
    --lower-dragon-table tablebases/krookk.uftb \
    --lower-dragon-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
    --lower-dragon-source-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
    --lower-dragon-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1 \
    --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
    --model-sha256 "${model_sha}" \
    --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --input tablebases/krookkghost.uftb \
    --normalized-input work/self-test-source-order-v6/krookkghost.normalized.uftb \
    --normalized-sha256 790d422dd9b80c916a9752b22ec4fe2a9e87c8bccce23c7179155366513a3785 \
    --lower-ghost-sidecar tablebases/kghostk.ufgm \
    --scratch "${scratch}" \
    --output "${output}" --output-arbitrary "${arbitrary}" \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
    --max-nodes 1250000000 --unique-slots 1073741824 \
    --compact-every 1 --resume-fixed-point --resume-iteration 22 \
    --resume-current-slot next --resume-bdd-slot b
  sha256sum "${output}" "${arbitrary}"
  echo 'rook_ghost_action_conditioned_v1 complete 1'
} >>"${log}" 2>&1

python3 "${source_root}/tools/tablebases/run_ultimate_ghost_ordinary_aws.py" \
  --source-root "${source_root}" --work "${root}" \
  --filename krookkghost.uftb --piece rook --orientation opposing \
  --source-table "${root}/tablebases/krookkghost.uftb" \
  --source-sha256 bc8bf23a28159535dd64ea7b5d420788e581da4b376b0b1dec273aaf42e09f41 \
  --lower-table "${root}/tablebases/krookk.uftb" \
  --lower-sha256 abeef6191dd9baa87918f74a379592dde9683be81675691e8d8e6550dcd1ce44 \
  --lower-model-sha256 8529cd2c84387a6d7e1baf7e2bd93913ab3b733c61e29d285b93f4c0c1854bc1 \
  --model-sha256 "${model_sha}" \
  --observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --lower-ghost-sidecar "${root}/tablebases/kghostk.ufgm" \
  --lower-ghost-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
  --lower-ghost-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
  --lower-ghost-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
  --lower-ghost-observation-sha256 890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23 \
  --finalize-existing

python3 - "${source_root}/build-certificate.json" <<PY
import json, pathlib
path = pathlib.Path(__import__('sys').argv[1])
path.write_text(json.dumps({
    'schema': 'ultimate-ghost-action-conditioned-build-v1',
    'source_sha256': '${source_sha}',
    'source_version_id': '${source_version}',
    'binary_sha256': '${binary_sha}',
    'binary_version_id': '${binary_version}',
    'model_sha256': '${model_sha}',
    'old_checkpoint_preserved': True,
    'checkpoint_manifest_sha256': __import__('hashlib').sha256(
        pathlib.Path('${checkpoint_manifest}').read_bytes()).hexdigest(),
    'observer_action_conditioning_residual': 0,
}, sort_keys=True, indent=2) + '\n')
PY
