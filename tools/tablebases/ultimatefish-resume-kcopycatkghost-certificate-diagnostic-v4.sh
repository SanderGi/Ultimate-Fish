#!/usr/bin/env bash
# Re-run the exact opposed Copycat/Ghost certificate with separate Bellman and
# monotonicity residual counters.  The retained converged ROBDD is reused.
set -euo pipefail

readonly bucket=ultimatefish-info-20260808-a4e679c6-831688117652
readonly source_sha=6e22a711f7c3b991167026979869bb7a430661fff5acccd9404662fd2fc65975
readonly source_version=R8Pl.jn0ncKMFkNMuBcIWZ4RyAYYGOO2
readonly source_key="sources/bundles/ghost-copycat-certificate-diagnostic-v4/sha256/${source_sha}/ultimatefish-ghost-copycat-certificate-diagnostic-v4-source.tar"
readonly model_sha=4ca03e2c208de7070d57b2324c4cf60978212bdaaa28a634b4657f9c9f530419
readonly observation_sha=890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23
readonly source_root=/mnt/ultimatefish/ghost-copycat-certificate-diagnostic-v4-6e22a711
readonly root=/mnt/ultimatefish/info-remap-fix-v2/kcopycatkghost-fresh-v1
readonly binary="${source_root}/ultimate_ghost_copycat_certificate_diagnostic_v4_linux"
readonly restored="${binary}.restored"
readonly log="${root}/work/logs/certificate-diagnostic-v4.log"

test "$(systemctl show ultimatefish-info-kcopycatkghost-singleton-dominance-v3.service -p ActiveState --value)" != active
grep -Fq 'reciprocal_ghost_extra_resume_converged iteration 53 independent_bellman_verification 1' \
  "${root}/work/logs/singleton-dominance-v3.log"
grep -Fq 'external Ghost-extra symbolic certificate has a residual' \
  "${root}/work/logs/singleton-dominance-v3.log"
test ! -e "${source_root}"
test ! -e "${log}"

install -d -m 0755 "${source_root}"
aws s3api get-object --region us-west-2 --bucket "${bucket}" \
  --key "${source_key}" --version-id "${source_version}" \
  "${source_root}/source.tar" >"${source_root}/source-get.json"
test "$(sha256sum "${source_root}/source.tar" | cut -d' ' -f1)" = "${source_sha}"
tar -xf "${source_root}/source.tar" -C "${source_root}"

cd "${source_root}"
taskset -c 30 clang++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  -Wno-error=range-loop-construct -include sstream \
  -Isrc/ultimate -Isrc/ultimate/tablebases \
  -DULTIMATE_GHOST_ORDINARY_PIECE=Copycat \
  -DULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY -DULTIMATE_GHOST_EXTRA_IS_COPYCAT \
  src/ultimate/tablebases/ghost_ordinary_information_tablebase.cpp \
  src/ultimate/tablebases/ghost_ordinary_information_solver.cpp \
  src/ultimate/tablebases/ghost_public_extra_model.cpp \
  src/ultimate/tablebases/external_robdd.cpp \
  src/ultimate/tablebases/ghost_information_probe.cpp \
  src/ultimate/tablebases/information.cpp \
  src/ultimate/position.cpp src/ultimate/nnue.cpp -o "${binary}"
install -d -m 0755 "${source_root}/self-test"
taskset -c 30 "${binary}" --self-test --orientation opposing \
  --scratch "${source_root}/self-test/kcopycatkghost" \
  --input "${root}/tablebases/kcopycatkghost.uftb" \
  --source-sha256 24236242cb82f0158d827daeaf0762dee9d551532feaaaedc891bbe8576efe50 \
  >"${source_root}/self-test.log" 2>&1
test "$(sha256sum "${source_root}/self-test/kcopycatkghost.normalized.uftb" | cut -d' ' -f1)" = \
  736075bc025eecff05311bdf2e14b5e0eb2ebfcd6af51e06bb8b94fdcddc8dfb

readonly binary_sha="$(sha256sum "${binary}" | cut -d' ' -f1)"
readonly binary_key="sources/binaries/ghost-copycat-certificate-diagnostic-v4/sha256/${binary_sha}/ultimate_ghost_copycat_certificate_diagnostic_v4_linux"
aws s3api put-object --region us-west-2 --bucket "${bucket}" --key "${binary_key}" \
  --body "${binary}" \
  --metadata "sha256=${binary_sha},source-sha256=${source_sha},model-sha256=${model_sha}" \
  >"${source_root}/binary-put.json"
readonly binary_version="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["VersionId"])' "${source_root}/binary-put.json")"
aws s3api get-object --region us-west-2 --bucket "${bucket}" --key "${binary_key}" \
  --version-id "${binary_version}" "${restored}" >"${source_root}/binary-get.json"
test "$(sha256sum "${restored}" | cut -d' ' -f1)" = "${binary_sha}"
cmp "${binary}" "${restored}"

{
  echo "copycat_ghost_certificate_diagnostic_v4 source_sha256 ${source_sha} source_version ${source_version} binary_sha256 ${binary_sha} binary_version ${binary_version} model_sha256 ${model_sha} iteration 53 checkpoint_preserved 1"
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
    --output work/results/kcopycatkghost-singleton-dominance-v4.ufiw \
    --output-arbitrary work/results/kcopycatkghost-singleton-dominance-v4.ufgd \
    --lower-sidecar-sha256 472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb \
    --lower-source-sha256 11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5 \
    --lower-model-sha256 ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5 \
    --lower-observation-sha256 "${observation_sha}" \
    --max-nodes 500000000 --unique-slots 1073741824 --compact-every 1 \
    --resume-fixed-point --resume-converged --resume-iteration 53 \
    --resume-current-slot next --resume-bdd-slot a
} >"${log}" 2>&1
